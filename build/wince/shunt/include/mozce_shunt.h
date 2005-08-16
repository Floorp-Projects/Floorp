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

//////////////////////////////////////////////////////////
// Function Mapping
//////////////////////////////////////////////////////////

// From asswert.cpp
#ifdef _assert
#undef _assert
#endif
#define _assert		mozce_assert

#ifdef assert
#undef assert
#endif
#define assert		mozce_assert


// From direct.cpp
#ifdef mkdir
#undef mkdir
#endif
#define mkdir		mozce_mkdir

#ifdef _mkdir
#undef _mkdir
#endif
#define _mkdir		mozce_mkdir

#ifdef rmdir
#undef rmdir
#endif
#define rmdir		mozce_rmdir


// From errno.cpp
#ifdef errno
#undef errno
#endif
#define errno		mozce_errno


// From io.cpp
#ifdef chmod
#undef chmod
#endif
#define chmod		mozce_chmod

#ifdef _isatty
#undef _isatty
#endif
#define _isatty		mozce_isatty

#ifdef isatty
#undef isatty
#endif
#define isatty		mozce_isatty


#ifdef fileno
#undef fileno
#endif
#define fileno		mozce_fileno

#ifdef _fileno
#undef _fileno
#endif
#define _fileno		mozce_fileno

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
#ifdef _mbsinc
#undef _mbsinc
#endif
#define _mbsinc		mozce_mbsinc

#ifdef _mbspbrk
#undef _mbspbrk
#endif
#define _mbspbrk	mozce_mbspbrk

#ifdef _mbsrchr
#undef _mbsrchr
#endif
#define _mbsrchr	mozce_mbsrchr

#ifdef _mbschr
#undef _mbschr
#endif
#define _mbschr		mozce_mbschr

#ifdef _mbctolower
#undef _mbctolower
#endif
#define _mbctolower tolower 

#ifdef _mbsicmp
#undef _mbsicmp
#endif
#define _mbsicmp    mozce_mbsicmp


// From process.cpp
#ifdef abort
#undef abort
#endif
#define abort		mozce_abort

#ifdef getenv
#undef getenv
#endif
#define getenv		mozce_getenv

#ifdef putenv
#undef putenv
#endif
#define putenv		mozce_putenv

#ifdef getpid
#undef getpid
#endif
#define getpid		mozce_getpid


// From signal.cpp
#ifdef raise
#undef raise
#endif
#define raise		mozce_raise

#ifdef signal
#undef signal
#endif
#define signal		mozce_signal


// From stat.cpp
#ifdef stat
#undef stat
#endif
#define stat		mozce_stat


// From stdio.cpp
#ifdef access
#undef access
#endif
#define access		mozce_access

#ifdef rewind
#undef rewind
#endif
#define rewind		mozce_rewind

#ifdef fdopen
#undef fdopen
#endif
#define	fdopen		mozce_fdopen

#ifdef _fdopen
#undef _fdopen
#endif
#define	_fdopen		mozce_fdopen

#ifdef perror
#undef perror
#endif
#define	perror		mozce_perror

#ifdef remove
#undef remove
#endif
#define remove		mozce_remove


#ifdef _getcwd
#undef _getcwd
#endif
#define _getcwd     mozce_getcwd

#ifdef getcwd
#undef getcwd
#endif
#define getcwd      mozce_getcwd

#ifdef printf
#undef printf
#endif
#define printf mozce_printf

#ifdef open
#undef open
#endif
#define open mozce_open

#ifdef _open
#undef _open
#endif
#define _open mozce_open

#ifdef close
#undef close
#endif
#define close mozce_close

#ifdef read
#undef read
#endif
#define read mozce_read

#ifdef write
#undef write
#endif
#define write mozce_write

#ifdef unlink
#undef unlink
#endif
#define unlink mozce_unlink

#ifdef lseek
#undef lseek
#endif
#define lseek mozce_lseek

// From stdlib.cpp
#ifdef _fullpath
#undef _fullpath
#endif
#define _fullpath	mozce_fullpath

#ifdef _splitpath
#undef _splitpath
#endif
#define _splitpath	mozce_splitpath

#ifdef _makepath
#undef _makepath
#endif
#define _makepath	mozce_makepath



#define lstrlenA  strlen
#define lstrcpyA  strcpy
#define lstrcmpA  strcmp
#define lstrcmpiA strcmpi
#define lstrcatA  strcat

#ifdef strdup
#undef strdup
#endif
#define strdup      _strdup

#ifdef stricmp
#undef stricmp
#endif
#define stricmp     _stricmp

#ifdef strcmpi
#undef strcmpi
#endif
#define strcmpi     _stricmp

#ifdef strnicmp
#undef strnicmp
#endif
#define strnicmp    _strnicmp


// From string.cpp
#ifdef strerror
#undef strerror
#endif
#define strerror	mozce_strerror

#ifdef wsprintfA
#undef wsprintfA
#endif
#define wsprintfA mozce_wsprintfA

// From time.cpp
#ifdef strftime
#undef strftime
#endif
#define strftime	mozce_strftime

#ifdef localtime_r
#undef localtime_r
#endif
#define localtime_r	mozce_localtime_r

#ifdef localtime
#undef localtime
#endif
#define localtime	mozce_localtime

#ifdef gmtime_r
#undef gmtime_r
#endif
#define gmtime_r	mozce_gmtime_r

#ifdef gmtime
#undef gmtime
#endif
#define gmtime	    mozce_gmtime

#ifdef mktime
#undef mktime
#endif
#define mktime	    mozce_mktime

//#ifdef time
//#undef time
//#endif
//#define time		mozce_time

#ifdef ctime
#undef ctime
#endif
#define ctime		mozce_ctime


#ifdef localeconv
#undef localeconv
#endif
#define localeconv  mozce_localeconv

//#ifdef lconv
//#undef lconv
//#endif
//#define lconv       mozce_lconv


// From win32.cpp
#ifdef Arc
#undef Arc
#endif
#define Arc                       mozce_Arc

#ifdef CallNextHookEx
#undef CallNextHookEx
#endif
#define CallNextHookEx            mozce_CallNextHookEx

#ifdef CreateDIBitmap
#undef CreateDIBitmap
#endif
#define CreateDIBitmap            mozce_CreateDIBitmap

#ifdef EnumChildWindows
#undef EnumChildWindows
#endif
#define EnumChildWindows          mozce_EnumChildWindows

#ifdef EnumFontFamiliesEx
#undef EnumFontFamiliesEx
#endif
#define EnumFontFamiliesEx        mozce_EnumFontFamiliesEx

#ifdef EnumThreadWindows
#undef EnumThreadWindows
#endif
#define EnumThreadWindows         mozce_EnumThreadWindows

#ifdef ExtSelectClipRgn
#undef ExtSelectClipRgn
#endif
#define ExtSelectClipRgn          mozce_ExtSelectClipRgn

#ifdef ExpandEnvironmentStrings
#undef ExpandEnvironmentStrings
#endif
#define ExpandEnvironmentStrings  mozce_ExpandEnvironmentStrings

#ifdef FIXED
#undef FIXED
#endif
#define FIXED                     mozce_FIXED

#ifdef FlashWindow
#undef FlashWindow
#endif
#define FlashWindow               mozce_FlashWindow

#ifdef FrameRect
#undef FrameRect
#endif
#define FrameRect                 mozce_FrameRect

#ifdef GdiFlush
#undef GdiFlush
#endif
#define GdiFlush                 mozce_GdiFlush

#ifdef GLYPHMETRICS
#undef GLYPHMETRICS
#endif
#define GLYPHMETRICS              mozce_GLYPHMETRICS

#ifdef GetCurrentProcess
#undef GetCurrentProcess
#endif
#define GetCurrentProcess         mozce_GetCurrentProcess

#ifdef GetCurrentProcessId
#undef GetCurrentProcessId
#endif
#define GetCurrentProcessId       mozce_GetCurrentProcessId

#ifdef GetCurrentThreadId
#undef GetCurrentThreadId
#endif
#define GetCurrentThreadId        mozce_GetCurrentThreadId


#ifdef GetDIBits
#undef GetDIBits
#endif
#define GetDIBits                 mozce_GetDIBits

#ifdef GetEnvironmentVariable
#undef GetEnvironmentVariable
#endif
#define GetEnvironmentVariable    mozce_GetEnvironmentVariable

#ifdef GetFontData
#undef GetFontData
#endif
#define GetFontData               mozce_GetFontData

#ifdef GetFullPathName
#undef GetFullPathName
#endif
#define GetFullPathName           mozce_GetFullPathName

#ifdef GetIconInfo
#undef GetIconInfo
#endif
#define GetIconInfo               mozce_GetIconInfo

#ifdef GetMapMode
#undef GetMapMode
#endif
#define GetMapMode                mozce_GetMapMode

#ifdef GetMessageA
#undef GetMessageA
#endif
#define GetMessageA               mozce_GetMessage

#ifdef GetMessageW
#undef GetMessageW
#endif
#define GetMessageW               mozce_GetMessage

#ifdef GetMessageTime
#undef GetMessageTime
#endif
#define GetMessageTime            mozce_GetMessageTime

#ifdef GetOutlineTextMetrics
#undef GetOutlineTextMetrics
#endif
#define GetOutlineTextMetrics     mozce_GetOutlineTextMetrics

#ifdef GetScrollPos
#undef GetScrollPos
#endif
#define GetScrollPos              mozce_GetScrollPos

#ifdef GetScrollRange
#undef GetScrollRange
#endif
#define GetScrollRange            mozce_GetScrollRange

#ifdef GetShortPathName
#undef GetShortPathName
#endif
#define GetShortPathName          mozce_GetShortPathName

#ifdef GetSystemTimeAsFileTime
#undef GetSystemTimeAsFileTime
#endif
#define GetSystemTimeAsFileTime   mozce_GetSystemTimeAsFileTime

#ifdef GetTextCharset
#undef GetTextCharset
#endif
#define GetTextCharset            mozce_GetTextCharset

#ifdef GetTextCharsetInfo
#undef GetTextCharsetInfo
#endif
#define GetTextCharsetInfo        mozce_GetTextCharsetInfo

#ifdef GetUserName
#undef GetUserName
#endif
#define GetUserName               mozce_GetUserName


#ifdef GetWindowPlacement
#undef GetWindowPlacement
#endif
#define GetWindowPlacement       mozce_GetWindowPlacement

#ifdef InvertRgn
#undef InvertRgn
#endif
#define InvertRgn                 mozce_InvertRgn

#ifdef IsIconic
#undef IsIconic
#endif
#define IsIconic                  mozce_IsIconic

#ifdef LINEDDAPROC
#undef LINEDDAPROC
#endif
#define LINEDDAPROC               mozce_LINEDDAPROC

#ifdef LPtoDP
#undef LPtoDP
#endif
#define LPtoDP                    mozce_LPtoDP

#ifdef LineDDA
#undef LineDDA
#endif
#define LineDDA                   mozce_LineDDA

#ifdef LineDDAProc
#undef LineDDAProc
#endif
#define LineDDAProc               mozce_LineDDAProc

#ifdef MAT2
#undef MAT2
#endif
#define MAT2                      mozce_MAT2

#ifdef MsgWaitForMultipleObjects
#undef MsgWaitForMultipleObjects
#endif
#define MsgWaitForMultipleObjects mozce_MsgWaitForMultipleObjects 

#ifdef MulDiv
#undef MulDiv
#endif
#define MulDiv                    mozce_MulDiv

#ifdef OUTLINETEXTMETRIC
#undef OUTLINETEXTMETRIC
#endif
#define OUTLINETEXTMETRIC         mozce_OUTLINETEXTMETRIC

#ifdef OpenIcon
#undef OpenIcon
#endif
#define OpenIcon                  mozce_OpenIcon

#ifdef Pie
#undef Pie
#endif
#define Pie                       mozce_Pie

#ifdef PeekMessageA
#undef PeekMessageA
#endif
#define PeekMessageA              mozce_PeekMessage

#ifdef PeekMessageW
#undef PeekMessageW
#endif
#define PeekMessageW              mozce_PeekMessage

#ifdef RegCreateKey
#undef RegCreateKey
#endif
#define RegCreateKey              mozce_RegCreateKey

#ifdef SetArcDirection
#undef SetArcDirection
#endif
#define SetArcDirection           mozce_SetArcDirection

#ifdef SetDIBits
#undef SetDIBits
#endif
#define SetDIBits                 mozce_SetDIBits

#ifdef SetMenu
#undef SetMenu
#endif
#define SetMenu                   mozce_SetMenu

#ifdef SetPolyFillMode
#undef SetPolyFillMode
#endif
#define SetPolyFillMode           mozce_SetPolyFillMode

#ifdef SetStretchBltMode
#undef SetStretchBltMode
#endif
#define SetStretchBltMode         mozce_SetStretchBltMode

#ifdef SetWindowsHookEx
#undef SetWindowsHookEx
#endif
#define SetWindowsHookEx          mozce_SetWindowsHookEx

#ifdef SetWindowTextA
#undef SetWindowTextA
#endif
#define SetWindowTextA            mozce_SetWindowTextA

#ifdef TlsAlloc
#undef TlsAlloc
#endif
#define TlsAlloc                  mozce_TlsAlloc

#ifdef TlsFree
#undef TlsFree
#endif
#define TlsFree                   mozce_TlsFree

#ifdef UnhookWindowsHookEx
#undef UnhookWindowsHookEx
#endif
#define UnhookWindowsHookEx       mozce_UnhookWindowsHookEx

#ifdef WaitMessage
#undef WaitMessage
#endif
#define WaitMessage               mozce_WaitMessage

#ifdef _CoLockObjectExternal
#undef _CoLockObjectExternal
#endif
#define _CoLockObjectExternal     mozce_CoLockObjectExternal

#ifdef IsClipboardFormatAvailable
#undef IsClipboardFormatAvailable
#endif
#define IsClipboardFormatAvailable mozce_IsClipboardFormatAvailable

#ifdef OleFlushClipboard
#undef OleFlushClipboard
#endif
#define OleFlushClipboard        mozce_OleFlushClipboard

#ifdef OleGetClipboard
#undef OleGetClipboard
#endif
#define OleGetClipboard          mozce_OleGetClipboard

#ifdef OleQueryLinkFromData
#undef OleQueryLinkFromData
#endif
#define OleQueryLinkFromData     mozce_OleQueryLinkFromData

#ifdef OleSetClipboard
#undef OleSetClipboard
#endif
#define OleSetClipboard          mozce_OleSetClipboard


// From win32a.cpp

#ifdef CopyFileA
#undef CopyFileA
#endif
#define CopyFileA                 mozce_CopyFileA

#ifdef CreateDCA
#undef CreateDCA
#endif
#define CreateDCA                 mozce_CreateDCA

#ifdef CreateDCA2
#undef CreateDCA2
#endif
#define CreateDCA2                mozce_CreateDCA2

#ifdef CreateDirectoryA
#undef CreateDirectoryA
#endif
#define CreateDirectoryA          mozce_CreateDirectoryA


// We use a method named CreateEvent.  We do not want to map
// CreateEvent to CreateEventA
#ifdef CreateEvent
#undef CreateEvent
#endif
#define CreateEvent               CreateEvent

#ifdef CreateEventA
#undef CreateEventA
#endif
#define CreateEventA              mozce_CreateEventA

#ifdef CreateFileA
#undef CreateFileA
#endif
#define CreateFileA               mozce_CreateFileA

#ifdef CreateFileMappingA
#undef CreateFileMappingA
#endif
#define CreateFileMappingA        mozce_CreateFileMappingA

#ifdef CreateFontIndirectA
#undef CreateFontIndirectA
#endif
#define CreateFontIndirectA       mozce_CreateFontIndirectA

#ifdef CreateMutexA
#undef CreateMutexA
#endif
#define CreateMutexA              mozce_CreateMutexA

#ifdef CreateProcessA
#undef CreateProcessA
#endif
#define CreateProcessA            mozce_CreateProcessA

#ifdef CreateSemaphoreA
#undef CreateSemaphoreA
#endif
#define CreateSemaphoreA          mozce_CreateSemaphoreA

#ifdef CreateWindowExA
#undef CreateWindowExA
#endif
#define CreateWindowExA           mozce_CreateWindowExA

#ifdef DefWindowProcA
#undef DefWindowProcA
#endif
#define DefWindowProcA            mozce_DefWindowProcA

#ifdef DeleteFileA
#undef DeleteFileA
#endif
#define DeleteFileA               mozce_DeleteFileA

#ifdef DrawTextA
#undef DrawTextA
#endif
#define DrawTextA                 mozce_DrawTextA

#ifdef EnumFontFamiliesA
#undef EnumFontFamiliesA
#endif
#define EnumFontFamiliesA         mozce_EnumFontFamiliesA

#ifdef TextOut
#undef TextOut
#endif
#define TextOut                   mozce_TextOutA

#ifdef ExtTextOutA
#undef ExtTextOutA
#endif
#define ExtTextOutA               mozce_ExtTextOutA

#ifdef FindResourceA
#undef FindResourceA
#endif
#define FindResourceA             mozce_FindResourceA

#ifdef FindWindowA
#undef FindWindowA
#endif
#define FindWindowA               mozce_FindWindowA

#ifdef FormatMessageA
#undef FormatMessageA
#endif
#define FormatMessageA            mozce_FormatMessageA

#ifdef GetClassInfoA
#undef GetClassInfoA
#endif
#define GetClassInfoA             mozce_GetClassInfoA

#ifdef GetClassNameA
#undef GetClassNameA
#endif
#define GetClassNameA             mozce_GetClassNameA

#ifdef GetCurrentDirectoryA
#undef GetCurrentDirectoryA
#endif
#define GetCurrentDirectoryA      mozce_GetCurrentDirectoryA

#ifdef GetDlgItemTextA
#undef GetDlgItemTextA
#endif
#define GetDlgItemTextA           mozce_GetDlgItemTextA

#ifdef GetEnvironmentVariableA
#undef GetEnvironmentVariableA
#endif
#define GetEnvironmentVariableA   mozce_GetEnvironmentVariableA

#ifdef GetFileAttributesA
#undef GetFileAttributesA
#endif
#define GetFileAttributesA        mozce_GetFileAttributesA

#ifdef GetFileVersionInfoA
#undef GetFileVersionInfoA
#endif
#define GetFileVersionInfoA       mozce_GetFileVersionInfoA

#ifdef GetFileVersionInfoSizeA
#undef GetFileVersionInfoSizeA
#endif
#define GetFileVersionInfoSizeA   mozce_GetFileVersionInfoSizeA

#ifdef GetGlyphOutlineA
#undef GetGlyphOutlineA
#endif
#define GetGlyphOutlineA          mozce_GetGlyphOutlineA

#ifdef GetLocaleInfoA
#undef GetLocaleInfoA
#endif
#define GetLocaleInfoA            mozce_GetLocaleInfoA

#ifdef GetModuleFileNameA
#undef GetModuleFileNameA
#endif
#define GetModuleFileNameA        mozce_GetModuleFileNameA

#ifdef GetModuleHandleA
#undef GetModuleHandleA
#endif
#define GetModuleHandleA          mozce_GetModuleHandleA

#ifdef GetObjectA
#undef GetObjectA
#endif
#define GetObjectA                mozce_GetObjectA

#ifdef GetOpenFileNameA
#undef GetOpenFileNameA
#endif
#define GetOpenFileNameA          mozce_GetOpenFileNameA

#ifdef GetProcAddress
#undef GetProcAddress
#endif
#define GetProcAddress            mozce_GetProcAddressA

#ifdef GetProcAddressA
#undef GetProcAddressA
#endif
#define GetProcAddressA           mozce_GetProcAddressA

#ifdef GetPropA
#undef GetPropA
#endif
#define GetPropA                  mozce_GetPropA

#ifdef GetSaveFileNameA
#undef GetSaveFileNameA
#endif
#define GetSaveFileNameA          mozce_GetSaveFileNameA

#ifdef GetSystemDirectory
#undef GetSystemDirectory
#endif
#define GetSystemDirectory        mozce_GetSystemDirectoryA

#ifdef GetSystemDirectoryA
#undef GetSystemDirectoryA
#endif
#define GetSystemDirectoryA       mozce_GetSystemDirectoryA

#ifdef GetTextExtentExPointA
#undef GetTextExtentExPointA
#endif
#define GetTextExtentExPointA     mozce_GetTextExtentExPointA

#ifdef GetTextFaceA
#undef GetTextFaceA
#endif
#define GetTextFaceA              mozce_GetTextFaceA

#ifdef GetTextMetricsA
#undef GetTextMetricsA
#endif
#define GetTextMetricsA           mozce_GetTextMetricsA

#ifdef GetVersionExA
#undef GetVersionExA
#endif
#define GetVersionExA             mozce_GetVersionExA

#ifdef GetWindowsDirectory
#undef GetWindowsDirectory
#endif
#define GetWindowsDirectory       mozce_GetWindowsDirectoryA

#ifdef GetWindowsDirectoryA
#undef GetWindowsDirectoryA
#endif
#define GetWindowsDirectoryA      mozce_GetWindowsDirectoryA

#ifdef GlobalAddAtomA
#undef GlobalAddAtomA
#endif
#define GlobalAddAtomA            mozce_GlobalAddAtomA

#ifdef LoadCursorA
#undef LoadCursorA
#endif
#define LoadCursorA               mozce_LoadCursorA

#ifdef LoadIconA
#undef LoadIconA
#endif
#define LoadIconA                 mozce_LoadIconA

#ifdef LoadImageA
#undef LoadImageA
#endif
#define LoadImageA                mozce_LoadImageA

#ifdef LoadLibraryA
#undef LoadLibraryA
#endif
#define LoadLibraryA              mozce_LoadLibraryA

#ifdef LoadMenuA
#undef LoadMenuA
#endif
#define LoadMenuA                 mozce_LoadMenuA

#ifdef LoadStringA
#undef LoadStringA
#endif
#define LoadStringA               mozce_LoadStringA

#ifdef MessageBoxA
#undef MessageBoxA
#endif
#define MessageBoxA               mozce_MessageBoxA

#ifdef MoveFileA
#undef MoveFileA
#endif
#define MoveFileA                 mozce_MoveFileA

#ifdef OpenSemaphoreA
#undef OpenSemaphoreA
#endif
#define OpenSemaphoreA            mozce_OpenSemaphoreA

#ifdef OutputDebugStringA
#undef OutputDebugStringA
#endif
#define OutputDebugStringA        mozce_OutputDebugStringA

#ifdef PostMessageA
#undef PostMessageA
#endif
#define PostMessageA              mozce_PostMessageA

#ifdef RegEnumKeyExA
#undef RegEnumKeyExA
#endif
#define RegEnumKeyExA             mozce_RegEnumKeyExA

#ifdef RegOpenKeyExA
#undef RegOpenKeyExA
#endif
#define RegOpenKeyExA             mozce_RegOpenKeyExA

#ifdef RegQueryValueExA
#undef RegQueryValueExA
#endif
#define RegQueryValueExA          mozce_RegQueryValueExA

#ifdef RegSetValueExA
#undef RegSetValueExA
#endif
#define RegSetValueExA            mozce_RegSetValueExA

#ifdef RegCreateKeyExA
#undef RegCreateKeyExA
#endif
#define RegCreateKeyExA           mozce_RegCreateKeyExA

#ifdef RegDeleteValueA
#undef RegDeleteValueA
#endif
#define RegDeleteValueA           mozce_RegDeleteValueA

#ifdef RegisterClassA
#undef RegisterClassA
#endif
#define RegisterClassA            mozce_RegisterClassA

#ifdef RegisterClipboardFormatA
#undef RegisterClipboardFormatA
#endif
#define RegisterClipboardFormatA  mozce_RegisterClipboardFormatA

#ifdef RegisterWindowMessageA
#undef RegisterWindowMessageA
#endif
#define RegisterWindowMessageA    mozce_RegisterWindowMessageA

#ifdef RemoveDirectoryA
#undef RemoveDirectoryA
#endif
#define RemoveDirectoryA          mozce_RemoveDirectoryA

#ifdef RemovePropA
#undef RemovePropA
#endif
#define RemovePropA               mozce_RemovePropA

#ifdef SendMessageA
#undef SendMessageA
#endif
#define SendMessageA              mozce_SendMessageA

#ifdef SetCurrentDirectoryA
#undef SetCurrentDirectoryA
#endif
#define SetCurrentDirectoryA      mozce_SetCurrentDirectoryA

#ifdef SetCurrentDirectory
#undef SetCurrentDirectory
#endif
#define SetCurrentDirectory      mozce_SetCurrentDirectoryA

#ifdef SetDlgItemTextA
#undef SetDlgItemTextA
#endif
#define SetDlgItemTextA           mozce_SetDlgItemTextA

#ifdef SetEnvironmentVariable
#undef SetEnvironmentVariable
#endif
#define SetEnvironmentVariable    mozce_SetEnvironmentVariableA

#ifdef SetEnvironmentVariableA
#undef SetEnvironmentVariableA
#endif
#define SetEnvironmentVariableA   mozce_SetEnvironmentVariableA

#ifdef SetPropA
#undef SetPropA
#endif
#define SetPropA                  mozce_SetPropA

#ifdef StartDocA
#undef StartDocA
#endif
#define StartDocA                 mozce_StartDocA

#ifdef UnregisterClassA
#undef UnregisterClassA
#endif
#define UnregisterClassA          mozce_UnregisterClassA

#ifdef VerQueryValueA
#undef VerQueryValueA
#endif
#define VerQueryValueA            mozce_VerQueryValueA


#ifdef CreateDialogIndirectParamA
#undef CreateDialogIndirectParamA
#endif
#define CreateDialogIndirectParamA CreateDialogIndirectParamW

#ifdef SystemParametersInfoA
#undef SystemParametersInfoA
#endif
#define SystemParametersInfoA      SystemParametersInfoW

#ifdef DispatchMessageA
#undef DispatchMessageA
#endif
#define DispatchMessageA           DispatchMessageW

#ifdef CallWindowProcA
#undef CallWindowProcA
#endif
#define CallWindowProcA            CallWindowProcW

#ifdef GetWindowLongA
#undef GetWindowLongA
#endif
#define GetWindowLongA             GetWindowLongW

#ifdef SetWindowLongA
#undef SetWindowLongA
#endif
#define SetWindowLongA             SetWindowLongW


#undef FindFirstFile
#undef FindNextFile

#ifdef FindFirstFile
#undef FindFirstFile
#endif
#define FindFirstFile              FindFirstFileW

#ifdef FindNextFile
#undef FindNextFile
#endif
#define FindNextFile               FindNextFileW



#ifdef GetProp
#undef GetProp
#endif
#define GetProp                   mozce_GetPropA

#ifdef SetProp
#undef SetProp
#endif
#define SetProp                   mozce_SetPropA

#ifdef RemoveProp
#undef RemoveProp
#endif
#define RemoveProp                mozce_RemovePropA



// From win32w.cpp
#ifdef GetCurrentDirectory
#undef GetCurrentDirectory
#endif
#define GetCurrentDirectory       mozce_GetCurrentDirectoryW

#ifdef GetCurrentDirectoryW
#undef GetCurrentDirectoryW
#endif
#define GetCurrentDirectoryW      mozce_GetCurrentDirectoryW

#ifdef GetGlyphOutlineW
#undef GetGlyphOutlineW
#endif
#define GetGlyphOutlineW          mozce_GetGlyphOutlineW

#ifdef GetSystemDirectoryW
#undef GetSystemDirectoryW
#endif
#define GetSystemDirectoryW       mozce_GetSystemDirectoryW

#ifdef GetWindowsDirectoryW
#undef GetWindowsDirectoryW
#endif
#define GetWindowsDirectoryW      mozce_GetWindowsDirectoryW

#ifdef OpenSemaphoreW
#undef OpenSemaphoreW
#endif
#define OpenSemaphoreW            mozce_OpenSemaphoreW

#ifdef SetCurrentDirectoryW
#undef SetCurrentDirectoryW
#endif
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
  MOZCE_SHUNT_API int mozce_fileno(FILE* inHandle);
  
  // From mbstring.cpp
  MOZCE_SHUNT_API unsigned char* mozce_mbsinc(const unsigned char* inCurrent);
  MOZCE_SHUNT_API unsigned char* mozce_mbspbrk(const unsigned char* inString, const unsigned char* inStrCharSet);
  MOZCE_SHUNT_API unsigned char* mozce_mbschr(const unsigned char* inString, unsigned int inC);
  MOZCE_SHUNT_API unsigned char* mozce_mbsrchr(const unsigned char* inString, unsigned int inC);
  MOZCE_SHUNT_API int            mozce_mbsicmp(const unsigned char *string1, const unsigned char *string2);
  
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
  MOZCE_SHUNT_API int mozce_access(const char *path, int mode);
  MOZCE_SHUNT_API void mozce_rewind(FILE* inStream);
  MOZCE_SHUNT_API FILE* mozce_fdopen(int inFD, const char* inMode);
  MOZCE_SHUNT_API void mozce_perror(const char* inString);
  MOZCE_SHUNT_API int mozce_remove(const char* inPath);

  MOZCE_SHUNT_API char* mozce_getcwd(char* buff, size_t size);
  
  MOZCE_SHUNT_API int mozce_printf(const char *, ...);

  MOZCE_SHUNT_API int mozce_open(const char *pathname, int flags, int mode);
  MOZCE_SHUNT_API int mozce_close(int fp);
  MOZCE_SHUNT_API size_t mozce_read(int fp, void* buffer, size_t count);
  MOZCE_SHUNT_API size_t mozce_write(int fp, const void* buffer, size_t count);
  MOZCE_SHUNT_API int mozce_unlink(const char *pathname);
  MOZCE_SHUNT_API int mozce_lseek(int fildes, int offset, int whence);


  // From stdlib.cpp
  MOZCE_SHUNT_API void mozce_splitpath(const char* inPath, char* outDrive, char* outDir, char* outFname, char* outExt);
  MOZCE_SHUNT_API void mozce_makepath(char* outPath, const char* inDrive, const char* inDir, const char* inFname, const char* inExt);
  MOZCE_SHUNT_API char* mozce_fullpath(char *, const char *, size_t);
  
  // From string.cpp
  MOZCE_SHUNT_API char* mozce_strerror(int);
  MOZCE_SHUNT_API int mozce_wsprintfA(LPTSTR lpOut, LPCTSTR lpFmt, ... );

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
  MOZCE_SHUNT_API BOOL mozce_GetWindowPlacement(HWND window, WINDOWPLACEMENT *lpwndpl);
  MOZCE_SHUNT_API BOOL mozce_InvertRgn(HDC inDC, HRGN inRGN);
  MOZCE_SHUNT_API int mozce_GetScrollPos(HWND inWnd, int inBar);
  MOZCE_SHUNT_API BOOL mozce_GetScrollRange(HWND inWnd, int inBar, LPINT outMinPos, LPINT outMaxPos);
  MOZCE_SHUNT_API HRESULT mozce_CoLockObjectExternal(IUnknown* inUnk, BOOL inLock, BOOL inLastUnlockReleases);
  MOZCE_SHUNT_API HRESULT mozce_OleSetClipboard(IDataObject* inDataObj);
  MOZCE_SHUNT_API HRESULT mozce_OleGetClipboard(IDataObject** outDataObj);
  MOZCE_SHUNT_API HRESULT mozce_OleFlushClipboard(void);
  MOZCE_SHUNT_API HRESULT mozce_OleQueryLinkFromData(IDataObject* inSrcDataObject);
  MOZCE_SHUNT_API BOOL  mozce_IsClipboardFormatAvailable(UINT format);

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
  MOZCE_SHUNT_API HANDLE mozce_GetCurrentProcessId(void);
  MOZCE_SHUNT_API DWORD mozce_TlsAlloc(void);
  MOZCE_SHUNT_API BOOL mozce_TlsFree(DWORD dwTlsIndex);
  MOZCE_SHUNT_API DWORD GetCurrentThreadId(void);

  MOZCE_SHUNT_API DWORD mozce_MsgWaitForMultipleObjects(DWORD nCount, const HANDLE* pHandles, BOOL bWaitAll, DWORD dwMilliseconds, DWORD dwWakeMask);

  MOZCE_SHUNT_API BOOL mozce_PeekMessage(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg);
  MOZCE_SHUNT_API BOOL mozce_GetMessage(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax);
  MOZCE_SHUNT_API LONG mozce_GetMessageTime(void);

  // from win32a.cpp
  
  MOZCE_SHUNT_API DWORD mozce_GetGlyphOutlineA(HDC inDC, CHAR inChar, UINT inFormat, void* inGM, DWORD inBufferSize, LPVOID outBuffer, CONST mozce_MAT2* inMAT2);
  MOZCE_SHUNT_API ATOM mozce_GlobalAddAtomA(LPCSTR lpString);
  MOZCE_SHUNT_API ATOM mozce_RegisterClassA(CONST WNDCLASSA *lpwc);
  MOZCE_SHUNT_API BOOL mozce_CopyFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, BOOL bFailIfExists);
  MOZCE_SHUNT_API BOOL mozce_CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
  MOZCE_SHUNT_API BOOL mozce_RemoveDirectoryA(LPCSTR lpPathName);
  MOZCE_SHUNT_API HANDLE mozce_CreateMutexA(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCSTR lpName);
  MOZCE_SHUNT_API BOOL mozce_CreateProcessA(LPCSTR pszImageName, LPCSTR pszCmdLine, LPSECURITY_ATTRIBUTES psaProcess, LPSECURITY_ATTRIBUTES psaThread, BOOL fInheritHandles, DWORD fdwCreate, LPVOID pvEnvironment, LPSTR pszCurDir, LPSTARTUPINFO psiStartInfo, LPPROCESS_INFORMATION pProcInfo);
  MOZCE_SHUNT_API BOOL mozce_ExtTextOutA(HDC inDC, int inX, int inY, UINT inOptions, LPCRECT inRect, LPCSTR inString, UINT inCount, const LPINT inDx);
  MOZCE_SHUNT_API BOOL mozce_TextOutA(HDC  hdc, int  nXStart, int  nYStart, LPCSTR  lpString, int  cbString);

  MOZCE_SHUNT_API BOOL mozce_GetClassInfoA(HINSTANCE hInstance, LPCSTR lpClassName, LPWNDCLASS lpWndClass);
  MOZCE_SHUNT_API int mozce_GetClassNameA(HWND hWnd, LPTSTR lpClassName, int nMaxCount);
  MOZCE_SHUNT_API BOOL mozce_GetFileVersionInfoA(LPSTR inFilename, DWORD inHandle, DWORD inLen, LPVOID outData);
  MOZCE_SHUNT_API BOOL mozce_GetTextExtentExPointA(HDC inDC, LPCSTR inStr, int inLen, int inMaxExtent, LPINT outFit, LPINT outDx, LPSIZE inSize);
  MOZCE_SHUNT_API BOOL mozce_GetVersionExA(LPOSVERSIONINFOA lpv);
  MOZCE_SHUNT_API BOOL mozce_DeleteFileA(LPCSTR lpFileName);
  MOZCE_SHUNT_API BOOL mozce_MoveFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName);
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

  MOZCE_SHUNT_API LONG mozce_RegSetValueExA(HKEY hKey, const char *valname, DWORD dwReserved, DWORD dwType, const BYTE* lpData, DWORD dwSize);
  MOZCE_SHUNT_API LONG mozce_RegCreateKeyExA(HKEY hKey, const char *subkey, DWORD dwRes, LPSTR lpszClass, DWORD ulOptions, REGSAM samDesired, LPSECURITY_ATTRIBUTES sec_att, PHKEY phkResult, DWORD *lpdwDisp);

  MOZCE_SHUNT_API LONG mozce_RegDeleteValueA(HKEY hKey, LPCTSTR lpValueName);

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

  MOZCE_SHUNT_API BOOL mozce_SetWindowTextA(HWND hWnd, LPCSTR lpString);


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
