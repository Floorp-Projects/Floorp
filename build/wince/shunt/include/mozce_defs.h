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

#ifndef _MOZCE_DEFS
#define _MOZCE_DEFS

#ifdef MOZCE_SHUNT_EXPORTS
#define MOZCE_SHUNT_API __declspec(dllexport)
#else
#define MOZCE_SHUNT_API __declspec(dllimport)
#endif

//#define USE_NC_LOGGING 1

#define NOMINMAX

#ifndef XP_WIN
#define XP_WIN
#endif

#ifndef XP_WIN32
#define XP_WIN32 1
#endif

#include <windows.h>

#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

//////////////////////////////////////////////////////////
// Various Definations
//////////////////////////////////////////////////////////

// for errors.h

#ifdef EPERM
#undef EPERM
#endif
#define	EPERM		1	/* Operation not permitted */

#ifdef ENOENT
#undef ENOENT
#endif
#define	ENOENT		2	/* No such file or directory */

#ifdef ESRCH
#undef ESRCH
#endif
#define	ESRCH		3	/* No such process */

#ifdef EINTR
#undef EINTR
#endif
#define	EINTR		4	/* Interrupted system call */

#ifdef EIO
#undef EIO
#endif
#define	EIO		5	/* Input/output error */

#ifdef ENXIO
#undef ENXIO
#endif
#define	ENXIO		6	/* Device not configured */

#ifdef E2BIG
#undef E2BIG
#endif
#define	E2BIG		7	/* Argument list too long */

#ifdef ENOEXEC
#undef ENOEXEC
#endif
#define	ENOEXEC		8	/* Exec format error */

#ifdef EBADF
#undef EBADF
#endif
#define	EBADF		9	/* Bad file descriptor */

#ifdef ECHILD
#undef ECHILD
#endif
#define	ECHILD		10	/* No child processes */

#ifdef EDEADLK
#undef EDEADLK
#endif
#define	EDEADLK		11	/* Resource deadlock avoided */

#ifdef was
#undef was
#endif
				/* 11 was EAGAIN */

#ifdef ENOMEM
#undef ENOMEM
#endif
#define	ENOMEM		12	/* Cannot allocate memory */

#ifdef EACCES
#undef EACCES
#endif
#define	EACCES		13	/* Permission denied */

#ifdef EFAULT
#undef EFAULT
#endif
#define	EFAULT		14	/* Bad address */

#ifdef ENOTBLK
#undef ENOTBLK
#endif
#define	ENOTBLK		15	/* Block device required */

#ifdef EBUSY
#undef EBUSY
#endif
#define	EBUSY		16	/* Device busy */

#ifdef EEXIST
#undef EEXIST
#endif
#define	EEXIST		17	/* File exists */

#ifdef EXDEV
#undef EXDEV
#endif
#define	EXDEV		18	/* Cross-device link */

#ifdef ENODEV
#undef ENODEV
#endif
#define	ENODEV		19	/* Operation not supported by device */

#ifdef ENOTDIR
#undef ENOTDIR
#endif
#define	ENOTDIR		20	/* Not a directory */

#ifdef EISDIR
#undef EISDIR
#endif
#define	EISDIR		21	/* Is a directory */

#ifdef EINVAL
#undef EINVAL
#endif
#define	EINVAL		22	/* Invalid argument */

#ifdef ENFILE
#undef ENFILE
#endif
#define	ENFILE		23	/* Too many open files in system */

#ifdef EMFILE
#undef EMFILE
#endif
#define	EMFILE		24	/* Too many open files */

#ifdef ENOTTY
#undef ENOTTY
#endif
#define	ENOTTY		25	/* Inappropriate ioctl for device */

#ifdef ETXTBSY
#undef ETXTBSY
#endif
#define	ETXTBSY		26	/* Text file busy */

#ifdef EFBIG
#undef EFBIG
#endif
#define	EFBIG		27	/* File too large */

#ifdef ENOSPC
#undef ENOSPC
#endif
#define	ENOSPC		28	/* No space left on device */

#ifdef ESPIPE
#undef ESPIPE
#endif
#define	ESPIPE		29	/* Illegal seek */

#ifdef EROFS
#undef EROFS
#endif
#define	EROFS		30	/* Read-only file system */

#ifdef EMLINK
#undef EMLINK
#endif
#define	EMLINK		31	/* Too many links */

#ifdef EPIPE
#undef EPIPE
#endif
#define	EPIPE		32	/* Broken pipe */

#ifdef EDOM
#undef EDOM
#endif
#define	EDOM		33	/* Numerical argument out of domain */

#ifdef ERANGE
#undef ERANGE
#endif
#define	ERANGE		34	/* Result too large */

#ifdef EAGAIN
#undef EAGAIN
#endif
#define	EAGAIN		35	/* Resource temporarily unavailable */

// in winsock.h

#ifdef EBADRPC
#undef EBADRPC
#endif
#define	EBADRPC		72	/* RPC struct is bad */

#ifdef ERPCMISMATCH
#undef ERPCMISMATCH
#endif
#define	ERPCMISMATCH	73	/* RPC version wrong */

#ifdef EPROGUNAVAIL
#undef EPROGUNAVAIL
#endif
#define	EPROGUNAVAIL	74	/* RPC prog. not avail */

#ifdef EPROGMISMATCH
#undef EPROGMISMATCH
#endif
#define	EPROGMISMATCH	75	/* Program version wrong */

#ifdef EPROCUNAVAIL
#undef EPROCUNAVAIL
#endif
#define	EPROCUNAVAIL	76	/* Bad procedure for program */

#ifdef ENOLCK
#undef ENOLCK
#endif
#define	ENOLCK		77	/* No locks available */

#ifdef ENOSYS
#undef ENOSYS
#endif
#define	ENOSYS		78	/* Function not implemented */

#ifdef EOVERFLOW
#undef EOVERFLOW
#endif
#define	EOVERFLOW	79	/* Value too large to be stored in data type */


// From signal.h
#define SIGABRT         0
#define SIGSEGV         1
#define _SIGCOUNT       2 /* LAST ONE, SIZES BUFFER */

// From stdio.h

#define BUFSIZ 512
#define O_RDONLY       0x0000  // open for reading only
#define O_WRONLY       0x0001  // open for writing only
#define O_RDWR         0x0002  // open for reading and writing
#define O_APPEND       0x0008  // writes done at eof

#define O_TEXT         0x4000  // file mode is text (translated)
#define O_BINARY       0x8000  // file mode is binary (untranslated)

#define O_CREAT        0x0100  // create and open file
#define O_TRUNC        0x0200  // open and truncate
#define O_EXCL         0x0400  // open only if file doesn't already exist

#define _O_TEMPORARY    0x0040  // temporary file bit

#define _O_CREAT        O_CREAT
#define _O_TRUNC        O_TRUNC
#define _O_WRONLY       O_WRONLY

// From stdlib.h
#define _MAX_PATH       MAX_PATH

// From sys/types.h
typedef int ptrdiff_t;
typedef long _off_t;
typedef long off_t;

// From sys/stat.h
#if !defined(_STAT_DEFINED)
#define _STAT_DEFINED
#define _S_IFDIR    0040000 /* stat, is a directory */
#define _S_IFREG    0100000 /* stat, is a normal file */
#define _S_IREAD    0000400 /* stat, can read */
#define _S_IWRITE   0000200 /* stat, can write */
#define	_S_IEXEC	0000100

struct mozce_stat
{
    unsigned short st_mode;
    _off_t st_size;
    time_t st_ctime;
    time_t st_atime;
    time_t st_mtime;
};

#define _stat mozce_stat
#define stat mozce_stat
#endif /* _STAT_DEFINED */


// From time.h

#define _TM_DEFINED
struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};


typedef struct mozce_GLYPHMETRICS 
{ 
  UINT  gmBlackBoxX; 
  UINT  gmBlackBoxY; 
  POINT gmptGlyphOrigin; 
  short gmCellIncX; 
  short gmCellIncY; 
} mozce_GLYPHMETRICS;

typedef struct mozce_PANOSE { 
  BYTE bFamilyType; 
  BYTE bSerifStyle; 
  BYTE bWeight; 
  BYTE bProportion; 
  BYTE bContrast; 
  BYTE bStrokeVariation; 
  BYTE bArmStyle; 
  BYTE bLetterform; 
  BYTE bMidline; 
  BYTE bXHeight; 
} mozce_PANOSE;

typedef struct mozce_OUTLINETEXTMETRIC 
{ 
  UINT   otmSize; 
  TEXTMETRIC otmTextMetrics; 
  BYTE   otmFiller; 
  mozce_PANOSE otmPanoseNumber; 
  UINT   otmfsSelection; 
  UINT   otmfsType; 
  int    otmsCharSlopeRise; 
  int    otmsCharSlopeRun; 
  int    otmItalicAngle; 
  UINT   otmEMSquare; 
  int    otmAscent; 
  int    otmDescent; 
  UINT   otmLineGap; 
  UINT   otmsCapEmHeight; 
  UINT   otmsXHeight; 
  RECT   otmrcFontBox; 
  int    otmMacAscent; 
  int    otmMacDescent; 
  UINT   otmMacLineGap; 
  UINT   otmusMinimumPPEM; 
  POINT  otmptSubscriptSize; 
  POINT  otmptSubscriptOffset; 
  POINT  otmptSuperscriptSize; 
  POINT  otmptSuperscriptOffset; 
  UINT   otmsStrikeoutSize; 
  int    otmsStrikeoutPosition; 
  int    otmsUnderscoreSize; 
  int    otmsUnderscorePosition; 
  PSTR   otmpFamilyName; 
  PSTR   otmpFaceName; 
  PSTR   otmpStyleName; 
  PSTR   otmpFullName; 
} mozce_OUTLINETEXTMETRIC;


#define SW_SHOWMINIMIZED 2 
#define GGO_METRICS 0
#define GGO_GLYPH_INDEX 1

/****************************************************************************
**  exdispid.h
**
**  ??? Wondering what these really should be.
****************************************************************************/
#define DISPID_QUIT                     (__LINE__ + 3000) /* 103 */
#define DISPID_PROGRESSCHANGE           (__LINE__ + 3000) /* 108 */
#define DISPID_WINDOWMOVE               (__LINE__ + 3000) /* 109 */
#define DISPID_WINDOWRESIZE             (__LINE__ + 3000) /* 110 */
#define DISPID_WINDOWACTIVATE           (__LINE__ + 3000) /* 111 */

#define CBM_INIT 4

#ifndef MM_TEXT
#define MM_TEXT             1
#endif


typedef struct _BLENDFUNCTION 
{
  BYTE     BlendOp;
  BYTE     BlendFlags;
  BYTE     SourceConstantAlpha;
  BYTE     AlphaFormat;
} BLENDFUNCTION, *PBLENDFUNCTION, *LPBLENDFUNCTION;

#define AC_SRC_OVER 0


#ifndef SM_CYVTHUMB
#define SM_CYVTHUMB 9
#endif

#ifndef SM_CXHTHUMB
#define SM_CXHTHUMB 10
#endif

#ifndef DFCS_SCROLLSIZEGRIP
#define DFCS_SCROLLSIZEGRIP     0x0008
#endif


#ifndef RDW_NOINTERNALPAINT
#define RDW_NOINTERNALPAINT 0
#endif

#ifndef LR_LOADFROMFILE
#define LR_LOADFROMFILE 0
#endif

#ifndef MA_NOACTIVATE
#define MA_NOACTIVATE 1
#endif

#ifndef MA_ACTIVATE
#define MA_ACTIVATE 1
#endif

#ifndef WM_MOUSEACTIVATE
#define WM_MOUSEACTIVATE WM_ACTIVATE
#endif

typedef struct WINDOWPLACEMENT
{
  UINT  length;
  UINT  flags;
  UINT  showCmd;
  POINT ptMinPosition;
  POINT ptMaxPosition;
  RECT  rcNormalPosition;
} WINDOWPLACEMENT, *PWINDOWPLACEMENT, *LPWINDOWPLACEMENT;

typedef void (*_sigsig)(int inSignal);


typedef struct mozce_FIXED { 
  WORD  fract; 
  short value; 
} mozce_FIXED; 

typedef struct mozce_MAT2 { 
  mozce_FIXED eM11; 
  mozce_FIXED eM12; 
  mozce_FIXED eM21; 
  mozce_FIXED eM22; 
} mozce_MAT2; 
  
#endif
