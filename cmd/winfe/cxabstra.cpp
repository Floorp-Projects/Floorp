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

#include "cxabstra.h"

#include "xp_thrmo.h"
#include "timer.h"  // for nettimer since can't call base class
#include "libi18n.h"	// International function 
#include "template.h"
#include "dialog.h"
#include "mainfrm.h"
#ifdef MOZ_MAIL_NEWS
#include "fldrfrm.h"
#include "thrdfrm.h"
#include "msgfrm.h"
#include "msgcom.h"
#include "wfemsg.h"
#endif /* MOZ_MAIL_NEWS */
#include "winclose.h"
#include "libevent.h"
#include "np.h"
#include "intl_csi.h"
#include "mkhelp.h"	// for HelpInfoStruct
#include "feimage.h"
#include "cxicon.h"
#ifdef EDITOR
#include "edt.h"
#endif /* EDITOR */
#include "mozilla.h"

CAbstractCX::CAbstractCX()  {
//	Purpose:    Constructor for the abstract base class.
//	Arguments:  void
//	Returns:    none
//	Comments:   Responsible for creating the XP context.

    m_pLastStatus = NULL;

    //  First set our context type.
    //  This is really irrelevant, just a reminder to those which derive from us.
    m_cxType = Abstract;

    //  Create the XP context.
    m_pXPCX = XP_NewContext();
	m_pImageGroupContext = 0;
	m_bImagesLoading = FALSE;
	m_bImagesLooping = FALSE;
	m_bImagesDelayed = FALSE;
	m_bMochaImagesLoading = FALSE;
	m_bMochaImagesLooping = FALSE;
	m_bMochaImagesDelayed = FALSE;

    m_bNetHelpWnd = FALSE;
    
	//	Set its type to an invalid default.
	//	Up to others to correctly set.
	m_pXPCX->type = MWContextAny;

	m_pXPCX->fontScalingPercentage = 1.0;

    //  Assign in the common front end functions.
    m_pXPCX->funcs = new ContextFuncs;
#define MAKE_FE_FUNCS_PREFIX(funkage) CFE_##funkage
#define MAKE_FE_FUNCS_ASSIGN m_pXPCX->funcs->
#include "mk_cx_fn.h"

    //  Set the FE data to simply point to this class.
    m_pXPCX->fe.cx = this;

	//	Initialize the global history of the object.
    SHIST_InitSession(m_pXPCX);

	//	We haven't been told to attempt to interrupt the load.
	//  Nor have we decided to self destruct.
	m_bIdleInterrupt = FALSE;
	m_bIdleDestroy = FALSE;

    //  We have no client pull information.
    m_pClientPullData = FALSE;
    m_pClientPullTimeout = FALSE;

	//	Reset our stopwatch, in case it never gets initialized.
	ResetStopwatch();

	//	We haven't been destroyed yet.
	m_bDestroyed = FALSE;

	//	We have ncapi data yet.
	m_pNcapiUrlData = NULL;

	//	We are initially allowing clicks.
	GetContext()->waitingMode = FALSE;

	//	Add the context to the XP list.
	XP_AddContextToList(GetContext());

	//	No levels of interruptedness.
	m_iInterruptNest = 0;
}

int16  CAbstractCX::GetWinCsid()
{
	return INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(m_pXPCX));
}
/*
 * Mocha has removed all of its info from this context, it's
 *   safe to trash it now
 */
void CAbstractCX::MochaDestructionComplete()
{
	//	Now, attempt to interrupt.  This allows us to clean up a context, even
	//		if the network library is blocking.
	//	Eventually, this will cause this context to delete itself.
	Interrupt();
}

/*
 * Mocha has removed all of its info from this context
 */
void mocha_done_callback(void * data)
{
    ((CAbstractCX *)data)->MochaDestructionComplete();
}

void CAbstractCX::DestroyContext()	{
//	Purpose:	Before deleting a context, you must destroy it for proper cleanup.
//	Arguments:	void
//	Returns:	void
//	Comments:	Because the XP libraries perform calls on a context when it is going
//					away, and those calls are virtual, we must first "destroy" a context,
//					and then we are able to delete it properly.

    if(m_bDestroyed == FALSE)   {
	    //	Only call in here once, please.
        //  We have to lock the application from exiting until we receive
        //      a series of callbacks and eventually remove the context
        //      from memory.
        //  This lock is removed in the destructor.
        AfxOleLockApp();

	    //	Have the document interrupt.
	    //	We call again when we get the mocha complete callback, but that 
        //      causes us to get deleted physically after the destroyed flag 
        //      is set.
	    Interrupt();

#ifndef MOZ_NGLAYOUT
        //  Layout needs to cleanup.
        LO_DiscardDocument(m_pXPCX);
#endif

	    //	End the session history of the context.
        SHIST_EndSession(m_pXPCX);

#ifdef MOZ_MAIL_NEWS
	    // Free any memory used for MIME objects
	    MimeDestroyContextData(m_pXPCX);
#endif /* MOZ_MAIL_NEWS */

        //	We are now destroyed, set a member to let us know.
	    m_bDestroyed = TRUE;

		if(m_pImageGroupContext)   {
			IL_RemoveGroupObserver(m_pImageGroupContext, ImageGroupObserver, (void*)GetContext());
			IL_DestroyGroupContext(m_pImageGroupContext);
            m_pXPCX->img_cx = NULL;
        }

 		if (m_pXPCX->color_space) {
            IL_ReleaseColorSpace(m_pXPCX->color_space);
            m_pXPCX->color_space = NULL;
        }
            
	    //	Remove ourselves from the XP context list.
        //  Must be done before call to mocha, or lists in
        //      unstable state.
	    XP_RemoveContextFromList(m_pXPCX);

#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
#else
        //  Have mocha perform cleanup and continue destruction in the
        //    callback
        ET_RemoveWindowContext(m_pXPCX, mocha_done_callback, this);
#endif /* MOZ_NGLAYOUT */
    }
}

/*-----------------------------------------------**
** Let a load complete before self destructing.  **
**-----------------------------------------------*/
void CAbstractCX::NiceDestroyContext() {
    m_bIdleDestroy = FALSE;
    if(m_bDestroyed == FALSE && GetContext()) {
        if(XP_IsContextBusy(GetContext())) {
            //  Try again later.
            m_bIdleDestroy = TRUE;
            FEU_RequestIdleProcessing(GetContext());
        }
        else {
            //  Time's up.
            DestroyContext();
            return;
        }
    }
}

//  Routine to detect when we can exit.
void fe_finish_exit(void *pClosure)
{
    BOOL bCanExit = TRUE;
    static void *pExitTimer = NULL;

    if(pClosure != NULL)    {
        //  Called via timeout.
        //  Clear the timeout backpointer.
        pExitTimer = NULL;
    }
    else    {
        //  Called via destructor (anal).
        //  Make sure we haven't already set the timer.
        if(pExitTimer != NULL)  {
            return;
        }
    }

#ifdef MOZ_LOC_INDEP
	/* li_stuff - time to do last synch */
	if (theApp.LIStuffEnd() == FALSE) {
        //  synching files.
        bCanExit = FALSE;
	}
#endif // MOZ_LOC_INDEP

    if(AfxOleCanExitApp() == FALSE) {
        //  Outstanding com lock.
        bCanExit = FALSE;
    }

    if(XP_ListCount(XP_GetGlobalContextList()) != 0) {
        //  Context is out there.
        bCanExit = FALSE;
    }

    if(theApp.m_pMainWnd != NULL)   {
        if(bCanExit)    {
            TRACE("Posting WM_CLOSE to %p hidden frame to exit app.\n", theApp.m_pMainWnd);
            theApp.m_pMainWnd->PostMessage(WM_CLOSE);
        }
        else    {
            //  Have to retry in a little while.
            pExitTimer = FE_SetTimeout(fe_finish_exit, (void *)-1, 1000);
            ASSERT(pExitTimer != NULL);
        }
    }
}

CAbstractCX::~CAbstractCX() {
//	Purpose:    Destroy the context.
//	Arguments:  void
//	Returns:    none
//	Comments:   Perform any necessary cleanup also.

    //  Can remove lock set in DestroyContext.
    //  Window of instability gone.
    AfxOleUnlockApp();

    //  Let those watching this context that it is no more (NCAPI).
    CWindowChangeItem::Closing(GetContextID());

	//	If you get this ASSERTION, then you should call DestroyContext and
	//		NEVER delete the context.
	ASSERT(m_bDestroyed == TRUE);

	//	Get rid of the last and default status.
    if (m_pLastStatus)	{
        XP_FREE(m_pLastStatus);
		m_pLastStatus = NULL;
	}
	if(m_pXPCX->defaultStatus != NULL)	{
		XP_FREE(m_pXPCX->defaultStatus);
		m_pXPCX->defaultStatus = NULL;
	}

	//	If we allocated a title for the context, get rid of it.
	if(m_pXPCX->title != NULL)	{
		XP_FREE(m_pXPCX->title);
		m_pXPCX->title = NULL;
	}

	//	Get rid of its name.
	if(m_pXPCX->name != NULL)	{
		XP_FREE(m_pXPCX->name);
		m_pXPCX->name = NULL;
	}

    // Free the transparent pixel used by the Image Library.
    if(m_pXPCX->transparent_pixel) {
        XP_FREE(m_pXPCX->transparent_pixel);
        m_pXPCX->transparent_pixel = NULL;
    }

	/* EA: Remove any help information associated with this context */	
 	if (m_pXPCX && (m_pXPCX->pHelpInfo != NULL)) {
 		if (((HelpInfoStruct *) m_pXPCX->pHelpInfo)->topicURL != NULL) {
 			XP_FREE(((HelpInfoStruct *) m_pXPCX->pHelpInfo)->topicURL);
 			((HelpInfoStruct *) m_pXPCX->pHelpInfo)->topicURL = NULL;
 		}
 
  		XP_FREE(m_pXPCX->pHelpInfo);
  		m_pXPCX->pHelpInfo = NULL;
 	}

    //  Delete our function table in the XP context.
    memset(m_pXPCX->funcs, 0, sizeof(*(m_pXPCX->funcs)));
    delete m_pXPCX->funcs;
    m_pXPCX->funcs = NULL;

    //  Delete our XP context.
	XP_DeleteContext(m_pXPCX);
    m_pXPCX = NULL;

	// NOTE: Frames and Views should be taken care of by sub-classes

    // There are three "hiding" contexts
#ifdef MOZ_MAIL_NEWS
#define HIDDENCXS 4
#else
#define HIDDENCXS 2
#endif /* MOZ_MAIL_NEWS */
    //  bookmarks window
    //  address book window
    //  biff / check mail context
    //  Misc. URL loader context which switch context, ie: NetHelp, etc.
    // Check the number of items in the context list to see if we should exit.
    if(XP_ListCount(XP_GetGlobalContextList()) == HIDDENCXS)	{
        //  These are the stragglers that we need to see if visible,
        //      and if not, remove them at this time.
        //  All or none.
        //  Do in seperate for loops or list state is undefined.
        CAbstractCX *pCXs[HIDDENCXS];
        int iTraverse;
        for(iTraverse = 0; iTraverse < HIDDENCXS; iTraverse++)  {
            pCXs[iTraverse] = ABSTRACTCX((MWContext *)XP_ListGetObjectNum(XP_GetGlobalContextList(), iTraverse + 1));
        }
        for(iTraverse = 0; iTraverse < HIDDENCXS; iTraverse++)  {
            if(pCXs[iTraverse]) {
                pCXs[iTraverse]->DestroyContext();
            }
        }
        
        //  Because we've killed off the hidden contexts, clear pointers
        //      to them.
        theApp.m_pSlaveCX = NULL;
        theApp.m_pRDFCX = NULL;

        //  Start to detect when we can exit the application.
        fe_finish_exit(NULL);
    }
}

void CAbstractCX::SetContextName(const char *pName)	{
	//	Set the context name.
	if(GetContext()->name != NULL)	{
		XP_FREE(GetContext()->name);
	}
	if(pName != NULL)	{
		GetContext()->name = XP_STRDUP(pName);
	}
	else	{
		GetContext()->name = NULL;
	}
}

void CAbstractCX::SetParentContext(MWContext *pParentContext)	{
	//	Set the context's parent context.
	GetContext()->grid_parent = pParentContext;

	//	Also set that this is a grid cell.
	//	Can't have a grid parent if not a true grid.
	GetContext()->is_grid_cell = TRUE;

	//	Finally, update what is called the parent's list of children.
	//	Removal is taken care of automagically.
	XP_UpdateParentContext(GetContext());
}

BOOL CAbstractCX::IsGridCell() const	{
	return(GetContext()->is_grid_cell);
}

BOOL CAbstractCX::IsGridParent() const	{
    BOOL bRetval = FALSE;
    MWContext *pContext = GetContext();
    if(pContext) {
        bRetval = !XP_ListIsEmpty(pContext->grid_children);
    }
    return bRetval;
}

CWnd *CAbstractCX::GetDialogOwner() const	{
//	Purpose:    Return the parent/owner of any dialogs for the context.
//	Arguments:  void
//	Returns:    CWnd *	The parent as needed.
//	Comments:   Generic use function to more easily define the owner
//					of any dialogs.

	//	As a default, we'll be returning the desktop window.
	return(CWnd::GetDesktopWindow());
}

// This routine changes the context of the specified URL structure to be
// a browser context. Returns the new context if successful and NULL otherwise
MWContext *SwitchToBrowserContext(URL_Struct* pUrlStruct)
{
	ASSERT(NET_IsSafeForNewContext(pUrlStruct));

	// Use an existing browser window if we have one
	CFrameWnd*	pFrameWnd = FEU_GetLastActiveFrame(MWContextBrowser, FEU_FINDBROWSERONLY);
	MWContext*	pNewContext = NULL;

	if (pFrameWnd) {
		CFrameGlue *pGlue = CFrameGlue::GetFrameGlue(pFrameWnd);
		CWinCX*		pWinCX;

		if (pGlue && ((pWinCX = pGlue->GetMainWinContext()) != NULL)) {
			pNewContext = pWinCX->GetContext();

			// Don't use this context if it's busy or doesn't want to be used.
			if (ABSTRACTCX(pNewContext)->IsContextStoppable() 
						    || pNewContext->restricted_target)
				pNewContext = NULL;  // create a new context instead
			else {	
				pWinCX->GetUrl( pUrlStruct, FO_CACHE_AND_PRESENT, TRUE );
				// Make sure the window is visible
				if(pFrameWnd->IsIconic())
					pFrameWnd->ShowWindow(SW_RESTORE);
				else {
#ifdef XP_WIN16
					pFrameWnd->BringWindowToTop();
#else
					pFrameWnd->SetForegroundWindow();
#endif
				}
			}
		}
	}

	// If we couldn't find an existing browser window, then create a new one
	if (!pNewContext) {
		// Create a new browser window
		pNewContext = CFE_CreateNewDocWindow(NULL, pUrlStruct);
	}

	return pNewContext;
}

int CAbstractCX::NormalGetUrl(const char *pUrl, const char *pReferrer, const char *pTarget, BOOL bForceNew)	{
	//	Generic brain dead interface to GetUrl.
	URL_Struct *pUrlStruct = NET_CreateURLStruct(pUrl, NET_DONT_RELOAD);

	//	Add the referrer field.
	if(pReferrer != NULL)	{
		pUrlStruct->referer = XP_STRDUP(pReferrer);
	}

	//	Add the target name.
	if(pTarget != NULL)	{
		if (pTarget[0] == '_')	{
			pUrlStruct->window_target = XP_STRDUP("");
		}
		else	{
			pUrlStruct->window_target = XP_STRDUP(pTarget);
		}
	}

	//	Load it.
#ifdef EDITOR
    // Use Edit present type to filter mime types we can't edit
    return( GetUrl(pUrlStruct, EDT_IS_EDITOR(GetContext()) ? FO_CACHE_AND_EDIT : FO_CACHE_AND_PRESENT, TRUE, bForceNew));
#else
	return(GetUrl(pUrlStruct, FO_CACHE_AND_PRESENT, TRUE, bForceNew));
#endif
}

int CAbstractCX::GetUrl(URL_Struct *pUrl, FO_Present_Types iFormatOut, BOOL bReallyLoad, BOOL bForceNew)	{
//	Purpose:	Load a url into the context class.
//	Arguments:	pUrl	The URL to load
//				iFormatOut	The format out of the load (helps determine the content type converter
//								along with the mime type).
//				bReallyLoading	Is a switch to tell us not to actually request the URL.
//								This helps and context changing URLs to bootstrap the loading UI.
//	Returns:	int	As NET_GetURL
//	Comments:	Use this instead of NET_GetURL

    //  Determine URL type.  -1 is out of range.
    int iUrlType = -1;
    if(pUrl && pUrl->address) {
        iUrlType = NET_URL_Type(pUrl->address);
    }

    //  Switch on URL type to see how we should handle.
    int iRetval = MK_NO_ACTION;
    switch(iUrlType) {
        case NETHELP_TYPE_URL: {
            MWContext *pNetHelpCX = FE_GetNetHelpContext();
            if(pNetHelpCX) {
                iRetval = NET_GetURL(pUrl, FO_PRESENT, pNetHelpCX, CFE_SimpleGetUrlExitRoutine);
            }
            break;
        }
        default: {
        	//	See if this is the current page (named anchor), if so don't load.
        	//	But don't do this if we're not really initiating the load ourselves.
        	//	Also make sure that this is a present type, if not, don't check
        	//		for this named anchor stuff.
        	//	If this isn't acceptable behaviour, we really need to split what
        	//		you're doing into a new context (view source dialog could be this
        	//		way as an example).
        	int32 lX, lY;
        	BOOL bNamedAnchor = FALSE;
        	if(((iFormatOut & 0x1f ) == FO_PRESENT) && bReallyLoad == TRUE && XP_FindNamedAnchor(GetContext(), pUrl, &lX, &lY) == TRUE)	{
        		if(IsWindowContext())	{
        			//	Make the location visible on the screen....
        			PANECX(GetContext())->MakeElementVisible(lX, lY);
        		}
        		if(pUrl->history_num == 0)	{
                    //  Create URL from prev history entry to preserve security attributes, etc.
        			History_entry *pHist = SHIST_GetCurrent(&GetContext()->hist);
            		URL_Struct *pCopyURL = SHIST_CreateURLStructFromHistoryEntry(GetContext(), pHist);

                    //  Swap addresses.
                    char *pSaveAddr = pUrl->address;
                    pUrl->address = pCopyURL->address;
                    pCopyURL->address = pSaveAddr;

                    //  Free old URL, and reassign.
        			NET_FreeURLStruct(pUrl);
                    pUrl = pCopyURL;

        			SHIST_AddDocument(GetContext(), SHIST_CreateHistoryEntry(pUrl, pHist->title));
        		}
        		else	{
        			SHIST_SetCurrent(&GetContext()->hist, pUrl->history_num);
        		}

        		//	Didn't have to perform a load.
        		//	However, we should call the normal routines to stop all the UI....
        		bReallyLoad = FALSE;
        		bNamedAnchor = TRUE;
        	}

        	//	The URLs anchor and referrer may be a local file path.
        	//	Change them to be network worthy.
        	CString csUrl;
        	if(pUrl->address != NULL)	{
        		WFE_ConvertFile2Url(csUrl, pUrl->address);
        		XP_FREE(pUrl->address);
        		pUrl->address = XP_STRDUP(csUrl);
        	}
        	if(pUrl->referer != NULL)	{
        		WFE_ConvertFile2Url(csUrl, pUrl->referer);
        		XP_FREE(pUrl->referer);
        		pUrl->referer = XP_STRDUP(csUrl);
        	}

#ifdef MOZ_MAIL_NEWS
            if ((((iFormatOut & 0x1f ) == FO_PRESENT) 
				  && (GetContext()->type != MWContextMetaFile) 
        		  && MSG_NewWindowRequired(GetContext(), pUrl->address)) 
				  || bForceNew)

            {
                MWContextType type = MWContextBrowser;
                if ( MSG_RequiresMailWindow ( pUrl->address ) )    
                    type = MWContextMail;
                else if ( MSG_RequiresNewsWindow ( pUrl->address ) )
                    type = MWContextNews;
               
                if (( ( type == MWContextNews ) || ( type == MWContextMail ) ) && !bForceNew )
                {
        			CMailNewsFrame *pMailNewsFrame = NULL;
        			MSG_PaneType paneType = MSG_PaneTypeForURL(pUrl->address);

        			switch ( paneType )
        			{
        			case MSG_THREADPANE:
        				{
        				MSG_FolderInfo *folder = MSG_GetFolderInfoFromURL(WFE_MSGGetMaster(), pUrl->address);
						BOOL bContinue = FALSE;
        				if (folder){
        					C3PaneMailFrame::Open(folder, MSG_MESSAGEKEYNONE, &bContinue);
						}
						if (!bContinue)
							return MK_CHANGING_CONTEXT;
						else
							break;
						}
        			case MSG_MESSAGEPANE:
						{
        				C3PaneMailFrame *pThreadFrame = CMailNewsFrame::GetLastThreadFrame();
						if (pThreadFrame && pThreadFrame == CWnd::GetActiveWindow() &&
							pThreadFrame->GetMainContext() == this)
							break;
        				pMailNewsFrame = CMessageFrame::Open();
        				if ( pMailNewsFrame ) {
        					pUrl->msg_pane = pMailNewsFrame->GetPane();
        					pMailNewsFrame->GetContext()->GetUrl( pUrl, iFormatOut, bReallyLoad );
        				}
                        return MK_CHANGING_CONTEXT;
						}
        	        case MSG_FOLDERPANE:
        			default:
        				pMailNewsFrame = CFolderFrame::Open();
        				if ( pMailNewsFrame )
        					pMailNewsFrame->GetContext()->GetUrl( pUrl, iFormatOut, bReallyLoad );
                        return MK_CHANGING_CONTEXT;
        			}
                }
                else if (GetContextType() != IconCX && GetContextType() != Pane) {
                    MWContext* pNewContext = SwitchToBrowserContext(pUrl);
                    return MK_CHANGING_CONTEXT;
                }
            }
#endif // MOZ_MAIL_NEWS

        	//	Our first (well, almost) task is to interrupt the load.
        	//	Only one load per window should be instantiated by the front end.
            //  Do this only if this is the context which is going to handle it.
            //  If this is a view source request, don't interrupt the context
            //  because we will be creating a new window.
        	if(bNamedAnchor == FALSE && bReallyLoad == TRUE &&
                iFormatOut != FO_CACHE_AND_VIEW_SOURCE)	{
        		Interrupt();
        	}

        	//	Disable clicking.
        	GetContext()->waitingMode = TRUE;

        	//	The fun part about all this is, is that we don't want to call reentrantly
        	//		into the netlib, or else our idle interrupt routine will interrupt
        	//		both requests.  Check to see if we're waiting for an idle interrupt.
        	//	Use the client pull mechanism to get around this.
        	//	Also, if we're not supposed to really call to load this URL, then don't.
        	if(bReallyLoad == TRUE)	{
        		if(m_bIdleInterrupt == FALSE)	{
        			//	Simply call the netlib routine, filling in some default data.
        			//	Pass along the common exit routine, which will call the appropriate
        			//		contexts exit routine.
        			int iOldInProcessNet = winfeInProcessNet; 
        			winfeInProcessNet = TRUE;
        			StartAnimation();
					if ( m_cxType == IconCX)
        				iRetval = NET_GetURL(pUrl, iFormatOut, GetContext(), Icon_GetUrlExitRoutine);
					else
        				iRetval = NET_GetURL(pUrl, iFormatOut, GetContext(), CFE_GetUrlExitRoutine);
        			FE_UpdateStopState(GetContext());

        			winfeInProcessNet = iOldInProcessNet;
        		}
        		else	{
        			//	Set client pull on this URL.
                    FEU_ClientPull(GetContext(), 250, pUrl, iFormatOut, TRUE);

        			//	Return that no action was taken, yet.
        			iRetval = MK_NO_ACTION;
        		}
        	}
        	else	{
        		//	Indicate that we skipped loading since we're allowing someone to change
        		//		context.
        		iRetval = MK_CHANGING_CONTEXT;
        	}

        	//	If we were a named anchor, clean everything we did up.
        	if(bNamedAnchor == TRUE)	{
        		iRetval = MK_DATA_LOADED;

        		//	We need to update the location bar's text, since LayoutNewDocument was never
        		//		called (routine responsible for this).
        		if(IsFrameContext() && GetContext()->type == MWContextBrowser && !EDT_IS_EDITOR(GetContext())){

        			if(IsGridCell() == FALSE)	{
        				TRACE("Updating location bar for named anchor\n");
        				CWinCX *pWinCX = WINCX(GetContext());
        				CFrameGlue *pFrame = pWinCX->GetFrame();

        				if(pFrame){
        					CURLBar *pUrlBar = (CURLBar *)pFrame->GetChrome()->GetToolbar(ID_LOCATION_TOOLBAR);
        					if(pUrlBar)
        						pUrlBar->UpdateFields(pUrl->address);
        				}
        			}
        		}

        		//	Call the exit routine on the URL.
        		//	Just use the common one that would have been used anyhow.
        		CFE_GetUrlExitRoutine(pUrl, iRetval, GetContext());

        		//	If there aren't any connections for the window, also call all connections
        		//		complete.
        		//	Context by context version, don't use XP_IsContextBusy.
        		if(NET_AreThereActiveConnectionsForWindow(GetContext()) == FALSE)	{
        			FE_AllConnectionsComplete(GetContext());
        		}
        	}
        	break;
        }
    }

    //  All Done.
    return(iRetval);
}

void CAbstractCX::Reload(NET_ReloadMethod iReloadType)	{
//	Purpose:	Reloads the current document in the context's history.
//	Arguments:	void
//	Returns:	void
//	Comments:	Standard stuff.

	if(CanCreateUrlFromHist())	{
        URL_Struct *pUrl = CreateUrlFromHist(FALSE, NULL, iReloadType == NET_RESIZE_RELOAD);

        //	Force the load (normally).
        if(pUrl)    {
		    pUrl->force_reload = iReloadType;

		    GetUrl(pUrl, FO_CACHE_AND_PRESENT);
        }
	}
}

extern "C" Bool LO_LayingOut(MWContext * context);

void CAbstractCX::NiceReload(int usePassInType, NET_ReloadMethod iReloadType )	{
//	Purpose:	Reloads the current document in the context's history, only
//					if we're currently not loading.
//	Arguments:	void
//	Returns:	void
//	Comments:	Standard stuff.

	MWContext * context = GetContext();
	if(XP_IsContextBusy(context) == TRUE 
#ifndef MOZ_NGLAYOUT
|| LO_LayingOut(context)
#endif
)	{
		FEU_RequestIdleProcessing(context);
		context->reSize = TRUE;
	}
	else	{
		context->reSize = FALSE;
#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
#else
		NPL_SamePage(context);
#endif
		if( usePassInType ) {
			Reload( iReloadType );
			return;
		}

		if (MAIL_NEWS_TYPE(context->type)) {
		    Reload(NET_NORMAL_RELOAD);
		}
		else {
		    Reload(NET_RESIZE_RELOAD);
		}
	}
}

void CAbstractCX::Back()	{
//	Purpose:	Goes back one in the history.
//	Arguments:	void
//	Returns:	void
//	Comments:	Standard stuff.

	History_entry *pHist = SHIST_GetPrevious(GetContext());
	if (GetContext()->grid_children) {
#ifdef MOZ_NGLAYOUT
    XP_ASSERT(0);
#else
	    if (LO_BackInGrid(GetDocumentContext())) {
		return;
	    }
#endif
	}
	if(pHist)	{
		GetUrl(SHIST_CreateURLStructFromHistoryEntry(GetContext(), pHist), FO_CACHE_AND_PRESENT);
	}
}

void CAbstractCX::Forward()	{
//	Purpose:	Goes forward one in the history.
//	Arguments:	void
//	Returns:	void
//	Comments:	Standard stuff.

	History_entry *pHist = SHIST_GetNext(GetContext());
	if (GetContext()->grid_children) {
#ifdef MOZ_NGLAYOUT
    XP_ASSERT(0);
#else
	    if (LO_ForwardInGrid(GetDocumentContext())) {
		return;
	    }
#endif
	}
	if(pHist)	{
		GetUrl(SHIST_CreateURLStructFromHistoryEntry(GetContext(), pHist), FO_CACHE_AND_PRESENT);
	}
}

#ifndef MOZ_NGLAYOUT
XL_TextTranslation CAbstractCX::TranslateText(URL_Struct *pUrl, const char *pFileName, const char *pPrefix, int iWidth)  {
//	Purpose:    Translate a url into text.
//	Arguments:  pUrl        The URL to translate.
//              pFileName   The file in which to save the translation, CANNOT BE NULL.
//              pPrefix     A prefix for each line, CANNOT BE NULL.
//              iWidth      The width in characters of each line.
//	Returns:    XL_TextTranslation as XL_TranslateText
//	Comments:   Wraps all the work of manually calling the text translation routines.
//              There's no need to track the PrintSetup struct we create in this function,
//                  as it will come back in the translation exit routine and can be
//                  freed there.

    //  Create a new print setup.
    PrintSetup *pTextFE = XP_NEW(PrintSetup);

    //  Initialize.
    XL_InitializeTextSetup(pTextFE);

    //  Assign in the needed members.
    pTextFE->width = iWidth;
    if(pPrefix)
        pTextFE->prefix = XP_STRDUP(pPrefix);
    else
        pTextFE->prefix = XP_STRDUP("");
    pTextFE->eol = "\r\n";
    pTextFE->filename = XP_STRDUP(pFileName);
    pTextFE->out = fopen(pFileName, "wb");
    pTextFE->completion = CFE_TextTranslationExitRoutine;   //  Abstracted exit routine.
    pTextFE->url = pUrl;
    pTextFE->carg = CX2VOID(this, CAbstractCX);

    //  Do it.
    return(XL_TranslateText(GetContext(), pUrl, pTextFE));
}
#endif /* MOZ_NGLAYOUT */

void CAbstractCX::Interrupt()	{
//	Purpose:	Interrupt any loads in a context.
//	Arguments:	void
//	Returns:	void
//	Comments:	If able, this will interrupt any loads in a context.
//					If unable, then sets a flag for idle time interrupting.

	//	Increment our level of nested ness in the Interrupt function.
	//	Problem is, that this is the only function which actually deletes
	//		a context.
	//	If we're nested several levels, then each tries to free the context.
	m_iInterruptNest++;

	//	Provide a way to interrupt even if we're winfeInProcessNet but there
	//		is currently no activity.
	//	If we're not busy, then there's no need to interrupt.
	if(IsContextStoppable() == FALSE)	{
		//	Clear the idle interrupt if already set.
		m_bIdleInterrupt = FALSE;
	}
	//	Reentrant safe version.
	else if(winfeInProcessNet == FALSE)	{
		winfeInProcessNet = TRUE;
		XP_InterruptContext(GetContext());
		winfeInProcessNet = FALSE;

		//	Set the flag that we need no idle interrupt.
		//	This is to avoid cases where we have set to idle interrupt, and get
		//		called again where we actually do interrupt.
		m_bIdleInterrupt = FALSE;
	}
	else	{
        //  Request some idle processing.
        FEU_RequestIdleProcessing(GetContext());
		m_bIdleInterrupt = TRUE;
	}

	//	Decrement the level of nestedness.
	m_iInterruptNest--;

	//	Check to see if we're supposed to delete this thing.
	//	Idle interrupt will be true if unable to do anything, and will need
	//		idle time so we don't destroy it then.
	//	m_bDestroyed will be true, requesting the deletion.
	//	m_iInterruptNest will be 0, indicating that we are at the bottom
	//		most level of interrupt call.
	if(m_bDestroyed == TRUE &&
		m_bIdleInterrupt == FALSE &&
		m_iInterruptNest == 0)	{

		//	Delete.
		delete this;
	}

	return;
}

BOOL CAbstractCX::IsContextStoppableRecurse() {
   MWContext *pChild;

   if (!GetContext())
	   return FALSE;
   if ((m_bImagesLoading  || m_bImagesLooping) && !m_bImagesDelayed)
	   return TRUE;

   if ((m_bMochaImagesLoading || m_bMochaImagesLooping) && !m_bMochaImagesDelayed)
	   return TRUE;

   XP_List *pTraverse = GetContext()->grid_children;
   while (pChild = (MWContext *)XP_ListNextObject(pTraverse)) {
	   if (ABSTRACTCX(pChild)->IsContextStoppableRecurse())
			return TRUE;
   }   
   
   return FALSE;
}

// A wrapper around XP_IsContextStoppable.  This is necessary
// because a context is stoppable if its image context is
// stoppable, and the image context is owned by the Front End.
BOOL CAbstractCX::IsContextStoppable() {
	return (IsContextStoppableRecurse() ||
			XP_IsContextStoppable(GetContext()));
}

void CAbstractCX::MailDocument()	{
#ifdef MOZ_MAIL_NEWS
	if(CanMailDocument())	{
#ifdef EDITOR
    if (EDT_IS_EDITOR(GetContext())) {
      // Don't need to force saving document, EDT_MailDocument
      // will do the right thing.
      EDT_MailDocument(GetContext());
    }
    else {
      MSG_MailDocumentURL(GetContext(),NULL);
    }
#endif
	}
#endif /* MOZ_MAIL_NEWS */
}

BOOL CAbstractCX::CanMailDocument()	{
	BOOL bRetval = TRUE;
   
#ifdef MOZ_MAIL_NEWS
	if (!theApp.m_hPostalLib)
      return(FALSE);
#endif // MOZ_MAIL_NEWS

	if(IsDestroyed())	{
		bRetval = FALSE;
	}
	else if(CanCreateUrlFromHist() == FALSE)	{
		bRetval = FALSE;
	}
	else if(IsGridParent())	{
		bRetval = FALSE;
	}

	return(bRetval);
}

void CAbstractCX::OpenUrl()	{
	//	If we've a parent context, let them deal with this.
	if( GetParentContext() )	{
		ABSTRACTCX(GetParentContext())->OpenUrl();
	} else if( !IsDestroyed() )	{
		ASSERT( GetDialogOwner() );
		//	Have the frame bring up the open url dialog.
		if( GetDialogOwner() ) {
		    CDialogURL urlDlg(GetDialogOwner(), GetContext());
    		urlDlg.DoModal();
		}
	}
}

BOOL CAbstractCX::CanOpenUrl()	{
	//	If we've a parent context, let them deal with this.
	if(GetParentContext())	{
		return(ABSTRACTCX(GetParentContext())->CanOpenUrl());
	} else if(IsDestroyed())	{
		return(FALSE);
	} else {
		//	Perhaps not if we're in some kiosk mode.
		return( GetDialogOwner() ? TRUE : FALSE);
	}
}

void CAbstractCX::NewWindow()	{
	//	Create a new window.
	if(CanNewWindow())	{
		CFE_CreateNewDocWindow(GetContext(), NULL);
	}
}

BOOL CAbstractCX::CanNewWindow()	{
	//	Can always get a new window (well....)
	BOOL bRetval = TRUE;
	if(IsDestroyed() == TRUE)	{
		bRetval = FALSE;
	}
	else	{
        // Limit windows only for win16
#ifdef XP_WIN16
		//	Limit to 8 top level browser contexts.
        //  We may have Edit windows open and we run into
        //   the limit of four real quick!
		if(XP_ContextCount(MWContextBrowser, TRUE) >= 8)	{
			bRetval = FALSE;
		}
#endif
	}

	return(bRetval);
}

static void UpdateUI(CAbstractCX *pCX)
{
	if(pCX->IsFrameContext() == TRUE)	{
		CWinCX *pWinCX = WINCX(pCX->GetContext());
		CFrameGlue *pFrame = pWinCX->GetFrame();
		// We need to make sure the toolbar buttons are correctly updated. 
		// Most importantly, the stop button needs to be updated
		if(pFrame){
			CFrameWnd*	pFrameWnd = pFrame->GetFrameWnd();

			if (pFrameWnd)
				pFrameWnd->SendMessageToDescendants(WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);
		}
	}
}

void CAbstractCX::AllBack()	{
	//	Pass up to the parent for the ALL effect.
	if(GetParentContext())	{
		ABSTRACTCX(GetParentContext())->AllBack();
	}
	else if(IsDestroyed() == FALSE)	{
		Back();
		UpdateUI(this);
	}
}

BOOL CAbstractCX::CanAllBack()	{
	//	Ask the parent for the ALL effect.
	if(GetParentContext())	{
		return(ABSTRACTCX(GetParentContext())->CanAllBack());
	}

	BOOL bRetval = TRUE;
	if(IsDestroyed())	{
		bRetval = FALSE;
	}
	else	{
		//	Ask the session history.
		bRetval = SHIST_CanGoBack((GetContext()));
	}

	return(bRetval);
}

void CAbstractCX::AllForward()	{
	//	Pass up to the parent for the ALL effect.
	if(GetParentContext())	{
		ABSTRACTCX(GetParentContext())->AllForward();
	}
	else if(IsDestroyed() == FALSE)	{
		Forward();
		UpdateUI(this);
	}
}

BOOL CAbstractCX::CanAllForward()	{
	//	Ask the parent for the ALL effect.
	if(GetParentContext())	{
		return(ABSTRACTCX(GetParentContext())->CanAllForward());
	}

	BOOL bRetval = TRUE;
	if(IsDestroyed())	{
		bRetval = FALSE;
	}
	else	{
		//	Ask the session history.
		bRetval = SHIST_CanGoForward((GetContext()));
	}

	return(bRetval);
}

void CAbstractCX::AllInterrupt()	{
	//	Pass up to the parent for the ALL effect.
	if(GetParentContext())	{
		ABSTRACTCX(GetParentContext())->AllInterrupt();
	}
	else if(IsDestroyed() == FALSE)	{
		Interrupt();
	}
}

BOOL CAbstractCX::CanAllInterrupt()	{
	//	Ask the parent for the ALL effect.
	if(GetParentContext())	{
		return(ABSTRACTCX(GetParentContext())->CanAllInterrupt());
	}

	BOOL bRetval = TRUE;
	if(IsDestroyed())	{
		bRetval = FALSE;
	}
	else	{
		//	See if the context is busy.
		bRetval = IsContextStoppable();
	}

	return(bRetval);
}

void CAbstractCX::AllReload(NET_ReloadMethod iReloadType)	{
	//	Pass up to the parent for the ALL effect.
	if(GetParentContext())	{
		ABSTRACTCX(GetParentContext())->AllReload(iReloadType);
	}
	else if(IsDestroyed() == FALSE)	{
		Reload(iReloadType);
	}
}

BOOL CAbstractCX::CanAllReload()	{
	//	Ask the parent for the ALL effect.
	if(GetParentContext())	{
		return(ABSTRACTCX(GetParentContext())->CanAllReload());
	}

	BOOL bRetval = TRUE;
	if(IsDestroyed())	{
		bRetval = FALSE;
	}
	else	{
		//	See if the context can be reloaded (has history).
	    History_entry *pHist = SHIST_GetCurrent(&(GetContext()->hist));
		if(pHist == NULL)	{
			bRetval = FALSE;
		}
	}

	return(bRetval);
}

BOOL CAbstractCX::CanAddToBookmarks()
{
	if( GetParentContext() )	{
		return(ABSTRACTCX(GetParentContext())->CanAddToBookmarks());
	}

	if( IsDestroyed() )	{
		return(FALSE);
	}

	//	Can only do this if we have something currently loaded.
	if( !CanCreateUrlFromHist() )	{
		return(FALSE);
	}

	return(TRUE);
}

void CAbstractCX::AddToBookmarks()
{
	if( GetParentContext() )	{
		ABSTRACTCX(GetParentContext())->AddToBookmarks();
		return;
	}

	if( !CanAddToBookmarks() )	{
		return;
	}

	History_entry *pHistEnt = SHIST_GetCurrent( &(GetContext()->hist) );
	ASSERT(pHistEnt);

	HT_AddBookmark( pHistEnt->address, GetContext()->title );
}

void CAbstractCX::CopySelection()	{
#ifdef MOZ_NGLAYOUT
    ASSERT(0);
#else
	if(CanCopySelection() == FALSE)	{
		return;
	}

	HANDLE h;
	HANDLE hData;
	char * string;
	char * text;

	text = (char *)LO_GetSelectionText(GetDocumentContext());
	if(!text)	{
		//	Layout is reporting a selection, but we can't get it.
		ASSERT(0);
		return;
	}

	if(!::OpenClipboard(NULL)) {
		FE_Alert(GetContext(), szLoadString(IDS_OPEN_CLIPBOARD));
		return;
	}

	if(!::EmptyClipboard()) {
		FE_Alert(GetContext(), szLoadString(IDS_EMPTY_CLIPBOARD));
		return;
	}
      
	int len = XP_STRLEN(text) + 1;

	hData = GlobalAlloc(GMEM_MOVEABLE, (DWORD) len);
	string = (char *) GlobalLock(hData);
	strncpy(string, text, len);
	string[len - 1] = '\0';
	GlobalUnlock(hData);

	h = ::SetClipboardData(CF_TEXT, hData); 

#ifdef XP_WIN32
	int datacsid = GetWinCsid() & ~CS_AUTO;
	if((CS_USER_DEFINED_ENCODING != datacsid) && (0 != datacsid))
	{
		len = CASTINT((INTL_StrToUnicodeLen(datacsid, (unsigned char*)text)+1) * 2);
		hData = GlobalAlloc(GMEM_MOVEABLE, (DWORD) len);
		string = (char *) GlobalLock(hData);
		INTL_StrToUnicode(datacsid, (unsigned char*)text, (INTL_Unicode*)string, len);

		GlobalUnlock(hData);		
	}
	h = ::SetClipboardData(CF_UNICODETEXT, hData); 

#endif

	::CloseClipboard();
	XP_FREE(text);
#endif
}

BOOL CAbstractCX::CanCopySelection()	{
	BOOL bRetval;
    History_entry *pHist = SHIST_GetCurrent(&(GetContext()->hist));
	if(IsDestroyed())	{
		bRetval = FALSE;
	}
	else if(pHist == NULL)	{
			bRetval = FALSE;
	}
	else	{
#ifdef MOZ_NGLAYOUT
    XP_ASSERT(0);
    bRetval = FALSE;
#else
		//	Is there anything selected to be copied?
		bRetval = LO_HaveSelection(GetDocumentContext());
#endif
	}

	return(bRetval);
}

//	Look up a context via ID.
//	Used mainly by DDE and external application context lookup (out of process).
//	Returns NULL on failure.
//	To get the context ID, call FE_GetContextID or CAbstractCX::GetContextID
CAbstractCX *CAbstractCX::FindContextByID(DWORD dwID)
{
	MWContext *pTraverseContext = NULL;
	CAbstractCX *pTraverseCX = NULL;

	XP_List * thelist = XP_GetGlobalContextList();
	while (pTraverseContext = (MWContext *)XP_ListNextObject(thelist))  {
		if(pTraverseContext != NULL && ABSTRACTCX(pTraverseContext) != NULL)	{
			pTraverseCX = ABSTRACTCX(pTraverseContext);

			if(pTraverseCX->GetContextID() == dwID)	{
				break;
			}
			pTraverseCX = NULL;
		}
	}

	return(pTraverseCX);
}

//	Create a url struct from the history, with appropriate checking
//		for NULL and such, so that we don't have this code scattered
//		throughout the client.
//	bClearStateData is a flag set to erase any data that can't be
//		tossed around without upsetting the client (form data).
//	It's off by default, so be careful out there.
URL_Struct *CAbstractCX::CreateUrlFromHist(BOOL bClearStateData, SHIST_SavedData *pSavedData, BOOL bWysiwyg)
{
#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
  return NULL;
#else
	TRACE("Creating URL from the current history entry.\n");

	//	Make sure that we're not destroyed.
	if(IsDestroyed())	{
		return(NULL);
	}

	//	Do we have a history entry from which to create
	//		the url?
	History_entry *pHistoryEntry = SHIST_GetCurrent(&(GetContext()->hist));
	if(pHistoryEntry == NULL || pHistoryEntry->address == NULL)	{
		return(NULL);
	}

	URL_Struct *pUrl = NULL;
    if(!bWysiwyg)   {
        pUrl = SHIST_CreateURLStructFromHistoryEntry(GetContext(), pHistoryEntry);
    }
    else    {
        pUrl = SHIST_CreateWysiwygURLStruct(GetContext(), pHistoryEntry);
    }

	if(pUrl == NULL)	{
		return(NULL);
	}
	
	if(bClearStateData != FALSE)	{
		TRACE("Clearing URL state data.\n");

        //  We may be wanting to copy the form data beyond the scope of this function.
        //  Make sure layout saves the current state.
        LO_SaveFormData(GetDocumentContext());

        //  Are we to copy the session history to the parameter passed in (used in
        //      subsequent call to copy the form data)?
        if(pSavedData)  {
            memcpy(pSavedData, (void *)&(pUrl->savedData), sizeof(SHIST_SavedData));
        }

		//	If this structure changes in the future to hold data which can be carried
		//		across contexts, then we lose.
		memset((void *)&(pUrl->savedData), 0, sizeof(SHIST_SavedData));
	}

	return(pUrl);
#endif /* MOZ_NGLAYOUT */
}

//	Used mainly in cmdui enablers to determine if we could load from 
//		the current history entry.
BOOL CAbstractCX::CanCreateUrlFromHist()
{
	//	Make sure that we're not destroyed.
	if(IsDestroyed())	{
		return(FALSE);
	}

	//	Do we have a history entry from which to create
	//		the url?
	History_entry *pHistoryEntry = SHIST_GetCurrent(&(GetContext()->hist));
	if(pHistoryEntry == NULL || pHistoryEntry->address == NULL)	{
		return(FALSE);
	}

	//	Looks safe.
	return(TRUE);
}

void CAbstractCX::ResetStopwatch()
{
		m_ttStopwatch = theApp.GetTime();
		m_ttOldwatch = m_ttStopwatch - 1;
}


