/*
   THIS FILE IS A PART OF GTA V SCRIPT HOOK SDK
			   http://dev-c.com
		   (C) Alexander Blade 2015 (modified)
*/

/*
 Reworked to run the script DLL without relying on ScriptHookV.
 Instead of calling scriptRegister/scriptUnregister (which are provided
 by ScriptHookV), we create a dedicated thread on DLL attach that runs
 `ScriptMain()` directly.
*/

#include "lib\ScriptHookV_SDK\inc\main.h"
#include "script.h"
#include "utils.h"
#include <windows.h>
#include <atomic>
#include <format>
#include <sstream>
#include <iostream>

static HANDLE g_scriptThread = nullptr;
static std::atomic_bool g_shouldTerminate{ false };

static DWORD WINAPI ScriptThreadProc(LPVOID)
{
	// Run the script entrypoint
	__try
	{
		ScriptMain();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		// swallow exceptions to avoid crashing host process
	}
	return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hModule);

		g_scriptThread = CreateThread(
			nullptr,
			0,
			ScriptThreadProc,
			nullptr,
			0,
			nullptr);

		if (!g_scriptThread)
		{
			// If thread creation fails, there's not much we can do here.
		}
		break;

	case DLL_PROCESS_DETACH:
		g_shouldTerminate.store(true);

		// Wait a short time for the script thread to exit cleanly.
		if (g_scriptThread)
		{
			WaitForSingleObject(g_scriptThread, 5000);
			CloseHandle(g_scriptThread);
			g_scriptThread = nullptr;
		}
		break;
	}
	return TRUE;
}

/// <summary>
/// The audioTimeout field controls something related to the synchronization of the game thread and the audio thread. 
/// This occassionally causes stutters at high FPS or when the game is paused. Setting it to 0 bypasses this synchronization check
/// and fixes the stutters.
/// </summary>
bool enableStutterFix = true;

/// <summary>
/// The game is capped to 188FPS by the audio engine because of an engine setting called rage::g_audUseFrameLimiter.
/// Disabling this audio frame limiter uncaps the FPS of the game up to another limit of 270fps, imposed by CSystem::EndFrame.
/// As of version 1.1 we also remove that frame limiter to truly unlock the fps.
/// </summary>
bool uncapFPS = true;

void main()
{
	static bool useSynchronousAudio = true;
	static bool lastUseSynchronousAudio = false;

	uintptr_t base = (uintptr_t)GetModuleHandle(NULL);

	/*
	*\tAdapted from the CitizenFX project, retrieved 2022-12-04.
	*\tYou can view the original project and license terms at:
	*\thttps://runtime.fivem.net/fivem-service-agreement-4.pdf
	*\thttps://github.com/citizenfx/fivem/blob/master/code/LICENSE
	*/
	const char* asynchronousAudioPattern = "E8 ? ? ? ? 40 38 35 ? ? ? ? 75 05";
	const char* audioTimeoutPattern = "8B 15 ? ? ? ? 41 03 D6 3B";

	/*
	*\tCredit to Special For for finding this CSystem frame limiter sleep loop and
	*/
	const char* frameLimiterPattern = "F3 44 0F 59 05 ? ? ? ? 0F 28 C7 F3 41 0F 58 C0 0F 2F C6 72 ? E8";
	const char* audioLimiter2Pattern = "48 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 40 38 35 ?? ?? ?? ?? 75 ?? 40 38 35 ?? ?? ?? ?? 75 ?? E8 ?? ?? ?? ?? 84 C0";

	bool* asynchronousAudio = get_address<bool*>((uintptr_t)PatternScan(GetModuleHandleW(NULL), asynchronousAudioPattern) + 8);
	int* audioTimeout = get_address<int*>((uintptr_t)PatternScan(GetModuleHandleW(NULL), audioTimeoutPattern) + 2);

	uint8_t* frameLimiterLoop = (uint8_t*)(PatternScan(GetModuleHandleW(NULL), frameLimiterPattern));
	uint8_t* audioLimiter2 = (uint8_t*)(PatternScan(GetModuleHandleW(NULL), audioLimiter2Pattern));
	if (uncapFPS) {
		*asynchronousAudio = false;

		// If you load a save, these will be null because we have already patched that code and the pattern doesn't match
		if (frameLimiterLoop && audioLimiter2) {
			nop(frameLimiterLoop + 20, 2);
			nop(audioLimiter2 + 7, 5);
		}
	}

	if (enableStutterFix) {
		*audioTimeout = 0;
	}
}

extern "C" void ScriptMain()
{
	main();
}
