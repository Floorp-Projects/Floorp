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

#ifndef MOZCE_DEFS
#define MOZCE_DEFS

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

#ifdef MOZCE_SHUNT_EXPORTS
#define MOZCE_SHUNT_API __declspec(dllexport)
#else
#define MOZCE_SHUNT_API __declspec(dllimport)
#endif


//////////////////////////////////////////////////////////
// Various Definations
//////////////////////////////////////////////////////////

// for errors.h
#define EINVAL              __LINE__
#define EAGAIN              __LINE__
#define EINTR               __LINE__
#define ENOMEM              __LINE__
#define EBADF               __LINE__
#define EFAULT              __LINE__
#define EACCES              __LINE__
#define EIO                 __LINE__
#define ENOTDIR             __LINE__
#define EDEADLOCK           __LINE__
#define EFBIG               __LINE__
#define ENOSPC              __LINE__
#define EPIPE               __LINE__
#define ESPIPE              __LINE__
#define EISDIR              __LINE__
#define ENOENT              __LINE__
#define EROFS               __LINE__
#define EBUSY               __LINE__
#define EXDEV               __LINE__
#define EEXIST              __LINE__
#define ENFILE              __LINE__
#define EDEADLK             __LINE__
#define ERANGE              __LINE__
#define EPERM               __LINE__
#define ENOSYS              __LINE__

// From signal.h
#define SIGABRT         0
#define SIGSEGV         1
#define _SIGCOUNT       2 /* LAST ONE, SIZES BUFFER */

typedef void (*_sigsig)(int inSignal);

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
#define _MAX_DRIVE      MAX_PATH
#define _MAX_DIR        MAX_PATH
#define _MAX_EXT        MAX_PATH
#define _MAX_FNAME      MAX_PATH
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

#ifdef MOZCE_SHUNT_EXPORTS
struct tm {
#else
struct mozce_tm {
#endif
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


struct mozce_lconv {
  const char* thousandsSeparator;
  const char* decimalSeparator;
  const char* numGrouping;
};


typedef struct mozce_GLYPHMETRICS 
{ 
  UINT  gmBlackBoxX; 
  UINT  gmBlackBoxY; 
  POINT gmptGlyphOrigin; 
  short gmCellIncX; 
  short gmCellIncY; 
} mozce_GLYPHMETRICS; 


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

#endif
