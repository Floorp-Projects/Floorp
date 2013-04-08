#include <Windows.h>
#include <Shlwapi.h>
#include <strsafe.h>

// Linker options
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:wmainCRTStartup")

#define NUM_MAX_PATH_BYTES sizeof(wchar_t) * MAX_PATH + 1
#define PROFILE_ARG L" -profile "

// Options that can be overridden at build time with -D
#ifndef B2G_NAME
#define B2G_NAME L"b2g.exe"
#endif
#ifndef GAIA_PATH
#define GAIA_PATH L"gaia\\profile"
#endif

void error(wchar_t* msg){
    MessageBoxW(nullptr, msg, L"Error starting program", MB_OK | MB_ICONERROR);
}

/* This function takes a string which represents a windows path, orig.
 * The file portion of the path is stripped off and replaced with the
 * path component, file.  This function returns a string which represents
 * a windows path with the modifications requested and needs to be freed
 * explicitly by the calling function.
 */
wchar_t* make_path_with_leaf_file_name(wchar_t* orig, wchar_t* file){
    wchar_t* buffer = (wchar_t*) malloc(NUM_MAX_PATH_BYTES);
    if (!buffer) {
        return nullptr;
    }
    if (FAILED(StringCchCopyW(buffer, NUM_MAX_PATH_BYTES, orig))) {
        error(L"Error copying string");
        free(buffer);
        buffer = nullptr;
    }
    PathRemoveFileSpecW(buffer);
    if (!PathAppendW(buffer, file)) {
        error(L"Unable to append file to directory");
        free(buffer);
        buffer = nullptr;
    }
    return buffer;
}

BOOL execute(wchar_t* binary_path, wchar_t* args, int cp_flags) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    
    if (!CreateProcessW(
        binary_path,
        args,
        nullptr,
        nullptr,
        FALSE,
        cp_flags,
        nullptr,
        nullptr,
        &si,
        &pi)){
            error(L"Could not execute program");
            return FALSE;
    }

    WaitForInputIdle(pi.hProcess, 0);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

int wmain(int argc, wchar_t *argv[], wchar_t *envp[]){
    int cp_flags;
    wchar_t* b2g_path = make_path_with_leaf_file_name(argv[0], B2G_NAME);
    wchar_t* profile_path = make_path_with_leaf_file_name(argv[0], GAIA_PATH);
	// 10 chars for the ' -profile ' portion of the argument
    wchar_t* args = (wchar_t*) malloc(2 * NUM_MAX_PATH_BYTES + wcslen(PROFILE_ARG));
    if (FAILED(StringCchPrintfW(args, NUM_MAX_PATH_BYTES, L"\"%ws\"%ws\"%ws\"", b2g_path, PROFILE_ARG, profile_path))) {
        error(L"Could not create argument string");
        ExitProcess(1);
    }
#ifdef SHOW_CONSOLE
    cp_flags = 0;
#else
    cp_flags = DETACHED_PROCESS;
#endif
    if (!execute(b2g_path, args, cp_flags)) {
        error(L"Failed to launch program");
    }
    free(profile_path);
    free(b2g_path);
    free(args);
    profile_path = b2g_path = args = nullptr;
    
}
