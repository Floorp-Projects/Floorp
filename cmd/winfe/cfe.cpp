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
#include "msgcom.h"

#include "cxabstra.h"

#include "xp_thrmo.h"
#include "libi18n.h"	// International function 
#include "template.h"
#include "mainfrm.h"
#include "timer.h"
#include "feselect.h"
#ifdef MOZ_MAIL_NEWS
#include "compfrm.h"
#endif /* MOZ_MAIL_NEWS */
#include "prefapi.h"
#include "fmabstra.h"
extern "C" {
#include "httpurl.h"
}

#ifdef DEBUG_WHITEBOX
#include "qa.h"
#define IS_MAIL_READER(pMWContext)          (pMWContext->type == MWContextMail)
#endif

#define IS_MESSAGE_COMPOSE(pMWContext)      (pMWContext->type == MWContextMessageComposition)

/* implemented in fenet.cpp */
void FE_DeleteDNSList(MWContext * currentContext);

//  All common front end functions.
//  Those needing specific optimizations, should do so here depending upon
//      the type of context.
//  As you will notice below, all functions call a virtual member of the abstract class.
//      These map to the appropriate class implementations at run time.
//  05-01-95    created GAB

void CFE_Alert(MWContext *pContext, const char *pMessage)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: Alert Blocking\n", pContext);
		return;
	}
    else if(pMessage)   {
	    char *pWinMessage = FE_Windowsify(pMessage);
        if(pWinMessage) {
	        //	If the context has ncapi data, have it pass off this information to
	        //		external applications first, and see if we pass it on from here.
	        BOOL bWeAlert = TRUE;
	        if(ABSTRACTCX(pContext)->m_pNcapiUrlData != NULL)	{
		        if(ABSTRACTCX(pContext)->m_pNcapiUrlData->Alert(pWinMessage) != 0)	{
			        bWeAlert = FALSE;
		        }
	        }

	        if(bWeAlert == TRUE)	{
    	        ABSTRACTCX(pContext)->Alert(pContext, pWinMessage);
	        }
	        XP_FREE(pWinMessage);
        }
    }
}

void CFE_AllConnectionsComplete(MWContext *pContext)	{

	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: AllConnectionsComplete Blocking\n", pContext);
		return;
	}

#ifdef MOZ_MAIL_NEWS
    if (IS_MESSAGE_COMPOSE(pContext)) {
		MSG_Pane *pPane = MSG_FindPane( pContext, MSG_COMPOSITIONPANE );
		ASSERT( pPane );
        MSG_MailCompositionAllConnectionsComplete ( pPane );
	}

	if (NET_IsOffline()) {
		FE_Progress(pContext, szLoadString(IDS_STATUS_OFFLINE));
	} else 
#endif   
    {
		//	Set the progress to be complete.
		FE_Progress(pContext, szLoadString(IDS_DOC_DONE));
	}

    ABSTRACTCX(pContext)->AllConnectionsComplete(pContext);
	FE_DeleteDNSList(pContext);

#ifdef EDITOR
#ifdef XP_WIN16    
    if( EDT_IS_EDITOR(pContext) ){
        // For some bizzare reason, we don't get proper focus 
        //   when starting an Edit frame+View in Win16
        // This fixes that
        ::SetFocus(PANECX(pContext)->GetPane());
    }
#endif
#endif

#ifdef DEBUG_WHITEBOX
	// AfxMessageBox("cfe.cpp: Try Again?");
	if (IS_MAIL_READER(pContext)) {
		if (QATestCaseStarted == FALSE) {
			QADoDeleteMessageEventHandler();
		}
	}
#endif
}

void CFE_UpdateStopState(MWContext *pContext)	{
	TRACE("CFE_UpdateStopState(%p)\n", pContext);

	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: UpdateStopState Blocking\n", pContext);
		return;
	}
    ABSTRACTCX(pContext)->UpdateStopState(pContext);
#ifdef EDITOR
#ifdef XP_WIN16    
    if( EDT_IS_EDITOR(pContext) ){
        // For some bizzare reason, we don't get proper focus 
        //   when starting an Edit frame+View in Win16
        // This fixes that
        ::SetFocus(PANECX(pContext)->GetPane());
    }
#endif
#endif
}

void CFE_BeginPreSection(MWContext *pContext)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: BeginPreSection Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->BeginPreSection(pContext);
}

void CFE_SetCallNetlibAllTheTime(MWContext *pContext)
{
	XP_ASSERT(0);  // depricated
}
void XP_SetCallNetlibAllTheTime(MWContext *pContext)
{
	XP_ASSERT(0);  // depricated
}

void CFE_ClearCallNetlibAllTheTime(MWContext *pContext)
{
	XP_ASSERT(0);  // depricated
}
void XP_ClearCallNetlibAllTheTime(MWContext *pContext)
{
	XP_ASSERT(0);  // depricated
}

void CFE_ClearView(MWContext *pContext, int iView)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: ClearView Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->ClearView(pContext, iView);
}

XP_Bool CFE_Confirm(MWContext *pContext, const char *pConfirmMessage)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: Confirm Blocking\n", pContext);
		return(FALSE);
	}

	char *pWinConfirmMessage = FE_Windowsify(pConfirmMessage);

	//	If the context has ncapi data, have it pass off this information to
	//		external applications first, and see if we pass it on from here.
	BOOL bWeConfirm = TRUE;
	XP_Bool bReturn;
	if(ABSTRACTCX(pContext)->m_pNcapiUrlData != NULL)	{
		bReturn = ABSTRACTCX(pContext)->m_pNcapiUrlData->Confirm(pWinConfirmMessage);
		if(bReturn == TRUE || bReturn == FALSE)	{
			bWeConfirm = FALSE;
		}
	}

	if(bWeConfirm == TRUE)	{
    	bReturn = ABSTRACTCX(pContext)->Confirm(pContext, pWinConfirmMessage);
	}

	XP_FREE(pWinConfirmMessage);
	return(bReturn);
}

MWContext *CFE_CreateNewDocWindow(MWContext *pContext, URL_Struct *pURL)	{
	if(pContext != NULL)	{
		if(ABSTRACTCX(pContext)->IsDestroyed())	{
			TRACE("Context %p Destroyed :: CreateNewDocWindow Blocking\n", pContext);
			//	Don't allow this to happen if the context has been destroyed...
			return(NULL);
		}

		MWContext *pRetval = ABSTRACTCX(pContext)->CreateNewDocWindow(pContext, pURL);
		if(pRetval != NULL)	{
			return(pRetval);
		}
	}

	//	Regardless of the type of context we currently are, we are going to
	//		create a new CMainFrame.
	//	The contexts can do whatever they want in the derived class, but this is
	//		the base implementation.  If they return a context, then we won't do this.

	//	Cause a frame to open.
	if(NULL == theApp.m_ViewTmplate->OpenDocumentFile(NULL))	{
		return(NULL);
	}

	//	The new frame will be the last one in the application's frame list.
	CMainFrame *pFrame;
	CGenericFrame *pGenFrame;
	for(pGenFrame = theApp.m_pFrameList; pGenFrame->m_pNext; pGenFrame = pGenFrame->m_pNext) {
		/* No Body */;
	}
    
    pFrame = (CMainFrame *) pGenFrame;
	MWContext *pNewContext = pFrame->GetMainContext()->GetContext();

	//	Appropriate assignment of options/prefs can only happen if we are also
	//		owned by a CMainFrame, check.
	if(pContext != NULL) {
		if (ABSTRACTCX(pContext)->IsFrameContext() && pContext->type == MWContextBrowser)	{
		//	Assign over our options.

		//	Toolbars...
			LPNSTOOLBAR pIToolBar = NULL;
			pFrame->GetChrome()->QueryInterface( IID_INSToolBar, (LPVOID *) &pIToolBar );
			if ( pIToolBar ) {
				pIToolBar->Release();
			}

			//	Directory buttons and location box and Default Encoding.
			pFrame->GetChrome()->ShowToolbar(ID_LOCATION_TOOLBAR, pFrame->m_bLocationBar );
			pFrame->m_iCSID = INTL_DefaultDocCharSetID(pContext);
		}

		//	Set the miscellaneous XP context properties.
		pNewContext->fancyFTP = pContext->fancyFTP;	
		pNewContext->fancyNews = pContext->fancyNews;
	}

    //  If there was no URL specified to load, load the homepage.
    //  This is a change in behavior from 4.0 and prior browsers.
    //  This allows the pref (blank, home, last visited) to have more
    //      meaning and gives user better customization.
    if(pURL == NULL)	{
        pFrame->OnLoadHomePage();
    }
    else	{
        //	Load the URL passed into this function.
        ABSTRACTCX(pNewContext)->GetUrl(pURL, FO_CACHE_AND_PRESENT);
    }

    //	New frame window up, and filled out appropriately.
	return(pNewContext);
}

#ifdef EDITOR
MWContext *FE_CreateNewEditWindow(MWContext *pContext, URL_Struct *pURL)	{
	if(pContext != NULL)	{
		if(ABSTRACTCX(pContext)->IsDestroyed())	{
			TRACE("Context %p Destroyed :: CreateNewDocWindow Blocking\n", pContext);
			//	Don't allow this to happen if the context has been destroyed...
			return(NULL);
		}

		MWContext *pRetval = ABSTRACTCX(pContext)->CreateNewDocWindow(pContext, pURL);
		if(pRetval != NULL)	{
			return(pRetval);
		}
	}

	//	Regardless of the type of context we currently are, we are going to
	//		create a new CMainFrame.
	//	The contexts can do whatever they want in the derived class, but this is
	//		the base implementation.  If they return a context, then we won't do this.

	//	Cause a frame to open.
	if(NULL == theApp.m_EditTmplate->OpenDocumentFile(NULL))	{
		return(NULL);
	}

	//	The new frame will be the last one in the application's frame list.
	CMainFrame *pFrame;
	CGenericFrame *pGenFrame;
	for(pGenFrame = theApp.m_pFrameList; pGenFrame->m_pNext; pGenFrame = pGenFrame->m_pNext) {
		/* No Body */;
	}
    
    pFrame = (CMainFrame *) pGenFrame;
	MWContext *pNewContext = pFrame->GetMainContext()->GetContext();

	//	Appropriate assignment of options/prefs can only happen if we are also
	//		owned by a CMainFrame, check.
	if(pContext != NULL && ABSTRACTCX(pContext)->IsFrameContext() && 
       pContext->type == MWContextBrowser){
        pFrame->m_iCSID = INTL_DefaultDocCharSetID(pContext);
	}

	//	Set the miscellaneous XP context properties.
	if(pContext != NULL)	{
		pNewContext->fancyFTP = pContext->fancyFTP;	
		pNewContext->fancyNews = pContext->fancyNews;

		//	Copy the session history over.
		SHIST_CopySession(pNewContext, pContext);
	}

	//	If there was no URL specified to load, load what's in the history.
	//	Only take URLs from a browser window.
	if(pURL == NULL)	{
		//	Load the oldest thing in its history (most likely the home page).
		XP_List *pOldest = SHIST_GetList(pNewContext);
		History_entry *pEntry = (History_entry *)XP_ListNextObject(pOldest);
		if(pEntry == NULL)	{
			//	Nothing to load, we're done.
			return(pNewContext);
		}

		//	don't load from non browser windows.
		if(pContext == NULL || pContext->type == MWContextBrowser || pContext->type == MWContextPane)	{
            URL_Struct *pUrl = SHIST_CreateURLStructFromHistoryEntry(pContext ? pContext : pNewContext, pEntry);
			if(pUrl == NULL)	{
				//	Nothing to load?  we're done.
				return(pNewContext);
			}

			//	Set the current session history for the new context.
			SHIST_SetCurrent(&(pNewContext->hist), 0);

			//	finally Load it.
			ABSTRACTCX(pNewContext)->GetUrl(pUrl, FO_CACHE_AND_EDIT);
		}
	}
	else if(pURL != NULL)	{
		//	Load the URL passed into this function.
		ABSTRACTCX(pNewContext)->GetUrl(pURL, FO_CACHE_AND_EDIT);
	}

	//	New frame window up, and filled out appropriately.
	return(pNewContext);
}

#endif	//EDITOR

void CFE_DisplayBullet(MWContext *pContext, int iLocation, LO_BullettStruct *pBullet)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: DisplayBullet Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->DisplayBullet(pContext, iLocation, pBullet);
}

#ifndef MOZ_NGLAYOUT
void CFE_DisplayEdge(MWContext *pContext, int iLocation, LO_EdgeStruct *pEdge)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: DisplayEdge Blocking\n", pContext);
		return;
	}

	ABSTRACTCX(pContext)->DisplayEdge(pContext, iLocation, pEdge);
}

void CFE_DisplayEmbed(MWContext *pContext, int iLocation, LO_EmbedStruct *pEmbed)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: DisplayEmbed Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->DisplayEmbed(pContext, iLocation, pEmbed);
}

void CFE_DisplayFormElement(MWContext *pContext, int iLocation, LO_FormElementStruct *pFormElement)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: DisplayFormElement Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->DisplayFormElement(pContext, iLocation, pFormElement);
}
#endif

void CFE_DisplayBorder(MWContext *pContext, int iLocation, int x, int y, int width, int height, int bw, LO_Color *color, LO_LineStyle style)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: DisplayBorder Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->DisplayBorder(pContext, iLocation, x, y, width, height, bw, color, style);
}

void CFE_DisplayFeedback(MWContext *pContext, int iLocation, LO_Element *pElement)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: DisplayFeedback Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->DisplayFeedback(pContext, iLocation, pElement);
}

void CFE_DisplayHR(MWContext *pContext, int iLocation, LO_HorizRuleStruct *pHorizRule)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		TRACE("Context %p Destroyed :: DisplayHR Blocking\n", pContext);
		//	Don't allow this to happen if the context has been destroyed...
		return;
	}

    ABSTRACTCX(pContext)->DisplayHR(pContext, iLocation, pHorizRule);
}
void CFE_DisplayJavaApp(MWContext *pContext, int iLocation, LO_JavaAppStruct *pJava)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		TRACE("Context %p Destroyed :: DisplayJavaApp Blocking\n", pContext);
		return;
	}

	ABSTRACTCX(pContext)->DisplayJavaApp(pContext, iLocation, pJava);
}

void CFE_DisplayLineFeed(MWContext *pContext, int iLocation, LO_LinefeedStruct *pLineFeed, XP_Bool iClear)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: DisplayLineFeed Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->DisplayLineFeed(pContext, iLocation, pLineFeed, iClear);
}

void CFE_DisplaySubDoc(MWContext *pContext, int iLocation, LO_SubDocStruct *pSubDoc)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: DisplaySubDoc Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->DisplaySubDoc(pContext, iLocation, pSubDoc);
}

void CFE_DisplayCell(MWContext *pContext, int iLocation, LO_CellStruct *pCell)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: DisplayCell Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->DisplayCell(pContext, iLocation, pCell);
}
void CFE_DisplaySubtext(MWContext *pContext, int iLocation, LO_TextStruct *pText, int32 lStartPos, int32 lEndPos, XP_Bool iClear)    {
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: DisplaySubtext Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->DisplaySubtext(pContext, iLocation, pText, lStartPos, lEndPos, iClear);
}

void CFE_DisplayTable(MWContext *pContext, int iLocation, LO_TableStruct *pTable)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: DisplayTable Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->DisplayTable(pContext, iLocation, pTable);
}

void CFE_DisplayText(MWContext *pContext, int iLocation, LO_TextStruct *pText, XP_Bool iClear)   {
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: DisplayText Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->DisplayText(pContext, iLocation, pText, iClear);
}

void CFE_EnableClicking(MWContext *pContext)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: EnableClicking Blocking\n", pContext);
		return;
	}

	//	Enable clicking now.
    if(pContext->waitingMode)   {
	    pContext->waitingMode = FALSE;

        ABSTRACTCX(pContext)->EnableClicking(pContext);
    }
}

void CFE_EndPreSection(MWContext *pContext)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: EndPreSection Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->EndPreSection(pContext);
}

#ifdef LAYERS
void CFE_EraseBackground(MWContext *pContext, int iLocation, int32 x, int32 y, uint32 width, uint32 height, LO_Color *pColor)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: EraseBackground Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->EraseBackground(pContext, iLocation, x, y, width, height, pColor);
}
void CFE_SetDrawable(MWContext *pContext, CL_Drawable *drawable)
{
    if(ABSTRACTCX(pContext)->IsDestroyed())	{
	//	Don't allow this to happen if the context has been destroyed...
	TRACE("Context %p Destroyed :: SetDrawable Blocking\n", pContext);
	return;
    }

    ABSTRACTCX(pContext)->SetDrawable(pContext, drawable);
}
#endif

#ifdef TRANSPARENT_APPLET
/* java specific functions to allow delayed window creation and transparency */
void CFE_HandleClippingView(MWContext *pContext, LJAppletData *appletD, int x, int y, int width, int height) {
    if(ABSTRACTCX(pContext)->IsDestroyed())	{
        //	Don't allow this to happen if the context has been destroyed...
        TRACE("Context %p Destroyed :: HandleClippingView Blocking\n", pContext);
        return;
    }

    WINCX(pContext)->HandleClippingView(pContext, appletD, x, y, width, height);
}

void CFE_DrawJavaApp(MWContext *pContext, int iLocation, LO_JavaAppStruct *pJava) {
    if(ABSTRACTCX(pContext)->IsDestroyed())	{
	//	Don't allow this to happen if the context has been destroyed...
	TRACE("Context %p Destroyed :: DrawJavaApp Blocking\n", pContext);
	return;
    }

    WINCX(pContext)->DrawJavaApp(pContext, iLocation, pJava);
}
#endif

int CFE_FileSortMethod(MWContext *pContext)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: FileSortMethod Blocking\n", pContext);
		return(-1);
	}

    return(ABSTRACTCX(pContext)->FileSortMethod(pContext));
}

extern char *EDT_NEW_DOC_NAME;

void CFE_FinishedLayout(MWContext *pContext)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: FinishedLayout Blocking\n", pContext);
		return;
	}

	//	Let the image library no that the page is no longer loading.
//	IL_EndPage(pContext);
    ABSTRACTCX(pContext)->FinishedLayout(pContext);

	//	See if the history title has been set yet.
	History_entry *pHist = SHIST_GetCurrent(&(pContext->hist));

    // Message compose window title is set by msglib. We don't want to overwrite
    // it here. - kamal
	if (pHist != NULL && pHist->title == NULL && 
	   ( pContext->type == MWContextBrowser || pContext->type == MWContextEditor || pContext->type == MWContextPane) )	{
		//	Make the URL the window title if there was none supplied.
        //  unless its a new document -- leave that one blank
		if(pContext->title == NULL)	{
#ifdef EDITOR
            if ( EDT_IS_EDITOR( pContext ) && 
                 _stricmp(pHist->address, EDT_NEW_DOC_NAME) != 0 ){
                // Let CWinCX::SetDocTitle() do name condensing for display,
                //  so editor can detect cases where Title = Address
    			FE_SetDocTitle(pContext, pHist->address);
                return;
            }
#endif //EDITOR
			CString csTitle = pHist->address;
			WFE_CondenseURL(csTitle, 50, FALSE);
			FE_SetDocTitle(pContext, (char *)(const char *)csTitle);
		}
	}
}

#ifndef MOZ_NGLAYOUT
void CFE_FormTextIsSubmit(MWContext *pContext, LO_FormElementStruct *pFormElement)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: FormTextIsSubmit Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->FormTextIsSubmit(pContext, pFormElement);
}

void CFE_FreeEdgeElement(MWContext *pContext, LO_EdgeStruct *pEdge)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: FreeEdgeElement Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->FreeEdgeElement(pContext, pEdge);
}
#endif

void
CFE_CreateEmbedWindow(MWContext *pContext, NPEmbeddedApp *pApp) {
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: CreateEmbedWindow Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->CreateEmbedWindow(pContext, pApp);
}

void
CFE_SaveEmbedWindow(MWContext *pContext, NPEmbeddedApp *pApp) {
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: SaveEmbedWindow Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->SaveEmbedWindow(pContext, pApp);
}

void
CFE_RestoreEmbedWindow(MWContext *pContext, NPEmbeddedApp *pApp) {
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: RestoreEmbedWindow Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->RestoreEmbedWindow(pContext, pApp);
}

void
CFE_DestroyEmbedWindow(MWContext *pContext, NPEmbeddedApp *pApp) {
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: DestroyEmbedWindow Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->DestroyEmbedWindow(pContext, pApp);
}

#ifndef MOZ_NGLAYOUT
void CFE_FreeEmbedElement(MWContext *pContext, LO_EmbedStruct *pEmbed)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: FreeEmbedElement Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->FreeEmbedElement(pContext, pEmbed);
}

extern "C" void 
FE_FreeFormElement(MWContext *pContext, LO_FormElementData *pFormElement)	
{
    //	Get our front end form element, and have it do its thang.
    CFormElement *pFormClass = CFormElement::GetFormElement(NULL, pFormElement);
    if(pFormClass != NULL)
        pFormClass->FreeFormElement(pFormElement);
}
#endif

extern "C" void 
FE_ReleaseTextAttrFeData(MWContext * pContext, LO_TextAttr *attr)
{
	if( attr )
		attr->FE_Data = NULL;       // cached fonts are released through m_cplCachedFontList
}

void CFE_FreeJavaAppElement(MWContext *pContext, LJAppletData *appletD)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: FreeJavaAppElement Blocking\n", pContext);
		return;
	}

    WINCX(pContext)->FreeJavaAppElement(pContext, appletD);
}

void CFE_HideJavaAppElement(MWContext *pContext, LJAppletData *pJava)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: HideJavaAppElement Blocking\n", pContext);
		return;
	}

    WINCX(pContext)->HideJavaAppElement(pContext, pJava);
}

#ifndef MOZ_NGLAYOUT
void CFE_GetEmbedSize(MWContext *pContext, LO_EmbedStruct *pEmbed, NET_ReloadMethod bReload)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: GetEmbedSize Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->GetEmbedSize(pContext, pEmbed, bReload);
}

void CFE_GetFormElementInfo(MWContext *pContext, LO_FormElementStruct *pFormElement)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: GetFormElementInfo Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->GetFormElementInfo(pContext, pFormElement);
}

void CFE_GetFormElementValue(MWContext *pContext, LO_FormElementStruct *pFormElement, XP_Bool bHidden)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: GetFormElementValue Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->GetFormElementValue(pContext, pFormElement, bHidden);
}
#endif

void CFE_GetJavaAppSize(MWContext *pContext, LO_JavaAppStruct *pJava, NET_ReloadMethod bReload)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: GetJavaAppSize Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->GetJavaAppSize(pContext, pJava, bReload);
}

#ifdef LAYERS
void CFE_GetTextFrame(MWContext *pContext, LO_TextStruct *pText,
		      int32 start, int32 end, XP_Rect *frame)   {
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: GetTextFrame Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->GetTextFrame(pContext, pText, start, end, frame);
}
#endif

int CFE_GetTextInfo(MWContext *pContext, LO_TextStruct *pText, LO_TextInfo *pTextInfo)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: GetTextInfo Blocking\n", pContext);
		return(-1);
	}

    return(ABSTRACTCX(pContext)->GetTextInfo(pContext, pText, pTextInfo));
}

void CFE_GraphProgress(MWContext *pContext, URL_Struct *pURL, int32 lBytesReceived, int32 lBytesSinceLastTime, int32 lContentLength)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: GraphProgress Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->GraphProgress(pContext, pURL, lBytesReceived, lBytesSinceLastTime, lContentLength);

	time_t ttCurTime = theApp.GetTime();
	if(ABSTRACTCX(pContext)->ProgressReady(ttCurTime) == TRUE)	{
		const char *pProgress = XP_ProgressText(lContentLength, lBytesReceived, 1, ABSTRACTCX(pContext)->GetElapsedSeconds(ttCurTime) + 1);
		if(pProgress != NULL)	{
			FE_Progress(pContext, pProgress);

			//	If the context has ncapi data, have it pass off this information to
			//		external applications too.
			if(ABSTRACTCX(pContext)->m_pNcapiUrlData != NULL)	{
				ABSTRACTCX(pContext)->m_pNcapiUrlData->MakingProgress(pProgress, CASTINT(lContentLength != 0 ? (lBytesReceived * 100 / lContentLength) % 100 : 0));
			}
		}
	}
}

void CFE_GraphProgressDestroy(MWContext *pContext, URL_Struct *pURL, int32 lContentLength, int32 lTotalBytesRead)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: GraphProgressDestroy Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->GraphProgressDestroy(pContext, pURL, lContentLength, lTotalBytesRead);
}

void CFE_GraphProgressInit(MWContext *pContext, URL_Struct *pURL, int32 lContentLength)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: GraphProgressInit Blocking\n", pContext);
		return;
	}

	ABSTRACTCX(pContext)->ResetStopwatch();
    ABSTRACTCX(pContext)->GraphProgressInit(pContext, pURL, lContentLength);

	//	If the context has ncapi data, have it pass off this information to
	//		external applications too.
	//	Get the data from the URL if available, then from the context....
	CNcapiUrlData *pNcapi = NULL;
	if(pURL != NULL)	{
		pNcapi = NCAPIDATA(pURL);
	}
	if(pContext != NULL && pNcapi == NULL)	{
		pNcapi = ABSTRACTCX(pContext)->m_pNcapiUrlData;
	}
	if(pNcapi != NULL)	{
		pNcapi->InitializeProgress();
	}
}

void CFE_LayoutNewDocument(MWContext *pContext, URL_Struct *pURL, int32 *pWidth, int32 *pHeight, int32 *pmWidth, int32 *pmHeight)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: LayoutNewDocument Blocking\n", pContext);
		return;
	}

	//	Remove any previous document title.
	if(pContext->title != NULL)	{
		XP_FREE(pContext->title);
		pContext->title = NULL;
	}

	//	Remove the default status.
	if(pContext->defaultStatus != NULL)	{
		XP_FREE(pContext->defaultStatus);
		pContext->defaultStatus = NULL;
	}

	//  Reset state reported by image context.
	ABSTRACTCX(pContext)->SetImagesLoading(FALSE);
	ABSTRACTCX(pContext)->SetImagesLooping(FALSE);
	ABSTRACTCX(pContext)->SetImagesDelayed(FALSE);

	// Reset state reported by mocha image context.
	ABSTRACTCX(pContext)->SetMochaImagesLoading(FALSE);
	ABSTRACTCX(pContext)->SetMochaImagesLooping(FALSE);
	ABSTRACTCX(pContext)->SetMochaImagesDelayed(FALSE);

	//	Add the document to the context session history.
	SHIST_AddDocument(pContext, SHIST_CreateHistoryEntry(pURL, NULL));

    ABSTRACTCX(pContext)->LayoutNewDocument(pContext, pURL, pWidth, pHeight, pmWidth, pmHeight);

	//	Allow clicking now.
	FE_EnableClicking(pContext);
}

/* Only call this function from OnMouseMove().  This caches the last
 * message from anywhere but a mouse movement and displays the
 * last message sent whenever it is cleared from the mouse movement.
 */

void wfe_Progress(MWContext *pContext, const char *pMessage)	{
	if(pMessage == NULL || *pMessage == '\0')	{
		if(pContext->defaultStatus)	{
			FE_Progress(pContext, pContext->defaultStatus);
		}
		else	{
			FE_Progress(pContext, ABSTRACTCX(pContext)->m_pLastStatus);
		}
    }
    else	{
        ABSTRACTCX(pContext)->Progress(pContext,pMessage);
	}
}

void CFE_Progress(MWContext *pContext, const char *pMessage)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: Progress Blocking\n", pContext);
		return;
	}
    
    //  Only allocate a last status if we're not simply passing in the
    //      same last status.
    if (pMessage != ABSTRACTCX(pContext)->m_pLastStatus) {
        //  Free off the old last status.
        if (ABSTRACTCX(pContext)->m_pLastStatus)    {
            XP_FREE(ABSTRACTCX(pContext)->m_pLastStatus);
        }

        //  Determine what the last status is depending on the message.
        if (pMessage && *pMessage)   {
            //  Make a copy of the message into last status.
            ABSTRACTCX(pContext)->m_pLastStatus = XP_STRDUP(pMessage);
        }
        else    {
            //  No last status.
            ABSTRACTCX(pContext)->m_pLastStatus = NULL;
        }
    }

	//	We may be changing what we show if pMessage is NULL or has no context.
	if(pMessage == NULL || *pMessage == '\0')	{
        //  There is no message string (missing).
		//	If the context has a default string we should show, do that instead.
		if(pContext->defaultStatus && *(pContext->defaultStatus))	{
		    FE_Progress(pContext, pContext->defaultStatus);
		}
        else {
            // There was no message text and no default text so it looks like
            //   someone wants the text cleared altogether.
            ABSTRACTCX(pContext)->Progress(pContext, " ");
        }
	}
    else    {
        //  Display the message if present.
        ABSTRACTCX(pContext)->Progress(pContext, pMessage);
    }
}

char *CFE_Prompt(MWContext *pContext, const char *pPrompt, const char *pDefault)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: Prompt Blocking\n", pContext);
		return(NULL);
	}

	char *pWinPrompt = FE_Windowsify(pPrompt);
	char *pWinDefault = FE_Windowsify(pDefault);
	char *pReturn = ABSTRACTCX(pContext)->Prompt(pContext, pWinPrompt, pWinDefault);
	XP_FREE(pWinPrompt);
	if (pWinDefault)
		XP_FREE(pWinDefault);
	return(pReturn);
}

char *CFE_PromptWithCaption(MWContext *pContext, const char *pCaption, const char *pPrompt, const char *pDefault)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: Prompt Blocking\n", pContext);
		return(NULL);
	}

	char *pWinPrompt = FE_Windowsify(pPrompt);
	char *pWinDefault = FE_Windowsify(pDefault);
	char *pWinCaption = FE_Windowsify(pCaption);
	char *pReturn = ABSTRACTCX(pContext)->PromptWithCaption(pContext, pWinCaption, pWinPrompt, pWinDefault);
	XP_FREE(pWinPrompt);
	if (pWinDefault)
		XP_FREE(pWinDefault);
	return(pReturn);
}

char *CFE_PromptPassword(MWContext *pContext, const char *pMessage)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: PromptPassword Blocking\n", pContext);
		return(NULL);
	}

	char *pWinMessage = FE_Windowsify(pMessage);
	char *pReturn = ABSTRACTCX(pContext)->PromptPassword(pContext, pWinMessage);
	XP_FREE(pWinMessage);
	return(pReturn);
}

XP_Bool CFE_PromptUsernameAndPassword(MWContext *pContext, const char *pMessage, char **ppUsername, char **ppPassword)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: PromptUsernameAndPassword Blocking\n", pContext);
		return(FALSE);
	}

	char *pWinMessage = FE_Windowsify(pMessage);
	XP_Bool bReturn = ABSTRACTCX(pContext)->PromptUsernameAndPassword(pContext, pWinMessage, ppUsername, ppPassword);
	XP_FREE(pWinMessage);
	return(bReturn);
}

#ifndef MOZ_NGLAYOUT
void CFE_ResetFormElement(MWContext *pContext, LO_FormElementStruct *pFormElement)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: ResetFormElement Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->ResetFormElement(pContext, pFormElement);
}
#endif

void CFE_SetBackgroundColor(MWContext *pContext, uint8 cRed, uint8 cGreen, uint8 cBlue)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: SetBackgroundColor Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->SetBackgroundColor(pContext, cRed, cGreen, cBlue);
}

void CFE_SetDocDimension(MWContext *pContext, int iLocation, int32 lWidth, int32 lLength)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: SetDocDimension Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->SetDocDimension(pContext, iLocation, lWidth, lLength);
}

void CFE_SetDocPosition(MWContext *pContext, int iLocation, int32 lX, int32 lY)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: SetDocPosition Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->SetDocPosition(pContext, iLocation, lX, lY);
}

void CFE_SetDocTitle(MWContext *pContext, char *pTitle)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: SetDocTitle Blocking\n", pContext);
		return;
	}

	//	Default implementation allocs a string to put into the context.
	if(pContext->title != NULL)	{
		XP_FREE(pContext->title);
		pContext->title = NULL;
	}

	if(pTitle != NULL)	{
		pContext->title = XP_STRDUP(pTitle);
	}

    ABSTRACTCX(pContext)->SetDocTitle(pContext, pTitle);

	//	Update the session history after calling context class, they may have
	//		munged the title.
	SHIST_SetTitleOfCurrentDoc( pContext );
}

#ifndef MOZ_NGLAYOUT
void CFE_SetFormElementToggle(MWContext *pContext, LO_FormElementStruct *pFormElement, XP_Bool bToggle)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: SetFormElementToggle Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->SetFormElementToggle(pContext, pFormElement, bToggle);
}
#endif

void CFE_SetProgressBarPercent(MWContext *pContext, int32 lPercent)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: SetProgressBarPercent Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->SetProgressBarPercent(pContext, lPercent);
}

XP_Bool CFE_ShowAllNewsArticles(MWContext *pContext)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: ShowAllNewsArticles Blocking\n", pContext);
		return(FALSE);
	}

    return(ABSTRACTCX(pContext)->ShowAllNewsArticles(pContext));
}

char *CFE_TranslateISOText(MWContext *pContext, int iCharset, char *pISOText)
{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		//	Just return what's passed in.
		TRACE("Context %p Destroyed :: TranslateISOText Blocking\n", pContext);
		return(pISOText);
	}

    return(ABSTRACTCX(pContext)->TranslateISOText(pContext, iCharset, pISOText));
}

XP_Bool CFE_UseFancyFTP(MWContext *pContext)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: UseFancyFTP Blocking\n", pContext);
		return(FALSE);
	}

    return(ABSTRACTCX(pContext)->UseFancyFTP(pContext));
}

XP_Bool CFE_UseFancyNewsgroupListing(MWContext *pContext)	{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: UseFancyNewsgroupListing Blocking\n", pContext);
		return(FALSE);
	}

    return(ABSTRACTCX(pContext)->UseFancyNewsgroupListing(pContext));
}

//  Get URL exit routines.
void CFE_GetUrlExitRoutine(URL_Struct *pUrl, int iStatus, MWContext *pContext)  {
	//	Enable clicking now.
	FE_EnableClicking(pContext);

	//	Report any error.
	if(iStatus < 0 && pUrl->error_msg != NULL)	{
		FE_Alert(pContext, pUrl->error_msg);
	}

	// Make sure the context is still valid. It's possible it's been deleted
	// out from under us while we were displaying the modal dialog box (and in
	// a sub-dispatch message loop)
	if (!XP_IsContextInList(pContext)) {
		return;
	}

#ifdef EDITOR
    // Do stuff specific to the editor
    FE_EditorGetUrlExitRoutine(pUrl, iStatus, pContext);

#ifdef MOZ_MAIL_NEWS
    if (IS_MESSAGE_COMPOSE(pContext))
    {
        CGenericFrame * pFrame = wfe_FrameFromXPContext(pContext);
        if (pFrame)
        {
            CComposeFrame * pCompose = (CComposeFrame*)pFrame;
            if (pCompose->UseHtml() && !pCompose->Initialized())
                pCompose->GoldDoneLoading();
        }
    }
#endif // MOZ_MAIL_NEWS
#endif //EDITOR

    ABSTRACTCX(pContext)->GetUrlExitRoutine(pUrl, iStatus, pContext);

	if(iStatus != MK_CHANGING_CONTEXT)	{

		if (pContext->type == MWContextBrowser && !EDT_IS_EDITOR(pContext)) {
			char keyword[64];
			NET_getInternetKeyword(pUrl, keyword, sizeof(keyword));
			ABSTRACTCX(pContext)->SetInternetKeyword(keyword);
		}

		//	We autoproduce a title for those contexts which have none.
        //  Message compose window title is set by msglib. We don't want to overwrite 
        //  it here. - kamal
		if(pContext->title == NULL && pUrl->address != NULL &&
		   (pContext->type == MWContextBrowser || pContext->type == MWContextPane) && !EDT_IS_EDITOR(pContext) )	{

			//	Limit the automatically set titles to 50 chars.
			CString csTitle = pUrl->address;
			WFE_CondenseURL(csTitle, 50, FALSE);
			FE_SetDocTitle(pContext, (char *)(const char *)csTitle);
		}

		//	Since a page was loaded, go through all internal contexts and update their
		//		anchors so that we can have an updated display on all relevant windows.
		XP_RefreshAnchors();

		//	If the url has ncapi data, have it pass off this information to
		//		external applications too (must happen before URL struct is
		//		freed off (all connections complete).
		if(NCAPIDATA(pUrl) != NULL)	{
			NCAPIDATA(pUrl)->EndProgress();
		}

		//	Make sure the NCAPI Url data will let us free off the URL.
		if(NCAPIDATA(pUrl) == NULL || NCAPIDATA(pUrl)->CanFreeUrl() == TRUE)	{
            FEU_DeleteUrlData(pUrl, NULL);
			NET_FreeURLStruct(pUrl);
		}
	}
}

//  Get URL exit routines.
void CFE_SimpleGetUrlExitRoutine(URL_Struct *pUrl, int iStatus, MWContext *pContext)  {
    if(iStatus != MK_CHANGING_CONTEXT)	{
        NET_FreeURLStruct(pUrl);
    }
}

void CFE_TextTranslationExitRoutine(PrintSetup *pTextFE)    {
	//	Enable clicking now.
	FE_EnableClicking(VOID2CX(pTextFE->carg, CAbstractCX)->GetContext());

    VOID2CX(pTextFE->carg, CAbstractCX)->TextTranslationExitRoutine(pTextFE);

    //  Clean up what the CAbstractCX was responsible for allocating.
    XP_FREE(pTextFE->prefix);
    XP_FREE(pTextFE->filename);
    fclose(pTextFE->out);

    //  Don't remove the print setup.
    //	It is freed elsewhere by other
    //		code (TextFE stuff).
}

void CFE_GetDocPosition(MWContext * pContext, int iLoc, int32 * pX, int32 * pY)
{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: GetDocPosition Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->GetDocPosition(pContext, iLoc, pX, pY);
}

#ifdef LAYERS
PRBool FE_HandleLayerEvent(MWContext * pContext, CL_Layer * pLayer,
			   CL_Event * pEvent)
{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: HandleLayerEvent Blocking\n", pContext);
		return PR_FALSE;
	}

    return (PRBool)ABSTRACTCX(pContext)->HandleLayerEvent(pLayer, pEvent);
}

PRBool FE_HandleEmbedEvent(MWContext * pContext, LO_EmbedStruct *embed,
			   CL_Event * pEvent)
{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: HandleEmbedEvent Blocking\n", pContext);
		return PR_FALSE;
	}

    return (PRBool)ABSTRACTCX(pContext)->HandleEmbedEvent(embed, pEvent);
}
#endif
//
// Scroll the document so the given X, Y coordinate is in view.  Actually
//   call the scrolling routines here so that we are sure that the form elements
//   get moved too.
//
extern "C" void 
FE_ScrollDocTo(MWContext * pContext, int iLoc, int32 X, int32 Y)
{
    //  Map this directly to SetDocPosition.
    FE_SetDocPosition(pContext, iLoc, X<0?0:X, Y<0?0:Y);
}

    //
// Scroll the document so the given X, Y coordinate is in view.  Actually
//   call the scrolling routines here so that we are sure that the form elements
//   get moved too.
//
extern "C" void 
FE_ScrollDocBy(MWContext * pContext, int iLoc, int32 X, int32 Y)
{
	int32 pX, pY;

	FE_GetDocPosition(pContext, iLoc, &pX, &pY);
	pX +=X;
	pY +=Y;
	
	FE_SetDocPosition(pContext, iLoc, pX<0?0:pX, pY<0?0:pY);
}

extern "C" void
FE_BackCommand(MWContext *pContext)
{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: BackCommand Blocking\n", pContext);
		return;
	}

	ABSTRACTCX(pContext)->AllBack();
}

extern "C" void
FE_ForwardCommand(MWContext *pContext)
{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: ForwardCommand Blocking\n", pContext);
		return;
	}

	ABSTRACTCX(pContext)->AllForward();
}

extern "C" void 
FE_HomeCommand (MWContext *pContext)
{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: HomeCommand Blocking\n", pContext);
		return;
	}

	ABSTRACTCX(pContext)->GoHome();
}

extern "C" void 
FE_PrintCommand(MWContext *pContext)
{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: PrintCommand Blocking\n", pContext);
		return;
	}

	ABSTRACTCX(pContext)->PrintContext();
}

XP_Bool 
FE_FindCommand(MWContext *pContext, char* szName, XP_Bool bCaseSensitive, XP_Bool bBackwards,
					XP_Bool bWrap)
{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: FindCommand Blocking\n", pContext);
		return FALSE;
	}

	if (szName)
	    return ABSTRACTCX(pContext)->DoFind(NULL, (const char *)szName, bCaseSensitive, 
				!bBackwards, FALSE);
	else 
	    ABSTRACTCX(pContext)->AllFind(pContext);
	    return FALSE;

}

extern "C" void 
FE_GetWindowOffset(MWContext *pContext, int32 *sx, int32 *sy)
{
	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: WindowToScreenXY Blocking\n", pContext);
		return;
	}

	ABSTRACTCX(pContext)->GetWindowOffset(sx, sy);
}

extern "C" void FE_Alert(MWContext *pContext, const char *pMsg)
{
	if(pContext != NULL)	{

		// we NEVER want biff to put up an alert
		if (pContext->type == MWContextBiff)
			return;
		if (IS_MESSAGE_COMPOSE(pContext)) {
			CGenericFrame * pFrame = wfe_FrameFromXPContext(pContext);
			pFrame->GetChrome()->StopAnimation();
		}
		pContext->funcs->Alert(pContext, pMsg);
	} else	{
		MessageBox(NULL, pMsg, "Netscape", MB_OK);
	}
}

extern "C" int32 FE_GetContextID(MWContext *pContext)
{
    CAbstractCX *pCX = ABSTRACTCX(pContext);

	if(pCX != NULL)	{
		return((int32)pCX->GetContextID());
	}
	else	{
		return(XP_DOCID(pContext));
	}
}

extern "C" void FE_UrlChangedContext(URL_Struct *pUrl, MWContext *pOldContext, MWContext *pNewContext)	{
	//	A Url has changed context.
	//	We need to mark it in the new context if it has ncapi_data (which we use
	//		to track such things under windows).
	if(pUrl->ncapi_data != NULL)	{
		//	Tell it about the change.
		CNcapiUrlData *pUrlData = (CNcapiUrlData *)pUrl->ncapi_data;
		pUrlData->ChangeContext(ABSTRACTCX(pNewContext));
	}
}

extern "C" void FE_UpdateStopState(MWContext *pContext)	{

	if(ABSTRACTCX(pContext)->IsDestroyed())	{
		//	Don't allow this to happen if the context has been destroyed...
		TRACE("Context %p Destroyed :: UpdateStopState Blocking\n", pContext);
		return;
	}

    ABSTRACTCX(pContext)->UpdateStopState(pContext);
}

// Called by the security library to indicate that the user has choosen
// when-to-ask-for-password preferences
//
// XXX - jsw - remove me
void FE_SetPasswordAskPrefs(MWContext *context, int askPW, int timeout)
{
}

static char fixFont[12] = "Courier New";
static char	propFont[6] = "Arial";
extern "C" XP_Bool FE_CheckFormTextAttributes(MWContext *context,
                LO_TextAttr *oldAttr, LO_TextAttr *newAttr, int32 type)
{
	if (oldAttr && newAttr && !oldAttr->font_face)  {
	    memcpy(newAttr,oldAttr, sizeof(LO_TextAttr)); 
	// setup the text attribute here.

		CDCCX *pDCCX = CXDC(context);
		int csid = INTL_DefaultWinCharSetID(context);

		if (oldAttr->fontmask & LO_FONT_FIXED) {
			if (csid == CS_LATIN1)  {
				newAttr->font_face = fixFont;
			}
			else {
				newAttr->font_face = (char*)IntlGetUIFixFaceName(csid);
			}
		}
		else {
			if (csid == CS_LATIN1)  {
				// this is a hack, so default Arial font for form element will not look so
				// big.
				if (newAttr->size > 1);
					newAttr->size -=1;
				newAttr->font_face = propFont;
			}
			else    {
    			newAttr->font_face = (char*)IntlGetUIPropFaceName(csid);
			}

		}
		newAttr->font_weight = FW_NORMAL;  /* 100, 200, ... 900 */
		newAttr->FE_Data = NULL;     /* For the front end to store font IDs */
	return TRUE;
	}
	else
		return FALSE;
	
}

extern "C" XP_Bool FE_IsNetcasterInstalled(void) {
	return FEU_IsNetcasterAvailable();
}

extern "C" void FE_RunNetcaster(MWContext *context) {
	FEU_OpenNetcaster();
}

extern "C" MWContext *FE_IsNetcasterRunning(void) {
	return theApp.m_pNetcasterWindow;
}

#ifdef SHACK
void CFE_DisplayBuiltin(MWContext *context, int iLocation, LO_BuiltinStruct *builtin_struct)
{
    return;
}

void CFE_FreeBuiltinElement(MWContext *context, LO_BuiltinStruct *builtin_struct)
{
    return;
}
#endif //SHACK
