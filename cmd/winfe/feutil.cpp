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

#include "stdafx.h"
#include "fe_proto.h"
#include "hiddenfr.h"
#include "postal.h"
#include "helper.h"
#include "prefapi.h"
#ifdef MOZ_MAIL_NEWS 
#include "mailfrm.h"
#endif // MOZ_MAIL_NEWS
#include "toolbar2.h"
#include "VerReg.h"
#include "libmocha.h"
#include "ngdwtrst.h"

#if defined(OJI)
#include "jvmmgr.h"
#elif defined(JAVA)
#include "java.h"
#endif

extern "C" {
#include "xpgetstr.h"
extern int XP_ALERT_NETCASTER_NO_JS;
extern int XP_ALERT_CANT_RUN_NETCASTER;
};


//	File used to house all general purpose front end
//		function which have no other specific home.
//	All functions in this file should begin with FEU.

//	Way to get a frame window pointer out of a context ID.
//	Must be of the appropriate type (Browser, mail, any, etc).

extern char szOLEFileType[MAX_INTERNAL_OLEFORMAT][4];

CFrameWnd *FEU_FindFrameByID(DWORD dwID, MWContextType cxType)
{
	//	Use the frame glue to do most of the work.
	CFrameGlue *pFrameGlue = CFrameGlue::FindFrameByID(dwID, cxType);
	if(pFrameGlue == NULL)	{
		return(NULL);
	}

	//	Return the frame window of the frame glue, can be NULL.
	return(pFrameGlue->GetFrameWnd());
}

//	Way to get a frame window that was last active browser window.
CFrameWnd *FEU_GetLastActiveFrame(MWContextType cxType, int nFindEditor)
{
	//	Have the frame glue look it up.
	CFrameGlue *pFrameGlue = CFrameGlue::GetLastActiveFrame(cxType, nFindEditor);
	if(pFrameGlue == NULL)	{
		return(NULL);
	}

	//	Return the frame window as known by the glue.
	return(pFrameGlue->GetFrameWnd());
}

//	Way to get a frame window that was last active browser window.
CAbstractCX *FEU_GetLastActiveFrameContext(MWContextType cxType, int nFindEditor)
{
	//	Have the frame glue look it up.
	CFrameGlue *pFrameGlue = CFrameGlue::GetLastActiveFrame(cxType, nFindEditor);
	if(pFrameGlue == NULL)	{
		return(NULL);
	}

	//	Return the frame window as known by the glue.
	return(pFrameGlue->GetMainContext());
}

CFrameWnd *FEU_GetLastActiveFrameByCustToolbarType(CString custToolbar, CFrameWnd *pCurrentFrame, BOOL bUseSaveInfo)
{
		//	Have the frame glue look it up.
	CFrameGlue *pFrameGlue = CFrameGlue::GetLastActiveFrameByCustToolbarType(custToolbar, pCurrentFrame, bUseSaveInfo);
	if(pFrameGlue == NULL)	{
		return(NULL);
	}

	//	Return the frame window as known by the glue.
	return(pFrameGlue->GetFrameWnd());

}

// Way to get the bottommost frame of t ype cxType
CFrameWnd *FEU_GetBottomFrame(MWContextType cxType, int nFindEditor)
{

	CFrameGlue *pFrameGlue = CFrameGlue::GetBottomFrame(cxType, nFindEditor);
	if(pFrameGlue == NULL)
		return NULL;

	//Return the frame window as known by the glue.
	return(pFrameGlue->GetFrameWnd());

}

//	Way to get a frame window that was last active browser window.
int FEU_GetNumActiveFrames(MWContextType cxType, int nFindEditor)
{
	//	Have the frame glue look it up.
	int nCount = CFrameGlue::GetNumActiveFrames(cxType, nFindEditor);

	//	Return the frame window as known by the glue.
	return(nCount);
}

//	Way to get the context ID out of the frame which was last active.
//	Returns 0 on error.
DWORD FEU_GetLastActiveFrameID(MWContextType cxType)
{
	DWORD dwRetval = 0;

	//	Look up the last active frame of said type.
	CFrameGlue *pFrameGlue = CFrameGlue::GetLastActiveFrame(cxType);
	if(pFrameGlue != NULL)	{
		//	Must have a context.
		if(pFrameGlue->GetActiveContext() != NULL)	{
			dwRetval = pFrameGlue->GetActiveContext()->GetContextID();
		}
		else if(pFrameGlue->GetMainContext() != NULL)	{
			dwRetval = pFrameGlue->GetMainContext()->GetContextID();
		}
	}

	return(dwRetval);
}

//#ifndef NO_TAB_NAVIGATION 
// Scroll the current MWContext so that the Rect is visible
// FEU_MakeRectVisible() is subtracted from old FEU_MakeFormVisible(),
// and is used not only for Form elements, but other Tabable
// elements, such as links.
void FEU_MakeRectVisible(MWContext *pContext, const UINT left, const UINT top, const UINT right, const UINT bottom)
{
    if(pContext == NULL )   
		return;

    if(! ABSTRACTCX(pContext) || ! ABSTRACTCX(pContext)->IsWindowContext() ) 
		return;

	LTRB Rect( left, top, right, bottom);
	CPaneCX *pCX = PANECX(pContext);
    int32 lX = pCX->GetOriginX();
    int32 lY = pCX->GetOriginY();
    BOOL bMove = FALSE;

    //  If the element is partially to the right of the screen, we only want to
    //      move enough to get it fully on the screen.
    if(Rect.left < pCX->GetOriginX() + pCX->GetWidth() &&
        Rect.right > pCX->GetOriginX() + pCX->GetWidth() &&
        Rect.Width() < pCX->GetWidth())   {
        lX += Rect.right - (pCX->GetOriginX() + pCX->GetWidth());
        bMove = TRUE;
    }

    //  If the element is partially to the bottom of the screen, we only want to
    //      move enough to get it fully onto the screen.
    if(Rect.top < pCX->GetOriginY() + pCX->GetHeight() &&
        Rect.bottom > pCX->GetOriginY() + pCX->GetHeight() &&
        Rect.Height() < pCX->GetHeight()) {
        lY += Rect.bottom - (pCX->GetOriginX() + pCX->GetHeight());
        bMove = TRUE;
    }

    //  If the element is not fully on the screen, then we want to move so that it
    //      is on the screen at whatever cost that may be.
    if(Rect.left < lX || Rect.left > lX + pCX->GetWidth() || Rect.top < lY ||
        Rect.top > lY + pCX->GetHeight())  {
        lX = Rect.left;
        lY = Rect.top;
        bMove = TRUE;
    }

    //  Move if needed.
    if(bMove)   {
        FE_SetDocPosition(pContext, FE_VIEW, lX, lY);
    }

}	// FEU_MakeRectVisible(

// old version is named as FEU_MakeFormVisible()
// Scroll the current MWContext so that the child window is visible
// New version.
void FEU_MakeElementVisible(MWContext *pContext, LO_Any  *pElement)
{
    if( NULL == pElement)
		return;
    //  Figure up where the form element actually lies.
    LTRB Rect;
    Rect.left = pElement->x + pElement->x_offset;
    Rect.top = pElement->y + pElement->y_offset;
    Rect.right = Rect.left + pElement->width;
    Rect.bottom = Rect.top + pElement->height;

	FEU_MakeRectVisible( pContext, CASTINT(Rect.left), CASTINT(Rect.top), CASTINT(Rect.right), CASTINT(Rect.bottom) );
}
//#else	/* NO_TAB_NAVIGATION */
//#endif	/* NO_TAB_NAVIGATION */

//	The purpose of FEU_AhAhAhAhStayingAlive is to house the one and only
//		saturday night fever function; named after Chouck's idol.
//	This function will attempt to do all that is necessary in order
//		to keep the application's messages flowing and idle loops
//		going when we need to finish an asynchronous operation
//		synchronously.
//	The current cases that cause this are RPC calls into the
//		application where we need to return a value or produce output
//		from only one entry point before returning to the caller.
//
//	If and when you modify this function, get your changes reviewed.
//	It is too vital that this work, always.
//
//	The function only attempts to look at one message at a time, or
//		propigate one idle call at a time, keeping it's own idle count.
//	This is not a loop.  YOU must provide the loop which calls this function.
//
//	Due to the nature and order of which we process windows messages, this
//		can seriously mess with the flow of control through the client.
//	If there is any chance at all that you can ensure that you are at the
//		bottom of the message queue before doing this, then please take those
//		measures.
extern "C" void FEU_StayingAlive()
{
	static long lIdleCounter = 0;

	//	Stage 1.
	//	See if there are any messages which need to be propigated.
	MSG msg;
	if(::PeekMessage(&msg, NULL, NULL, NULL, PM_NOREMOVE))	{
		BOOL bPumpVal = theApp.NSPumpMessage();

		//	If this assertion fails, then we received a WM_QUIT, and 
		//		the user is about to receive a dialog from the OS saying
		//		we are not responding to the system's request to shut down.
		//	Nothing we can do and still accomplish what we are trying to do....
		ASSERT(bPumpVal);

		//	Reset the idle counter.
		lIdleCounter = 0;
	}
	else	{
		//	Stage 2.
		//	Call the Apps Idle loop.
		//	Ignore wether or not it says it needs or does not need more idle time.
		//	It is wholly dependent upon wether or not there are events in the queue.
		theApp.OnIdle(lIdleCounter++);
	}
}

//	A utility function to block returning until a context is no longer
//		found in the context list.
void FEU_BlockUntilDestroyed(DWORD dwContextID)
{
	TRACE("Entering FEU_BlockUntilDestroyed(%lu)\n", dwContextID);

	//	Loop until the context is not in the context list, meaning it
	//		has been destroyed.
	while(CAbstractCX::FindContextByID(dwContextID) != NULL)	{
		//	Keep the app going.
		FEU_StayingAlive();
	}

	TRACE("Leaving FEU_BlockUntilDestroyed(%lu)\n", dwContextID);
}

//
// Dynamically open the MAPI libraries for mail posting
//
void FEU_OpenMapiLibrary() 
{
	BOOL bLoadOK = FALSE;

    theApp.m_fnOpenMailSession = NULL;
    theApp.m_fnComposeMailMessage = NULL;
    theApp.m_fnUnRegisterMailClient = NULL; 
	theApp.m_fnShowMailBox = NULL;
	theApp.m_fnShowMessageCenter = NULL;
	theApp.m_fnCloseMailSession = NULL;
	theApp.m_fnGetMenuItemString = NULL;

    UINT fuErrorMode = SetErrorMode(SEM_NOOPENFILEERRORBOX);

	char * prefStr = NULL;
	PREF_CopyCharPref("mail.altmail_dll",&prefStr);
	theApp.m_hPostalLib = LoadLibrary(prefStr);
	if (prefStr) XP_FREE(prefStr);

    SetErrorMode(fuErrorMode);
#ifdef XP_WIN32 
    if(theApp.m_hPostalLib) 
	{
#else 
	if(theApp.m_hPostalLib > HINSTANCE_ERROR) 
	{ 
#endif
		//If we don't find "ShowMailBox" then we know it's either an old or invalid dll
		SHOWMAILBOX testProc = (SHOWMAILBOX)::GetProcAddress(theApp.m_hPostalLib, "ShowMailBox");
		if(testProc) 
		{
			REGISTERMAIL regProc = (REGISTERMAIL)::GetProcAddress(theApp.m_hPostalLib, "RegisterMailClient");
			theApp.m_fnOpenMailSession = 
				(OPENMAIL)::GetProcAddress(theApp.m_hPostalLib, "OpenMailSession");
			theApp.m_fnComposeMailMessage = 
				(COMPOSEMAIL)::GetProcAddress(theApp.m_hPostalLib, "ComposeMailMessage");
			theApp.m_fnUnRegisterMailClient = 
				(UNREGISTERMAIL)::GetProcAddress(theApp.m_hPostalLib, "UnRegisterMailClient");
			theApp.m_fnShowMailBox =  
				(SHOWMAILBOX)::GetProcAddress(theApp.m_hPostalLib, "ShowMailBox"); 
			theApp.m_fnShowMessageCenter =  
				(SHOWMESSAGECENTER)::GetProcAddress(theApp.m_hPostalLib, "ShowMessageCenter"); 
			theApp.m_fnCloseMailSession =  
				(CLOSEMAIL)::GetProcAddress(theApp.m_hPostalLib, "CloseMailSession"); 
			theApp.m_fnGetMenuItemString =
				(GETMENUITEMSTRING)::GetProcAddress(theApp.m_hPostalLib, "GetMenuItemString"); 

			if(theApp.m_fnOpenMailSession && theApp.m_fnComposeMailMessage && theApp.m_fnUnRegisterMailClient
				&& theApp.m_fnShowMailBox && theApp.m_fnShowMessageCenter && theApp.m_fnCloseMailSession
				&& theApp.m_fnGetMenuItemString && regProc)
			{
				POSTCODE status = (*regProc) (theApp.m_pHiddenFrame->m_hWnd, "Netscape Mail System");
	            if(status == POST_OK)
					bLoadOK = TRUE;
				else
					MessageBox(NULL, szLoadString(IDS_ALTMAIL_REGISTER_FAILED), szLoadString(AFX_IDS_APP_TITLE), MB_OK | MB_ICONEXCLAMATION); 
			}
			else
				MessageBox(NULL, szLoadString(IDS_ALTMAIL_MISSING_FUNCTIONS), szLoadString(AFX_IDS_APP_TITLE), MB_OK | MB_ICONEXCLAMATION); 
		} 
		else
			MessageBox(NULL, szLoadString(IDS_ALTMAIL_OLD_DLL), szLoadString(AFX_IDS_APP_TITLE), MB_OK | MB_ICONEXCLAMATION); 
    }
	else 
	{ 
		#ifdef XP_WIN16
			//In Win16 the LoadLibrary returns < 32 when it fails
			//and since we check m_hPostalLib != NULL throughout
			//the code let's set m_hPostalLib to NULL.
			theApp.m_hPostalLib = NULL;
		#endif

		MessageBox(NULL, szLoadString(IDS_ALTMAIL_MISSING_DLL), szLoadString(AFX_IDS_APP_TITLE), MB_OK | MB_ICONEXCLAMATION); 
	} 

	if(!bLoadOK && theApp.m_hPostalLib)
	{
		FreeLibrary(theApp.m_hPostalLib);
		theApp.m_hPostalLib = NULL;
	}
}

void FEU_CloseMapiLibrary()
{
	if(theApp.m_fnCloseMailSession) 
		(*theApp.m_fnCloseMailSession) ();
    if(theApp.m_fnUnRegisterMailClient)
        (*theApp.m_fnUnRegisterMailClient) ();
    if(theApp.m_hPostalLib)
        FreeLibrary(theApp.m_hPostalLib);
    theApp.m_hPostalLib = NULL;

    // call init if reload
    theApp.m_bInitMapi = TRUE;

    // clear out all function pointers
    theApp.m_fnOpenMailSession = NULL;
    theApp.m_fnComposeMailMessage = NULL;
    theApp.m_fnUnRegisterMailClient = NULL; 
	theApp.m_fnShowMailBox = NULL; 
	theApp.m_fnShowMessageCenter = NULL;
	theApp.m_fnCloseMailSession = NULL;
	theApp.m_fnGetMenuItemString = NULL;
}

#ifdef XP_WIN16
//	16 bits needs a GetDiskFreeSpace call.
BOOL GetDiskFreeSpace(LPCTSTR lpRootPathName, LPDWORD lpSectorsPerCluster,
	LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters)
{
	//	Init.
	*lpSectorsPerCluster = 0;
	*lpBytesPerSector = 0;
	*lpNumberOfFreeClusters = 0;
	*lpTotalNumberOfClusters = 0;

	//	Saftey dance.
	if(lpRootPathName == NULL)	{
		return(FALSE);
	}
	unsigned uDrive = 0;
	if(strlen(lpRootPathName) > 1 && lpRootPathName[1] == ':')	{
		//	Determine what drive we're looking at.
		uDrive = toupper(*lpRootPathName) - 'A' + 1;
		if(uDrive < 1 || uDrive > 26)	{
			//	Use default drive....
			uDrive = 0;
		}
	}

	//	Ask for amount of free disk space.
	_diskfree_t dtFree;
	memset(&dtFree, 0, sizeof(dtFree));
	if(_dos_getdiskfree(uDrive, &dtFree))	{
		//	Call failure.
		return(FALSE);
	}

	//	assign what we found out.
	*lpSectorsPerCluster = dtFree.sectors_per_cluster;
	*lpBytesPerSector = dtFree.bytes_per_sector;
	*lpNumberOfFreeClusters = dtFree.avail_clusters;
	*lpTotalNumberOfClusters = dtFree.total_clusters;

	//	Success.
	return(TRUE);
}
#endif

//	See if content length can fit on a disk.
//	If not, ask the user to confirm what they want (to write anyway).
//	pFileName should contain the full path information for the file.
//	All failures in this code mean success, actual errors should be
//		found by other code attempting to open or write the file.
//	Only on user denial to write will this code return failure.

// Implemented in femess.cpp, declared in msgcom.h
extern "C" uint32 FE_DiskSpaceAvailable (MWContext *context, const char *lpszPath );

BOOL FEU_ConfirmFreeDiskSpace(MWContext *pContext, const char *pFileName, int32 lContentLength)
{
	//	Absence of context means success since can't confirm space with user.
	if(pContext == NULL)	{
		return(TRUE);
	}

	//	Absence of a valid content length means success.
	if(lContentLength <= 0)	{
		TRACE("Invalid content length, can't check free disk space.\n");
		return(TRUE);
	}

	DWORD dwFreeSpace = FE_DiskSpaceAvailable( pContext, pFileName );

	//	If the length is greater than this, we need to confirm with the user on what to do.
	if(dwFreeSpace < (DWORD)lContentLength)	{
		//	Ask the user.
		CString csAsk;
		csAsk.LoadString(IDS_CONFIRM_DISK_SPACE);
		size_t stSize = strlen(pFileName) + csAsk.GetLength() + 10;
		char *pBuffer = new char[stSize];
		if(pBuffer != NULL)	{
			sprintf(pBuffer, csAsk, pFileName);
			BOOL bRetval = FE_Confirm(pContext, pBuffer);
			delete [] pBuffer;
			return(bRetval);
		}
	}

	//	Let it happen.
	return(TRUE);
}

#if defined(XP_WIN16)
//	There is literally no 32 bit equivalent for GetFreeSystemResources.
//	Usually set pString to the file name, and iDigit to the line number.
//	bBox controls wether or not a trace message will be used, or a dialog box.
void FEU_FreeResources(const char *pString, int iDigit, BOOL bBox)
{
	UINT uSystem = GetFreeSystemResources(GFSR_SYSTEMRESOURCES);
	UINT uGdi = GetFreeSystemResources(GFSR_GDIRESOURCES);
	UINT uUser = GetFreeSystemResources(GFSR_USERRESOURCES);

	char aBuffer[1024];
	sprintf(aBuffer, "%s:%d\n%u System\n%u GDI\n%u User\n", pString, iDigit, uSystem, uGdi, uUser);

	if(bBox)	{
		::MessageBox(NULL, aBuffer, "Resources", MB_OK);
	}
	TRACE(aBuffer);
}
#endif

//	Remove all trailing backslashes from the string.
char *FEU_NoTrailingBackslash(char *pBackslash)	{
	if(pBackslash)	{
		int32 lTempLen = XP_STRLEN(pBackslash);
		while(lTempLen)	{
			lTempLen--;	//	Back off one for comparison of the last char.
			if(*(pBackslash + lTempLen) == '\\')	{
				//	Knock it off.
				*(pBackslash + lTempLen) = '\0';
			}
			else	{
				//	No more backslashes to whack on the end.
				break;
			}
		}
	}

	return(pBackslash);
}

//  Replace all occurrences of original in pStr with new
void FEU_ReplaceChar(char *pStr, char original, char replace)
{
	char *pFound;

	while((pFound = strchr(pStr, original)) != NULL) {
		*pFound = replace;
	}
}

CString FEU_EscapeAmpersand(CString str)
{

	CString newStr(""), leftStr, rightStr;
	int nIndex;
	rightStr = str;

	while((nIndex = rightStr.Find('&')) != -1)
	{
		leftStr = rightStr.Left(nIndex);
		newStr = newStr + leftStr;
		newStr = newStr + "&&";
		rightStr = rightStr.Right(rightStr.GetLength() - nIndex - 1);
	}

	// put in everything after the last ampersand.
	newStr = newStr + rightStr;
	return newStr;

}



//  Write string values to the registry in the said location.
BOOL FEU_RegistryWizard(HKEY hRoot, const char *pKey, const char *pValue)
{
    if(!pKey)   {
        return(FALSE);
    }

    HKEY hKey;
    LONG lResultCreate = RegCreateKey(hRoot, pKey, &hKey);

    if(lResultCreate != ERROR_SUCCESS)    {
        return(FALSE);
    }

    LONG lResultValue = RegSetValue(hKey, NULL, REG_SZ,
                          pValue ? pValue : "",
                          pValue ? XP_STRLEN(pValue) + 1 : 1);

    LONG lResultClose = RegCloseKey(hKey);

    if(lResultValue != ERROR_SUCCESS || lResultClose != ERROR_SUCCESS)  {
        return(FALSE);
    }

    return(TRUE);
}


// support quoted name.
// clapse multiple space is not quoted.
int FEU_ExtractCommaDilimetedFontName(const char *pArgList, int offSetByte, char *argItem)
{
	int		theQuote = '\0';
	int		length = 0;
	int		isLastSpace = 0;
	
    const char *pTraverse = pArgList + offSetByte;
	*argItem = '\0';

    //  Handle all stupidness.
    if(pTraverse == NULL || *pTraverse == '\0' || offSetByte < 0)    {
        return(0);		// no arg found
    }

	// skip leading space
	while(*pTraverse && isspace(*pTraverse))  
		pTraverse++;
	
	// See if this arg is quoted
	theQuote = '\0';
	if( *pTraverse == '"' || *pTraverse == '\'' ) {
		theQuote = *pTraverse++;
	}

	if( theQuote != '\0' ) {
		// quoted arg
		while( *pTraverse ) {
			if( *pTraverse != theQuote ) {
				*argItem++ = * pTraverse++;
				length++;
				if( length >= MAXFONTFACENAME -1 ) {
					// overflow
					*(--argItem) = '\0';
					return(-1);       
				}
			} else {
				*argItem = '\0';	// terminate
				pTraverse++;		// skip the quote
				break;
			}
		}

		// we passed the quote, now get over the comma
		// ignore any char between closing quote and comma
		while( *pTraverse && *pTraverse != ',' )     
			pTraverse++;
		
		if( *pTraverse == ',' )
			pTraverse++;
	
		return( pTraverse - pArgList );   // offset for next arg

	} 
	
	// arg without quote
	while( *pTraverse && *pTraverse != ',' ) {
		if( isLastSpace && isspace(*pTraverse) ) {
			pTraverse++;           // clapse multiple space
			continue;
		} else {
			isLastSpace = isspace(*pTraverse);
			*argItem++ = *pTraverse++;
			length++;
			if( length >= MAXFONTFACENAME -1 ) {
				// overflow
				*(--argItem) = '\0';
				return(-1);       
			}
		}
	}	// while( *pTraverse  )

	*argItem = '\0';		// terminator
	// remove trailing space
	while( --length && isspace( *(--argItem) ) )
		*argItem = '\0';		// terminator


	if( *pTraverse == ',' )
		pTraverse++;

	return( pTraverse - pArgList );   // offset for next arg
}

#ifdef not_support_quoted_font_face_name
//  As the function indicates.
//  Allocated return values to be freed by caller.
char *FEU_ExtractCommaDilimetedString(const char *pArgList, int iArgToExtract)
{
    char *pRetval = NULL;

    //  Handle all stupidness.
    if(pArgList == NULL || iArgToExtract <= 0)    {
        return(pRetval);
    }

    //  Skip to comma of the arg in question.
    const char *pTraverse = pArgList;
    do  {
        iArgToExtract--;
        if(iArgToExtract)   {
            //  Search for next comma.
            pTraverse = strchr(pTraverse, ',');
            if(pTraverse)   {
                //  Go past comma.
                pTraverse++;
            }
        }
    } while(iArgToExtract && pTraverse);

    if(pTraverse)   {
        //  Skip all whitespace before the string.
        while(isspace(*pTraverse))  {
            pTraverse++;
        }

        //  Copy till next comma or end of string.
        //  Make sure not empty.
        char *pEnd = strchr(pTraverse, ',');
        int iAlloc = 0;
        if(pEnd == NULL)    {
            iAlloc = strlen(pTraverse) + 1;
        }
        else    {
            iAlloc = pEnd - pTraverse + 1;
        }

        //  If we've something to allocate.
        if(iAlloc)  {
            pRetval = (char *)XP_ALLOC(iAlloc);
            if(pRetval) {
                //  clear it out.
                memset(pRetval, 0, iAlloc);

                //  Copy over the amount -1.
                strncpy(pRetval, pTraverse, iAlloc - 1);

                //  Walk backwards through the string, clearing off
                //      any space.
                pEnd = pRetval + iAlloc - 2;
                while(pEnd >= pRetval && isspace(*pEnd))   {
                    *pEnd = '\0';
                    pEnd--;
                }
            }
        }
    }

    return(pRetval);
}
#endif // not_support_quoted_font_face_name

//  As the function indicates.
//  Allocated return values to be freed by caller.
char *FEU_ExtractCommaDilimetedQuotedString(const char *pArgList, int iArgToExtract) {
    char *pRetval = NULL;

    //  Handle all stupidness.
    if(pArgList == NULL || iArgToExtract <= 0)    {
        return(pRetval);
    }

    //  Find the first argument.
    //  First arg will be right after the first quote.
    const char *pTraverse = pArgList;
    while(*pTraverse != '\"' && *pTraverse != '\0')   {
        pTraverse++;
    }

    //  If we have an argument.
    if(*pTraverse == '\"')  {
        pTraverse++;

        //  Okay, we need to find out where the ending comma exists.
        int iQuoteLevel = 1;
        const char *pQuote = pTraverse;
        while(iQuoteLevel)  {
            //  Go until we reach another quote.
            while(*pQuote != '\"' && *pQuote != '\0')    {
                pQuote++;
            }

            //  Did we reach a quote?
            if(*pQuote == '\"') {
                pQuote++;

                //  Assume this is an ending quote if we can find a comma before another quote.
                iQuoteLevel--;
                if(iQuoteLevel == 0)    {
                    //  Begin ananlysis.
                    const char *pComma = pQuote;
                    while(*pComma != '\"' && *pComma != ',' && *pComma != '\0') {
                        pComma++;
                    }

                    //  What are we lookit at?
                    if(*pComma == '\"') {
                        //  Another quote.
                        //  Up the quote count by 2.
                        iQuoteLevel += 2;
                    }
                    else if(*pComma == ',') {
                        //  We're out of here!
                        //  Is this the first argument, though?
                        if(iArgToExtract != 1)  {
                            //  What they want is a different one.
                            //  Go recursive and let them pick up where we left off.
                            pRetval = FEU_ExtractCommaDilimetedQuotedString(pComma + 1, iArgToExtract - 1);
                        }
                        else    {
                            //  We have the argument they want.
                            //  Exact dimenstion of the argument are from pTraverse to pQuote - 2;
                            //  Cathch the boundry case of "",
                            if(pQuote - 2 >= pTraverse) {
                                int iLength = pQuote - pTraverse;
                                pRetval = (char *)XP_ALLOC(iLength);
                                if(pRetval) {
                                    memset(pRetval, 0, iLength);

                                    int iCounter = 0;
                                    while(pTraverse <= pQuote - 2)  {
                                        pRetval[iCounter] = *pTraverse;
                                        pTraverse++;
                                        iCounter++;
                                    }
                                }
                            }
                        }
                    }
                    else    {
                        //  End of string, get out of while loop.
                        //  However, if this is the only argument we're looking for, we may already have
                        //      what we need.
                        if(iArgToExtract == 1)  {
                            //  We have the argument they want.
                            //  Exact dimenstion of the argument are from pTraverse to pQuote - 2;
                            //  Cathch the boundry case of "",
                            if(pQuote - 2 >= pTraverse) {
                                int iLength = pQuote - pTraverse;
                                pRetval = (char *)XP_ALLOC(iLength);
                                if(pRetval) {
                                    memset(pRetval, 0, iLength);

                                    int iCounter = 0;
                                    while(pTraverse <= pQuote - 2)  {
                                        pRetval[iCounter] = *pTraverse;
                                        pTraverse++;
                                        iCounter++;
                                    }
                                }
                            }
                        }
                        break;
                    }
                }
            }
            else    {
                //  end of string, just get out of the while loop.
                break;
            }
        }
    }

    return(pRetval);
}

/****************************************************************************
*
*	FEU_TransBlt
*
*	PARAMETERS:
*		pSrcDC		- pointer to source DC (with selected image)
*		pDstDC		- pointer to destination DC (where image is to be drawn)
*		ptSrc		- source x,y coordinates
*		ptDst		- destination x,y coordinates
*		nWidth		- image width
*		nHeight		- image height
*		rgbTrans	- transparent (background) color = RGB(255, 0, 255)
*
*	RETURNS:
*		TRUE if successful.
*
*	DESCRIPTION:
*		This function is called to do a transparent BitBlt. The background
*		or transparent color is given by rgbTrans, with the default being
*		pink {RGB(255, 0, 255)}. Pixels of this color will be masked out, so
*		that the image is drawn without disturbing the destination.
*
*		Note that all masking operations are done in a memory DC to avoid
*		flicker. pDstDC should be an actual screen DC.
*
****************************************************************************/

BOOL FEU_TransBlt(CDC * pSrcDC, CDC * pDstDC, const CPoint & ptSrc,
	const CPoint & ptDst, int nWidth, int nHeight, HPALETTE hPalette,
	const COLORREF rgbTrans /*= RGB(255, 0, 255)*/)
{
	return FEU_TransBlt( pDstDC->m_hDC, ptDst.x, ptDst.y, nWidth, nHeight,
						 pSrcDC->m_hDC, ptSrc.x, ptSrc.y, hPalette, rgbTrans );
}

// More useful version that migrates more sanely from BitBlt.

BOOL FEU_TransBlt(HDC hDstDC, int nXDest, int nYDest, int nWidth, int nHeight, 
				  HDC hSrcDC, int nXSrc, int nYSrc, HPALETTE hPalette, COLORREF rgbTrans )
{

	BOOL bRtn = TRUE;
	
	// We'll paint our bitmap using the "true mask" method for transparency.
	// We assume a pre-designated color for the background (transparent) color,
	// and create our monochrome mask at run time.
		
	// Create mask for transparent blits
	HPALETTE hOldPal = ::SelectPalette(hSrcDC, hPalette, FALSE);
//	::RealizePalette(hSrcDC);
	HDC hMaskDC = ::CreateCompatibleDC(hDstDC);
	HBITMAP hbmMask = ::CreateBitmap(nWidth, nHeight, 1, 1, NULL);
	HBITMAP hOldMaskBmp = (HBITMAP) ::SelectObject(hMaskDC, hbmMask);
	COLORREF rgbOldBk = ::SetBkColor(hSrcDC, rgbTrans);	  
	::BitBlt(hMaskDC, 0, 0, nWidth, nHeight, hSrcDC, nXSrc, nYSrc, SRCCOPY);
	::SetBkColor(hSrcDC, rgbOldBk);
	::SelectPalette(hSrcDC, hOldPal, TRUE);
		
	// First, copy the existing image from the destination to memory for
	// flicker free painting.
	HDC hMemDC = ::CreateCompatibleDC(hDstDC);
	HBITMAP hbmMem = ::CreateCompatibleBitmap(hDstDC, nWidth, nHeight);
	HBITMAP hOldMemBmp = (HBITMAP) ::SelectObject(hMemDC, hbmMem);
	::BitBlt(hMemDC, 0, 0, nWidth, nHeight, hDstDC, nXDest, nYDest, SRCCOPY);
	
	// Next, blit our bitmap, the mask, then our bitmap again to the memory
	// DC using the proper raster ops.
   	COLORREF rgbOldTxt = ::SetTextColor(hMemDC, PALETTERGB(0, 0, 0));
    rgbOldBk = SetBkColor(hMemDC, PALETTERGB(255, 255, 255));
	::BitBlt(hMemDC, 0, 0, nWidth, nHeight, hSrcDC, nXSrc, nYSrc, SRCINVERT);
	::BitBlt(hMemDC, 0, 0, nWidth, nHeight, hMaskDC, 0, 0, SRCAND);
	::BitBlt(hMemDC, 0, 0, nWidth, nHeight, hSrcDC, nXSrc, nYSrc, SRCINVERT);
   	::SetTextColor(hMemDC, rgbOldTxt);
    ::SetBkColor(hMemDC, rgbOldBk);
	
	// Finally, blit from memory DC back to the destination
	bRtn = ::BitBlt(hDstDC, nXDest, nYDest, nWidth, nHeight, hMemDC, 0, 0,
					SRCCOPY);
	
	// Cleanup
	::SelectObject(hMaskDC, hOldMaskBmp);
	VERIFY(::DeleteDC(hMaskDC));
	VERIFY(::DeleteObject(hbmMask));
	
	::SelectObject(hMemDC, hOldMemBmp);
	VERIFY(::DeleteDC(hMemDC));
	VERIFY(::DeleteObject(hbmMem));

	return(bRtn);
} // END OF	FUNCTION FEU_TransBlt()

//  This function create because no one knows how to use SetWindowPlacement
//      correctly.
void FEU_InitWINDOWPLACEMENT(HWND hWindow, WINDOWPLACEMENT *pWindowPlacement)
{
    //  Safety
    if(!pWindowPlacement)    {
        return;
    }

    //  Thrash to zero.
    memset(pWindowPlacement, 0, sizeof(WINDOWPLACEMENT));

    //  Set the length member.
    pWindowPlacement->length = sizeof(WINDOWPLACEMENT);

    //  Safety
    if(!hWindow) {
        return;
    }

    //  Initialize the structure with the current window settings.
    BOOL bGotIt = ::GetWindowPlacement(hWindow, pWindowPlacement);
    ASSERT(bGotIt);

    //  Hacker now free to change members that need to be changed without
    //      thrashing other stuff too.
}

//  Mouse timer handler (to handle scrolling selections).
MouseTimerData::MouseTimerData(MWContext *pContext)
{
    //  By default treat as a context notification.
    m_pContext = pContext;
    m_pType = m_ContextNotify;
}

void FEU_MouseTimer(void *vpReallyMouseTimerData)
{
    //  Cast.
    MouseTimerData *pData = (MouseTimerData *)vpReallyMouseTimerData;

    //  Decide what to do by type.
    if(pData->m_pType == MouseTimerData::m_ContextNotify)   {
        //  If the left button is down, we need to set up a timer to
        //      call the mouse move function again so that scrolling selections
        //      work correctly.
        //  GARRETT:  PANECX will want the mouse stuff.
        if(XP_IsContextInList(pData->m_pContext) &&
            ABSTRACTCX(pData->m_pContext) &&
            ABSTRACTCX(pData->m_pContext)->IsFrameContext())   {
            CWinCX *pWinCX = WINCX(pData->m_pContext);

            //  Don't do autoscroll if left button isn't down and isn't
            //      already an timout registered.
            if(pWinCX->m_bLBDown && !pWinCX->m_bScrollingTimerSet)   {
                //  Set up the timer.
                MouseTimerData *pLater = new MouseTimerData(pData->m_pContext);
                if(pLater)  {
                    //  Manually set to be a timer notification.
                    pLater->m_pType = MouseTimerData::m_TimerNotify;

                    //  Set that the context has a scrolling timeout event.
                    pWinCX->m_bScrollingTimerSet = TRUE;

                    //  Set the timer to call us back in X number milliseconds.
                    FE_SetTimeout(FEU_MouseTimer, (void *)pLater, 100);
                }
            }
        }

        //  Do not delete the passed in data.
        //  Up to the caller to do so.
    }
    else if(pData->m_pType == MouseTimerData::m_TimerNotify)    {
        //  If the left button is still down, we need to act like a mouse
        //      move occurred on the context so scrolling selections work.
        if(XP_IsContextInList(pData->m_pContext) &&
            ABSTRACTCX(pData->m_pContext) &&
            ABSTRACTCX(pData->m_pContext)->IsFrameContext())   {
            CWinCX *pWinCX = WINCX(pData->m_pContext);
            //  We need to remove the previously allocated mouse data that
            //      we allocated ourselves.
            delete pData;

            if(pWinCX->m_bLBDown && pWinCX->m_bScrollingTimerSet)   {
                //  Clear the scrolling timer flag (do before call or won't
                //      set another timeout).
                pWinCX->m_bScrollingTimerSet = FALSE;

                //  This will set up another timer if needed.
                BOOL bReturnImmediately = FALSE;
                pWinCX->OnMouseMoveCX(pWinCX->m_uMouseFlags, pWinCX->m_cpMMove, bReturnImmediately);
                if(bReturnImmediately)  {
                    return;
                }
            }

        }

    }
}

///  Mouse timer handler (to handle moves outside the window.).
MouseMoveTimerData::MouseMoveTimerData(MWContext *pContext)
{
    //  By default treat as a context notification.
    m_pContext = pContext;
}

void FEU_MouseMoveTimer(void *vpReallyMouseMoveTimerData)
{
    //  Cast.
    MouseMoveTimerData *pData = (MouseMoveTimerData *)vpReallyMouseMoveTimerData;
    if (!pData)
	return;

    if(XP_IsContextInList(pData->m_pContext) &&
	ABSTRACTCX(pData->m_pContext) &&
	ABSTRACTCX(pData->m_pContext)->IsFrameContext())   {
	CWinCX *pWinCX = WINCX(pData->m_pContext);

	if(!pWinCX->m_bMouseMoveTimerSet)   {
            //  Set up the timer.
            MouseMoveTimerData *pLater = new MouseMoveTimerData(pData->m_pContext);
            if(pLater)  {
                //  Set that the context has a mousemove timeout event.
                pWinCX->m_bMouseMoveTimerSet = TRUE;

                //  Set the timer to call us back in X number milliseconds.
                FE_SetTimeout(FEU_MouseMoveTimer, (void *)pLater, 200);
            }
        }

	else {
	    POINT mp;
	    // get mouse position
	    ::GetCursorPos(&mp);

	    if ((::WindowFromPoint(mp) != pWinCX->GetPane()) && !pWinCX->m_bLBDown) {
		// We've moved outside the window.  Stop looping the timer and 
		// send a simulated mousemove to the view.
		delete pData;

		::ScreenToClient(pWinCX->GetPane(), &mp);

                BOOL bReturnImmediately = FALSE;
                pWinCX->OnMouseMoveCX(pWinCX->m_uMouseFlags, mp, bReturnImmediately);

		pWinCX->m_bMouseMoveTimerSet = FALSE;
            }
	    else {
		// We're still in the window.  Set another timer.
		FE_SetTimeout(FEU_MouseMoveTimer, (void *)pData, 200);
	    }
        }
    }
}

//  Context wants to have idle processing check to see if any actions
//      need be taken with it.
class MWContextList {
public:
    MWContext *m_pContext;
    MWContextList *m_pNext;
};
MWContextList *listIdleContexts = NULL;
void *timerIdleContexts = NULL;
#define IDLETIMEOUT 50

void idleTimer(void *pNULL)
{
    timerIdleContexts = NULL;
    if(FEU_DoIdleProcessing())  {
        timerIdleContexts = FE_SetTimeout(idleTimer, NULL, IDLETIMEOUT);
    }
}

void FEU_RequestIdleProcessing(MWContext *pContext)
{
    //  Make sure not already in list.
    MWContextList *pTraverse = listIdleContexts;
    while(pTraverse)    {
        if(pTraverse->m_pContext == pContext)   {
            //  Already here, don't do it.
            return;
        }
        pTraverse = pTraverse->m_pNext;
    }

    //  Not in list, add to tail.
    MWContextList *pNewEntry = new MWContextList();
    if(pNewEntry)   {
        MWContextList **ppNext = &listIdleContexts;
        while(*ppNext)  {
            ppNext = &((*ppNext)->m_pNext);
        }

        pNewEntry->m_pNext = NULL;
        pNewEntry->m_pContext = pContext;
        *ppNext = pNewEntry;        
    }

    //  If we don't have a timer, then start one up.
    if(timerIdleContexts == NULL)   {
        timerIdleContexts = FE_SetTimeout(idleTimer, NULL, IDLETIMEOUT);
    }
}

BOOL FEU_DoIdleProcessing()
{
    //  Check our list and do one action per context at a time.
    //  Remove the context from the list if no known action need be
    //      taken with it.
    MWContextList *pEntry = listIdleContexts;
    if(pEntry)  {
        //  Do NOT use else ifs.  Instead use "if(bNoAction &&" everywhere
        //  Must set bNoAction to FALSE after an action is taken.
        BOOL bNoAction = TRUE;

        //  Context still valid?
        //  Could have been blown away since it registered.
        if(bNoAction && XP_IsContextInList(pEntry->m_pContext))  {
            //  Front end or XP context (XP not handled).
            //  Do not check the destroyed flag here, or contexts never
            //      get really freed off, instead tighten the called code.
            if(bNoAction && ABSTRACTCX(pEntry->m_pContext))  {
                CAbstractCX *pCX = ABSTRACTCX(pEntry->m_pContext);

                //  Lastly is the don't care case, abstract only.
                if(bNoAction)   {
                    //	Do an interrupt if needed.
	                if(bNoAction && pCX->m_bIdleInterrupt == TRUE)	{
                        bNoAction = FALSE;
		                pCX->Interrupt();		                
	                }
                    //  Check for nice reloading.
                    if(bNoAction && pCX->GetContext()->reSize)  {
                        bNoAction = FALSE;
                        pCX->NiceReload();
                    }
                    //  Check for self destruct.
                    if(bNoAction && pCX->m_bIdleDestroy == TRUE) {
                        bNoAction = FALSE;
                        pCX->NiceDestroyContext();
                    }
                }
            }
        }

        // Take it out of the list and free it since it's now idle.
        if(bNoAction)   {
            //  Removal from list.
            listIdleContexts = pEntry->m_pNext;
            //  Removal from the universe.
            delete pEntry;
        }
    }

    //  Return wether or not more entries exist (more time is needed).
    return(listIdleContexts == NULL ? FALSE : TRUE);
}

//  Function to handle client pull.
class ClientPullTimerData   {
public:
    MWContext *m_pContext;
    URL_Struct *m_pUrl;
    History_entry *m_pVerifyHistory;
    int m_iFormatOut;
    BOOL m_bCanInterrupt;
};
void FEU_ClientPull(MWContext *pContext, uint32 ulMilliseconds, URL_Struct *pUrl, int iFormatOut, BOOL bCanInterrupt)
{
    BOOL bSetToPull = FALSE;

    //  Safety dance.
    if(pUrl && pContext &&
        ABSTRACTCX(pContext) &&
        !ABSTRACTCX(pContext)->IsDestroyed())    {
        CAbstractCX *pCX = ABSTRACTCX(pContext);

        //  If we've already got a client pull timeout for the context, we
        //      should clear it (only want one of these at any given time).
        void *pClearMe = pCX->m_pClientPullTimeout;
        if(pClearMe)  {
            pCX->m_pClientPullTimeout = NULL;
            FE_ClearTimeout(pClearMe);
        }

        //  Same for the timeout's argument pointer, but it can be non-null
        //      even when the timeout pointer was null (if FEU_ClientPullNow
        //      calls us directly to try again in a second, and then returns
        //      early without deleting pData or clearing m_pClientPullData).
        //  Instead of freeing it and then immediately allocating a new one,
        //      just reuse it, but take care further below to clear pCX's
        //      pointer to it before deleting it, if FE_SetTimeout fails.
        ClientPullTimerData *pData =
            (ClientPullTimerData *)pCX->m_pClientPullData;
        if(!pData)  {
            //  Create a new client pull structure.
            pData = new ClientPullTimerData();
        }

        if(pData)   {
            //  Fill in the timer data.
            pData->m_pContext = pCX->GetContext();
            pData->m_pUrl = pUrl;
            pData->m_pVerifyHistory = pCX->GetContext()->hist.cur_doc_ptr;
            pData->m_iFormatOut = iFormatOut;
            pData->m_bCanInterrupt = bCanInterrupt;

            //  Tell the context about it.
            //  Register the timeout.
            pCX->m_pClientPullTimeout =
                FE_SetTimeout(FEU_ClientPullNow, (void *)pData, ulMilliseconds);

            //  Looks like this will work.
            if(pCX->m_pClientPullTimeout)   {
                pCX->m_pClientPullData = (void *)pData;
                bSetToPull = TRUE;
            }
            else    {
                pCX->m_pClientPullData = NULL;
                delete pData;
            }
        }
    }
    
    if(!bSetToPull && pUrl)   {
        //  Can't do client pull on this context.
        NET_FreeURLStruct(pUrl);
    }
}

void FEU_ClientPullNow(void *vpReallyClientPullTimerData)
{
    ClientPullTimerData *pData = (ClientPullTimerData *)vpReallyClientPullTimerData;
    BOOL bPulledUrl = FALSE;

    //  Is the context still around?
    if(XP_IsContextInList(pData->m_pContext) &&
        ABSTRACTCX(pData->m_pContext) &&
        !ABSTRACTCX(pData->m_pContext)->IsDestroyed())  {
        CAbstractCX *pCX = ABSTRACTCX(pData->m_pContext);

        //  There must not be more than one pull timeout pending!
        XP_ASSERT(pData == pCX->m_pClientPullData);

        //  We are called only via FE_SetTimeout, so clear this pointer now
        //      before it dangles at free memory.
        pCX->m_pClientPullTimeout = NULL;

        //  Are we on the same history entry?
        if(pCX->GetContext()->hist.cur_doc_ptr == pData->m_pVerifyHistory)   {
            
            //  Client pull isn't allowed to interrupt.
            if(!pData->m_bCanInterrupt && XP_IsContextBusy(pCX->GetContext()))   {
                //  Try again in a second.
                FEU_ClientPull(pCX->GetContext(), 1000, pData->m_pUrl, pData->m_iFormatOut, FALSE);

                //  Old data, if present, freed in FEU_ClientPull.
                //  We can not continue, as the old data may now be gone,
                //      and we look at the old timer pull data further below.
                return;
            }

            //  Clear the members in the context before load,
            //      as they may be set again instantly and we
            //      get a double free below.
            pCX->m_pClientPullData = NULL;

            //  Ask for it now.
            pCX->GetUrl(pData->m_pUrl, pData->m_iFormatOut);

            //  Set that we've requested it.
            bPulledUrl = TRUE;
        }
        else    {
            //  Clear m_pClientPullData because we're about to free pData.
            pCX->m_pClientPullData = NULL;
        }
    }

    if(!bPulledUrl)    {
        //  Free off the URL struct ourselves.
        NET_FreeURLStruct(pData->m_pUrl);
    }

    //  Free off the timer data.
    delete pData;
}

//  Cause the global history to be saved periodically.
void FEU_GlobalHistoryTimeout(void *pNULL)
{
    //  Set the next timeout.
    FE_SetTimeout(FEU_GlobalHistoryTimeout, NULL, 120000UL);

    //  Flush it, if not first call.
    if(!pNULL)   {
	    GH_SaveGlobalHistory();
    }
}

// This routine will return the handle of the pop-up menu whose first
// menu item has the specified command ID
HMENU FEU_FindSubmenu(HMENU hMenu, UINT nFirstCmdID)
{
	int nCount = ::GetMenuItemCount(hMenu);

	for (int i = 0; i < nCount; i++) {
		HMENU hSubmenu = ::GetSubMenu(hMenu, i);

		if (hSubmenu && (::GetMenuItemID(hSubmenu, 0) == nFirstCmdID))
			return hSubmenu;
	}

	return NULL;
}

char *pTipText = NULL;

// Get a resource string and extract the tooltip portion
//   if a '\n' is found in the string
//   USE RESULT QUICKLY! 
//   (String is in our temp buffer and should not be FREEd)
char * FEU_GetToolTipText( UINT nID )
{
    pTipText = szLoadString(nID);
    if( ! *pTipText ){
        return NULL;
    }

    // Scan for the '\n' separating status line hints from
    //   the tooltip text - use latter if found
    char *pTemp = pTipText;
    do {
        if( *pTemp == '\n' && *(pTemp+1) != '\0' ){
            pTipText = pTemp+1;
            // Find a second "\n" and terminate string there
            char * pBreak = strchr(pTipText, '\n');
            if( pBreak )
                *pBreak = '\0';
            break;
        }
        pTemp++;
    } while( *pTemp != '\0' );

    return pTipText;
}


char *FE_FindFileExt(char * path)
{
    char *pRetval = NULL;
    if(path) {
        int len = strlen(path);

        char *ptr = path + len - 1;
        while ((*ptr != '.') && (len > 0)) {
            len--;
            if (len > 0) ptr--;
        }
        if (len > 0) {
            pRetval = ptr;
        }
    }
    return(pRetval);
}

void FE_LongNameToDosName(char* dest, char* source)
{
    ASSERT(dest && source);
    if(dest) {
        *dest = '\0';
        if(source) {
            char *ext = FE_FindFileExt(source);

            if (ext && (ext - source) > 8) {
                // the Name is > dos file name
                strncpy(dest, source, 8);
                strncpy(&dest[8],ext, 4);
                dest[12] = '\0';
            }
            else {
                strcpy(dest, source);
            }
        }
    }
}
// This function will return FALSE, if we do not want to use the default type that libnet provide.

BOOL  FE_FileType(char * path, 
					char * mimeType, 
					char * encoding)
{

	// try to get the extension from the end of the string.
	char *ext = FE_FindFileExt(path);
	if (ext) {
		ext++; // move pass the '.';

		NET_cdataStruct *cdata;
		XP_List * list_ptr;

		list_ptr = cinfo_MasterListPointer();
		while((cdata = (NET_cdataStruct *) XP_ListNextObject(list_ptr)) != NULL)
		{
			if(cdata->ci.type && !strcasecomp(mimeType, cdata->ci.type)){
				for (int i = 0; i <  cdata->num_exts; i++) {
					if (strcasecomp(ext, cdata->exts[i]) == 0)
						return TRUE;
				}
			}
		}

	}
	return FALSE;
}

BOOL FEU_IsConferenceAvailable(void)
{
	CString install;

#ifdef WIN32
    install = FEU_GetInstallationDirectory(szLoadString(IDS_CONFERENCE_REGISTRY), szLoadString(IDS_PATHNAME));
#else ifdef XP_WIN16
    install = FEU_GetInstallationDirectory(szLoadString(IDS_CONFERENCE),szLoadString(IDS_INSTALL_DIRECTORY));
#endif

	return (!install.IsEmpty());
}

BOOL FEU_IsCalendarAvailable(void)
{

#ifdef _WIN32
	CString calRegistry;

	calRegistry.LoadString(IDS_CALENDAR_REGISTRY);

	calRegistry = FEU_GetCurrentRegistry(calRegistry);
	if(calRegistry.IsEmpty())
		return FALSE;
	CString install = FEU_GetInstallationDirectory(calRegistry, szLoadString(IDS_INSTALL_DIRECTORY));
#else
	
	CString install = FEU_GetInstallationDirectory(szLoadString(IDS_CALENDAR), szLoadString(IDS_INSTALL_DIRECTORY));

#endif

	return (!install.IsEmpty());


}

BOOL FEU_IsIBMHostOnDemandAvailable(void)
{


#ifdef _WIN32
	CString ibmHostRegistry;

	ibmHostRegistry.LoadString(IDS_3270_REGISTRY);

	ibmHostRegistry = FEU_GetCurrentRegistry(ibmHostRegistry);
	if(ibmHostRegistry.IsEmpty())
		return FALSE;

	CString install = FEU_GetInstallationDirectory(ibmHostRegistry, szLoadString(IDS_INSTALL_DIRECTORY));
#else 
	
	CString install = FEU_GetInstallationDirectory(szLoadString(IDS_3270), szLoadString(IDS_INSTALL_DIRECTORY));
	
#endif

	return (!install.IsEmpty());

}

BOOL FEU_IsAimAvailable(void)
{
	BOOL success = FALSE;
	char szPath[_MAX_PATH + 1];
	char shortPath[_MAX_PATH + 1];

	CString install("");
	unsigned long cbData = sizeof(szPath);

#ifdef _WIN32
	CString aimRegistry;

	aimRegistry.LoadString(IDS_AIM_REGISTRY);

	HKEY aimKey;

	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, aimRegistry, 0, KEY_QUERY_VALUE, &aimKey) != ERROR_SUCCESS)
	  return FALSE;

	success = (RegQueryValueEx(aimKey, 0, NULL, NULL, (LPBYTE) szPath, &cbData) == ERROR_SUCCESS);

	GetShortPathName(szPath, shortPath, _MAX_PATH+1);
	install = shortPath;

	RegCloseKey(aimKey);
#endif // _WIN32

#ifdef XP_WIN16
	// Do the right thing
	GetPrivateProfileString("InstallRoot", "Path", "none", szPath, cbData, "aim.ini");
	install = szPath;
	if (install != "none")
	{
		success = TRUE;
		install = install + "\\aim.exe";
	}
#endif // XP_WIN16

	if (success)
	{
		char    szKey[_MAX_EXT + 1];  // space for '.'
		char    szClass[128];
		LONG	lResult;
		LONG	lcb;

		// Look up the file association key which maps a file extension
		// to a file class
		wsprintf(szKey, ".%s", "aim");

		lcb = sizeof(szClass);
		lResult = RegQueryValue(HKEY_CLASSES_ROOT, szKey, szClass, &lcb);
	
#ifdef _WIN32
		ASSERT(lResult != ERROR_MORE_DATA);
#endif
		if (lResult != ERROR_SUCCESS || lcb <= 1)
		{
			// Configure the MIME type, download prompt, etc. etc.
			NET_cdataStruct *launchData = fe_NewFileType("AOL Instant Messenger Launch",
						"aim",
						"application/x-aim",
						install);  // Create and register the file type
			CHelperApp *pHelperApp = (CHelperApp *)launchData->ci.fe_data;
			pHelperApp->how_handle = HANDLE_SHELLEXECUTE;
		}
		else 
		{
			// The key exists but the application may be stale, i.e., it may have changed
			// on us.  Just write out the current path to the aimfile key.
			SetShellOpenCommand("aimfile", install);
		}

// Always write out the defaulticon info for Win32, since the AIM app may have moved.
#ifdef _WIN32
		CString iconPath = install + ",0";
		HKEY iconKey;
		if (RegCreateKey(HKEY_CLASSES_ROOT, "aimfile\\DefaultIcon", &iconKey) ==
					ERROR_SUCCESS)
		{
			RegSetValue(iconKey, "", REG_SZ, iconPath, iconPath.GetLength());
			RegCloseKey(iconKey);
		}
#endif // _WIN32

		CString profileLaunch = theApp.m_UserDirectory + "\\launch.aim";
		FILE* fp = fopen(profileLaunch, "r");
		if (fp == NULL)
		{
			// Need to create launch.aim file in the user's dir.
			fp = fopen(profileLaunch, "w");
			fprintf(fp, "DWH\n");
			fclose(fp);

			// Need to add to personal toolbar
			CString convLaunch;
			WFE_ConvertFile2Url(convLaunch, profileLaunch);
			CString aimName;
			aimName.LoadString(ID_WINDOW_AIM);
			

/* MUST CONVERT AIM BOOKMARK CREATION TO AURORA (Dave H.)
			BM_Entry* newBookmark = BM_NewUrl(aimName, convLaunch, NULL, NULL);
			
			theApp.GetLinkToolbarManager().InitializeToolbarHeader();
			BM_Entry* pRoot = theApp.GetLinkToolbarManager().GetToolbarHeader();


			if (pRoot != NULL)
				BM_PrependChildToHeader(theApp.m_pBmContext, pRoot, newBookmark);
*/
		}
		else fclose(fp);
	}

	return success;
}

void FEU_OpenAim(void)
{
	char szPath[_MAX_PATH + 1];
	char shortPath[_MAX_PATH + 1];

	CString install("none");
	unsigned long cbData = sizeof(szPath);

#ifdef _WIN32
	CString aimRegistry;
	aimRegistry.LoadString(IDS_AIM_REGISTRY);

	HKEY aimKey;

	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, aimRegistry, 0, KEY_QUERY_VALUE, &aimKey) != ERROR_SUCCESS)
	  return;

	if (RegQueryValueEx(aimKey, 0, NULL, NULL, (LPBYTE) szPath, &cbData) == ERROR_SUCCESS) 
	{
		RegCloseKey(aimKey);
#ifdef _WIN32
			GetShortPathName(szPath, shortPath, _MAX_PATH+1);
			install = shortPath;
#else
			install = szPath;
#endif
	}
	else return;

#endif // _WIN32

#ifdef XP_WIN16
	// Do the right thing
	GetPrivateProfileString("InstallRoot", "Path", "none", szPath, cbData, "aim.ini");
	install = szPath;
	if (install == "none")
	   return;
	install = install + "\\aim.exe";
#endif // XP_WIN16

	WinExec(install, SW_SHOW);	
}

void FEU_OpenNetcaster(void)
{
#ifdef JAVA		

 	if(FEU_IsNetcasterAvailable() == FALSE) {
		MessageBox(NULL, XP_GetString(XP_ALERT_CANT_RUN_NETCASTER), szLoadString(AFX_IDS_APP_TITLE), MB_OK | MB_ICONEXCLAMATION); 
 		return;
	}

    if(!theApp.m_pNetcasterWindow) { 
 		Chrome          netcasterChrome;
 		URL_Struct*     URL_s, *splashURL_s;
 		MWContext       *netcasterContext;
		REGERR			regErr = REGERR_FAIL;
		char			netcasterURL[300] = "";
		char			netcasterSplashURL[300] = "";

		// First, get the URL 

    /* Due to a bug using the Windows Registry to detect Netcaster
       installation with both Ratbert and Dogbert installed, we
       need check the XP registry first. If success, use that.  If
       failure (due to a bug), use Windows.  If both are installed
       to unique locations and then one of them deleted, Windows
       will return false, but the XP code should save our butts in
       95% of the cases by returning true.

       BTW, this code needs to be componentized more.  Since today
       is code freeze, I cut and posted code below to preserve existing
       code paths.  This code should be better organized for 4.03.
    */

#ifdef XP_WIN32
		
		regErr = VR_GetPath("Netcaster/tab.htm", 300, netcasterURL);
		VR_Close();

        if (regErr != REGERR_OK) {

            CString netcaster = "Software\\Netscape\\Netcaster\\";

		    netcaster = FEU_GetCurrentRegistry(netcaster);
		    if(netcaster.IsEmpty()) {
			    regErr = 3 ;
		    } else {

			    CString install = FEU_GetInstallationDirectory(netcaster, szLoadString(IDS_INSTALL_DIRECTORY));

			    if(install.IsEmpty()) {
				    regErr = 3 ;
			    } else {

				    
				    strcpy(netcasterURL,install.GetBuffer(300));

				    XP_STRCAT(netcasterURL, "\\tab.htm") ;

				    strcpy(netcasterSplashURL,install.GetBuffer(300));
				    XP_STRCAT(netcasterSplashURL, "\\ncsplash.htm") ;

				    regErr = REGERR_OK ;
			    }

		    }
        } else {

    		REGERR			splashErr;
    
    		splashErr = VR_GetPath("Netcaster/ncsplash.htm", 300, netcasterSplashURL);
	    	VR_Close();

            if (splashErr != REGERR_OK) {

                CString netcaster = "Software\\Netscape\\Netcaster\\";

		        netcaster = FEU_GetCurrentRegistry(netcaster);
		        if(!netcaster.IsEmpty()) {

			        CString install = FEU_GetInstallationDirectory(netcaster, szLoadString(IDS_INSTALL_DIRECTORY));

			        if(!install.IsEmpty()) {
				        
				        strcpy(netcasterSplashURL,install.GetBuffer(300));
				        XP_STRCAT(netcasterSplashURL, "\\ncsplash.htm") ;

			        }

		        }


            }

        }
#else
		regErr = VR_GetPath("Netcaster/tab.htm", 300, netcasterURL);
		VR_Close();

#endif
		

		if (regErr == REGERR_OK) {
            BOOL javaEnabled = FALSE;
#if defined(OJI)
            JVMMgr* jvmMgr = JVM_GetJVMMgr();
            if (jvmMgr) {
                NPIJVMPlugin* jvm = jvmMgr->GetJVM();
                if (jvm) {
                    javaEnabled = jvm->GetJVMEnabled();
                    jvm->Release();
                }
                jvmMgr->Release();
            }
#elif defined(JAVA)
            javaEnabled = LJ_GetJavaEnabled();
#endif
			// Now check to see if Java and JS are enabled
			if (!LM_GetMochaEnabled() || !javaEnabled) {
				MessageBox(NULL, XP_GetString(XP_ALERT_NETCASTER_NO_JS), szLoadString(AFX_IDS_APP_TITLE), MB_OK | MB_ICONEXCLAMATION); 
				return;
			}

			// First, bring up the splash screen, if it exists
#ifdef XP_WIN32

			if (XP_STRLEN(netcasterSplashURL) > 0) {

				MWContext		*newContext;

				memset(&netcasterChrome, 0, sizeof(Chrome));
 				netcasterChrome.type = MWContextBrowser;
 				netcasterChrome.w_hint = 400;
 				netcasterChrome.h_hint = 150;
 				netcasterChrome.l_hint = 100;
 				netcasterChrome.t_hint = 100;
 				netcasterChrome.topmost = FALSE;
 				netcasterChrome.z_lock = FALSE;
 				netcasterChrome.location_is_chrome = TRUE;
 				netcasterChrome.disable_commands = TRUE;
 				netcasterChrome.hide_title_bar = FALSE;
 				netcasterChrome.restricted_target = TRUE;
 				netcasterChrome.allow_close = TRUE;
				netcasterChrome.show_bottom_status_bar = FALSE;

				splashURL_s = NET_CreateURLStruct(netcasterSplashURL, NET_DONT_RELOAD);
 
				newContext = FE_MakeNewWindow(NULL, 
								 splashURL_s, 
 								 "Netcaster_Splash", 
 								 &netcasterChrome);

			}
#endif

 			memset(&netcasterChrome, 0, sizeof(Chrome));

 			netcasterChrome.w_hint = 22;
 			netcasterChrome.h_hint = 55;
 			netcasterChrome.l_hint = -300;
 			netcasterChrome.t_hint = 0;
 			netcasterChrome.topmost = TRUE;
 			netcasterChrome.z_lock = TRUE;
 			netcasterChrome.location_is_chrome = TRUE;
 			netcasterChrome.disable_commands = TRUE;
 			netcasterChrome.hide_title_bar = TRUE;
 			netcasterChrome.restricted_target = TRUE;
 			netcasterChrome.allow_close = TRUE;
 
			URL_s = NET_CreateURLStruct(netcasterURL, NET_DONT_RELOAD);
 
			netcasterContext = FE_MakeNewWindow(NULL, 
												 URL_s, 
 												 "Netcaster_SelectorTab", 
 												 &netcasterChrome);

			theApp.m_pNetcasterWindow = netcasterContext ;


		} else {
			MessageBox(NULL, XP_GetString(XP_ALERT_CANT_RUN_NETCASTER), szLoadString(AFX_IDS_APP_TITLE), MB_OK | MB_ICONEXCLAMATION); 
		} 
	} else {

		if(! ABSTRACTCX(theApp.m_pNetcasterWindow) || ! ABSTRACTCX(theApp.m_pNetcasterWindow)->IsFrameContext() ) {
			MessageBox(NULL, XP_GetString(XP_ALERT_CANT_RUN_NETCASTER), szLoadString(AFX_IDS_APP_TITLE), MB_OK | MB_ICONEXCLAMATION); 
			return;
		} else {
			// We are running, so give us focus...
			//theApp.m_pNetcasterWindow->fe.cx->SetFocus() ;

	        if((theApp.m_pNetcasterWindow->fe.cx->IsFrameContext() == TRUE))  {
	            CWinCX *pWinCX = VOID2CX(theApp.m_pNetcasterWindow->fe.cx, CWinCX);

				if (!pWinCX)
					return ;

				HWND hwnd = pWinCX->GetPane() ;

				if (IsWindow(hwnd))
					SetFocus(hwnd) ;


	        }

		}

	}
#endif /* JAVA */		
}

BOOL FEU_IsNetcasterAvailable(void)
{
#ifdef JAVA 

#ifdef XP_WIN32

    /* Due to a bug using the Windows Registry to detect Netcaster
       installation with both Ratbert and Dogbert installed, we
       need check the XP registry first. If success, use that.  If
       failure (due to a bug), use Windows.  If both are installed
       to unique locations and then one of them was deleted, Windows
       will return false, but the XP code should save our butts in
       95% of the cases by returning true.
    */

	CString netcaster = "Software\\Netscape\\Netcaster\\";

	if (VR_InRegistry("Netcaster") == REGERR_OK) {
		
		// The registry may be invalid since the user can delete
		// the files.  Check to see if one of the files exists.		
        // I believe this checks file existence too....
		int rc = VR_ValidateComponent("Netcaster/tab.htm") ;
		
		if (rc == REGERR_OK) {
            VR_Close();
			return (TRUE) ;
        }
    } 

	VR_Close();

    /*
        begin Windows code check
    */

	netcaster = FEU_GetCurrentRegistry(netcaster);

	if(netcaster.IsEmpty())
		return FALSE;
	CString install = FEU_GetInstallationDirectory(netcaster, szLoadString(IDS_INSTALL_DIRECTORY));

	if (!install.IsEmpty()) {

		// stat the directory first
	   struct _stat buf;
	   int result;

	   /* Get data associated with "stat.c": */
	   result = _stat( install.GetBuffer(256), &buf );

	   /* Check if statistics are valid: */
	   if( result != 0 )
		   return FALSE;

		return (TRUE);
	}
#endif
#endif /* JAVA */
	return (FALSE);

}


void  FE_ConvertSpace(char *newName)
{
	char *p;

	p = newName;
	if (strchr(p, '%')) {
		for(p = newName; *p; p++) {
			if ((*p == '%') && (*(p+1) == 0x32) && (*(p+2) == 0x30)) {
				*p = ' ';
				strcpy(p+1, p+3);
			}
		}
	}
}

#ifdef XP_WIN32
CString FEU_GetCurrentRegistry(const CString &componentString)
{

	HKEY hKey;
	LONG lResult;
	char szPath[_MAX_PATH + 1];

	CString currentVersion, main, currentRegString("");
	currentVersion.LoadString(IDS_CURRENTVERSION);
	main.LoadString(IDS_MAIN);

	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, componentString, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
		unsigned long cbData = sizeof(szPath);
		lResult = RegQueryValueEx(hKey, currentVersion, NULL, NULL, (LPBYTE) szPath, &cbData);

		RegCloseKey(hKey);
		if(lResult == ERROR_SUCCESS)
		{
			currentRegString = componentString + szPath;
			currentRegString = currentRegString + main;
		}
	}
	return currentRegString;
}
#endif

CString FEU_GetInstallationDirectory(const CString &productString, const CString &installationString)
{      
#ifdef _WIN32
  
	
	HKEY hKey;
	LONG lResult;
	char szPath[_MAX_PATH + 1];
	CString install("");

	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, productString, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {

		unsigned long cbData = sizeof(szPath);
		lResult = RegQueryValueEx(hKey, installationString, NULL, NULL, (LPBYTE) szPath, &cbData);

		RegCloseKey(hKey);
		if(lResult == ERROR_SUCCESS)
			install = szPath;
	
	}

	return install;
#else ifdef XP_WIN16
	char szPath[_MAX_PATH + 1];

	const char *pOldProfile = theApp.m_pszProfileName;   
	
	UINT nSize = GetWindowsDirectory(szPath, _MAX_PATH + 1);
	XP_STRCPY(szPath + nSize, szLoadString(IDS_NSCPINI));
	theApp.m_pszProfileName = szPath;      
	
	CString version = theApp.GetProfileString(productString, szLoadString(IDS_CURRENTVERSION), NULL);
	if(version.IsEmpty())
	{	
		theApp.m_pszProfileName = pOldProfile; 
		return version;
	}

	CString currentVersion =  "-" + version; 
	currentVersion = productString + currentVersion;
	
	CString install = theApp.GetProfileString(currentVersion, installationString, NULL);
	
	theApp.m_pszProfileName = pOldProfile; 
        
    return install;
#endif 
}


//  Send/Post a message to all top level frames in our app.
void FEU_FrameBroadcast(BOOL bSend, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    //  Go through list of frames.
    for(CGenericFrame *pFrame = theApp.m_pFrameList;
        pFrame != NULL;
        pFrame = pFrame->m_pNext) {
        if(pFrame->GetSafeHwnd()) {
            if(bSend)   {
                pFrame->SendMessage(Msg, wParam, lParam);
            }
            else    {
                pFrame->PostMessage(Msg, wParam, lParam);
            }
        }
    }

    //  Get the hidden one too.
    if(theApp.m_pMainWnd && theApp.m_pMainWnd->GetSafeHwnd())   {
        if(bSend)   {
            theApp.m_pMainWnd->SendMessage(Msg, wParam, lParam);
        }
        else    {
            theApp.m_pMainWnd->PostMessage(Msg, wParam, lParam);
        }
    }
}

void FEU_AltMail_ShowMailBox(void)
{
	if(theApp.m_bInitMapi) 
	{
		if(theApp.m_fnOpenMailSession) 
		{
			POSTCODE status = (*theApp.m_fnOpenMailSession) (NULL, NULL);
			if(status == POST_OK) 
				theApp.m_bInitMapi = FALSE;
			else
				return;
		}
	}

	if(theApp.m_fnShowMailBox)
		(*theApp.m_fnShowMailBox) ();
}

void FEU_AltMail_ShowMessageCenter(void)
{
	if(theApp.m_bInitMapi) 
	{
		if(theApp.m_fnOpenMailSession) 
		{
			POSTCODE status = (*theApp.m_fnOpenMailSession) (NULL, NULL);
			if(status == POST_OK) 
				theApp.m_bInitMapi = FALSE;
			else
				return;
		}
	}

	if(theApp.m_fnShowMessageCenter)
		(*theApp.m_fnShowMessageCenter) ();
}

//Modify communicators menu with alt mail specified menu
//Remove menu item if pszAltMailMenuStr is empty
void FEU_AltMail_ModifyMenu(CMenu* pMenuCommunicator, UINT uiCommunicatorMenuID, LPCSTR pszAltMailMenuStr)
{
	if(*pszAltMailMenuStr)
	{
		CString csNewMenuStr;
		#ifdef XP_WIN32
			if(pMenuCommunicator->GetMenuString(uiCommunicatorMenuID, csNewMenuStr, MF_BYCOMMAND))
		#else
			if(pMenuCommunicator->GetMenuString(uiCommunicatorMenuID, csNewMenuStr.GetBuffer(256), 256, MF_BYCOMMAND))
		#endif
		{
			#ifdef XP_WIN16
				csNewMenuStr.ReleaseBuffer();
			#endif

			int iPos;
			if((iPos = csNewMenuStr.Find('\t')) != -1)
			{
				//Accelerator present. Add it to the end of AltMail menu string
				CString csAccel = csNewMenuStr.Right(csNewMenuStr.GetLength() - iPos);
				csNewMenuStr = pszAltMailMenuStr + csAccel;
			}
			else //No accelerator. use alt mail str as is
				csNewMenuStr = pszAltMailMenuStr;

			pMenuCommunicator->ModifyMenu(uiCommunicatorMenuID, MF_BYCOMMAND, uiCommunicatorMenuID, csNewMenuStr);
		}
	}
	else
		pMenuCommunicator->RemoveMenu(uiCommunicatorMenuID, MF_BYCOMMAND);
}

//Modify "Messenger MailBox" and "Message Center" menu items
//to strings supplied by the AltMail DLL
void FEU_AltMail_SetAltMailMenus(CMenu* pMenuCommunicator)
{
	char szAltMailMenuStr[_MAX_PATH];

	szAltMailMenuStr[0] = '\0';
	if(POST_OK == (*theApp.m_fnGetMenuItemString)(ALTMAIL_MenuMailBox, szAltMailMenuStr, _MAX_PATH))
		FEU_AltMail_ModifyMenu(pMenuCommunicator, ID_TOOLS_INBOX, szAltMailMenuStr);

	szAltMailMenuStr[0] = '\0';
	if(POST_OK == (*theApp.m_fnGetMenuItemString)(ALTMAIL_MenuMessageCenter, szAltMailMenuStr, _MAX_PATH))
		FEU_AltMail_ModifyMenu(pMenuCommunicator, ID_TOOLS_MAIL, szAltMailMenuStr);
}

BOOL FEU_Execute(MWContext *pContext, const char *pCommand, const char *pParams)
{
    BOOL bRetval = TRUE;
    BOOL bHandled = FALSE;
    HINSTANCE hSpawn = (HINSTANCE)2; // 2 is application not found.

    if(bHandled == FALSE) {
        hSpawn = ShellExecute(NULL, NULL, pCommand, pParams, NULL, SW_SHOW);
        if((int)hSpawn == 2 && pCommand && pParams) {
            //  Failure, Application not found.
            //  Some legacy helpers (netscape viewers section of preferences)
            //      specify switches in the pCommand such that ShellExecute
            //      fails.  Re-attempt with WinExec to see if it works
            //      correctly (CyberCash, Norton Anti-Virus).
            //  Both command and params are specified in this scenario.
            CString csCommand = CString(pCommand) + " " + CString(pParams);
            hSpawn = (HINSTANCE)WinExec(csCommand, SW_SHOW);
        }
        bHandled = TRUE;
    }

    if((int)hSpawn < 32)	{
        bRetval = FALSE;

        char szMsg[80];
        switch((int)hSpawn) {
        case 0:
        case 8:
            sprintf(szMsg, szLoadString(IDS_WINEXEC_0_8));
            break;
        case 2:                                      
        case 3:
            sprintf(szMsg, szLoadString(IDS_WINEXEC_2_3));
            break;
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
            sprintf(szMsg, szLoadString(IDS_WINEXEC_10_THRU_15));
            break;
        case 16:
            sprintf(szMsg, szLoadString(IDS_WINEXEC_16));
            break;
        case 21:
            sprintf(szMsg, szLoadString(IDS_WINEXEC_21));
            break;
        default:
            sprintf(szMsg, szLoadString(IDS_WINEXEC_XX), (UINT)hSpawn);
            break;
        }        

        FE_Alert(pContext, szMsg);
    }

    return(bRetval);
}


/*///////////////////////////////////////////////////////////////////////
// Find the executable which will handle the filename.                 //
// Retval BOOL:    Wether or not an executable was found.              //
// pFileName:      The content to be executed.                         //
// pExecutable:    Buffer to hold the name of executable if found.     //
// bIdentity:      Wether or not the pFileName can be the pExecutable. //
// bExtension:     Whether or not pFileName is actually an extension.  //
///////////////////////////////////////////////////////////////////////*/
BOOL FEU_FindExecutable(const char *pFileName, char *pExecutable, BOOL bIdentity, BOOL bExtension) {
    BOOL bRetval = FALSE;
    BOOL bFound = FALSE;
    BOOL bFreeFileName = FALSE;
    
    char aViewer[_MAX_PATH];
    memset(aViewer, 0, sizeof(aViewer));

    //  pFileName may not be a file name, but an extension.
    //  We want to support just extensions for ease, so check on it.
    if(bExtension && pFileName) {
        //  Do we need to add a period?
        char aExt[_MAX_EXT];
        if(*pFileName != '.')   {
            aExt[0] = '.';
            aExt[1] = '\0';
            strcat(aExt, pFileName);
            pFileName = aExt;
        }

        //  Fill out the rest of the name.
        bFreeFileName = TRUE;
        pFileName = (const char *)WH_TempFileName(xpTemporary, "G", pFileName);
    }

    if(!bFound && pFileName && *pFileName) {
        //   Does pFileName actually exists, if not we will need to 
        //      create it temporarily so that ::FindExecutable will work.
        BOOL bCreatedFile = FALSE;
        if(_access(pFileName, 00) != 0) {
            HFILE hCreate = _lcreat(pFileName, 0);
            if(hCreate != HFILE_ERROR) {
                bCreatedFile = TRUE;
                _lclose(hCreate);
            }
        }

        HINSTANCE hRet = FindExecutable(pFileName, NULL, aViewer);
        if((int)hRet > 32) {
            //  Found a viewer.
            bFound = TRUE;
        }
        
        /* Cleanup */
        if(bCreatedFile) {
            _unlink(pFileName);
        }
    }

    //  Do we fill in the return value?
    //  Note that we fill it in regardless of the bIdentity flag.
    if(pExecutable)  {
        strcpy(pExecutable, aViewer);
    }

    //  Decide return value based on bIdentity if an EXE found.
    if(bFound)   {
        if(bIdentity) {
            bRetval = TRUE;
        }
        else {
            //  Must make sure executable and filename are not the
            //      same.
            //  Do a case insensitive comparison.
            char *pCompareFile = (char *)pFileName;
            char *pCompareExe = (char *)aViewer;
            
    #ifdef XP_WIN32
            //  On win32, we compare short names to avoid any confusion.
            char aShortFile[_MAX_PATH];
            char aShortExe[_MAX_PATH];
            
            memset(aShortFile, 0, sizeof(aShortFile));
            memset(aShortExe, 0, sizeof(aShortExe));
            
            GetShortPathName(pCompareFile, aShortFile, sizeof(aShortFile));
            GetShortPathName(pCompareExe, aShortExe, sizeof(aShortFile));
            
            pCompareFile = aShortFile;
            pCompareExe = aShortExe;
    #endif

            //  Case insensitive comparison.
            if(stricmp(pCompareFile, pCompareExe))   {
                //  Not the same.
                bRetval = TRUE;
            }
        }
    }
    
    if(bFreeFileName && pFileName) {
        XP_FREE((void *)pFileName);
        pFileName = NULL;
    }

    return(bRetval);
}

// Return TRUE if the given path points to an existing
//   directory.  Attempt to create the directory if we can
BOOL FEU_SanityCheckDir(const char * dir)
{
    int ret;
    XP_StatStruct statinfo; 

    ret = _stat((char *) dir, &statinfo);
    if(ret == -1) {

        // see if we can just create it
#ifdef __WATCOMC__
        ret = mkdir(dir);
#else
        ret = _mkdir(dir);
#endif
    
        // still couldn't create it
        if(ret == -1)
            return(FALSE);

    }

    return(TRUE);
    
}

// Return TRUE if the given file has a valid path           
BOOL FEU_SanityCheckFile(const char * file)
{
    int ret;
    XP_StatStruct statinfo; 

    ret = _stat((char *) file, &statinfo);
    if(ret == -1)
        return(FALSE);
    else
        return(TRUE);
    
}

void FEU_DeleteUrlData(URL_Struct *pUrl, MWContext *pCX)	{
	//	A Url is about to be removed.
	//	Clean up any extra stuff that we know about but Netlib doesn't.
	if(pUrl->ncapi_data != NULL)	{
		CNcapiUrlData *pUrlData = (CNcapiUrlData *)pUrl->ncapi_data;
		delete pUrlData;
	}
}



BOOL FEU_IsNetscapeFrame(CWnd* pFocusWnd) 
{
	if (pFocusWnd->IsKindOf(RUNTIME_CLASS(CGenericView))) 
		return TRUE;
	else if (pFocusWnd->IsKindOf(RUNTIME_CLASS(CGenericFrame)))
		return TRUE;
#ifdef MOZ_MAIL_NEWS      
	else if (pFocusWnd->IsKindOf(RUNTIME_CLASS(CMsgListFrame)))
		return TRUE;
#endif /* MOZ_MAIL_NEWS */      
//	else if (pFocusWnd->IsKindOf(RUNTIME_CLASS(CNSToolbar2)))
//		return TRUE;
	else return FALSE;

}

BOOL CALLBACK WinEnumProc(HWND hwnd, LPARAM lParam)
{
 // check to see if the Windows title is "Netscape's Hidden Frame"
 char winText[256];
 if (GetWindowText(hwnd, winText, sizeof(winText)) != 0)
 {
  if (lstrcmpi(winText, "Netscape's Hidden Frame") == 0)
  {
   *((HWND*)lParam) = hwnd;
   return(FALSE);
  }
 }
 return(TRUE);
}

extern "C"
HWND FindNavigatorHiddenWindow(void)
{
 // find Navigator's hidden frame
 HWND hWnd = NULL;
 EnumWindows(WinEnumProc, (LPARAM)&hWnd);
 return(hWnd);
}


//Used to determine initialization action.  If we detect a previous instance
//and it is in the middle of initializing, find out how far along it is by
//looking for it's hidden frame.  If the previous instance has a hidden frame
//then we return TRUE and alow the second instance to DDE to the first.  If it
//doesn't have a hidden frame, we wait for 10 seconds in the case that the first
//and the second instance started at nearly the same time. Thus, giving the first 
//instance a chance to create it's hidden frame and allowing the second instance
//to open a successful DDE conversation to the first instance.
//This function should be called from InitInstance prior to the Initialization of 
//the NetscapeSlaveClass.  This call is only need for win32.  This same call
//will be made for win16 but from the NAVSTART.EXE application.  
#ifdef XP_WIN32
BOOL BailOrStay(void)
{
	if (FindWindow("NetscapeSlaveClass",NULL))
	{
		if (!FindNavigatorHiddenWindow())
		{
			Sleep(10000);//give the first instance a chance to start
			if(!FindNavigatorHiddenWindow()) //If it's not present yet we just exit.
			{                                //First instance is probably running the profile manager.
				return FALSE;
			}
		}			
	}
	return TRUE;
}
#endif

//  Do not change the current drive to a floppy.
BOOL FEU_GetSaveFileName(OPENFILENAME *pOFN)
{
    //  Check to see if the dialog will ignore directory changes.
    BOOL bCheckFloppy = (pOFN && !(pOFN->Flags & OFN_NOCHANGEDIR));
    char aOriginalPath[MAX_PATH];
    if(bCheckFloppy) {
        //  Make sure we're not already on a floppy.
        if(FEU_GetCurrentDriveType() == DRIVE_REMOVABLE) {
            //  We don't care what they do.
            bCheckFloppy = FALSE;
        }
        else {
            //  Save current directory.
            DWORD dwCopy = ::GetCurrentDirectory(sizeof(aOriginalPath), aOriginalPath);
            if(0 == dwCopy || sizeof(aOriginalPath) < dwCopy) {
                //  On failure, switch to not care.
                bCheckFloppy = FALSE;
            }
        }
    }

    BOOL bRetval = ::GetSaveFileName(pOFN);

    //  Are we to check if we are on a floppy drive?
    if(bCheckFloppy) {
        BOOL bFloppy = (FEU_GetCurrentDriveType() == DRIVE_REMOVABLE);
        if(bFloppy) {
            //  Go back to a non-floppy location.
            //  Don't care on failure.
            ::SetCurrentDirectory(aOriginalPath);
        }
    }

    return(bRetval);
}

//  Do not change the current drive to a floppy.
BOOL FEU_GetOpenFileName(OPENFILENAME *pOFN)
{
    //  Check to see if the dialog will ignore directory changes.
    BOOL bCheckFloppy = (pOFN && !(pOFN->Flags & OFN_NOCHANGEDIR));
    char aOriginalPath[MAX_PATH];
    if(bCheckFloppy) {
        //  Make sure we're not already on a floppy.
        if(FEU_GetCurrentDriveType() == DRIVE_REMOVABLE) {
            //  We don't care what they do.
            bCheckFloppy = FALSE;
        }
        else {
            //  Save current directory.
            DWORD dwCopy = ::GetCurrentDirectory(sizeof(aOriginalPath), aOriginalPath);
            if(0 == dwCopy || sizeof(aOriginalPath) < dwCopy) {
                //  On failure, switch to not care.
                bCheckFloppy = FALSE;
            }
        }
    }

    BOOL bRetval = ::GetOpenFileName(pOFN);

    //  Are we to check if we are on a floppy drive?
    if(bCheckFloppy) {
        BOOL bFloppy = (FEU_GetCurrentDriveType() == DRIVE_REMOVABLE);
        if(bFloppy) {
            //  Go back to a non-floppy location.
            //  Don't care on failure.
            ::SetCurrentDirectory(aOriginalPath);
        }
    }

    return(bRetval);
}

//  Return the current Drive's type
UINT FEU_GetCurrentDriveType(void)
{
#ifdef XP_WIN16
    return(::GetDriveType(::_getdrive() - 1));
#else
    return(::GetDriveType(NULL));
#endif
}

#ifdef XP_WIN16
DWORD GetCurrentDirectory(DWORD nBufferLength, LPSTR lpBuffer)
{
    DWORD dwRetval = nBufferLength + MAX_PATH;
    char *pRetval = NULL;
    if(lpBuffer) {
        pRetval = _getcwd(lpBuffer, nBufferLength);
    }
    if(pRetval) {
        dwRetval = XP_STRLEN(lpBuffer);
    }
    return(dwRetval);
}

BOOL SetCurrentDirectory(LPCSTR lpPathName)
{
    BOOL bRetval = FALSE;
    if(lpPathName) {
        if(_chdir(lpPathName) == 0) {
            //  Must also change drive in 16 bits.
            char aDrive[_MAX_DRIVE];
            aDrive[0] = '\0';
            _splitpath(lpPathName, aDrive, NULL, NULL, NULL);
            if(aDrive[0]) {
                //  A drive, convert to numeral.
                aDrive[0] = toupper(aDrive[0]);
                if(_chdrive(aDrive[0] - 'A' + 1) == 0) {
                    bRetval = TRUE;
                }
            }
            else {
                //  No drive, then a success.
                bRetval = TRUE;
            }
        }
    }
    return(bRetval);
}
#endif

//  Determine the top most window below the given point.
//  No guarantees that the window belongs to our process, or what
//      type of window it is.
//  Invisible windows are not returned.
//  Disabled windows are not returned.
//
//  hWndTop same as GetTopWindow SDK function.
//  Point is in Screen coordinates.
HWND FEU_GetWindowFromPoint(HWND hWndTop, POINT *pPoint)
{
    HWND hRetval = NULL;
    if(pPoint) {
        HWND hTraverse = GetTopWindow(hWndTop);
        if(hTraverse) {
            RECT rWindow;
            do {
                memset(&rWindow, 0, sizeof(rWindow));
                GetWindowRect(hTraverse, &rWindow);
                if(PtInRect(&rWindow, *pPoint)) {
                    if(!IsWindowVisible(hTraverse)) {
                        //  Skip this one.
                        continue;
                    }
                    if(!IsWindowEnabled(hTraverse)) {
                        //  Window is visible, but disabled.
                        //  Have to return NULL.
                        break;
                    }
                    hRetval = hTraverse;
                    break;
                }
            } while(hTraverse = GetNextWindow(hTraverse, GW_HWNDNEXT));
        }
    }
    return(hRetval);
}

CNSGenFrame *FEU_GetDockingFrameFromPoint(POINT *pPoint)
{
    CNSGenFrame *pRetval = NULL;
    HWND hAttempt = FEU_GetWindowFromPoint(NULL, pPoint);
    if(hAttempt) {
        CWnd *pPerm = CWnd::FromHandlePermanent(hAttempt);
        if(pPerm && pPerm->IsKindOf(RUNTIME_CLASS(CNSGenFrame))) {
            CNSGenFrame *pDockable = (CNSGenFrame *)pPerm;
            if(pDockable->AllowDocking()) {
                pRetval = pDockable;
            }
        }
    }
    return(pRetval);
}

extern "C" PRBool FE_IsNetscapeDefault(void) {
	return PR_TRUE;
}

extern "C" PRBool FE_MakeNetscapeDefault(void) {
	return PR_TRUE;
}

//  Use this to create some COM objects, as we need to switch to the
//      directory where the .EXE sits if the current directory isn't
//      the same, as the pref COM DLLs have relative paths in the
//      registry.
HRESULT FEU_CoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID *ppv)
{
    HRESULT hRetval = NULL;

    char aOrigDir[MAX_PATH + 1];
    DWORD dwCheck = GetCurrentDirectory(sizeof(aOrigDir), aOrigDir);
    ASSERT(dwCheck);

    char aProgramDir[MAX_PATH + 1];
    FE_GetProgramDirectory(aProgramDir, sizeof(aProgramDir));

    BOOL bCheck = SetCurrentDirectory(aProgramDir);
    ASSERT(bCheck);

    hRetval = CoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);

    BOOL bRestoreCheck = SetCurrentDirectory(aOrigDir);
    ASSERT(bRestoreCheck);

    return(hRetval);
}

