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

#include <bldver.h> // for build version macros

#ifndef MOZCE_STATIC_BUILD
#ifdef MOZCE_SHUNT_EXPORTS
#define MOZCE_SHUNT_API __declspec(dllexport)
#else
#define MOZCE_SHUNT_API __declspec(dllimport)
#endif
#else
#define MOZCE_SHUNT_API
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

#ifdef HINSTANCE_ERROR
#undef HINSTANCE_ERROR
#endif
#define HINSTANCE_ERROR -1

#ifdef IDI_APPLICATION
#undef IDI_APPLICATION
#endif
#ifdef RC_INVOKED
#define IDI_APPLICATION 32512
#else
#define IDI_APPLICATION MAKEINTRESOURCE(32512)
#endif

//////////////////////////////////////////////////////////
// Various Definations
//////////////////////////////////////////////////////////

// From errno.h
#define EPERM           1
#define ENOENT          2
#define ESRCH           3
#define EINTR           4
#define EIO             5
#define ENXIO           6
#define E2BIG           7
#define ENOEXEC         8
#define EBADF           9
#define ECHILD          10
#define EAGAIN          11
#define ENOMEM          12
#define EACCES          13
#define EFAULT          14
#define EBUSY           16
#define EEXIST          17
#define EXDEV           18
#define ENODEV          19
#define ENOTDIR         20
#define EISDIR          21
#define ENFILE          23
#define EMFILE          24
#define ENOTTY          25
#define EFBIG           27
#define ENOSPC          28
#define ESPIPE          29
#define EROFS           30
#define EMLINK          31
#define EPIPE           32
#define EDOM            33
#define EDEADLK         36
#ifndef ENAMETOOLONG
#define ENAMETOOLONG    38
#endif
#define ENOLCK          39
#define ENOSYS          40
#ifndef ENOTEMPTY
#define ENOTEMPTY       41
#endif
// From cderr.h
#ifdef FNERR_INVALIDFILENAME
#undef FNERR_INVALIDFILENAME
#endif
#define FNERR_INVALIDFILENAME  0x3002

// From signal.h
#define SIGABRT         0
#define SIGSEGV         1
#define _SIGCOUNT       2 /* LAST ONE, SIZES BUFFER */

// From stdio.h

#define _MAX_FNAME     256

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

#define _IONBF          0x0004

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

struct stat
{
    unsigned short st_mode;
    _off_t st_size;
    time_t st_ctime;
    time_t st_atime;
    time_t st_mtime;
};

#define _stat stat
#endif /* _STAT_DEFINED */

#ifdef HANDLE_FLAG_INHERIT
#undef HANDLE_FLAG_INHERIT
#endif
#define HANDLE_FLAG_INHERIT 0x00000001


typedef struct GLYPHMETRICS 
{ 
  UINT  gmBlackBoxX; 
  UINT  gmBlackBoxY; 
  POINT gmptGlyphOrigin; 
  short gmCellIncX; 
  short gmCellIncY; 
} GLYPHMETRICS;


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


typedef struct FIXED { 
  WORD  fract; 
  short value; 
} FIXED; 

typedef struct MAT2 { 
  FIXED eM11; 
  FIXED eM12; 
  FIXED eM21; 
  FIXED eM22; 
} MAT2; 

#define AC_SRC_OVER                 0x00

//
// alpha format flags
//

#define AC_SRC_ALPHA                0x01

  
struct color{
	unsigned char Red;
	unsigned char Green;
	unsigned char Blue;
	double Alpha;
};

#ifndef SEE_MASK_FLAG_DDEWAIT
#define SEE_MASK_FLAG_DDEWAIT	0
#endif

#ifndef SEE_MASK_INVOKEIDLIST
#define SEE_MASK_INVOKEIDLIST	0
#endif



// if using WINCE 5.0 SDK, you need these:
#define LPRASPBDLG void*
#define LPRASDIALDLG void*

/* Graphics Modes */
#define GM_COMPATIBLE       1
#define GM_ADVANCED         2

//
// usp10.h: unicode
//
typedef void *SCRIPT_CACHE; 

typedef struct tag_SCRIPT_STATE { 
  WORD uBidiLevel :5; 
  WORD fOverrideDirection :1; 
  WORD fInhibitSymSwap :1; 
  WORD fCharShape :1; 
  WORD fDigitSubstitute :1; 
  WORD fInhibitLigate :1; 
  WORD fDisplayZWG :1; 
  WORD fArabicNumContext :1; 
  WORD fGcpClusters :1; 
  WORD fReserved :1; 
  WORD fEngineReserved :2; 
} SCRIPT_STATE;

typedef struct tag_SCRIPT_ANALYSIS {
  WORD eScript      :10; 
  WORD fRTL          :1; 
  WORD fLayoutRTL    :1; 
  WORD fLinkBefore   :1; 
  WORD fLinkAfter    :1; 
  WORD fLogicalOrder :1; 
  WORD fNoGlyphIndex :1; 
  SCRIPT_STATE s ; 
} SCRIPT_ANALYSIS;

typedef struct tag_SCRIPT_ITEM { 
  int iCharPos; 
  SCRIPT_ANALYSIS a; 
} SCRIPT_ITEM;

typedef struct {
  int   cBytes; 
  WORD  wgBlank; 
  WORD  wgDefault; 
  WORD  wgInvalid; 
  WORD  wgKashida; 
  int   iKashidaWidth; 
} SCRIPT_FONTPROPERTIES;

typedef struct {
  DWORD   langid              :16;  
  DWORD   fNumeric            :1;
  DWORD   fComplex            :1;
  DWORD   fNeedsWordBreaking  :1;   
  DWORD   fNeedsCaretInfo     :1;
  DWORD   bCharSet            :8;   
  DWORD   fControl            :1;   
  DWORD   fPrivateUseArea     :1;   
  DWORD   fNeedsCharacterJustify :1;
  DWORD   fInvalidGlyph       :1;
  DWORD   fInvalidLogAttr     :1;
  DWORD   fCDM                :1;
  DWORD   fAmbiguousCharSet   :1;
  DWORD   fClusterSizeVaries  :1;
  DWORD   fRejectInvalid      :1;
} SCRIPT_PROPERTIES;

typedef struct tag_SCRIPT_VISATTR { 
  WORD uJustification :4; 
  WORD fClusterStart :1; 
  WORD fDiacritic :1; 
  WORD fZeroWidth :1; 
  WORD fReserved :1; 
  WORD fShapeReserved :8; 
} SCRIPT_VISATTR;

#ifndef LSDEFS_DEFINED
typedef struct tagGOFFSET {
    LONG  du;
    LONG  dv;
} GOFFSET;
#endif

#define SCRIPT_UNDEFINED  0


typedef struct tag_SCRIPT_LOGATTR { 
  BYTE fSoftBreak :1; 
  BYTE fWhiteSpace :1; 
  BYTE fCharStop :1; 
  BYTE fWordStop :1; 
  BYTE fInvalid :1; 
  BYTE fReserved :3; 
} SCRIPT_LOGATTR;

typedef struct tag_SCRIPT_CONTROL { 
  DWORD uDefaultLanguage :16; 
  DWORD fContextDigits :1; 
  DWORD fInvertPreBoundDir :1; 
  DWORD fInvertPostBoundDir :1; 
  DWORD fLinkStringBefore :1; 
  DWORD fLinkStringAfter :1; 
  DWORD fNeutralOverride :1; 
  DWORD fNumericOverride :1; 
  DWORD fLegacyBidiClass :1; 
  DWORD fReserved :8; 
} SCRIPT_CONTROL;


typedef struct tagGCP_RESULTSA
    {
    DWORD   lStructSize;
    LPSTR     lpOutString;
    UINT FAR *lpOrder;
    int FAR  *lpDx;
    int FAR  *lpCaretPos;
    LPSTR   lpClass;
    LPWSTR  lpGlyphs;
    UINT    nGlyphs;
    int     nMaxFit;
    } GCP_RESULTSA, FAR* LPGCP_RESULTSA;
typedef struct tagGCP_RESULTSW
    {
    DWORD   lStructSize;
    LPWSTR    lpOutString;
    UINT FAR *lpOrder;
    int FAR  *lpDx;
    int FAR  *lpCaretPos;
    LPSTR   lpClass;
    LPWSTR  lpGlyphs;
    UINT    nGlyphs;
    int     nMaxFit;
    } GCP_RESULTSW, FAR* LPGCP_RESULTSW;
#ifdef UNICODE
typedef GCP_RESULTSW GCP_RESULTS;
typedef LPGCP_RESULTSW LPGCP_RESULTS;
#else
typedef GCP_RESULTSA GCP_RESULTS;
typedef LPGCP_RESULTSA LPGCP_RESULTS;
#endif // UNICODE


#define GCP_DBCS           0x0001
#define GCP_REORDER        0x0002
#define GCP_USEKERNING     0x0008
#define GCP_GLYPHSHAPE     0x0010
#define GCP_LIGATE         0x0020
////#define GCP_GLYPHINDEXING  0x0080
#define GCP_DIACRITIC      0x0100
#define GCP_KASHIDA        0x0400
#define GCP_ERROR          0x8000
#define FLI_MASK           0x103B

#define GCP_JUSTIFY        0x00010000L
////#define GCP_NODIACRITICS   0x00020000L
#define FLI_GLYPHS         0x00040000L
#define GCP_CLASSIN        0x00080000L
#define GCP_MAXEXTENT      0x00100000L
#define GCP_JUSTIFYIN      0x00200000L
#define GCP_DISPLAYZWG      0x00400000L
#define GCP_SYMSWAPOFF      0x00800000L
#define GCP_NUMERICOVERRIDE 0x01000000L
#define GCP_NEUTRALOVERRIDE 0x02000000L
#define GCP_NUMERICSLATIN   0x04000000L
#define GCP_NUMERICSLOCAL   0x08000000L

#define ETO_GLYPH_INDEX              0x0010
#define GGO_NATIVE         2
#define TT_POLYGON_TYPE   24

#define TT_PRIM_LINE       1
#define TT_PRIM_QSPLINE    2
#define TT_PRIM_CSPLINE    3

typedef struct tagPOINTFX
{
    FIXED x;
    FIXED y;
} POINTFX, FAR* LPPOINTFX;

typedef struct tagTTPOLYCURVE
{
    WORD    wType;
    WORD    cpfx;
    POINTFX apfx[1];
} TTPOLYCURVE, FAR* LPTTPOLYCURVE;

typedef struct tagTTPOLYGONHEADER
{
    DWORD   cb;
    DWORD   dwType;
    POINTFX pfxStart;
} TTPOLYGONHEADER, FAR* LPTTPOLYGONHEADER;

#define HALFTONE                     4
#define ETO_PDY                      0x2000
#define ALTERNATE                    1
#define WINDING                      2

#define PS_USERSTYLE        7

#define PS_ENDCAP_ROUND     0x00000000
#define PS_ENDCAP_SQUARE    0x00000100
#define PS_ENDCAP_FLAT      0x00000200
#define PS_ENDCAP_MASK      0x00000F00

#define PS_JOIN_ROUND       0x00000000
#define PS_JOIN_BEVEL       0x00001000
#define PS_JOIN_MITER       0x00002000
#define PS_JOIN_MASK        0x0000F000

#define PS_COSMETIC         0x00000000
#define PS_GEOMETRIC        0x00010000
#define PS_TYPE_MASK        0x000F0000

typedef struct tag_SCRIPT_DIGITSUBSTITUTE {
    DWORD  NationalDigitLanguage    :16;   // Language for native substitution
    DWORD  TraditionalDigitLanguage :16;   // Language for traditional substitution
    DWORD  DigitSubstitute          :8;    // Substitution type
    DWORD  dwReserved;                     // Reserved
} SCRIPT_DIGITSUBSTITUTE;

// Defines missing from widget/src/build
// WinNT.h
typedef struct _OSVERSIONINFOEXA {
    DWORD dwOSVersionInfoSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformId;
    CHAR   szCSDVersion[ 128 ];     // Maintenance string for PSS usage
    WORD   wServicePackMajor;
    WORD   wServicePackMinor;
    WORD   wSuiteMask;
    BYTE  wProductType;
    BYTE  wReserved;
} OSVERSIONINFOEXA, *POSVERSIONINFOEXA, *LPOSVERSIONINFOEXA;
typedef struct _OSVERSIONINFOEXW {
    DWORD dwOSVersionInfoSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformId;
    WCHAR  szCSDVersion[ 128 ];     // Maintenance string for PSS usage
    WORD   wServicePackMajor;
    WORD   wServicePackMinor;
    WORD   wSuiteMask;
    BYTE  wProductType;
    BYTE  wReserved;
} OSVERSIONINFOEXW, *POSVERSIONINFOEXW, *LPOSVERSIONINFOEXW, RTL_OSVERSIONINFOEXW, *PRTL_OSVERSIONINFOEXW;
#ifdef UNICODE
typedef OSVERSIONINFOEXW OSVERSIONINFOEX;
typedef POSVERSIONINFOEXW POSVERSIONINFOEX;
typedef LPOSVERSIONINFOEXW LPOSVERSIONINFOEX;
#else
typedef OSVERSIONINFOEXA OSVERSIONINFOEX;
typedef POSVERSIONINFOEXA POSVERSIONINFOEX;
typedef LPOSVERSIONINFOEXA LPOSVERSIONINFOEX;
#endif // UNICODE

// WinUser.h
#define WM_ENDSESSION                   0x0016

// defines required by msaa.h
typedef struct tagWINDOWINFO
{
    DWORD cbSize;
    RECT rcWindow;
    RECT rcClient;
    DWORD dwStyle;
    DWORD dwExStyle;
    DWORD dwWindowStatus;
    UINT cxWindowBorders;
    UINT cyWindowBorders;
    ATOM atomWindowType;
    WORD wCreatorVersion;
} WINDOWINFO, *PWINDOWINFO, *LPWINDOWINFO;

typedef struct tagCURSORINFO
{
    DWORD   cbSize;
    DWORD   flags;
    HCURSOR hCursor;
    POINT   ptScreenPos;
} CURSORINFO, *PCURSORINFO, *LPCURSORINFO;

#define CCHILDREN_TITLEBAR              5
#define CCHILDREN_SCROLLBAR             5

typedef struct tagTITLEBARINFO
{
    DWORD cbSize;
    RECT rcTitleBar;
    DWORD rgstate[CCHILDREN_TITLEBAR + 1];
} TITLEBARINFO, *PTITLEBARINFO, *LPTITLEBARINFO;

typedef struct tagSCROLLBARINFO
{
    DWORD cbSize;
    RECT rcScrollBar;
    int dxyLineButton;
    int xyThumbTop;
    int xyThumbBottom;
    int reserved;
    DWORD rgstate[CCHILDREN_SCROLLBAR + 1];
} SCROLLBARINFO, *PSCROLLBARINFO, *LPSCROLLBARINFO;

typedef struct tagMENUBARINFO
{
    DWORD cbSize;
    RECT rcBar;          // rect of bar, popup, item
    HMENU hMenu;         // real menu handle of bar, popup
    HWND hwndMenu;       // hwnd of item submenu if one
    BOOL fBarFocused:1;  // bar, popup has the focus
    BOOL fFocused:1;     // item has the focus
} MENUBARINFO, *PMENUBARINFO, *LPMENUBARINFO;


typedef struct tagGUITHREADINFO
{
    DWORD   cbSize;
    DWORD   flags;
    HWND    hwndActive;
    HWND    hwndFocus;
    HWND    hwndCapture;
    HWND    hwndMenuOwner;
    HWND    hwndMoveSize;
    HWND    hwndCaret;
    RECT    rcCaret;
} GUITHREADINFO, *PGUITHREADINFO, FAR * LPGUITHREADINFO;

typedef struct tagALTTABINFO
{
    DWORD cbSize;
    int cItems;
    int cColumns;
    int cRows;
    int iColFocus;
    int iRowFocus;
    int cxItem;
    int cyItem;
    POINT ptStart;
} ALTTABINFO, *PALTTABINFO, *LPALTTABINFO;

#define     GA_ROOT         2

#define NTM_TYPE1           0x00100000
#define SPI_GETDRAGFULLWINDOWS      0x0026

typedef ULONG SFGAOF;


typedef unsigned int   uintptr_t;
// From winuser.h
#define GR_GDIOBJECTS     0       /* Count of GDI objects */
#define GR_USEROBJECTS    1       /* Count of USER objects */
// wingdi.h
#define MWT_IDENTITY        1

typedef VOID CALLBACK LINEDDAPROC(
  int X,          // x-coordinate of point
  int Y,          // y-coordinate of point
  LPARAM lpData   // application-defined data
);

#define MOVEFILE_REPLACE_EXISTING       0x00000001
#define MOVEFILE_COPY_ALLOWED           0x00000002
#define MOVEFILE_DELAY_UNTIL_REBOOT     0x00000004
#define MOVEFILE_WRITE_THROUGH          0x00000008

#define MB_TASKMODAL MB_APPLMODAL

#define     GA_PARENT       1
#define     GA_ROOT         2
#define     GA_ROOTOWNER    3

//
// FILEDESCRIPTOR.dwFlags field indicate which fields are to be used
//

typedef struct _FILEDESCRIPTORA { // fod
    DWORD dwFlags;

    CLSID clsid;
    SIZEL sizel;
    POINTL pointl;

    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    CHAR   cFileName[ MAX_PATH ];
} FILEDESCRIPTORA, *LPFILEDESCRIPTORA;

typedef struct _FILEDESCRIPTORW { // fod
    DWORD dwFlags;

    CLSID clsid;
    SIZEL sizel;
    POINTL pointl;

    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    WCHAR  cFileName[ MAX_PATH ];
} FILEDESCRIPTORW, *LPFILEDESCRIPTORW;

#ifdef UNICODE
#define FILEDESCRIPTOR      FILEDESCRIPTORW
#define LPFILEDESCRIPTOR    LPFILEDESCRIPTORW
#else
#define FILEDESCRIPTOR      FILEDESCRIPTORA
#define LPFILEDESCRIPTOR    LPFILEDESCRIPTORA
#endif

//
// format of CF_FILEGROUPDESCRIPTOR
//
typedef struct _FILEGROUPDESCRIPTORA { // fgd
     UINT cItems;
     FILEDESCRIPTORA fgd[1];
} FILEGROUPDESCRIPTORA, * LPFILEGROUPDESCRIPTORA;

typedef struct _FILEGROUPDESCRIPTORW { // fgd
     UINT cItems;
     FILEDESCRIPTORW fgd[1];
} FILEGROUPDESCRIPTORW, * LPFILEGROUPDESCRIPTORW;

#ifdef UNICODE
#define FILEGROUPDESCRIPTOR     FILEGROUPDESCRIPTORW
#define LPFILEGROUPDESCRIPTOR   LPFILEGROUPDESCRIPTORW
#else
#define FILEGROUPDESCRIPTOR     FILEGROUPDESCRIPTORA
#define LPFILEGROUPDESCRIPTOR   LPFILEGROUPDESCRIPTORA
#endif

typedef struct
{
    SIZE        sizeDragImage;      // OUT - The length and Width of the
                                    //        rendered image
    POINT       ptOffset;           // OUT - The Offset from the mouse cursor to
                                    //        the upper left corner of the image
    HBITMAP     hbmpDragImage;      // OUT - The Bitmap containing the rendered
                                    //        drag images
    COLORREF    crColorKey;         // OUT - The COLORREF that has been blitted
                                    //        to the background of the images
} SHDRAGIMAGE, *LPSHDRAGIMAGE;

#endif // _MOZCE_DEFS
