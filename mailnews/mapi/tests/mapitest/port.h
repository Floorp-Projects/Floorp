/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
//

#ifndef PORT_H
#define PORT_H

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************\
 *                                                               *
 *  PORT.H                                                       *
 *                                                               *
 *  Win16/Win32 portability stuff                                *
 *                                                               *
 *  A.Sokolsky                                                   *
 *      3.10.94 distilled into this header                       *
 *                                                               *
\*****************************************************************/

/*
 * calling conventions
 */
#include <assert.h>
                                                      
                                                      
#ifndef CDECL
#define CDECL __cdecl
#endif  // CDECL

#ifndef PASCAL
#define PASCAL __pascal
#endif  // PASCAL

#ifdef FASTCALL
#error FASTCALL defined
#endif // FASTCALL


#ifdef NDEBUG
#define FASTCALL __fastcall
#else
#define FASTCALL PASCAL
#endif // NDEBUG   



#ifndef HWND2DWORD
#  ifdef WIN32
#    define HWND2DWORD(X_hWnd) ( (DWORD)(X_hWnd) )
#  else // WIN16
#    define HWND2DWORD(X_hWnd) ( (DWORD)MAKELONG(((WORD)(X_hWnd)), 0) )
#  endif
#endif // HWND2DWORD


/*
 * WIN16 - WIN32 compatibility stuff
 */

#ifdef WIN32

# define DLLEXPORT    __declspec( dllexport )
# define EXPORT
# define LOADDS
# define HUGE
# ifndef FAR
#  define FAR
# endif // FAR
# ifndef NEAR
#  define NEAR
# endif // NEAR 
# ifdef UNICODE
#  define SIZEOF(x)   (sizeof(x)/sizeof(WCHAR))
# else
#  define SIZEOF(x)   sizeof(x)
# endif

#else  // !WIN32 == WIN16

# define DLLEXPORT    
# define EXPORT       __export
# define LOADDS       __loadds
# define HUGE         __huge
# ifndef FAR
#   define FAR        __far
#   define NEAR       __near
# endif  // FAR
# define CONST const
# define SIZEOF(x)    sizeof(x)
# define CHAR char
# define TCHAR char
# define WCHAR char
# ifndef LPTSTR
#   define LPTSTR LPSTR
# endif
# ifndef LPCTSTR 
#   define LPCTSTR LPCSTR
# endif
# define UNREFERENCED_PARAMETER(x) x;
# ifndef TEXT
#   define TEXT(x) x
# endif
# define GetWindowTextW GetWindowText
# define lstrcpyW lstrcpy   

# define BN_DBLCLK BN_DOUBLECLICKED // ~~MRJ needed for custom control.
// ~~MRJ begin Win95 backward compat section
# define LPWSTR LPSTR
# define LPCWSTR LPCSTR

// button check state for WIN16
#ifndef BST_UNCHECKED
#define BST_UNCHECKED      0x0000
#endif

#ifndef BST_CHECKED
#define BST_CHECKED        0x0001
#endif


#ifndef WIN95_COMPAT
#  define WIN95_COMPAT
#endif

// ~~MRJ end Win95 compat section.

// critical section API stubs
typedef DWORD CRITICAL_SECTION;
typedef CRITICAL_SECTION FAR * LPCRITICAL_SECTION; 
#ifdef __cplusplus
inline void InitializeCriticalSection(LPCRITICAL_SECTION lpSection) {}
inline void DeleteCriticalSection(LPCRITICAL_SECTION lpSection) {}
inline void EnterCriticalSection(LPCRITICAL_SECTION lpSection) {}
inline void LeaveCriticalSection(LPCRITICAL_SECTION lpSection) {} 
#endif // __cplusplus 

// Added for nssock16 ---Neeti
#ifndef ZeroMemory
#include <memory.h>
#define ZeroMemory(PTR, SIZE) memset(PTR, 0, SIZE)
#endif // ZeroMemory

#endif // WIN16

/*
 * unix - windows compatibility stuff
 */
typedef DWORD u_int32;
typedef WORD  u_int16;
typedef BYTE  u_int8;
#ifdef WIN32
typedef short int Bool16;
#else  // WIN16
typedef BOOL  Bool16;
#endif // WIN16

/*
 * Cross Platform Compatibility
 */
#ifndef UNALIGNED
# ifdef _M_ALPHA
#  define UNALIGNED __unaligned
# else  // !_M_ALPHA
#  define UNALIGNED
# endif // !_M_ALPHA
#endif  // UNALIGNED

//
// RICHIE - for the Alpha port
//
#ifdef _M_ALPHA
# undef pascal
# undef PASCAL
# if (_MSC_VER >= 800) || defined(_STDCALL_SUPPORTED)
#  define pascal __stdcall
#  define PASCAL __stdcall
# else
#  define PASCAL
# endif
#endif


/*
 * Useful Types
 */
typedef char HUGE *HPSTR;
typedef const char HUGE *HPCSTR;
typedef unsigned char HUGE *HPBYTE;
typedef WORD HUGE *HPWORD;
typedef UINT FAR *LPUINT;
typedef BOOL (CALLBACK *USERABORTPROC)();
typedef BOOL (CALLBACK *PROGRESSPROC)(UINT uPos, UINT uRange);
typedef int INT;   // ~~MRJ a function needed this defined.
typedef MINMAXINFO FAR *LPMINMAXINFO; // ~~MRJ 

//
// stuff missing from windows.h
//
#ifndef MAKEWORD
#define MAKEWORD(low, high)    ((WORD)(((BYTE)(low)) | (((WORD)((BYTE)(high))) << 8)))
#endif // MAKEWORD

#ifdef WIN32
# ifndef hmemcpy
#  define hmemcpy memcpy
# endif // !defined(hmemcpy)
# define _fmemset memset

# include <malloc.h>

#ifdef __cplusplus

inline BOOL IsGDIObject(HGDIOBJ hObj) { return (hObj != 0); }
inline void *_halloc(long num, unsigned int size) { return malloc(num * size); }
inline void _hfree( void *memblock ) { free(memblock); } 
/*
inline BOOL IsInstance(HINSTANCE hInst) {
# ifdef WIN32
    return (hInst != 0);
# else // WIN16
    return (hInst > HINSTANCE_ERROR);
# endif  
}  
*/

#endif // __cplusplus

  WINUSERAPI HANDLE WINAPI LoadImageA(HINSTANCE, LPCSTR, UINT, int, int, UINT);

#endif // WIN32

#ifdef __cplusplus 
inline BOOL IsInstance(HINSTANCE hInst) {
# ifdef WIN32
    return (hInst != 0);
# else // WIN16
    return (hInst > HINSTANCE_ERROR);
# endif  
} 
inline void SetWindowSmallIcon(HINSTANCE hInst, HWND hWnd, UINT uIconResourceId) {
#ifdef WIN32
# ifndef WM_SETICON
#   define WM_SETICON 0x0080
# endif  // WM_SETICON
# ifndef IMAGE_ICON
#   define IMAGE_ICON 1
# endif
  assert(IsWindow(hWnd));
  HICON hIcon = (HICON)LoadImageA(hInst, MAKEINTRESOURCE(uIconResourceId), IMAGE_ICON,
    16, 16, 0);
  if(NULL != hIcon) {
    SendMessage(hWnd, WM_SETICON, FALSE, (LPARAM)hIcon);
  } else {
    HICON hIcon = LoadIcon(hInst, MAKEINTRESOURCE(uIconResourceId));
    assert(hIcon != 0);
    SendMessage(hWnd, WM_SETICON, FALSE, (LPARAM)hIcon);
  }
#endif // WIN32
}
#endif // __cplusplus

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* PORT_H */
