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

#include "mozce_defs.h"

#include <commdlg.h>

//////////////////////////////////////////////////////////
// Function Mapping
//////////////////////////////////////////////////////////

// From asswert.cpp
#define _assert		mozce_assert
#define assert		mozce_assert

// From direct.cpp
#define mkdir		mozce_mkdir
#define rmdir		mozce_rmdir

// From errno.cpp
#define errno		mozce_errno

// From io.cpp
#define chmod		mozce_chmod
#define _isatty		mozce_isatty
#define isatty		mozce_isatty

// math.cpp
/*
  #define fd_acos acos
  #define fd_asin asin
  #define fd_atan atan
  #define fd_cos cos
  #define fd_sin sin
  #define fd_tan tan
  #define fd_exp exp
  #define fd_log log
  #define fd_sqrt sqrt
  #define fd_ceil ceil
  #define fd_fabs fabs
  #define fd_floor floor
  #define fd_fmod fmod
  #define fd_atan2 atan2
  #define fd_copysign _copysign
  #define fd_pow pow
*/

// From mbstring.cpp
#define _mbsinc		mozce_mbsinc
#define _mbspbrk	mozce_mbspbrk
#define _mbsrchr	mozce_mbsrchr
#define _mbschr		mozce_mbschr
#define _mbctolower tolower 

// From process.cpp
#define abort		mozce_abort
#define getenv		mozce_getenv
#define putenv		mozce_putenv
#define getpid		mozce_getpid

// From signal.cpp
#define raise		mozce_raise
#define signal		mozce_signal

// From stat.cpp
#define stat		mozce_stat

// From stdio.cpp
#define rewind		mozce_rewind
#define	_fdopen		mozce_fdopen
#define	perror		mozce_perror
#define remove		mozce_remove

#define _getcwd     mozce_getcwd
#define getcwd      mozce_getcwd


// From stdlib.cpp
#define _fullpath	mozce_fullpath
#define _splitpath	mozce_splitpath
#define _makepath	mozce_makepath

#define strdup      _strdup
#define stricmp     _stricmp
#define strcmpi     _stricmp
#define strnicmp    _strnicmp


// From string.cpp
#define strerror	mozce_strerror

// From time.cpp
#define strftime	mozce_strftime
#define localtime_r	mozce_localtime_r
#define localtime	mozce_localtime
#define gmtime_r	mozce_gmtime_r
#define gmtime	    mozce_gmtime
#define mktime	    mozce_mktime
//#define time		mozce_time
#define ctime		mozce_ctime

#define localeconv  mozce_localeconv
//#define lconv       mozce_lconv

// From win32.cpp
#define Arc                       mozce_Arc
#define CallNextHookEx            mozce_CallNextHookEx
#define CreateDIBitmap            mozce_CreateDIBitmap
#define EnumChildWindows          mozce_EnumChildWindows
#define EnumFontFamiliesEx        mozce_EnumFontFamiliesEx
#define EnumThreadWindows         mozce_EnumThreadWindows
#define ExpandEnvironmentStrings  mozce_ExpandEnvironmentStrings
#define ExtSelectClipRgn          mozce_ExtSelectClipRgn
#define FIXED                     mozce_FIXED
#define FlashWindow               mozce_FlashWindow
#define FrameRect                 mozce_FrameRect
#define GLYPHMETRICS              mozce_GLYPHMETRICS
#define GdiFlush                  mozce_GdiFlush
#define GetACP                    mozce_GetACP
#define GetCurrentProcess         mozce_GetCurrentProcess
#define GetCurrentThreadId        mozce_GetCurrentThreadId
#define GetDIBits                 mozce_GetDIBits
#define GetEnvironmentVariable    mozce_GetEnvironmentVariable
#define GetFontData               mozce_GetFontData
#define GetFullPathName           mozce_GetFullPathName
#define GetIconInfo               mozce_GetIconInfo
#define GetMapMode                mozce_GetMapMode
#define GetMessageTime            mozce_GetMessageTime
#define GetOutlineTextMetrics     mozce_GetOutlineTextMetrics
#define GetScrollPos              mozce_GetScrollPos
#define GetScrollRange            mozce_GetScrollRange
#define GetShortPathName          mozce_GetShortPathName
#define GetSystemTimeAsFileTime   mozce_GetSystemTimeAsFileTime
#define GetTextCharset            mozce_GetTextCharset
#define GetTextCharsetInfo        mozce_GetTextCharsetInfo
#define GetUserName               mozce_GetUserName
#define InvertRgn                 mozce_InvertRgn
#define IsIconic                  mozce_IsIconic
#define LINEDDAPROC               mozce_LINEDDAPROC
#define LPtoDP                    mozce_LPtoDP
#define LineDDA                   mozce_LineDDA
#define LineDDAProc               mozce_LineDDAProc
#define MAT2                      mozce_MAT2
#define MsgWaitForMultipleObjects mozce_MsgWaitForMultipleObjects 
#define MulDiv                    mozce_MulDiv
#define OUTLINETEXTMETRIC         mozce_OUTLINETEXTMETRIC
#define OpenIcon                  mozce_OpenIcon
#define Pie                       mozce_Pie
#define RegCreateKey              mozce_RegCreateKey
#define SetArcDirection           mozce_SetArcDirection
#define SetDIBits                 mozce_SetDIBits
#define SetMenu                   mozce_SetMenu
#define SetPolyFillMode           mozce_SetPolyFillMode
#define SetStretchBltMode         mozce_SetStretchBltMode
#define SetWindowsHookEx          mozce_SetWindowsHookEx
#define TlsAlloc                  mozce_TlsAlloc
#define TlsFree                   mozce_TlsFree
#define UnhookWindowsHookEx       mozce_UnhookWindowsHookEx
#define WaitMessage               mozce_WaitMessage
#define _CoLockObjectExternal     mozce_CoLockObjectExternal
#define _OleFlushClipboard        mozce_OleFlushClipboard
#define _OleGetClipboard          mozce_OleGetClipboard
#define _OleQueryLinkFromData     mozce_OleQueryLinkFromData
#define _OleSetClipboard          mozce_OleSetClipboard


// From win32a.cpp

#define CopyFileA                 mozce_CopyFileA
#define CreateDCA                 mozce_CreateDCA
#define CreateDCA2                mozce_CreateDCA2
#define CreateDirectoryA          mozce_CreateDirectoryA

#pragma warning(disable : 4005) // OK to have no return value
// We use a method named CreateEvent.  We do not want to map
// CreateEvent to CreateEventA
#define CreateEvent               CreateEvent
#pragma warning(default : 4005) // restore default

#define CreateEventA              mozce_CreateEventA
#define CreateFileA               mozce_CreateFileA
#define CreateFileMappingA        mozce_CreateFileMappingA
#define CreateFontIndirectA       mozce_CreateFontIndirectA
#define CreateProcessA            mozce_CreateProcessA
#define CreateSemaphoreA          mozce_CreateSemaphoreA
#define CreateWindowExA           mozce_CreateWindowExA
#define DefWindowProcA            mozce_DefWindowProcA
#define DeleteFileA               mozce_DeleteFileA
#define DrawTextA                 mozce_DrawTextA
#define EnumFontFamiliesA         mozce_EnumFontFamiliesA
#define ExtTextOutA               mozce_ExtTextOutA
#define FindResourceA             mozce_FindResourceA
#define FindWindowA               mozce_FindWindowA
#define FormatMessageA            mozce_FormatMessageA
#define GetClassInfoA             mozce_GetClassInfoA
#define GetClassNameA             mozce_GetClassNameA
#define GetCurrentDirectoryA      mozce_GetCurrentDirectoryA
#define GetDlgItemTextA           mozce_GetDlgItemTextA
#define GetEnvironmentVariableA   mozce_GetEnvironmentVariableA
#define GetFileAttributesA        mozce_GetFileAttributesA
#define GetFileVersionInfoA       mozce_GetFileVersionInfoA
#define GetFileVersionInfoSizeA   mozce_GetFileVersionInfoSizeA
#define GetGlyphOutlineA          mozce_GetGlyphOutlineA
#define GetLocaleInfoA            mozce_GetLocaleInfoA
#define GetModuleFileNameA        mozce_GetModuleFileNameA
#define GetModuleHandleA          mozce_GetModuleHandleA
#define GetObjectA                mozce_GetObjectA
#define GetOpenFileNameA          mozce_GetOpenFileNameA
#define GetProcAddress            mozce_GetProcAddressA
#define GetProcAddressA           mozce_GetProcAddressA
#define GetPropA                  mozce_GetPropA
#define GetSaveFileNameA          mozce_GetSaveFileNameA
#define GetSystemDirectory        mozce_GetSystemDirectoryA
#define GetSystemDirectoryA       mozce_GetSystemDirectoryA
#define GetTextExtentExPointA     mozce_GetTextExtentExPointA
#define GetTextFaceA              mozce_GetTextFaceA
#define GetTextMetricsA           mozce_GetTextMetricsA
#define GetVersionExA             mozce_GetVersionExA
#define GetWindowsDirectory       mozce_GetWindowsDirectoryA
#define GetWindowsDirectoryA      mozce_GetWindowsDirectoryA
#define GlobalAddAtomA            mozce_GlobalAddAtomA
#define LoadCursorA               mozce_LoadCursorA
#define LoadIconA                 mozce_LoadIconA
#define LoadImageA                mozce_LoadImageA
#define LoadLibraryA              mozce_LoadLibraryA
#define LoadMenuA                 mozce_LoadMenuA
#define LoadStringA               mozce_LoadStringA
#define MessageBoxA               mozce_MessageBoxA
#define MoveFileA                 mozce_MoveFileA
#define OpenSemaphoreA            mozce_OpenSemaphoreA
#define OutputDebugStringA        mozce_OutputDebugStringA
#define PeekMessageA              mozce_PeekMessageA
#define PostMessageA              mozce_PostMessageA
#define RegEnumKeyExA             mozce_RegEnumKeyExA
#define RegOpenKeyExA             mozce_RegOpenKeyExA
#define RegQueryValueExA          mozce_RegQueryValueExA
#define RegisterClassA            mozce_RegisterClassA
#define RegisterClipboardFormatA  mozce_RegisterClipboardFormatA
#define RegisterWindowMessageA    mozce_RegisterWindowMessageA
#define RemoveDirectoryA          mozce_RemoveDirectoryA
#define RemovePropA               mozce_RemovePropA
#define SendMessageA              mozce_SendMessageA
#define SetCurrentDirectory       mozce_SetCurrentDirectoryA
#define SetCurrentDirectoryA      mozce_SetCurrentDirectoryA
#define SetDlgItemTextA           mozce_SetDlgItemTextA
#define SetEnvironmentVariable    mozce_SetEnvironmentVariableA
#define SetEnvironmentVariableA   mozce_SetEnvironmentVariableA
#define SetPropA                  mozce_SetPropA
#define StartDocA                 mozce_StartDocA
#define UnregisterClassA          mozce_UnregisterClassA
#define VerQueryValueA            mozce_VerQueryValueA

#define CreateDialogIndirectParamA CreateDialogIndirectParamW
#define DialogBoxIndirectParamA    DialogBoxIndirectParamW
#define SystemParametersInfoA      SystemParametersInfoW
#define GetMessageA                GetMessageW
#define DispatchMessageA           DispatchMessageW
#define CallWindowProcA            CallWindowProcW
#define GetWindowLongA             GetWindowLongW
#define SetWindowLongA             SetWindowLongW

#undef FindFirstFile
#undef FindNextFile

#define FindFirstFile              FindFirstFileW
#define FindNextFile               FindNextFileW


#define GetProp                   mozce_GetPropA
#define SetProp                   mozce_SetPropA
#define RemoveProp                mozce_RemovePropA


// From win32w.cpp
#define GetCurrentDirectory       mozce_GetCurrentDirectoryW
#define GetCurrentDirectoryW      mozce_GetCurrentDirectoryW
#define GetGlyphOutlineW          mozce_GetGlyphOutlineW
#define GetSystemDirectoryW       mozce_GetSystemDirectoryW
#define GetWindowsDirectoryW      mozce_GetWindowsDirectoryW
#define OpenSemaphoreW            mozce_OpenSemaphoreW
#define SetCurrentDirectoryW      mozce_SetCurrentDirectoryW

//////////////////////////////////////////////////////////
// Function Declarations
//////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C" {
#endif

  // From assert.cpp
  MOZCE_SHUNT_API void mozce_assert(int inExpression);
  
  // From direct.cpp
  MOZCE_SHUNT_API int mozce_mkdir(const char* inDirname);
  MOZCE_SHUNT_API int mozce_rmdir(const char* inDirname);
  
  // From errno.cpp
  extern MOZCE_SHUNT_API int mozce_errno;
  
  // From io.cpp
  MOZCE_SHUNT_API int mozce_chmod(const char* inFilename, int inMode);
  MOZCE_SHUNT_API int mozce_isatty(int inHandle);
  
  // From mbstring.cpp
  MOZCE_SHUNT_API unsigned char* mozce_mbsinc(const unsigned char* inCurrent);
  MOZCE_SHUNT_API unsigned char* mozce_mbspbrk(const unsigned char* inString, const unsigned char* inStrCharSet);
  MOZCE_SHUNT_API unsigned char* mozce_mbschr(const unsigned char* inString, unsigned int inC);
  MOZCE_SHUNT_API unsigned char* mozce_mbsrchr(const unsigned char* inString, unsigned int inC);
  
  // From process.cpp
  MOZCE_SHUNT_API void mozce_abort(void);
  MOZCE_SHUNT_API char* mozce_getenv(const char* inName);
  MOZCE_SHUNT_API int mozce_putenv(const char *a);
  MOZCE_SHUNT_API int mozce_getpid(void);

  // From signal.cpp
  MOZCE_SHUNT_API int mozce_raise(int inSignal);
  MOZCE_SHUNT_API _sigsig mozce_signal(int inSignal, _sigsig inFunc);
  
  // From stat.cpp
  MOZCE_SHUNT_API int mozce_stat(const char *, struct _stat *);
  
  // From stdio.cpp
  MOZCE_SHUNT_API void mozce_rewind(FILE* inStream);
  MOZCE_SHUNT_API FILE* mozce_fdopen(int inFD, const char* inMode);
  MOZCE_SHUNT_API void mozce_perror(const char* inString);
  MOZCE_SHUNT_API int mozce_remove(const char* inPath);

  MOZCE_SHUNT_API char* mozce_getcwd(char* buff, size_t size);

  // From stdlib.cpp
  MOZCE_SHUNT_API void mozce_splitpath(const char* inPath, char* outDrive, char* outDir, char* outFname, char* outExt);
  MOZCE_SHUNT_API void mozce_makepath(char* outPath, const char* inDrive, const char* inDir, const char* inFname, const char* inExt);
  MOZCE_SHUNT_API char* mozce_fullpath(char *, const char *, size_t);
  
  // From string.cpp
  MOZCE_SHUNT_API char* mozce_strerror(int);
  
  // From time.cpp
  MOZCE_SHUNT_API struct tm* mozce_localtime_r(const time_t* inTimeT,struct tm* outRetval);
  MOZCE_SHUNT_API struct tm* mozce_gmtime_r(const time_t* inTimeT, struct tm* outRetval);
  MOZCE_SHUNT_API time_t mozce_time(time_t* inStorage);
  MOZCE_SHUNT_API char* mozce_ctime(const time_t* timer);
  MOZCE_SHUNT_API struct tm* mozce_localtime(const time_t* inTimeT);
  MOZCE_SHUNT_API struct tm* mozce_gmtime(const time_t* inTimeT);
  MOZCE_SHUNT_API time_t mozce_mktime(struct tm* inTM);
  MOZCE_SHUNT_API size_t mozce_strftime(char *strDest, size_t maxsize, const char *format, const struct tm *timeptr);
  
  MOZCE_SHUNT_API struct lconv * mozce_localeconv(void);

  // from win32.cpp

  VOID CALLBACK mozce_LineDDAProc(int X, int Y, LPARAM lpData);
  typedef void (*mozce_LINEDDAPROC) (int X, int Y, LPARAM lpData);

  MOZCE_SHUNT_API int mozce_MulDiv(int inNumber, int inNumerator, int inDenominator);
  MOZCE_SHUNT_API int mozce_GetDIBits(HDC inDC, HBITMAP inBMP, UINT inStartScan, UINT inScanLines, LPVOID inBits, LPBITMAPINFO inInfo, UINT inUsage);
  MOZCE_SHUNT_API int mozce_SetDIBits(HDC inDC, HBITMAP inBMP, UINT inStartScan, UINT inScanLines, CONST LPVOID inBits, CONST LPBITMAPINFO inInfo, UINT inUsage);
  MOZCE_SHUNT_API HBITMAP mozce_CreateDIBitmap(HDC inDC, CONST BITMAPINFOHEADER *inBMIH, DWORD inInit, CONST VOID *inBInit, CONST BITMAPINFO *inBMI, UINT inUsage);
  MOZCE_SHUNT_API int mozce_SetPolyFillMode(HDC inDC, int inPolyFillMode);
  MOZCE_SHUNT_API int mozce_SetStretchBltMode(HDC inDC, int inStretchMode);
  MOZCE_SHUNT_API int mozce_ExtSelectClipRgn(HDC inDC, HRGN inRGN, int inMode);
  MOZCE_SHUNT_API DWORD mozce_ExpandEnvironmentStrings(LPCTSTR lpSrc, LPTSTR lpDst, DWORD nSize);

  MOZCE_SHUNT_API BOOL mozce_LineDDA(int inXStart, int inYStart, int inXEnd, int inYEnd, mozce_LINEDDAPROC inLineFunc, LPARAM inData);
  MOZCE_SHUNT_API int mozce_FrameRect(HDC inDC, CONST RECT *inRect, HBRUSH inBrush);
  MOZCE_SHUNT_API BOOL mozce_GdiFlush(void);
  MOZCE_SHUNT_API int mozce_SetArcDirection(HDC inDC, int inArcDirection);
  MOZCE_SHUNT_API BOOL mozce_Arc(HDC inDC, int inLeftRect, int inTopRect, int inRightRect, int inBottomRect, int inXStartArc, int inYStartArc, int inXEndArc, int inYEndArc);
  MOZCE_SHUNT_API BOOL mozce_Pie(HDC inDC, int inLeftRect, int inTopRect, int inRightRect, int inBottomRect, int inXRadial1, int inYRadial1, int inXRadial2, int inYRadial2);
  MOZCE_SHUNT_API DWORD mozce_GetFontData(HDC inDC, DWORD inTable, DWORD inOffset, LPVOID outBuffer, DWORD inData);
  MOZCE_SHUNT_API UINT mozce_GetTextCharset(HDC inDC);
  MOZCE_SHUNT_API UINT mozce_GetTextCharsetInfo(HDC inDC, LPFONTSIGNATURE outSig, DWORD inFlags);
  MOZCE_SHUNT_API UINT mozce_GetOutlineTextMetrics(HDC inDC, UINT inData, void* outOTM);
  MOZCE_SHUNT_API int mozce_EnumFontFamiliesEx(HDC inDC, LPLOGFONT inLogfont, FONTENUMPROC inFunc, LPARAM inParam, DWORD inFlags);
  MOZCE_SHUNT_API int mozce_GetMapMode(HDC inDC);
  MOZCE_SHUNT_API BOOL mozce_GetIconInfo(HICON inIcon, PICONINFO outIconinfo);
  MOZCE_SHUNT_API BOOL mozce_LPtoDP(HDC inDC, LPPOINT inoutPoints, int inCount);
  MOZCE_SHUNT_API LONG mozce_RegCreateKey(HKEY inKey, LPCTSTR inSubKey, PHKEY outResult);
  MOZCE_SHUNT_API BOOL mozce_WaitMessage(VOID);
  MOZCE_SHUNT_API BOOL mozce_FlashWindow(HWND inWnd, BOOL inInvert);
  MOZCE_SHUNT_API BOOL mozce_EnumChildWindows(HWND inParent, WNDENUMPROC inFunc, LPARAM inParam);
  MOZCE_SHUNT_API BOOL mozce_EnumThreadWindows(DWORD inThreadID, WNDENUMPROC inFunc, LPARAM inParam);
  MOZCE_SHUNT_API BOOL mozce_IsIconic(HWND inWnd);
  MOZCE_SHUNT_API BOOL mozce_OpenIcon(HWND inWnd);
  MOZCE_SHUNT_API HHOOK mozce_SetWindowsHookEx(int inType, void* inFunc, HINSTANCE inMod, DWORD inThreadId);
  MOZCE_SHUNT_API BOOL mozce_UnhookWindowsHookEx(HHOOK inHook);
  MOZCE_SHUNT_API LRESULT mozce_CallNextHookEx(HHOOK inHook, int inCode, WPARAM wParam, LPARAM lParam);
  MOZCE_SHUNT_API BOOL mozce_InvertRgn(HDC inDC, HRGN inRGN);
  MOZCE_SHUNT_API int mozce_GetScrollPos(HWND inWnd, int inBar);
  MOZCE_SHUNT_API BOOL mozce_GetScrollRange(HWND inWnd, int inBar, LPINT outMinPos, LPINT outMaxPos);
  MOZCE_SHUNT_API HRESULT mozce_CoLockObjectExternal(IUnknown* inUnk, BOOL inLock, BOOL inLastUnlockReleases);
  MOZCE_SHUNT_API HRESULT mozce_OleSetClipboard(IDataObject* inDataObj);
  MOZCE_SHUNT_API HRESULT mozce_OleGetClipboard(IDataObject** outDataObj);
  MOZCE_SHUNT_API HRESULT mozce_OleFlushClipboard(void);
  MOZCE_SHUNT_API HRESULT mozce_OleQueryLinkFromData(IDataObject* inSrcDataObject);
  //MOZCE_SHUNT_API void* mozce_SHBrowseForFolder(void* /*LPBROWSEINFOS*/ inBI);
  MOZCE_SHUNT_API BOOL mozce_SetMenu(HWND inWnd, HMENU inMenu);
  MOZCE_SHUNT_API BOOL mozce_GetUserName(LPTSTR inBuffer, LPDWORD inoutSize);
  MOZCE_SHUNT_API DWORD mozce_GetShortPathName(LPCTSTR inLongPath, LPTSTR outShortPath, DWORD inBufferSize);
  MOZCE_SHUNT_API DWORD mozce_GetEnvironmentVariable(LPCSTR lpName, LPCSTR lpBuffer, DWORD nSize);
  MOZCE_SHUNT_API HMENU mozce_LoadMenuA(HINSTANCE hInstance, LPCSTR lpMenuName);

  MOZCE_SHUNT_API void mozce_GetSystemTimeAsFileTime(LPFILETIME lpSystemTimeAsFileTime);
  MOZCE_SHUNT_API DWORD mozce_GetFullPathName(LPCTSTR lpFileName, DWORD nBufferLength, LPTSTR lpBuffer, LPTSTR* lpFilePart);

  MOZCE_SHUNT_API UINT mozce_GetACP(void);
  MOZCE_SHUNT_API HANDLE mozce_GetCurrentProcess(void);
  MOZCE_SHUNT_API DWORD mozce_TlsAlloc(void);
  MOZCE_SHUNT_API BOOL mozce_TlsFree(DWORD dwTlsIndex);
  MOZCE_SHUNT_API DWORD GetCurrentThreadId(void);

  MOZCE_SHUNT_API DWORD mozce_MsgWaitForMultipleObjects(DWORD nCount, const HANDLE* pHandles, BOOL bWaitAll, DWORD dwMilliseconds, DWORD dwWakeMask);

  MOZCE_SHUNT_API LONG mozce_GetMessageTime(void);

  // from win32a.cpp
  
  MOZCE_SHUNT_API DWORD mozce_GetGlyphOutlineA(HDC inDC, CHAR inChar, UINT inFormat, void* inGM, DWORD inBufferSize, LPVOID outBuffer, CONST mozce_MAT2* inMAT2);
  MOZCE_SHUNT_API ATOM mozce_GlobalAddAtomA(LPCSTR lpString);
  MOZCE_SHUNT_API ATOM mozce_RegisterClassA(CONST WNDCLASSA *lpwc);
  MOZCE_SHUNT_API BOOL mozce_CopyFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, BOOL bFailIfExists);
  MOZCE_SHUNT_API BOOL mozce_CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
  MOZCE_SHUNT_API BOOL mozce_RemoveDirectoryA(LPCSTR lpPathName);
  MOZCE_SHUNT_API BOOL mozce_CreateProcessA(LPCSTR pszImageName, LPCSTR pszCmdLine, LPSECURITY_ATTRIBUTES psaProcess, LPSECURITY_ATTRIBUTES psaThread, BOOL fInheritHandles, DWORD fdwCreate, LPVOID pvEnvironment, LPSTR pszCurDir, LPSTARTUPINFO psiStartInfo, LPPROCESS_INFORMATION pProcInfo);
  MOZCE_SHUNT_API BOOL mozce_ExtTextOutA(HDC inDC, int inX, int inY, UINT inOptions, LPCRECT inRect, LPCSTR inString, UINT inCount, const LPINT inDx);
  MOZCE_SHUNT_API BOOL mozce_GetClassInfoA(HINSTANCE hInstance, LPCSTR lpClassName, LPWNDCLASS lpWndClass);
  MOZCE_SHUNT_API int mozce_GetClassNameA(HWND hWnd, LPTSTR lpClassName, int nMaxCount);
  MOZCE_SHUNT_API BOOL mozce_GetFileVersionInfoA(LPSTR inFilename, DWORD inHandle, DWORD inLen, LPVOID outData);
  MOZCE_SHUNT_API BOOL mozce_GetTextExtentExPointA(HDC inDC, LPCSTR inStr, int inLen, int inMaxExtent, LPINT outFit, LPINT outDx, LPSIZE inSize);
  MOZCE_SHUNT_API BOOL mozce_GetVersionExA(LPOSVERSIONINFOA lpv);
  MOZCE_SHUNT_API BOOL mozce_DeleteFileA(LPCSTR lpFileName);
  MOZCE_SHUNT_API BOOL mozce_MoveFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName);
  MOZCE_SHUNT_API BOOL mozce_PeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg);
  MOZCE_SHUNT_API BOOL mozce_SetCurrentDirectoryA(LPCSTR inPathName);
  MOZCE_SHUNT_API BOOL mozce_VerQueryValueA(const LPVOID inBlock, LPSTR inSubBlock, LPVOID *outBuffer, PUINT outLen);
  MOZCE_SHUNT_API BOOL mozce_UnregisterClassA(LPCSTR lpClassName, HINSTANCE hInstance);
  MOZCE_SHUNT_API DWORD mozce_GetCurrentDirectoryA(DWORD inBufferLength, LPSTR outBuffer);
  MOZCE_SHUNT_API DWORD mozce_GetEnvironmentVariableA(LPSTR lpName, LPSTR lpBuffer, DWORD nSize);
  MOZCE_SHUNT_API DWORD mozce_GetFileAttributesA(LPCSTR lpFileName);
  MOZCE_SHUNT_API DWORD mozce_GetFileVersionInfoSizeA(LPSTR inFilename, LPDWORD outHandle);
  MOZCE_SHUNT_API DWORD mozce_GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize);
  MOZCE_SHUNT_API DWORD mozce_SetEnvironmentVariableA(LPSTR lpName, LPSTR lpBuffer);
  MOZCE_SHUNT_API HANDLE mozce_CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
  MOZCE_SHUNT_API HANDLE mozce_LoadImageA(HINSTANCE inInst, LPCSTR inName, UINT inType, int inCX, int inCY, UINT inLoad);
  MOZCE_SHUNT_API HANDLE mozce_OpenSemaphoreA(DWORD inDesiredAccess, BOOL inInheritHandle, LPCSTR inName);
  MOZCE_SHUNT_API HDC mozce_CreateDCA(LPCSTR inDriver, LPCSTR inDevice, LPCSTR inOutput, CONST DEVMODEA* inInitData);
  MOZCE_SHUNT_API HDC mozce_CreateDCA2(LPCSTR inDriver, LPCSTR inDevice, LPCSTR inOutput, CONST DEVMODE* inInitData);
  MOZCE_SHUNT_API HWND mozce_CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam );
  MOZCE_SHUNT_API HWND mozce_FindWindowA(LPCSTR inClass, LPCSTR inWindow);
  MOZCE_SHUNT_API LONG mozce_RegEnumKeyExA(HKEY inKey, DWORD inIndex, LPSTR outName, LPDWORD inoutName, LPDWORD inReserved, LPSTR outClass, LPDWORD inoutClass, PFILETIME inLastWriteTime);
  MOZCE_SHUNT_API LONG mozce_RegOpenKeyExA(HKEY inKey, LPCSTR inSubKey, DWORD inOptions, REGSAM inSAM, PHKEY outResult);
  MOZCE_SHUNT_API LONG mozce_RegQueryValueExA(HKEY inKey, LPCSTR inValueName, LPDWORD inReserved, LPDWORD outType, LPBYTE inoutBData, LPDWORD inoutDData);
  MOZCE_SHUNT_API LRESULT mozce_DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
  MOZCE_SHUNT_API LRESULT mozce_PostMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
  MOZCE_SHUNT_API LRESULT mozce_SendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
  MOZCE_SHUNT_API UINT mozce_GetSystemDirectoryA(LPSTR inBuffer, UINT inSize);
  MOZCE_SHUNT_API UINT mozce_GetWindowsDirectoryA(LPSTR inBuffer, UINT inSize);
  MOZCE_SHUNT_API UINT mozce_RegisterClipboardFormatA(LPCSTR inFormat);
  MOZCE_SHUNT_API UINT mozce_RegisterWindowMessageA(LPCSTR s);
  MOZCE_SHUNT_API VOID mozce_OutputDebugStringA(LPCSTR inOutputString);
  MOZCE_SHUNT_API int mozce_DrawTextA(HDC inDC, LPCSTR inString, int inCount, LPRECT inRect, UINT inFormat);
  MOZCE_SHUNT_API int mozce_GetLocaleInfoA(LCID Locale, LCTYPE LCType, LPSTR lpLCData, int cchData);
  MOZCE_SHUNT_API int mozce_LoadStringA(HINSTANCE inInstance, UINT inID, LPSTR outBuffer, int inBufferMax);
  MOZCE_SHUNT_API int mozce_MessageBoxA(HWND inWnd, LPCSTR inText, LPCSTR inCaption, UINT uType);

  MOZCE_SHUNT_API HINSTANCE mozce_LoadLibraryA(LPCSTR lpLibFileName);
  MOZCE_SHUNT_API int mozce_GetObjectA(HGDIOBJ hgdiobj, int cbBuffer, LPVOID lpvObject);
  MOZCE_SHUNT_API FARPROC mozce_GetProcAddressA(HMODULE hMod, const char *name);
  MOZCE_SHUNT_API HCURSOR mozce_LoadCursorA(HINSTANCE hInstance, LPCSTR lpCursorName);

  MOZCE_SHUNT_API int mozce_StartDocA(HDC hdc, CONST DOCINFO* lpdi);

  MOZCE_SHUNT_API BOOL mozce_GetOpenFileNameA(LPOPENFILENAMEA lpofna);
  MOZCE_SHUNT_API BOOL mozce_GetSaveFileNameA(LPOPENFILENAMEA lpofna);
  MOZCE_SHUNT_API BOOL mozce_CreateDirectoryA(const char *lpName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);

  MOZCE_SHUNT_API HMODULE mozce_GetModuleHandleA(const char *lpName);
  MOZCE_SHUNT_API HICON mozce_LoadIconA(HINSTANCE hInstance, LPCSTR lpIconName);

  MOZCE_SHUNT_API HRSRC mozce_FindResourceA(HMODULE  hModule, LPCSTR  lpName, LPCSTR  lpType);
  MOZCE_SHUNT_API int mozce_MessageBoxA(HWND hwnd, const char *txt, const char *caption, UINT flags);
  
  MOZCE_SHUNT_API UINT mozce_GetDlgItemTextA(HWND hDlg, int nIDDlgItem, LPSTR lpString, int nMaxCount);
  MOZCE_SHUNT_API BOOL mozce_SetDlgItemTextA(HWND hwndDlg, int idControl, LPCSTR lpsz);
  MOZCE_SHUNT_API HANDLE mozce_CreateEventA(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, const char *lpName);

  MOZCE_SHUNT_API HANDLE mozce_GetPropA(HWND hWnd, LPCSTR lpString);
  MOZCE_SHUNT_API BOOL mozce_SetPropA(HWND hWnd, LPCSTR lpString, HANDLE hData);
  MOZCE_SHUNT_API HANDLE mozce_RemovePropA(HWND hWnd, LPCSTR lpString);

  MOZCE_SHUNT_API HANDLE mozce_FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATA lpFindFileData); 
  MOZCE_SHUNT_API BOOL mozce_FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATA lpFindFileData);
 
  MOZCE_SHUNT_API HANDLE mozce_CreateFileMappingA(HANDLE hFile, LPSECURITY_ATTRIBUTES lpFileMappingAttributes, DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCSTR lpName);

  MOZCE_SHUNT_API DWORD mozce_FormatMessageA(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId, LPSTR lpBuffer, DWORD nSize, va_list* Arguments);

  MOZCE_SHUNT_API HANDLE mozce_CreateSemaphoreA(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes, LONG lInitialCount, LONG lMaximumCount, LPCSTR lpName);
  MOZCE_SHUNT_API HFONT mozce_CreateFontIndirectA(CONST LOGFONT* lplf);
  MOZCE_SHUNT_API int mozce_EnumFontFamiliesA(HDC hdc, LPCTSTR lpszFamily, FONTENUMPROC lpEnumFontFamProc, LPARAM lParam);
  MOZCE_SHUNT_API int mozce_GetTextFaceA(HDC hdc, int nCount,  LPTSTR lpFaceName);
  MOZCE_SHUNT_API BOOL mozce_GetTextMetricsA(HDC hdc, LPTEXTMETRIC lptm);


  // From win32w.cpp
  MOZCE_SHUNT_API BOOL mozce_SetCurrentDirectoryW(LPCTSTR inPathName);
  MOZCE_SHUNT_API DWORD mozce_GetCurrentDirectoryW(DWORD inBufferLength, LPTSTR outBuffer);
  MOZCE_SHUNT_API DWORD mozce_GetGlyphOutlineW(HDC inDC, WCHAR inChar, UINT inFormat, void* inGM, DWORD inBufferSize, LPVOID outBuffer, CONST void* inMAT2);
  MOZCE_SHUNT_API HANDLE mozce_OpenSemaphoreW(DWORD inDesiredAccess, BOOL inInheritHandle, LPCWSTR inName);
  MOZCE_SHUNT_API UINT mozce_GetSystemDirectoryW(LPWSTR inBuffer, UINT inSize);
  MOZCE_SHUNT_API UINT mozce_GetWindowsDirectoryW(LPWSTR inBuffer, UINT inSize);
  
#ifdef __cplusplus
};
#endif

#endif //MOZCE_SHUNT_H
