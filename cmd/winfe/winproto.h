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

#ifndef WINPROTO_H
#define WINPROTO_H
/*
 * Prototypes for functions local to the windows front end
 *
 * This file will be included by both C and C++ files so please
 *   be careful with how you do things
 */

/*  Bookmark drag and drop defines*/
#define NETSCAPE_BOOKMARK_FORMAT     "Netscape Bookmark"
#define NETSCAPE_BOOKMARK_FOLDER_FORMAT "Netscape Bookmark Folder"
#define NETSCAPE_SOURCETARGET_FORMAT "Netscape source-target"
#define NETSCAPE_BKMKLISTL_FORMAT    "Netscape Bookmark List (Long)"
#define NETSCAPE_HTNODE_FORMAT		 "Netscape RDF Item"

typedef struct {
    char szAnchor[1024];
    char szText[1024];
} BOOKMARKITEM;

typedef BOOKMARKITEM * LPBOOKMARKITEM;

/*
 * C Functions
 */

XP_BEGIN_PROTOS
extern char * wfe_LoadResourceString (LPCSTR lpszName);
XP_END_PROTOS

/*
 * C++ Functions
 */
extern char * FE_Windowsify(const char * In);
extern char* XP_NetToDosFileName(const char * NetName);
extern char * fe_MiddleCutString(char *string ,int iNum, BOOL bModifyString = FALSE);
void WFE_ConvertFile2Url(CString& csUrl, const char *LocalFileName);
CString WFE_EscapeURL(const char* name);
void WFE_CondenseURL(CString& csURL, UINT uLength, BOOL bParens = TRUE);	
BOOL WFE_CopyFile(const char *cpSrc, const char *cpTarg);
BOOL WFE_MoveFile(const char *cpSrc, const char *cpTarg);
#ifdef XP_WIN16
#define INVALID_HANDLE_VALUE NULL 
void * FindFirstFile(const char * a, void * b);
int FindNextFile(void * a, void * b);
void FindClose(void * a);
#endif


//	Draw helpers.

void WFE_DrawWindowsButtonRect(HDC hDC, LPRECT lpRect, BOOL bPushed = FALSE);
void WFE_DrawRaisedRect( HDC hDC, LPRECT lpRect );
void WFE_DrawLoweredRect( HDC hDC, LPRECT lpRect );
void WFE_Draw3DButtonRect( HDC hDC, LPRECT lpRect, BOOL bPushed );
void WFE_FillSolidRect(CDC *pDC, CRect crRect, COLORREF rgbFill);
void WFE_DrawHighlight( HDC hDC, LPRECT lpRect, COLORREF clrTopLeft, COLORREF clrBottomRight );

// Bitmap helpers

#define RGB_TRANSPARENT RGB(255,0,255)

void WFE_MakeTransparentBitmap( HDC hDC, HBITMAP hBitmap );
HBITMAP WFE_LoadSysColorBitmap( HINSTANCE hInst, LPCSTR lpszResource );
// Forces bitmap to be compatible with our palette
HBITMAP WFE_LoadBitmap( HINSTANCE hInst, HDC hDC, LPCSTR lpszResource );
HBITMAP wfe_LoadBitmap(HINSTANCE hInst, HDC hDC, LPCSTR pszBmName );
HBITMAP WFE_LoadTransparentBitmap(HINSTANCE hInstance, HDC hDC, COLORREF clrBackground, COLORREF clrTransparent,
								  HPALETTE hPalette, UINT nResID);
HBITMAP WFE_LoadBitmapFromFile (const char* szFileName);
void WFE_InitializeUIPalette(HDC hDC);
HPALETTE WFE_GetUIPalette(CWnd * parent);
BOOL WFE_IsGlobalPalette(HPALETTE hPal);
void WFE_DestroyUIPalette(void);
HBITMAP WFE_LookupLoadAndEnterBitmap(HDC hDC, UINT nID, BOOL bTransparent, HPALETTE hPalette, 
									 COLORREF clrBackground, COLORREF clrTransparent);
void WFE_RemoveBitmapFromMap(UINT nID);
void WFE_DestroyBitmapMap(void);
HFONT WFE_GetUIFont(HDC hDC);
void WFE_DestroyUIFont(void);
CFrameWnd *WFE_GetOwnerFrame(CWnd *pWnd);
int WFE_FindMenu(CMenu *pMenu, CString& menuItemName);
int WFE_FindMenuItem(CMenu *pMenu, UINT nCommand);

void fe_UpdateMControlMimeTypes(void); //CRN_MIME

//
// Prompt user for a file name
//
#define HTM       1
#define TXT       2
#define EXE       3
#define ALL	      4
// Editor should open ONLY html files!
#define HTM_ONLY -1
#define P12       -2		

// If pPageTitle is not null, then edit box is added to allow user to enter document title for edited documents
// Caller must free *ppPageTitle
extern char * wfe_GetSaveFileName(HWND m_hWnd, char * prompt, char * initial, int * type, char **ppPageTitle = NULL);

// If pOpenIntoEditor is not null, then radio buttons are added to select window type
//    set initial choice and read user's choice from this
extern char * wfe_GetExistingFileName(HWND m_hWnd, char * prompt, int type, XP_Bool bMustExist,
                                      BOOL * pOpenIntoEditor = NULL);
// CLM: Similar above, but used to select Image Files: *.jpg, *.gif
extern char * wfe_GetExistingImageFileName(HWND m_hWnd, char * prompt, XP_Bool bMustExist);

// Copy file routine for Editor - ask user permission
//  to overwrite existing file
// Params may be local file format or "file://" URL format,
//   and assumes full pathnames
BOOL FE_CopyFile(const char *cpSrc, const char *cpTarg);

// try to convert a URL to 8+3
char * fe_URLtoLocalName(const char * url, const char *pMimeType);

// pop up a little box and ask the user for a filename
char * fe_GetSaveFileName(HWND m_hWnd, char * initial);

// Module level prototypes
extern void FE_DeleteFileOnExit(const char *filename, const char *pURL);
extern void FE_DeleteTempFilesNow();
extern void FE_FlushCache();    
//Added pSwitcher to allow us to dictate which HINSTANCE to load the string from.  if NULL, we use AfxGetResourceHandle
class ResourceSwitcher;
extern char * szLoadString (UINT iID, ResourceSwitcher *pSwticher = NULL);

// Helper application handling functions
//
extern void fe_InitFileFormatTypes(void);
extern CHelperApp * fe_AddNewFileFormatType(const char *mime_type,const char *subtype); 
extern void fe_CleanupFileFormatTypes(void);
extern HotlistStruct * fe_find_hotlist_root (HotlistStruct * pListTop, BOOL add_p, MWContext * context);

// Get a file name given a type prefix and extension 
// Name is returned in static space --- user must not free
// WARNING: Unlike the older version of this routine (XP_FileName) this one
// returns a malloc'd string.
extern char * WH_TempFileName(int type, const char * prefix, const char * extension);

// Read in the user's sig
extern char * wfe_ReadUserSig(const char * msg);

// Set the display property for type to the given color
extern void wfe_SetLayoutColor(int type, COLORREF color);

class CDNSObj : public CObject 
{
public:
    CDNSObj();
    ~CDNSObj();
	int				  i_finished;
	XP_List			* m_sock_list;
	int 			  m_iError;
	char		    * m_host;
	struct hostent  * m_hostent;
	HANDLE			  m_handle;
	// Used by FE_DeleteDNSList in fenet.cpp
	MWContext		* context;
};

//	Form stuff.
#define BUTTON_FUDGE 6 // extra spacing around buttons and checkboxes

#define EDIT_SPACE   2 // vertical spacing above edit and combo box fields

#define BOX(pStr)  ::MessageBox(NULL, pStr, "Fancy", MB_ICONEXCLAMATION)

// CLM: Some simple functions to aid in COLORREF to LO_Color conversions

// Fill a LoColor structure
void WFE_SetLO_Color( COLORREF crColor, LO_Color *pLoColor );

// This allocates a new LoColor structure if it
//   does not exist
void WFE_SetLO_ColorPtr( COLORREF crColor, LO_Color **ppLoColor );

// Convert a LO_Color structure or get the default
//  default document colors
COLORREF WFE_LO2COLORREF( LO_Color * pLoColor, int iColorIndex );

// Parse a "#FFEEAA" style color string into colors make a COLORREF
BOOL WFE_ParseColor(char *pRGB, COLORREF * pCRef );

// Helper to clear data
void WFE_FreeLO_Color(LO_Color_struct **ppLo);

// Only drop target allowed is Composer
#ifdef EDITOR
// Drag drop images...
void WFE_DragDropImage(HGLOBAL h, MWContext *pMWContext);

// Fill struct with data about an image
//   for drag n drop and copying to clipboard
HGLOBAL WFE_CreateCopyImageData(MWContext*pMWContext, LO_ImageStruct *pImage);
#endif /* EDITOR */

// This will hide/show caret only if enabled
//   or hide/show DragDrop caret if currently dragging over the view
void WFE_HideEditCaret(MWContext * pMWContext);
void WFE_ShowEditCaret(MWContext * pMWContext);

// Return value for WFE_GetCurrentFontColor when no color is set
// Use as param with CColorComboBox::SetColor to set to default color (index 0)
#define DEFAULT_COLORREF 0xFFFFFFFF
// Return this when we have a mixed state - we don't have a single color
#define MIXED_COLORREF 0xFFFFFFF0

// Return value when Canceling out of CColorPicker
#define CANCEL_COLORREF 0xFFFFFF00

COLORREF WFE_GetCurrentFontColor(MWContext * pMWContext);

// Get a resource string and extract the tooltip portion
//   if a '\n' is found in the string
//   USE RESULT QUICKLY! 
//   (String is in our temp buffer and should not be FREEd)
char * FEU_GetToolTipText( UINT nID );
char *FE_FindFileExt(char * path);

extern int PR_CALLBACK WinFEPrefChangedFunc(const char *pref, void *data);
#ifdef MOZ_LDAP
extern int PR_CALLBACK DirServerListChanged(const char *pref, void *data);
#endif /* MOZ_LDAP */
extern void fe_InitNSPR(void *stackBase);
extern void fe_InitJava();
extern void OpenDraftExit (URL_Struct *url_struct, int status, MWContext *pContext);
extern BOOL fe_ShutdownJava();
extern BOOL fe_RegisterOLEIcon(LPSTR szCLSIDObject, LPSTR szObjectName, LPSTR szIconPath);
extern void WFE_DrawDragRect(CDC* pdc, LPCRECT lpRect, SIZE size,
	LPCRECT lpRectLast, SIZE sizeLast, CBrush* pBrush, CBrush* pBrushLast);

#endif // WINPROTO_H
