/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is MOZCE Lib.
 *
 * The Initial Developer of the Original Code is Doug Turner <dougt@meer.net>.

 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  John Wolfe <wolfe@lobo.us>
 * 
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#ifndef MOZCE_SHUNT_H
#define MOZCE_SHUNT_H

// This is to silence the #pragma deprecated warnings
#pragma warning(disable: 4068)

#include "mozce_defs.h"

#include <commdlg.h>
#include <stdio.h>
//////////////////////////////////////////////////////////
// Function Mapping
//////////////////////////////////////////////////////////

#ifndef MOZCE_SHUNT_EXPORTS

#define _mkdir		mkdir
#define _rmdir		rmdir
#define _chmod		chmod
#define _isatty		isatty
#undef fileno
#define fileno      (int)_fileno
#define _mbctolower tolower 
#define _mbsicmp    mbsicmp
#define _mbsdec     mbsdec
#define _getpid		getpid
#define _access		access
#define	_fdopen		fdopen
#define _getcwd     getcwd
#define _open open
#define _fullpath	fullpath
#define _splitpath	splitpath
#define _makepath	makepath
#define lstrlenA  strlen
#define lstrcpyA  strcpy
#define lstrcmpA  strcmp
#define lstrcmpiA strcmpi
#define lstrcatA  strcat
#define strdup      _strdup
#define stricmp     _stricmp
#define strcmpi     _stricmp
#define strnicmp    _strnicmp
#ifdef localeconv
#undef localeconv
#endif
#define localeconv  mozce_localeconv


// From win32.cpp

#define _beginthreadex(security, stack_size, start_proc, arg, flags, pid) \
        CreateThread(security, stack_size,(LPTHREAD_START_ROUTINE) start_proc, arg, flags, pid) 

#define ExpandEnvironmentStrings   ExpandEnvironmentStringsA

#ifdef GetMessageA
#undef GetMessageA
#endif
#define GetMessageA               GetMessageW
#define PostMessageA              PostMessageW
#define GetShortPathNameA         GetShortPathName

#define PeekMessageA              PeekMessageW

#define ShellExecuteA             ShellExecute

// We use a method named CreateEvent.  We do not want to map
// CreateEvent to CreateEventA
#ifdef CreateEvent
#undef CreateEvent
#endif
#define CreateEvent               CreateEvent

#define GetSystemDirectory        GetSystemDirectoryA
#define GetWindowsDirectory       GetWindowsDirectoryA
#define SetCurrentDirectory       SetCurrentDirectoryA
#define SetEnvironmentVariable    SetEnvironmentVariableA
#define CreateDialogIndirectParamA CreateDialogIndirectParamW
#define SystemParametersInfoA      SystemParametersInfoW
#define DispatchMessageA           DispatchMessageW
#define CallWindowProcA            CallWindowProcW
#define GetWindowLongA             GetWindowLongW
#define SetWindowLongA             SetWindowLongW

#define GetProp                   GetPropA
#define SetProp                   SetPropA
#define RemoveProp                RemovePropA

#define GetCurrentDirectory       GetCurrentDirectoryW
#define OpenSemaphore             OpenSemaphoreW
#define SetCurrentDirectoryW      SetCurrentDirectoryW

#define SetWorldTransform         SetWorldTransform
#define GetWorldTransform         GetWorldTransform
#define SetGraphicsMode           SetGraphicsMode

#define ScriptFreeCache           ScriptFreeCache

#undef GetProcAddress
#define GetProcAddress            GetProcAddressA

#define GetMonitorInfoW           GetMonitorInfo

// OutlineTextMetrics are tricky. It is a pain to convert W->A text metrics,
// and we have no A functions. Because we do not define UNICODE, headers specify A function versions
// We can override the function defines here, but can't override the typedefs (typedef TEXTMETRICW TEXTMETRIC)
// A define is a hack around this, but it might fail elsewhere.
// dougt code review
#undef GetOutlineTextMetrics
#define GetOutlineTextMetrics     GetOutlineTextMetricsW
#undef GetTextMetrics
#define GetTextMetrics            GetTextMetricsW
#define OUTLINETEXTMETRIC         OUTLINETEXTMETRICW
#define TEXTMETRIC                TEXTMETRICW
#undef wsprintf
#define wsprintf                wsprintfW

#endif // MOZCE_SHUNT_EXPORTS

//////////////////////////////////////////////////////////
// Function Declarations
//////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C" {
#endif

  // From assert.cpp
  MOZCE_SHUNT_API void mozce_assert(int inExpression);
  
  // From direct.cpp
  MOZCE_SHUNT_API int mkdir(const char* inDirname);
  MOZCE_SHUNT_API int rmdir(const char* inDirname);
  
  // From errno.cpp
  extern MOZCE_SHUNT_API int errno;
  
  // From io.cpp
  MOZCE_SHUNT_API int chmod(const char* inFilename, int inMode);
  MOZCE_SHUNT_API int isatty(int inHandle);

  MOZCE_SHUNT_API struct protoent* getprotobyname(const char* inName);
  MOZCE_SHUNT_API struct protoent* getprotobynumber(int inNumber);
  
  // From mbstring.cpp
  MOZCE_SHUNT_API unsigned char* _mbsinc(const unsigned char* inCurrent);
  MOZCE_SHUNT_API unsigned char* _mbspbrk(const unsigned char* inString, const unsigned char* inStrCharSet);
  MOZCE_SHUNT_API unsigned char* mbschr(const unsigned char* inString, unsigned int inC);
  MOZCE_SHUNT_API unsigned char* mbsrchr(const unsigned char* inString, unsigned int inC);
  MOZCE_SHUNT_API int            mbsicmp(const unsigned char *string1, const unsigned char *string2);
  MOZCE_SHUNT_API unsigned char* mbsdec(const unsigned char *string1, const unsigned char *string2);
  
  // From process.cpp
  MOZCE_SHUNT_API void abort(void);
  MOZCE_SHUNT_API char* getenv(const char* inName);
  MOZCE_SHUNT_API int putenv(const char *a);
  MOZCE_SHUNT_API int getpid(void);

  // From signal.cpp
  MOZCE_SHUNT_API int raise(int inSignal);
  MOZCE_SHUNT_API _sigsig signal(int inSignal, _sigsig inFunc);
  
  // From stat.cpp
  MOZCE_SHUNT_API int stat(const char *inPath, struct stat * outStat);
  
  // From stdio.cpp
  MOZCE_SHUNT_API int access(const char *path, int mode);
  MOZCE_SHUNT_API void rewind(FILE* inStream);
  MOZCE_SHUNT_API FILE* fdopen(int inFD, const char* inMode);
  MOZCE_SHUNT_API void perror(const char* inString);
  MOZCE_SHUNT_API int remove(const char* inPath);

  MOZCE_SHUNT_API char* getcwd(char* buff, size_t size);
  
  MOZCE_SHUNT_API int mozce_printf(const char *, ...);

  MOZCE_SHUNT_API int open(const char *pathname, int flags, int mode);
  MOZCE_SHUNT_API int close(int fp);
  MOZCE_SHUNT_API size_t read(int fp, void* buffer, size_t count);
  MOZCE_SHUNT_API size_t write(int fp, const void* buffer, size_t count);
  MOZCE_SHUNT_API int unlink(const char *pathname);
  MOZCE_SHUNT_API int lseek(int fildes, int offset, int whence);


  // From stdlib.cpp
  MOZCE_SHUNT_API void splitpath(const char* inPath, char* outDrive, char* outDir, char* outFname, char* outExt);
  MOZCE_SHUNT_API void makepath(char* outPath, const char* inDrive, const char* inDir, const char* inFname, const char* inExt);
  MOZCE_SHUNT_API char* fullpath(char *, const char *, size_t);
  MOZCE_SHUNT_API int _unlink(const char *filename );

  // From string.cpp
  MOZCE_SHUNT_API char* strerror(int);

  // From time.cpp
  MOZCE_SHUNT_API struct tm* localtime_r(const time_t* inTimeT,struct tm* outRetval);
  MOZCE_SHUNT_API struct tm* gmtime_r(const time_t* inTimeT, struct tm* outRetval);

  MOZCE_SHUNT_API wchar_t *_wgetcwd(wchar_t *buffer,int maxlen);

  MOZCE_SHUNT_API struct lconv * mozce_localeconv(void);
       
  MOZCE_SHUNT_API BOOL CreatePipe(PHANDLE hReadPipe, PHANDLE hWritePipe, LPSECURITY_ATTRIBUTES lpPipeAttributes, DWORD nSize);

  MOZCE_SHUNT_API DWORD_PTR SetThreadAffinityMask(HANDLE hThread, DWORD_PTR dwThreadAffinityMask);
  MOZCE_SHUNT_API BOOL GetProcessAffinityMask(HANDLE hProcess, PDWORD_PTR lpProcessAffinityMask, PDWORD_PTR lpSystemAffinityMask);

  VOID CALLBACK LineDDAProc(int X, int Y, LPARAM lpData);
  typedef void (*mozce_LINEDDAPROC) (int X, int Y, LPARAM lpData);

  MOZCE_SHUNT_API int MulDiv(int inNumber, int inNumerator, int inDenominator);
  MOZCE_SHUNT_API int GetDIBits(HDC inDC, HBITMAP inBMP, UINT inStartScan, UINT inScanLines, LPVOID inBits, LPBITMAPINFO inInfo, UINT inUsage);
  MOZCE_SHUNT_API int SetDIBits(HDC inDC, HBITMAP inBMP, UINT inStartScan, UINT inScanLines, CONST LPVOID inBits, CONST LPBITMAPINFO inInfo, UINT inUsage);
  MOZCE_SHUNT_API DWORD CommDlgExtendedError(void);
  MOZCE_SHUNT_API HBITMAP CreateDIBitmap(HDC inDC, CONST BITMAPINFOHEADER *inBMIH, DWORD inInit, CONST VOID *inBInit, CONST BITMAPINFO *inBMI, UINT inUsage);
  MOZCE_SHUNT_API int SetPolyFillMode(HDC inDC, int inPolyFillMode);
  MOZCE_SHUNT_API int SetStretchBltMode(HDC inDC, int inStretchMode);
  MOZCE_SHUNT_API int ExtSelectClipRgn(HDC inDC, HRGN inRGN, int inMode);
  MOZCE_SHUNT_API DWORD ExpandEnvironmentStrings(LPCTSTR lpSrc, LPTSTR lpDst, DWORD nSize);
  MOZCE_SHUNT_API DWORD ExpandEnvironmentStringsW(const unsigned short * lpSrc, const unsigned short * lpDst, DWORD nSize);

  MOZCE_SHUNT_API BOOL LineDDA(int inXStart, int inYStart, int inXEnd, int inYEnd, LINEDDAPROC inLineFunc, LPARAM inData);
  MOZCE_SHUNT_API int FrameRect(HDC inDC, CONST RECT *inRect, HBRUSH inBrush);
  MOZCE_SHUNT_API BOOL GdiFlush(void);
  MOZCE_SHUNT_API int SetArcDirection(HDC inDC, int inArcDirection);
  MOZCE_SHUNT_API BOOL Arc(HDC inDC, int inLeftRect, int inTopRect, int inRightRect, int inBottomRect, int inXStartArc, int inYStartArc, int inXEndArc, int inYEndArc);
  MOZCE_SHUNT_API BOOL Pie(HDC inDC, int inLeftRect, int inTopRect, int inRightRect, int inBottomRect, int inXRadial1, int inYRadial1, int inXRadial2, int inYRadial2);
  MOZCE_SHUNT_API UINT GetDriveType(const char* lpRootPathName);

  MOZCE_SHUNT_API DWORD GetFontData(HDC inDC, DWORD inTable, DWORD inOffset, LPVOID outBuffer, DWORD inData);
  MOZCE_SHUNT_API UINT GetTextCharset(HDC inDC);
  MOZCE_SHUNT_API UINT GetTextCharsetInfo(HDC inDC, LPFONTSIGNATURE outSig, DWORD inFlags);
  MOZCE_SHUNT_API int GetMapMode(HDC inDC);
  MOZCE_SHUNT_API BOOL GetIconInfo(HICON inIcon, PICONINFO outIconinfo);
  MOZCE_SHUNT_API BOOL LPtoDP(HDC inDC, LPPOINT inoutPoints, int inCount);
  MOZCE_SHUNT_API LONG RegCreateKey(HKEY inKey, LPCTSTR inSubKey, PHKEY outResult);
  MOZCE_SHUNT_API BOOL WaitMessage(VOID);
  MOZCE_SHUNT_API BOOL FlashWindow(HWND inWnd, BOOL inInvert);
  MOZCE_SHUNT_API BOOL EnumChildWindows(HWND inParent, WNDENUMPROC inFunc, LPARAM inParam);
  MOZCE_SHUNT_API BOOL EnumThreadWindows(DWORD inThreadID, WNDENUMPROC inFunc, LPARAM inParam);
  MOZCE_SHUNT_API BOOL IsIconic(HWND inWnd);
  MOZCE_SHUNT_API BOOL OpenIcon(HWND inWnd);
  MOZCE_SHUNT_API HANDLE OpenFileMapping(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCTSTR lpName);
  MOZCE_SHUNT_API HHOOK SetWindowsHookEx(int inType, void* inFunc, HINSTANCE inMod, DWORD inThreadId);
  MOZCE_SHUNT_API HINSTANCE ShellExecute(HWND hwnd, LPCSTR lpOperation, LPCSTR lpFile, LPCSTR lpParameters, LPCSTR lpDirectory, INT nShowCmd);
  MOZCE_SHUNT_API HINSTANCE ShellExecuteW(HWND hwnd, LPCWSTR lpOperation, LPCWSTR lpFile, LPCWSTR lpParameters, LPCWSTR lpDirectory, INT nShowCmd);
  MOZCE_SHUNT_API BOOL UnhookWindowsHookEx(HHOOK inHook);
  MOZCE_SHUNT_API LRESULT CallNextHookEx(HHOOK inHook, int inCode, WPARAM wParam, LPARAM lParam);

  MOZCE_SHUNT_API BOOL GetWindowPlacement(HWND window, WINDOWPLACEMENT *lpwndpl);
  MOZCE_SHUNT_API BOOL InvertRgn(HDC inDC, HRGN inRGN);
  MOZCE_SHUNT_API int GetScrollPos(HWND inWnd, int inBar);
  MOZCE_SHUNT_API BOOL GetScrollRange(HWND inWnd, int inBar, LPINT outMinPos, LPINT outMaxPos);
  MOZCE_SHUNT_API HRESULT CoLockObjectExternal(IUnknown* inUnk, BOOL inLock, BOOL inLastUnlockReleases);
  MOZCE_SHUNT_API HRESULT CoInitialize(LPVOID pvReserved);
  MOZCE_SHUNT_API LRESULT OleInitialize(LPVOID pvReserved);
  MOZCE_SHUNT_API void    OleUninitialize();;
  MOZCE_SHUNT_API HRESULT OleSetClipboard(IDataObject* inDataObj);
  MOZCE_SHUNT_API HRESULT OleGetClipboard(IDataObject** outDataObj);
  MOZCE_SHUNT_API HRESULT OleFlushClipboard(void);
  MOZCE_SHUNT_API HRESULT OleQueryLinkFromData(IDataObject* inSrcDataObject);

  //MOZCE_SHUNT_API void* mozce_SHBrowseForFolder(void* /*LPBROWSEINFOS*/ inBI);
  MOZCE_SHUNT_API BOOL SetMenu(HWND inWnd, HMENU inMenu);
  MOZCE_SHUNT_API BOOL GetUserName(LPTSTR inBuffer, LPDWORD inoutSize);
  MOZCE_SHUNT_API DWORD GetShortPathName(LPCTSTR inLongPath, LPTSTR outShortPath, DWORD inBufferSize);
  MOZCE_SHUNT_API DWORD GetShortPathNameW(LPCWSTR aLPATH, LPWSTR aSPATH, DWORD aLen);

  MOZCE_SHUNT_API DWORD GetEnvironmentVariable(LPCSTR lpName, LPCSTR lpBuffer, DWORD nSize);
  MOZCE_SHUNT_API DWORD GetEnvironmentVariableW(LPCWSTR lpName, LPWSTR lpBuffer, DWORD nSize);
  MOZCE_SHUNT_API HMENU LoadMenuA(HINSTANCE hInstance, LPCSTR lpMenuName);

  MOZCE_SHUNT_API void GetSystemTimeAsFileTime(LPFILETIME lpSystemTimeAsFileTime);
  MOZCE_SHUNT_API DWORD GetFullPathName(const char* lpFileName, DWORD nBufferLength, const char* lpBuffer, const char** lpFilePart);

  MOZCE_SHUNT_API UINT GetACP(void);

  MOZCE_SHUNT_API BOOL mozce_PeekMessage(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg);
  MOZCE_SHUNT_API BOOL mozce_GetMessage(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax);
  MOZCE_SHUNT_API LONG GetMessageTime(void);
  
  MOZCE_SHUNT_API BOOL LockFile(HANDLE hFile, DWORD dwFileOffsetLow, DWORD dwFileOffsetHigh,
                                      DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh);
  MOZCE_SHUNT_API BOOL UnlockFile(HANDLE hFile, DWORD dwFileOffsetLow, DWORD dwFileOffsetHigh,
                                        DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh);
  MOZCE_SHUNT_API BOOL SetWorldTransform(HDC hdc, CONST XFORM *lpXform );
  MOZCE_SHUNT_API BOOL GetWorldTransform(HDC hdc, LPXFORM lpXform );
  MOZCE_SHUNT_API int  SetGraphicsMode(HDC hdc, int iMode);

  // from win32a.cpp
  
  MOZCE_SHUNT_API BOOL SetHandleInformation(HANDLE hObject, DWORD dwMask, DWORD dwFlags);
  MOZCE_SHUNT_API BOOL GetHandleInformation(HANDLE hObject, LPDWORD lpdwFlags);

  MOZCE_SHUNT_API DWORD GetGlyphOutlineA(HDC inDC, CHAR inChar, UINT inFormat, void* inGM, DWORD inBufferSize, LPVOID outBuffer, CONST MAT2* inMAT2);

  MOZCE_SHUNT_API ATOM GlobalAddAtomA(LPCSTR lpString);
  MOZCE_SHUNT_API ATOM RegisterClassA(CONST WNDCLASSA *lpwc);
  MOZCE_SHUNT_API BOOL CopyFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, BOOL bFailIfExists);
  MOZCE_SHUNT_API BOOL CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
  MOZCE_SHUNT_API BOOL RemoveDirectoryA(LPCSTR lpPathName);
  MOZCE_SHUNT_API HANDLE CreateMutexA(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCSTR lpName);
  MOZCE_SHUNT_API BOOL CreateProcessA(LPCSTR pszImageName, LPCSTR pszCmdLine, LPSECURITY_ATTRIBUTES psaProcess, LPSECURITY_ATTRIBUTES psaThread, BOOL fInheritHandles, DWORD fdwCreate, LPVOID pvEnvironment, LPSTR pszCurDir, LPSTARTUPINFO psiStartInfo, LPPROCESS_INFORMATION pProcInfo);

  MOZCE_SHUNT_API BOOL TextOutA(HDC hdc, int  nXStart, int  nYStart, const char* lpString, int  cbString);

  MOZCE_SHUNT_API BOOL GetTextExtentExPointA(HDC inDC, const char* inStr, int inLen, int inMaxExtent, LPINT outFit, LPINT outDx, LPSIZE inSize);
  MOZCE_SHUNT_API BOOL GetVersionExA(LPOSVERSIONINFOA lpv);
  MOZCE_SHUNT_API BOOL DeleteFileA(LPCSTR lpFileName);
  MOZCE_SHUNT_API BOOL MoveFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName);
  MOZCE_SHUNT_API BOOL SetCurrentDirectoryA(LPCSTR inPathName);
  MOZCE_SHUNT_API BOOL VerQueryValueA(const LPVOID inBlock, LPSTR inSubBlock, LPVOID *outBuffer, PUINT outLen);
  MOZCE_SHUNT_API BOOL UnregisterClassA(LPCSTR lpClassName, HINSTANCE hInstance);
  MOZCE_SHUNT_API DWORD GetCurrentDirectoryA(DWORD inBufferLength, LPSTR outBuffer);
  MOZCE_SHUNT_API BOOL GetDiskFreeSpaceA(LPCTSTR lpRootPathName, LPDWORD lpSectorsPerCluster, LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters);
  MOZCE_SHUNT_API DWORD GetEnvironmentVariableA(LPSTR lpName, LPSTR lpBuffer, DWORD nSize);
  MOZCE_SHUNT_API DWORD GetFileAttributesA(LPCSTR lpFileName);
  MOZCE_SHUNT_API DWORD GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize);
  MOZCE_SHUNT_API DWORD SetEnvironmentVariableA(LPSTR lpName, LPSTR lpBuffer);
  MOZCE_SHUNT_API HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
  MOZCE_SHUNT_API HANDLE LoadImageA(HINSTANCE inInst, LPCSTR inName, UINT inType, int inCX, int inCY, UINT inLoad);
  MOZCE_SHUNT_API HANDLE OpenSemaphoreA(DWORD inDesiredAccess, BOOL inInheritHandle, LPCSTR inName);
  MOZCE_SHUNT_API HDC CreateDCA(LPCSTR inDriver, LPCSTR inDevice, LPCSTR inOutput, CONST DEVMODEA* inInitData);
  MOZCE_SHUNT_API HDC CreateDCA2(LPCSTR inDriver, LPCSTR inDevice, LPCSTR inOutput, CONST DEVMODE* inInitData);
  MOZCE_SHUNT_API HWND CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam );
  MOZCE_SHUNT_API HWND FindWindowA(LPCSTR inClass, LPCSTR inWindow);
  MOZCE_SHUNT_API LONG RegEnumKeyExA(HKEY inKey, DWORD inIndex, LPSTR outName, LPDWORD inoutName, LPDWORD inReserved, LPSTR outClass, LPDWORD inoutClass, PFILETIME inLastWriteTime);
  MOZCE_SHUNT_API LONG RegOpenKeyExA(HKEY inKey, LPCSTR inSubKey, DWORD inOptions, REGSAM inSAM, PHKEY outResult);
  MOZCE_SHUNT_API LONG RegQueryValueExA(HKEY inKey, LPCSTR inValueName, LPDWORD inReserved, LPDWORD outType, LPBYTE inoutBData, LPDWORD inoutDData);

  MOZCE_SHUNT_API LONG RegSetValueExA(HKEY hKey, const char *valname, DWORD dwReserved, DWORD dwType, const BYTE* lpData, DWORD dwSize);
  MOZCE_SHUNT_API LONG RegCreateKeyExA(HKEY hKey, const char *subkey, DWORD dwRes, LPSTR lpszClass, DWORD ulOptions, REGSAM samDesired, LPSECURITY_ATTRIBUTES sec_att, PHKEY phkResult, DWORD *lpdwDisp);

  MOZCE_SHUNT_API LONG RegDeleteValueA(HKEY hKey, const char* lpValueName);

  MOZCE_SHUNT_API LRESULT DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
  MOZCE_SHUNT_API LRESULT SendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
  MOZCE_SHUNT_API UINT GetSystemDirectoryA(LPSTR inBuffer, UINT inSize);
  MOZCE_SHUNT_API UINT GetWindowsDirectoryA(LPSTR inBuffer, UINT inSize);
  MOZCE_SHUNT_API UINT RegisterClipboardFormatA(LPCSTR inFormat);
  MOZCE_SHUNT_API UINT RegisterWindowMessageA(LPCSTR s);
  MOZCE_SHUNT_API VOID OutputDebugStringA(LPCSTR inOutputString);
  MOZCE_SHUNT_API int DrawTextA(HDC inDC, LPCSTR inString, int inCount, LPRECT inRect, UINT inFormat);
  MOZCE_SHUNT_API int GetLocaleInfoA(LCID Locale, LCTYPE LCType, LPSTR lpLCData, int cchData);
  MOZCE_SHUNT_API int LoadStringA(HINSTANCE inInstance, UINT inID, LPSTR outBuffer, int inBufferMax);
  MOZCE_SHUNT_API int MessageBoxA(HWND inWnd, LPCSTR inText, LPCSTR inCaption, UINT uType);

  MOZCE_SHUNT_API HINSTANCE LoadLibraryA(LPCSTR lpLibFileName);
  MOZCE_SHUNT_API int GetObjectA(HGDIOBJ hgdiobj, int cbBuffer, LPVOID lpvObject);
  MOZCE_SHUNT_API HBITMAP LoadBitmapA(HINSTANCE hInstance, LPCSTR lpCursorName);
  MOZCE_SHUNT_API HCURSOR LoadCursorA(HINSTANCE hInstance, LPCSTR lpCursorName);

  MOZCE_SHUNT_API BOOL GetOpenFileNameA(LPOPENFILENAMEA lpofna);
  MOZCE_SHUNT_API BOOL GetSaveFileNameA(LPOPENFILENAMEA lpofna);
  MOZCE_SHUNT_API BOOL CreateDirectoryA(const char *lpName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);

  MOZCE_SHUNT_API HMODULE GetModuleHandleA(const char *lpName);
  MOZCE_SHUNT_API HICON LoadIconA(HINSTANCE hInstance, LPCSTR lpIconName);

  MOZCE_SHUNT_API HRSRC FindResourceA(HMODULE  hModule, LPCSTR  lpName, LPCSTR  lpType);
  MOZCE_SHUNT_API int MessageBoxA(HWND hwnd, const char *txt, const char *caption, UINT flags);
  
  MOZCE_SHUNT_API UINT GetDlgItemTextA(HWND hDlg, int nIDDlgItem, LPSTR lpString, int nMaxCount);
  MOZCE_SHUNT_API BOOL SetDlgItemTextA(HWND hwndDlg, int idControl, LPCSTR lpsz);
  MOZCE_SHUNT_API HANDLE CreateEventA(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, const char *lpName);

  MOZCE_SHUNT_API HANDLE GetPropA(HWND hWnd, const char* lpString);
  MOZCE_SHUNT_API BOOL SetPropA(HWND hWnd, const char* lpString, HANDLE hData);
  MOZCE_SHUNT_API HANDLE RemovePropA(HWND hWnd, const char* lpString);

 
  MOZCE_SHUNT_API HANDLE CreateFileMappingA(HANDLE hFile, LPSECURITY_ATTRIBUTES lpFileMappingAttributes, DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCSTR lpName);

  MOZCE_SHUNT_API DWORD FormatMessageA(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId, LPSTR lpBuffer, DWORD nSize, va_list* Arguments);

  MOZCE_SHUNT_API HANDLE CreateSemaphoreA(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes, LONG lInitialCount, LONG lMaximumCount, LPCSTR lpName);
  MOZCE_SHUNT_API HFONT CreateFontIndirectA(CONST LOGFONTA* lplf);
  MOZCE_SHUNT_API int EnumFontFamiliesA(HDC hdc, const char* lpszFamily, FONTENUMPROC lpEnumFontFamProc, LPARAM lParam);
  MOZCE_SHUNT_API BOOL GetTextMetricsA(HDC hdc, TEXTMETRICA* lptm);

  MOZCE_SHUNT_API BOOL SetWindowTextA(HWND hWnd, LPCSTR lpString);


  // From win32w.cpp
  MOZCE_SHUNT_API BOOL SetCurrentDirectoryW(LPCTSTR inPathName);
  MOZCE_SHUNT_API DWORD GetCurrentDirectoryW(DWORD inBufferLength, LPTSTR outBuffer);
  MOZCE_SHUNT_API DWORD GetGlyphOutlineW(HDC inDC, WCHAR inChar, UINT inFormat, void* inGM, DWORD inBufferSize, LPVOID outBuffer, CONST VOID* inMAT2);
  MOZCE_SHUNT_API HANDLE OpenSemaphoreW(DWORD inDesiredAccess, BOOL inInheritHandle, LPCWSTR inName);
  MOZCE_SHUNT_API UINT GetSystemDirectoryW(LPWSTR inBuffer, UINT inSize);
  MOZCE_SHUNT_API UINT GetWindowsDirectoryW(LPWSTR inBuffer, UINT inSize);
  MOZCE_SHUNT_API BOOL SHGetSpecialFolderPathW(HWND hwnd, LPWSTR pszPath, int csidl, BOOL fCreate);

  // uniscribe usp10.h & WinGDI.h
  MOZCE_SHUNT_API HRESULT ScriptFreeCache(SCRIPT_CACHE* psc );

#define GGI_MARK_NONEXISTING_GLYPHS  0X0001

  MOZCE_SHUNT_API DWORD WINAPI GetGlyphIndicesA( HDC hdc,LPCSTR lpstr, int c, LPWORD pgi, DWORD fl);
  MOZCE_SHUNT_API DWORD WINAPI GetGlyphIndicesW( HDC hdc, LPCWSTR lpstr, int c,  LPWORD pgi, DWORD fl);
#define DEFAULT_GUI_FONT    SYSTEM_FONT
  
#define SIC_COMPLEX     1   // Treat complex script letters as complex
  MOZCE_SHUNT_API HRESULT WINAPI ScriptIsComplex(const WCHAR *pwcInChars, int cInChars, DWORD   dwFlags); 
  MOZCE_SHUNT_API DWORD WINAPI GetGlyphIndicesA( HDC hdc, LPCSTR lpstr,int c,LPWORD pgi, DWORD fl);
  MOZCE_SHUNT_API DWORD WINAPI GetGlyphIndicesW( HDC hdc, LPCWSTR lpstr,int c,LPWORD pgi, DWORD fl);
  MOZCE_SHUNT_API BOOL  WINAPI GetTextExtentExPointI (HDC hdc, LPWORD lpwszString,int cwchString,int nMaxExtent,LPINT lpnFit,LPINT lpnDx,LPSIZE lpSize);
  MOZCE_SHUNT_API HRESULT WINAPI ScriptGetProperties( const SCRIPT_PROPERTIES ***ppSp, int *piNumScripts);
  MOZCE_SHUNT_API HRESULT WINAPI ScriptGetFontProperties(HDC hdc, SCRIPT_CACHE *psc, SCRIPT_FONTPROPERTIES *sfp );
  MOZCE_SHUNT_API HRESULT WINAPI ScriptBreak(  const WCHAR *pwcChars, int cChars, const SCRIPT_ANALYSIS *psa, SCRIPT_LOGATTR *psla );
  MOZCE_SHUNT_API HRESULT WINAPI ScriptItemize( const WCHAR *pwcInChars, int cInChars, int cMaxItems, const SCRIPT_CONTROL *psControl, const SCRIPT_STATE *psState, SCRIPT_ITEM *pItems, int *pcItems ); 
  MOZCE_SHUNT_API BOOL WINAPI GetICMProfileW(HDC hDC, LPDWORD lpcbName,LPWSTR lpszFilename);
  MOZCE_SHUNT_API DWORD WINAPI GetGuiResources(HANDLE hProcess,DWORD uiFlags);
  MOZCE_SHUNT_API HRESULT WINAPI ScriptRecordDigitSubstitution(LCID Locale, SCRIPT_DIGITSUBSTITUTE  *psds); 

MOZCE_SHUNT_API HRESULT WINAPI ScriptItemize(const WCHAR *pwcInChars, int cInChars, int cMaxItems, const SCRIPT_CONTROL *psControl, const SCRIPT_STATE *psState,  SCRIPT_ITEM *pItems, int *pcItems );

MOZCE_SHUNT_API HWND WINAPI GetTopWindow(HWND hWnd);
#define GetNextWindow(hWnd, wCmd) GetWindow(hWnd, wCmd)

MOZCE_SHUNT_API BOOL WINAPI UpdateLayeredWindow(HWND hWnd, HDC hdcDst, POINT *pptDst,
                                SIZE *psize, HDC hdcSrc, POINT *pptSrc,
                                COLORREF crKey, BLENDFUNCTION *pblend,
                                DWORD dwFlags);

MOZCE_SHUNT_API BOOL
WINAPI
InitializeCriticalSectionAndSpinCount(
    __out LPCRITICAL_SECTION lpCriticalSection,
    __in  DWORD dwSpinCount
    );

MOZCE_SHUNT_API
DWORD
WINAPI
SetCriticalSectionSpinCount(
    __inout LPCRITICAL_SECTION lpCriticalSection,
    __in    DWORD dwSpinCount
    );

MOZCE_SHUNT_API
BOOL
WINAPI
GetSystemTimeAdjustment(
    __out PDWORD lpTimeAdjustment,
    __out PDWORD lpTimeIncrement,
    __out PBOOL  lpTimeAdjustmentDisabled
    );

MOZCE_SHUNT_API BOOL  WINAPI PolyBezierTo(__in HDC hdc, __in_ecount(cpt) CONST POINT * apt, __in DWORD cpt);
MOZCE_SHUNT_API BOOL WINAPI CloseFigure(__in HDC hdc);
MOZCE_SHUNT_API BOOL WINAPI SelectClipPath(__in HDC hdc, __in int mode);
MOZCE_SHUNT_API BOOL WINAPI EndPath(__in HDC hdc);
MOZCE_SHUNT_API BOOL WINAPI BeginPath(__in HDC hdc);
MOZCE_SHUNT_API BOOL WINAPI ModifyWorldTransform( __in HDC hdc, __in_opt CONST XFORM * lpxf, __in DWORD mode);
MOZCE_SHUNT_API BOOL WINAPI WidenPath(__in HDC hdc);
MOZCE_SHUNT_API BOOL WINAPI StrokePath(__in HDC hdc);
MOZCE_SHUNT_API HPEN WINAPI ExtCreatePen( __in DWORD iPenStyle,
                                    __in DWORD cWidth,
                                    __in CONST LOGBRUSH *plbrush,
                                    __in DWORD cStyle,
                                    __in_ecount_opt(cStyle) CONST DWORD *pstyle);
MOZCE_SHUNT_API BOOL WINAPI SetMiterLimit(__in HDC hdc, __in FLOAT limit, __out_opt PFLOAT old);
MOZCE_SHUNT_API BOOL WINAPI FillPath(__in HDC hdc);

MOZCE_SHUNT_API BOOL	      WINAPI GetICMProfileW(    __in HDC hdc, 
                                                __inout LPDWORD pBufSize,
                                                __out_ecount_opt(*pBufSize) LPWSTR pszFilename);
MOZCE_SHUNT_API HRESULT WINAPI ScriptShape(
    HDC                 hdc,            // In    Optional (see under caching)
    SCRIPT_CACHE       *psc,            // InOut Cache handle
    const WCHAR        *pwcChars,       // In    Logical unicode run
    int                 cChars,         // In    Length of unicode run
    int                 cMaxGlyphs,     // In    Max glyphs to generate
    SCRIPT_ANALYSIS    *psa,            // InOut Result of ScriptItemize (may have fNoGlyphIndex set)
    WORD               *pwOutGlyphs,    // Out   Output glyph buffer
    WORD               *pwLogClust,     // Out   Logical clusters
    SCRIPT_VISATTR     *psva,           // Out   Visual glyph attributes
    int                *pcGlyphs);      // Out   Count of glyphs generated

MOZCE_SHUNT_API HRESULT WINAPI ScriptPlace(
    HDC                     hdc,        // In    Optional (see under caching)
    SCRIPT_CACHE           *psc,        // InOut Cache handle
    const WORD             *pwGlyphs,   // In    Glyph buffer from prior ScriptShape call
    int                     cGlyphs,    // In    Number of glyphs
    const SCRIPT_VISATTR   *psva,       // In    Visual glyph attributes
    SCRIPT_ANALYSIS        *psa,        // InOut Result of ScriptItemize (may have fNoGlyphIndex set)
    int                    *piAdvance,  // Out   Advance wdiths
    GOFFSET                *pGoffset,   // Out   x,y offset for combining glyph
    ABC                    *pABC);      // Out   Composite ABC for the whole run (Optional)

MOZCE_SHUNT_API int   WINAPI SetMapMode(HDC, int);
MOZCE_SHUNT_API DWORD WINAPI GetCharacterPlacementW(  __in HDC hdc, __in_ecount(nCount) LPCWSTR lpString, __in int nCount, __in int nMexExtent, __inout LPGCP_RESULTSW lpResults, __in DWORD dwFlags);

MOZCE_SHUNT_API BOOL MoveFileExW(
    __in LPCWSTR lpExistingFileName,
    __in LPCWSTR lpNewFileName,
    __in DWORD dwFlags
    );

MOZCE_SHUNT_API int  _wrmdir(const wchar_t * _Path);
MOZCE_SHUNT_API int _wremove(const wchar_t * _Filename);
MOZCE_SHUNT_API int _wchmod(const wchar_t * _Filename, int _Mode);

MOZCE_SHUNT_API HWND GetAncestor(HWND hwnd, UINT gaFlags);
MOZCE_SHUNT_API UINT MapVirtualKeyA(UINT uCode, UINT uMapType);
MOZCE_SHUNT_API BOOL GetClassInfoA(HINSTANCE hInstance, LPCSTR lpClassName, LPWNDCLASSA lpWndClass);

MOZCE_SHUNT_API int WINAPI StartDocA(HDC, CONST DOCINFOA *);
#ifdef __cplusplus
};
#endif

#endif //MOZCE_SHUNT_H
