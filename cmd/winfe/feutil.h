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

#ifndef FE_UTILITIES__H
#define FE_UTILITIES__H

#ifndef	__AFXWIN_H__
	#error include 'stdafx.h' before including this	file for PCH
#endif

//	File used for general purpose functions for the
//		windows front end that have no other
//		specific home.
//	All functions should start with FEU to denote
//		their origin.

//  Wrap some SDK calls here
BOOL FEU_GetSaveFileName(OPENFILENAME *pOFN);
BOOL FEU_GetOpenFileName(OPENFILENAME *pOFN);
UINT FEU_GetCurrentDriveType(void);

//  Port some SDK calls here
#ifdef XP_WIN16
DWORD GetCurrentDirectory(DWORD nBufferLength, LPSTR lpBuffer);
BOOL SetCurrentDirectory(LPCSTR lpPathName);
#endif

#define FEU_FINDBROWSERONLY 0
#define FEU_FINDBROWSERANDEDITOR 1
#define FEU_FINDEDITORONLY 2

//	Way to get a frame window pointer out of a context ID.
CFrameWnd *FEU_FindFrameByID(DWORD dwID, MWContextType cxType = MWContextAny);

//	Way to get a frame window that was last active browser window.
CFrameWnd *FEU_GetLastActiveFrame(MWContextType cxType = MWContextAny, int bFindEditor = FEU_FINDBROWSERONLY);


// Way to get the main context from the frameGlue.
CAbstractCX *FEU_GetLastActiveFrameContext(MWContextType cxType = MWContextAny, int nFindEditor = FEU_FINDBROWSERONLY);

//  Way to get the last active frame window based on its customizable toolbar
//  bUseSaveInfo is true if custToolbar's save info field should be used to disregard frames if save info is FALSE
CFrameWnd *FEU_GetLastActiveFrameByCustToolbarType(CString custToolbar, CFrameWnd *pCurrentFrame, BOOL bUseSaveInfo);

// Way to get the bottommost frame of type cxType 
CFrameWnd *FEU_GetBottomFrame(MWContextType cxType = MWContextAny, int nFindEditor = FEU_FINDBROWSERONLY);

// Way to get the number of active frames that are of type cxType
int FEU_GetNumActiveFrames(MWContextType cxType = MWContextAny, int nFindEditor = FEU_FINDBROWSERONLY);


//	Way to get the context ID out of the frame which was last active.
DWORD FEU_GetLastActiveFrameID(MWContextType cxType = MWContextAny);

//	Way to block returning until a context is no longer in existance.
void FEU_BlockUntilDestroyed(DWORD dwContextID);

//	Ensure a disk has space to write on, or ask the user about it.
BOOL FEU_ConfirmFreeDiskSpace(MWContext *pContext, const char *pFileName, int32 lContentLength);

#if defined(XP_WIN16)
//	Debuggin resource stuff on 16 bits.
void FEU_FreeResources(const char *pString, int iDigit, BOOL bBox);

//	16 bits needs a GetDiskFreeSpace call.
BOOL GetDiskFreeSpace(LPCTSTR lpRootPathName, LPDWORD lpSectorsPerCluster,
	LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters);
#endif

//#ifndef NO_TAB_NAVIGATION 
void FEU_MakeRectVisible(MWContext *pContext, const UINT left, const UINT top, const UINT right, const UINT bottom);
void FEU_MakeElementVisible(MWContext *pContext, LO_Any  *pElement);
//#else /* NO_TAB_NAVIGATION  */

// scroll the given form into the visible part of the window                    
// void FEU_MakeFormVisible(MWContext *pContext, LO_FormElementStruct *pForm);
//#endif /* NO_TAB_NAVIGATION  */

//	Strip off all trailing backslashes
char *FEU_NoTrailingBackslash(char *pBackslash);

//  Replace all occurrences of original in pStr with replace
void FEU_ReplaceChar(char *pStr, char original, char replace);

// Replaces all occurrences of & with &&
CString FEU_EscapeAmpersand(CString str);

//  Write string values to the registry in the said location.
BOOL FEU_RegistryWizard(HKEY hRoot, const char *pKey, const char *pValue);

//  As the fucntion indicates.
// char *FEU_ExtractCommaDilimetedString(const char *pArgList, int iArgToExtract);
#define MAXFONTFACENAME     (LF_FACESIZE+1)
int FEU_ExtractCommaDilimetedFontName(const char *pArgList, int offSetByte, char *argItem);

//  As the fucntion indicates.
char *FEU_ExtractCommaDilimetedQuotedString(const char *pArgList, int iArgToExtract);

// Performs a transparent BitBlt - see the implementation for more info.
BOOL FEU_TransBlt(CDC * pSrcDC, CDC * pDstDC, const CPoint & ptSrc,
	const CPoint & ptDst, int nWidth, int nHeight, HPALETTE hPalette,
	const COLORREF rgbTrans = RGB(255, 0, 255));

// Performs a transparent BitBlt, same as BitBlt except rgbTrans is the
// color to be drawn transparently (not drawn)
BOOL FEU_TransBlt(HDC hDstDC, int nXDest, int nYDest, int nWidth, int nHeight, 
				  HDC hSrcDC, int nXSrc, int nYSrc, HPALETTE hPalette,
				  COLORREF rgbTrans = RGB(0xff, 0x00, 0xff));
	
//  Correctly initialize a structure that no one seems to get right.
void FEU_InitWINDOWPLACEMENT(HWND hWindow, WINDOWPLACEMENT *pWindowPlacement);

//  Mouse timer handler (to handle scrolling selections).
class MouseTimerData    {
public:
    enum InvokeType  {
        m_ContextNotify,
        m_TimerNotify
    } m_pType;
    MWContext *m_pContext;
    MouseTimerData(MWContext *pContext);
};
void FEU_MouseTimer(void *vpReallyMouseTimerData);

//  Mouse timer handler (to handle window exit messages).
class MouseMoveTimerData    {
public:
    MWContext *m_pContext;
    MouseMoveTimerData(MWContext *pContext);
};
void FEU_MouseMoveTimer(void *vpReallyMouseMoveTimerData);

//  Only remnant of great idle processing empire.
//  Context calls FEU_RequestIdleProcessing to get checked in the idle loop
//      by FEU_DoIdleProcessing.
void FEU_RequestIdleProcessing(MWContext *pContext);
BOOL FEU_DoIdleProcessing();

//  How to handle client pull requests.
void FEU_ClientPull(MWContext *pContext, uint32 ulMilliseconds, URL_Struct *pUrl, int iFormatOut, BOOL bCanInterrupt);
void FEU_ClientPullNow(void *vpReallyClientPullTimerData);

// Dynamically manage the MAPI libraries for mail posting
void FEU_OpenMapiLibrary();
void FEU_CloseMapiLibrary();

//  Global history flush timer.
void FEU_GlobalHistoryTimeout(void *pNULL);

// This routine will return the handle of the pop-up menu whose first
// menu item has the specified command ID
HMENU FEU_FindSubmenu(HMENU hMenu, UINT nFirstCmdID);

BOOL FEU_IsConferenceAvailable(void);
BOOL FEU_IsCalendarAvailable(void);
BOOL FEU_IsIBMHostOnDemandAvailable(void);
BOOL FEU_IsNetcasterAvailable(void);
void FEU_OpenNetcaster(void);
BOOL FEU_IsAimAvailable(void);
void FEU_OpenAim(void);
BOOL FEU_IsNetscapeFrame(CWnd* pFocusWnd);

#ifdef XP_WIN32
CString FEU_GetCurrentRegistry(const CString &componentString);
#endif
CString FEU_GetInstallationDirectory(const CString &productString,const CString &installationString);

void FEU_FrameBroadcast(BOOL bSend, UINT Msg, WPARAM wParam, LPARAM lParam);

void FEU_AltMail_ShowMailBox(void); 
void FEU_AltMail_ShowMessageCenter(void); 
void FEU_AltMail_SetAltMailMenus(CMenu* pMenuCommunicator); 
void FEU_AltMail_ModifyMenu(CMenu* pMenuCommunicator, UINT uiCommunicatorMenuID, LPCSTR pszAltMailMenuStr); 

BOOL FEU_Execute(MWContext *pContext, const char *pCommand, const char *pParams);
BOOL FEU_FindExecutable(const char *pFileName, char *pExecutable, BOOL bIdentity, BOOL bExtension = FALSE);

BOOL FEU_SanityCheckFile(LPCSTR pszFileName);
BOOL FEU_SanityCheckDir(LPCSTR pszDirName);

//  Free off misc URL data structs.
void FEU_DeleteUrlData(URL_Struct *pUrl, MWContext *pCX);

extern "C" {
// used in lj_init.c too:
extern HWND FindNavigatorHiddenWindow(void);
}

HWND FEU_GetWindowFromPoint(HWND hWndTop, POINT *pPoint);
CNSGenFrame *FEU_GetDockingFrameFromPoint(POINT *pPoint);


#endif //	FE_UTILITIES__H
