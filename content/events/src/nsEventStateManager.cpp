/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#define FORCE_PR_LOG

#include "nsCOMPtr.h"
#include "nsIEventStateManager.h"
#include "nsEventStateManager.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsIWidget.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsDOMEvent.h"
#include "nsHTMLAtoms.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLButtonElement.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsINameSpaceManager.h"  // for kNameSpaceID_HTML
#include "nsIWebShell.h"
#include "nsIBaseWindow.h"
#include "nsIScrollableView.h"
#include "nsIDOMSelection.h"
#include "nsIFrameSelection.h"
#include "nsIDeviceContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsISelfScrollingFrame.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIGfxTextControlFrame.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIEnumerator.h"
#include "nsFrameTraversal.h"

#include "nsIServiceManager.h"
#include "nsIPref.h"
#include "nsISessionHistory.h"

#include "nsXULAtoms.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIObserverService.h"
#include "prlog.h"

//we will use key binding by default now. this wil lbreak viewer for now
#define NON_KEYBINDING 0  

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

nsIContent * gLastFocusedContent = 0; // Strong reference
nsIDocument * gLastFocusedDocument = 0; // Strong reference
nsIPresContext* gLastFocusedPresContext = 0; // Weak reference

PRLogModuleInfo *MOUSEWHEEL;

PRUint32 nsEventStateManager::mInstanceCount = 0;

enum {
 MOUSE_SCROLL_N_LINES,
 MOUSE_SCROLL_PAGE,
 MOUSE_SCROLL_HISTORY
};

nsEventStateManager::nsEventStateManager()
  : mGestureDownPoint(0,0),
    m_haveShutdown(PR_FALSE)
{
  mLastMouseOverFrame = nsnull;
  mLastDragOverFrame = nsnull;
  mCurrentTarget = nsnull;
  mCurrentTargetContent = nsnull;
  mCurrentRelatedContent = nsnull;
  mLastLeftMouseDownContent = nsnull;
  mLastMiddleMouseDownContent = nsnull;
  mLastRightMouseDownContent = nsnull;

  mConsumeFocusEvents = PR_FALSE;

  // init d&d gesture state machine variables
  mIsTrackingDragGesture = PR_FALSE;
  mGestureDownFrame = nsnull;

  mLClickCount = 0;
  mMClickCount = 0;
  mRClickCount = 0;
  mActiveContent = nsnull;
  mHoverContent = nsnull;
  mDragOverContent = nsnull;
  mCurrentFocus = nsnull;
  mDocument = nsnull;
  mPresContext = nsnull;
  mCurrentTabIndex = 0;
  mLastWindowToHaveFocus = nsnull;
  mFirstBlurEvent = nsnull;
  mFirstFocusEvent = nsnull;
  NS_INIT_REFCNT();
  
  ++mInstanceCount;
  if (!MOUSEWHEEL)
    MOUSEWHEEL = PR_NewLogModule("MOUSEWHEEL");
}

NS_IMETHODIMP
nsEventStateManager::Init()
{
  nsresult rv;
  NS_WITH_SERVICE(nsIObserverService, observerService,
                  NS_OBSERVERSERVICE_PROGID, &rv);
  if (NS_SUCCEEDED(rv))
  {
    nsAutoString topic; topic.AssignWithConversion(NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    observerService->AddObserver(this, topic.GetUnicode());
  }

  rv = getPrefService();
  return rv;
}

nsEventStateManager::~nsEventStateManager()
{
  NS_IF_RELEASE(mCurrentTargetContent);
  NS_IF_RELEASE(mCurrentRelatedContent);

  NS_IF_RELEASE(mLastLeftMouseDownContent);
  NS_IF_RELEASE(mLastMiddleMouseDownContent);
  NS_IF_RELEASE(mLastRightMouseDownContent);

  NS_IF_RELEASE(mActiveContent);
  NS_IF_RELEASE(mHoverContent);
  NS_IF_RELEASE(mDragOverContent);
  NS_IF_RELEASE(mCurrentFocus);

  NS_IF_RELEASE(mDocument);

  NS_IF_RELEASE(mLastWindowToHaveFocus);
  NS_IF_RELEASE(mFirstBlurEvent);
  NS_IF_RELEASE(mFirstFocusEvent);
  
  --mInstanceCount;
  if(mInstanceCount == 0) {
    NS_IF_RELEASE(gLastFocusedContent);
    NS_IF_RELEASE(gLastFocusedDocument);
  }

  if (!m_haveShutdown) {
    Shutdown();

    // Don't remove from Observer service in Shutdown because Shutdown also
    // gets called from xpcom shutdown observer.  And we don't want to remove
    // from the service in that case.

    nsresult rv;

    NS_WITH_SERVICE (nsIObserverService, observerService,
                     NS_OBSERVERSERVICE_PROGID, &rv);
    if (NS_SUCCEEDED(rv))
      {
        nsAutoString topic; topic.AssignWithConversion(NS_XPCOM_SHUTDOWN_OBSERVER_ID);
        observerService->RemoveObserver(this, topic.GetUnicode());
      }
  }
  
}

nsresult
nsEventStateManager::Shutdown()
{
  if (mPrefService) {
    mPrefService = null_nsCOMPtr();
  }

  m_haveShutdown = PR_TRUE;
  return NS_OK;
}

nsresult
nsEventStateManager::getPrefService()
{
  nsresult rv = NS_OK;

  if (!mPrefService) {
    mPrefService = do_GetService(kPrefCID, &rv);
  }

  if (NS_FAILED(rv)) return rv;

  if (!mPrefService) return NS_ERROR_FAILURE;

  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::Observe(nsISupports *aSubject, const PRUnichar *aTopic,
                             const PRUnichar *someData) {
  nsAutoString topicString(aTopic);
  nsAutoString shutdownString; shutdownString.AssignWithConversion(NS_XPCOM_SHUTDOWN_OBSERVER_ID);

  if (topicString == shutdownString)
    Shutdown();

  return NS_OK;
}


NS_IMPL_ISUPPORTS1(nsEventStateManager, nsIEventStateManager)

NS_IMETHODIMP
nsEventStateManager::PreHandleEvent(nsIPresContext* aPresContext, 
                                 nsGUIEvent *aEvent,
                                 nsIFrame* aTargetFrame,
                                 nsEventStatus* aStatus,
                                 nsIView* aView)
{
  NS_ENSURE_ARG_POINTER(aStatus);
  NS_ENSURE_ARG(aPresContext);

  mCurrentTarget = aTargetFrame;
  NS_IF_RELEASE(mCurrentTargetContent);

  nsFrameState state;

  NS_ASSERTION(mCurrentTarget, "mCurrentTarget is null.  this should not happen.  see bug #13007");
  if (!mCurrentTarget) return NS_ERROR_NULL_POINTER;

  mCurrentTarget->GetFrameState(&state);
  state |= NS_FRAME_EXTERNAL_REFERENCE;
  mCurrentTarget->SetFrameState(state);

  *aStatus = nsEventStatus_eIgnore;
  
  NS_ASSERTION(aEvent, "aEvent is null.  this should never happen");
  if (!aEvent) return NS_ERROR_NULL_POINTER;

  switch (aEvent->message) {
  case NS_MOUSE_LEFT_BUTTON_DOWN:
    BeginTrackingDragGesture ( aEvent, aTargetFrame );
    mLClickCount = ((nsMouseEvent*)aEvent)->clickCount;
    SetClickCount(aPresContext, (nsMouseEvent*)aEvent, aStatus);
    break;
  case NS_MOUSE_MIDDLE_BUTTON_DOWN:
    mMClickCount = ((nsMouseEvent*)aEvent)->clickCount;
    SetClickCount(aPresContext, (nsMouseEvent*)aEvent, aStatus);
    break;
  case NS_MOUSE_RIGHT_BUTTON_DOWN:
    mRClickCount = ((nsMouseEvent*)aEvent)->clickCount;
    SetClickCount(aPresContext, (nsMouseEvent*)aEvent, aStatus);
    break;
  case NS_MOUSE_LEFT_BUTTON_UP:
    StopTrackingDragGesture();
  case NS_MOUSE_MIDDLE_BUTTON_UP:
  case NS_MOUSE_RIGHT_BUTTON_UP:
    SetClickCount(aPresContext, (nsMouseEvent*)aEvent, aStatus);
    break;
  case NS_MOUSE_MOVE:
    // on the Mac, GenerateDragGesture() may not return until the drag has completed
    // and so |aTargetFrame| may have been deleted (moving a bookmark, for example).
    // If this is the case, however, we know that ClearFrameRefs() has been called
    // and it cleared out |mCurrentTarget|. As a result, we should pass |mCurrentTarget|
    // into UpdateCursor().
    GenerateDragGesture(aPresContext, aEvent);
    UpdateCursor(aPresContext, aEvent, mCurrentTarget, aStatus);
    GenerateMouseEnterExit(aPresContext, aEvent);
    break;
  case NS_MOUSE_EXIT:
    GenerateMouseEnterExit(aPresContext, aEvent);
    //This is a window level mouseenter event and should stop here
    aEvent->message = 0;
    break;
  case NS_DRAGDROP_OVER:
    GenerateDragDropEnterExit(aPresContext, aEvent);
    break;
  case NS_GOTFOCUS:
    {
#ifdef DEBUG_hyatt
      printf("Got focus.\n");
#endif

      nsCOMPtr<nsIPresShell> presShell;
      aPresContext->GetShell(getter_AddRefs(presShell));
      
      if (!mDocument) {  
        if (presShell) {
          presShell->GetDocument(&mDocument);
        }
      }

      if (gLastFocusedDocument == mDocument)
         break;
        
      //fire focus
      nsEventStatus status = nsEventStatus_eIgnore;
      nsEvent focusevent;
      focusevent.eventStructType = NS_EVENT;
      focusevent.message = NS_FOCUS_CONTENT;

      if (mDocument) {
      
        if(gLastFocusedDocument && gLastFocusedDocument != mDocument) {
          // fire a blur, on the document only to keep ender happy
          nsEventStatus blurstatus = nsEventStatus_eIgnore;
          nsEvent blurevent;
          blurevent.eventStructType = NS_EVENT;
          blurevent.message = NS_BLUR_CONTENT;
      
          if(gLastFocusedPresContext) {
            gLastFocusedDocument->HandleDOMEvent(gLastFocusedPresContext, &blurevent, nsnull, NS_EVENT_FLAG_INIT, &blurstatus);
          }
        }
        
        // fire focus on window, not document
        nsCOMPtr<nsIScriptGlobalObject> globalObject;
        mDocument->GetScriptGlobalObject(getter_AddRefs(globalObject));
        if (globalObject) {
          nsIContent* currentFocus = mCurrentFocus;
          mCurrentFocus = nsnull;
          mDocument->HandleDOMEvent(aPresContext, &focusevent, nsnull, NS_EVENT_FLAG_INIT, &status);
          globalObject->HandleDOMEvent(aPresContext, &focusevent, nsnull, NS_EVENT_FLAG_INIT, &status); 
          mCurrentFocus = currentFocus;
        }

        
        NS_IF_RELEASE(gLastFocusedDocument);
        gLastFocusedDocument = mDocument;
        gLastFocusedPresContext = aPresContext;
        NS_IF_ADDREF(gLastFocusedDocument);
      }
    }
    break;
    
  case NS_LOSTFOCUS:
    {
      // Hold the blur, wait for the focus so we can query the style of the focus
      // target as to what to do with the event. If appropriate we fire the blur
      // at that time.
    }
    break;
    
 case NS_ACTIVATE:
    {
      // If we have a command dispatcher, and if it has a focused window and a
      // focused element in its focus memory, then restore the focus to those
      // objects.
      if (!mDocument) {
        nsCOMPtr<nsIPresShell> presShell;
        aPresContext->GetShell(getter_AddRefs(presShell));
        if (presShell) {
          presShell->GetDocument(&mDocument);
        }
      }

      nsCOMPtr<nsIDOMXULCommandDispatcher> commandDispatcher;
      nsCOMPtr<nsIDOMElement> focusedElement;
      nsCOMPtr<nsIDOMWindow> focusedWindow;
      nsCOMPtr<nsIDOMXULDocument> xulDoc = do_QueryInterface(mDocument);

      if (xulDoc) {
        // See if we have a command dispatcher attached.
        xulDoc->GetCommandDispatcher(getter_AddRefs(commandDispatcher));
        if (commandDispatcher) {
          // Obtain focus info from the command dispatcher.
          commandDispatcher->GetFocusedWindow(getter_AddRefs(focusedWindow));
          commandDispatcher->GetFocusedElement(getter_AddRefs(focusedElement));
        }
      }

	    if (!focusedWindow) {
          nsCOMPtr<nsIScriptGlobalObject> globalObject;
          mDocument->GetScriptGlobalObject(getter_AddRefs(globalObject));
          focusedWindow = do_QueryInterface(globalObject);
      }

	    // Sill no focused XULDocument, that is bad
	    if(!xulDoc && focusedWindow)
	    {
		    nsCOMPtr<nsPIDOMWindow> privateWindow = do_QueryInterface(focusedWindow);
        if(privateWindow){
		      nsCOMPtr<nsIDOMWindow> privateRootWindow;
		      privateWindow->GetPrivateRoot(getter_AddRefs(privateRootWindow));
          if(privateRootWindow) {
			      nsCOMPtr<nsIDOMDocument> privateParentDoc;
			      privateRootWindow->GetDocument(getter_AddRefs(privateParentDoc));
            xulDoc = do_QueryInterface(privateParentDoc);
          }
        }
		  
		    if (xulDoc) {
          // See if we have a command dispatcher attached.
          xulDoc->GetCommandDispatcher(getter_AddRefs(commandDispatcher));
          if (commandDispatcher) {
            // Obtain focus info from the command dispatcher.
            commandDispatcher->GetFocusedWindow(getter_AddRefs(focusedWindow));
            commandDispatcher->GetFocusedElement(getter_AddRefs(focusedElement));
          }
        }
	    }

      // Focus the DOM window.
      focusedWindow->Focus();

      if (focusedElement) {
        nsCOMPtr<nsIContent> focusContent = do_QueryInterface(focusedElement);
        nsCOMPtr<nsIDOMDocument> domDoc;
        nsCOMPtr<nsIDocument> document;
        focusedWindow->GetDocument(getter_AddRefs(domDoc));
        document = do_QueryInterface(domDoc);
        nsCOMPtr<nsIPresShell> shell;
        nsCOMPtr<nsIPresContext> context;
        shell = getter_AddRefs(document->GetShellAt(0));
        shell->GetPresContext(getter_AddRefs(context));
        focusContent->SetFocus(context);
      }       

      if (commandDispatcher) {
        commandDispatcher->SetActive(PR_TRUE);
        commandDispatcher->SetSuppressFocus(PR_FALSE); // Unsuppress and let the command dispatcher listen again.
      }  
    }
  break;
    
 case NS_DEACTIVATE:
    {
      if (!mDocument) {
        nsCOMPtr<nsIPresShell> presShell;
        aPresContext->GetShell(getter_AddRefs(presShell));
        if (presShell) {
          presShell->GetDocument(&mDocument);
        }
      }

      // We can get a deactivate on an Ender widget.  In this
      // case, we would like to obtain the DOM Window to start
      // with by looking at gLastFocusedContent.
      nsCOMPtr<nsIScriptGlobalObject> ourGlobal;
      if (gLastFocusedContent) {
        nsCOMPtr<nsIDocument> doc;
        gLastFocusedContent->GetDocument(*getter_AddRefs(doc));
		if(doc)
          doc->GetScriptGlobalObject(getter_AddRefs(ourGlobal));
        else {
		  mDocument->GetScriptGlobalObject(getter_AddRefs(ourGlobal));
		  NS_RELEASE(gLastFocusedContent);
		}
	  }
      else mDocument->GetScriptGlobalObject(getter_AddRefs(ourGlobal));
      
      // Suppress the command dispatcher for the duration of the
      // de-activation.  This will cause it to remember the last
      // focused sub-window and sub-element for this top-level
      // window.
      nsCOMPtr<nsIDOMXULCommandDispatcher> commandDispatcher;
      nsCOMPtr<nsIDOMWindow> rootWindow;
      nsCOMPtr<nsPIDOMWindow> ourWindow = do_QueryInterface(ourGlobal);
      if(ourWindow) {
        ourWindow->GetPrivateRoot(getter_AddRefs(rootWindow));
        if(rootWindow) {
          nsCOMPtr<nsIDOMDocument> rootDocument;
          rootWindow->GetDocument(getter_AddRefs(rootDocument));

          nsCOMPtr<nsIDOMXULDocument> xulDoc = do_QueryInterface(rootDocument);
          if (xulDoc) {
            // See if we have a command dispatcher attached.
            xulDoc->GetCommandDispatcher(getter_AddRefs(commandDispatcher));
            if (commandDispatcher) {
              // Suppress the command dispatcher.
              commandDispatcher->SetSuppressFocus(PR_TRUE);
            }
          }
        }
      }

      // Now fire blurs.  We have to fire a blur on the focused window
      // and on the focused element if there is one.
      if (gLastFocusedDocument && gLastFocusedPresContext) {
        // Blur the element.
        if (gLastFocusedContent) {
          // Retrieve this content node's pres context. it can be out of sync in
          // the Ender widget case.
          nsCOMPtr<nsIDocument> doc;
          gLastFocusedContent->GetDocument(*getter_AddRefs(doc));
          if (doc) {
            nsCOMPtr<nsIPresShell> shell = getter_AddRefs(doc->GetShellAt(0));
            if (shell) {
              nsCOMPtr<nsIPresContext> oldPresContext;
              shell->GetPresContext(getter_AddRefs(oldPresContext));

              nsEventStatus status = nsEventStatus_eIgnore;
              nsEvent event;
              event.eventStructType = NS_EVENT;
              event.message = NS_BLUR_CONTENT;
              nsCOMPtr<nsIEventStateManager> esm;
              oldPresContext->GetEventStateManager(getter_AddRefs(esm));
              esm->SetFocusedContent(gLastFocusedContent);
              gLastFocusedContent->HandleDOMEvent(oldPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status); 
              esm->SetFocusedContent(nsnull);
              NS_IF_RELEASE(gLastFocusedContent);
            }
          }
        }

        nsEventStatus status = nsEventStatus_eIgnore;
        nsEvent event;
        event.eventStructType = NS_EVENT;
        event.message = NS_BLUR_CONTENT;
    
        // fire blur on document and window
        nsCOMPtr<nsIScriptGlobalObject> globalObject;
        if(gLastFocusedDocument) {
          gLastFocusedDocument->GetScriptGlobalObject(getter_AddRefs(globalObject));
          gLastFocusedDocument->HandleDOMEvent(gLastFocusedPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
          if(globalObject)
            globalObject->HandleDOMEvent(gLastFocusedPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
        }
        // Now clear our our global variables
        mCurrentTarget = nsnull;
        NS_IF_RELEASE(gLastFocusedDocument);
        gLastFocusedPresContext = nsnull;
      }             

      if (commandDispatcher) {
        commandDispatcher->SetActive(PR_FALSE);
        commandDispatcher->SetSuppressFocus(PR_FALSE);
      }
    } 
    
    break;
    
  case NS_KEY_PRESS:
  case NS_KEY_DOWN:
  case NS_KEY_UP:
  case NS_MOUSE_SCROLL:
    {
      if (mCurrentFocus) {
        mCurrentTargetContent = mCurrentFocus;
        NS_ADDREF(mCurrentTargetContent);
      }
    }
    break;
  }
  return NS_OK;
}


// 
// BeginTrackingDragGesture
//
// Record that the mouse has gone down and that we should move to TRACKING state
// of d&d gesture tracker.
//
void
nsEventStateManager :: BeginTrackingDragGesture ( nsGUIEvent* inDownEvent, nsIFrame* inDownFrame )
{
  mIsTrackingDragGesture = PR_TRUE;
  mGestureDownPoint = inDownEvent->point;
  mGestureDownFrame = inDownFrame;
}


//
// StopTrackingDragGesture
//
// Record that the mouse has gone back up so that we should leave the TRACKING 
// state of d&d gesture tracker and return to the START state.
//
void
nsEventStateManager :: StopTrackingDragGesture ( )
{
  mIsTrackingDragGesture = PR_FALSE;
  mGestureDownPoint = nsPoint(0,0);
  mGestureDownFrame = nsnull;
}


//
// GenerateDragGesture
//
// If we're in the TRACKING state of the d&d gesture tracker, check the current position
// of the mouse in relation to the old one. If we've moved a sufficient amount from
// the mouse down, then fire off a drag gesture event.
//
// Note that when the mouse enters a new child window with its own view, the event's
// coordinates will be in relation to the origin of the inner child window, which could
// either be very different from that of the mouse coords of the mouse down and trigger
// a drag too early, or very similiar which might not trigger a drag. 
//
// Do we need to do anything about this? Let's wait and see.
//
void
nsEventStateManager :: GenerateDragGesture ( nsIPresContext* aPresContext, nsGUIEvent *aEvent )
{
  NS_WARN_IF_FALSE(aPresContext, "This shouldn't happen.");
  if ( IsTrackingDragGesture() ) {
    // figure out the delta in twips, since that is how it is in the event.
    // Do we need to do this conversion every time? Will the pres context really change on
    // us or can we cache it?
    long twipDeltaToStartDrag = 0;
    const long pixelDeltaToStartDrag = 5;
    nsCOMPtr<nsIDeviceContext> devContext;
    aPresContext->GetDeviceContext ( getter_AddRefs(devContext) );
    if ( devContext ) {
      float pixelsToTwips = 0.0;
      devContext->GetDevUnitsToTwips(pixelsToTwips);
      twipDeltaToStartDrag =  (long)(pixelDeltaToStartDrag * pixelsToTwips);
    }
 
    // fire drag gesture if mouse has moved enough
    if ( abs(aEvent->point.x - mGestureDownPoint.x) > twipDeltaToStartDrag ||
          abs(aEvent->point.y - mGestureDownPoint.y) > twipDeltaToStartDrag ) {
      nsEventStatus status = nsEventStatus_eIgnore;
      nsMouseEvent event;
      event.eventStructType = NS_DRAGDROP_EVENT;
      event.message = NS_DRAGDROP_GESTURE;
      event.widget = aEvent->widget;
      event.clickCount = 0;
      event.point = aEvent->point;
      event.refPoint = aEvent->refPoint;
      event.isShift = ((nsMouseEvent*)aEvent)->isShift;
      event.isControl = ((nsMouseEvent*)aEvent)->isControl;
      event.isAlt = ((nsMouseEvent*)aEvent)->isAlt;
      event.isMeta = ((nsMouseEvent*)aEvent)->isMeta;

      // dispatch to the DOM
      nsCOMPtr<nsIContent> lastContent;
      if ( mGestureDownFrame ) {
        mGestureDownFrame->GetContentForEvent(aPresContext, aEvent, getter_AddRefs(lastContent));
        if ( lastContent )
          lastContent->HandleDOMEvent(aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status); 
      }
      
      // dispatch to the frame
      if ( mGestureDownFrame )
        mGestureDownFrame->HandleEvent(aPresContext, &event, &status);   

      StopTrackingDragGesture();              
    }
  }
} // GenerateDragGesture


NS_IMETHODIMP
nsEventStateManager::PostHandleEvent(nsIPresContext* aPresContext, 
                                     nsGUIEvent *aEvent,
                                     nsIFrame* aTargetFrame,
                                     nsEventStatus* aStatus,
                                     nsIView* aView)
{
  NS_ENSURE_ARG(aPresContext);
  NS_ENSURE_ARG_POINTER(aStatus);
  mCurrentTarget = aTargetFrame;
  NS_IF_RELEASE(mCurrentTargetContent);  
  nsresult ret = NS_OK;

  nsFrameState state;
  mCurrentTarget->GetFrameState(&state);
  state |= NS_FRAME_EXTERNAL_REFERENCE;
  mCurrentTarget->SetFrameState(state);

  switch (aEvent->message) {
  case NS_MOUSE_LEFT_BUTTON_DOWN:
  case NS_MOUSE_MIDDLE_BUTTON_DOWN:
  case NS_MOUSE_RIGHT_BUTTON_DOWN: 
    {
      if (mConsumeFocusEvents) {
        mConsumeFocusEvents = PR_FALSE;
        break;
      }

      if (nsEventStatus_eConsumeNoDefault != *aStatus) {
        nsCOMPtr<nsIContent> newFocus;
        PRBool suppressBlur = PR_FALSE;
        if (mCurrentTarget) {
          mCurrentTarget->GetContentForEvent(mPresContext, aEvent, getter_AddRefs(newFocus));
          const nsStyleUserInterface* ui;
          mCurrentTarget->GetStyleData(eStyleStruct_UserInterface, ((const nsStyleStruct*&)ui));;
          suppressBlur = (ui->mUserFocus == NS_STYLE_USER_FOCUS_IGNORE);
        }

        nsIFrame* currFrame = mCurrentTarget;
        // Look for the nearest enclosing focusable frame.
        while (currFrame) {
          const nsStyleUserInterface* ui;
          currFrame->GetStyleData(eStyleStruct_UserInterface, ((const nsStyleStruct*&)ui));;
          if ((ui->mUserFocus != NS_STYLE_USER_FOCUS_IGNORE) &&
              (ui->mUserFocus != NS_STYLE_USER_FOCUS_NONE)) {
            currFrame->GetContent(getter_AddRefs(newFocus));
            break;
          }
          currFrame->GetParent(&currFrame);
        }

        if (newFocus && currFrame)
          ChangeFocus(newFocus, currFrame, PR_TRUE);
        else if (!suppressBlur)
          SetContentState(nsnull, NS_EVENT_STATE_FOCUS);

        SetContentState(newFocus, NS_EVENT_STATE_ACTIVE);
      }
    }
    break;
  case NS_MOUSE_LEFT_BUTTON_UP:
  case NS_MOUSE_MIDDLE_BUTTON_UP:
  case NS_MOUSE_RIGHT_BUTTON_UP:
    {
      ret = CheckForAndDispatchClick(aPresContext, (nsMouseEvent*)aEvent, aStatus);

      SetContentState(nsnull, NS_EVENT_STATE_ACTIVE);
      nsCOMPtr<nsIPresShell> shell;
      nsresult rv = aPresContext->GetShell(getter_AddRefs(shell));
      if (NS_SUCCEEDED(rv) && shell){
        nsCOMPtr<nsIFrameSelection> frameSel;
        rv = shell->GetFrameSelection(getter_AddRefs(frameSel));
        if (NS_SUCCEEDED(rv) && frameSel){
            frameSel->SetMouseDownState(PR_FALSE);
        }
      }
    }
    break;
  case NS_MOUSE_SCROLL:
    if (nsEventStatus_eConsumeNoDefault != *aStatus) {
      nsresult rv;
      rv = getPrefService();
      if (NS_FAILED(rv)) return rv;

      nsMouseScrollEvent *msEvent = (nsMouseScrollEvent*) aEvent;
      PRInt32 action = 0;
      PRInt32 numLines = 0;
      PRBool  aBool;


      if (msEvent->isShift) {
        mPrefService->GetIntPref("mousewheel.withshiftkey.action", &action);
        mPrefService->GetBoolPref("mousewheel.withshiftkey.sysnumlines", &aBool);
        if (aBool)
          numLines = msEvent->deltaLines;
        else
          mPrefService->GetIntPref("mousewheel.withshiftkey.numlines", &numLines);
      } else if (msEvent->isControl) {
        mPrefService->GetIntPref("mousewheel.withcontrolkey.action", &action);
        mPrefService->GetBoolPref("mousewheel.withcontrolkey.sysnumlines", &aBool);
        if (aBool)
          numLines = msEvent->deltaLines;
        else
          mPrefService->GetIntPref("mousewheel.withcontrolkey.numlines", &numLines);
      } else if (msEvent->isAlt) {
        mPrefService->GetIntPref("mousewheel.withaltkey.action", &action);
        mPrefService->GetBoolPref("mousewheel.withaltkey.sysnumlines", &aBool);
        if (aBool)
          numLines = msEvent->deltaLines;
        else
          mPrefService->GetIntPref("mousewheel.withaltkey.numlines", &numLines);
      } else {
        mPrefService->GetIntPref("mousewheel.withnokey.action", &action);
        mPrefService->GetBoolPref("mousewheel.withnokey.sysnumlines", &aBool);
        if (aBool)
          numLines = msEvent->deltaLines;
        else
          mPrefService->GetIntPref("mousewheel.withnokey.numlines", &numLines);
      }

      if ((msEvent->deltaLines < 0) && (numLines > 0))
        numLines = -numLines;

      switch (action) {
      case MOUSE_SCROLL_N_LINES:
        {
          nsIView* focusView = nsnull;
          nsIScrollableView* sv = nsnull;
          nsISelfScrollingFrame* sf = nsnull;
          nsIPresContext* mwPresContext = aPresContext;
          
#ifdef USE_FOCUS_FOR_MOUSEWHEEL
          if (NS_SUCCEEDED(GetScrollableFrameOrView(sv, sf, focusView)))
#else
          if (NS_SUCCEEDED(GetScrollableFrameOrView(mwPresContext,
                                                      aTargetFrame, aView, sv,
                                                      sf, focusView)))
#endif
            {
              if (sv)
                {
                  sv->ScrollByLines(numLines);
                  ForceViewUpdate(focusView);
                }
              else if (sf)
                sf->ScrollByLines(aPresContext, numLines);
            }
        // We may end up with a different PresContext than we started with.
        // If so, we are done with it now, so release it.

        if (mwPresContext != aPresContext)
          NS_RELEASE(mwPresContext);

        }

        break;

      case MOUSE_SCROLL_PAGE:
        {
          nsIView* focusView = nsnull;
          nsIScrollableView* sv = nsnull;
          nsISelfScrollingFrame* sf = nsnull;
          
#ifdef USE_FOCUS_FOR_MOUSEWHEEL
          if (NS_SUCCEEDED(GetScrollableFrameOrView(sv, sf, focusView)))
#else
          if (NS_SUCCEEDED(GetScrollableFrameOrView(aPresContext, aTargetFrame,
                                                    aView, sv, sf, focusView)))
#endif
            {
              if (sv)
                {
                  sv->ScrollByPages((numLines > 0) ? 1 : -1);
                  ForceViewUpdate(focusView);
                }
              else if (sf)
                sf->ScrollByPages(aPresContext, (numLines > 0) ? 1 : -1);
            }
        }
        break;
        
      case MOUSE_SCROLL_HISTORY:
        {
          nsCOMPtr<nsISupports> pcContainer;
          mPresContext->GetContainer(getter_AddRefs(pcContainer));
          if (pcContainer) {
            nsCOMPtr<nsIWebShell> webShell = do_QueryInterface(pcContainer);
            if (webShell) {
              nsCOMPtr<nsIWebShell> root;
              webShell->GetRootWebShell(*getter_AddRefs(root));
              if (nsnull != root) {
                nsCOMPtr<nsISessionHistory> sHist;
                root->GetSessionHistory(*getter_AddRefs(sHist));
                if (sHist) {
                  if (numLines > 0)
                  {
                    sHist->GoBack(root);
                  }
                  else
                  {
                    sHist->GoForward(root);
                  }
                }
              }
            }
          }
        }
        break;

      }
      *aStatus = nsEventStatus_eConsumeNoDefault;

    }

    break;
  
  case NS_DRAGDROP_DROP:
  case NS_DRAGDROP_EXIT:
    // clean up after ourselves. make sure we do this _after_ the event, else we'll
    // clean up too early!
    GenerateDragDropEnterExit(aPresContext, aEvent);
    break;
  case NS_KEY_PRESS:
    if (nsEventStatus_eConsumeNoDefault != *aStatus) {
      nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
      //This is to prevent keyboard scrolling while alt modifier in use.
      if (!keyEvent->isAlt) {
        switch(keyEvent->keyCode) {
          case NS_VK_TAB:
            if (mConsumeFocusEvents) {
              mConsumeFocusEvents = PR_FALSE;
              break;
            }
            //Shift focus forward or back depending on shift key
            ShiftFocus(!((nsInputEvent*)aEvent)->isShift);
            *aStatus = nsEventStatus_eConsumeNoDefault;
            break;

//the problem is that viewer does not have xul so we cannot completely eliminate these 
#if NON_KEYBINDING
          case NS_VK_PAGE_DOWN: 
          case NS_VK_PAGE_UP:
            if (!mCurrentFocus) {
              nsIScrollableView* sv = GetNearestScrollingView(aView);
              if (sv) {
                nsKeyEvent * keyEvent = (nsKeyEvent *)aEvent;
                sv->ScrollByPages((keyEvent->keyCode != NS_VK_PAGE_UP) ? 1 : -1);
              }
            }
            break;
          case NS_VK_HOME: 
          case NS_VK_END:
            if (!mCurrentFocus) {
              nsIScrollableView* sv = GetNearestScrollingView(aView);
              if (sv) {
                nsKeyEvent * keyEvent = (nsKeyEvent *)aEvent;
                sv->ScrollByWhole((keyEvent->keyCode != NS_VK_HOME) ? PR_FALSE : PR_TRUE);
              }
            }
            break;
          case NS_VK_DOWN: 
          case NS_VK_UP:
            if (!mCurrentFocus) {
              nsIScrollableView* sv = GetNearestScrollingView(aView);
              if (sv) {
                nsKeyEvent * keyEvent = (nsKeyEvent *)aEvent;
                sv->ScrollByLines((keyEvent->keyCode == NS_VK_DOWN) ? 1 : -1);
              
                // force the update to happen now, otherwise multiple scrolls can
                // occur before the update is processed. (bug #7354)
                nsIViewManager* vm = nsnull;
             	  if (NS_OK == aView->GetViewManager(vm) && nsnull != vm) {
             	    // I'd use Composite here, but it doesn't always work.
                  // vm->Composite();
                  vm->ForceUpdate();
                  NS_RELEASE(vm);
                }
              }
            }
            break;
        case 0: /* check charcode since keycode is 0 */
          {
          //Spacebar
            nsKeyEvent * keyEvent = (nsKeyEvent *)aEvent;
            if (keyEvent->charCode == 0x20) {
              if (!mCurrentFocus) {
                nsIScrollableView* sv = GetNearestScrollingView(aView);
                if (sv) {
                  sv->ScrollByPages(1);
                }
              }
            }
          }
          break;
#endif //NON_KEYBINDING
        }
      }
    }
  }

  //Reset target frame to null to avoid mistargetting after reentrant event
  mCurrentTarget = nsnull;

  return ret;
}

NS_IMETHODIMP
nsEventStateManager::SetPresContext(nsIPresContext* aPresContext)
{
  if (aPresContext == nsnull) {
    // A pres context is going away. Make sure we do cleanup.
    if (mPresContext == gLastFocusedPresContext) {
      gLastFocusedPresContext = nsnull;
      NS_IF_RELEASE(gLastFocusedDocument);
      NS_IF_RELEASE(gLastFocusedContent);
    }
  }

  mPresContext = aPresContext;
  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::ClearFrameRefs(nsIFrame* aFrame)
{
  if (aFrame == mLastMouseOverFrame)
    mLastMouseOverFrame = nsnull;
  if (aFrame == mLastDragOverFrame)
    mLastDragOverFrame = nsnull;
  if (aFrame == mGestureDownFrame)
    mGestureDownFrame = nsnull;
  if (aFrame == mCurrentTarget) {
    if (aFrame) {
      aFrame->GetContent(&mCurrentTargetContent);
    }
    mCurrentTarget = nsnull;
  }
  return NS_OK;
}

nsIScrollableView*
nsEventStateManager::GetNearestScrollingView(nsIView* aView)
{
  nsIScrollableView* sv;
  if (NS_OK == aView->QueryInterface(NS_GET_IID(nsIScrollableView),
                                     (void**)&sv)) {
    return sv;
  }

  nsIView* parent;
  aView->GetParent(parent);

  if (nsnull != parent) {
    return GetNearestScrollingView(parent);
  }

  return nsnull;
}

nsISelfScrollingFrame*
nsEventStateManager::GetParentSelfScrollingFrame(nsIFrame* aFrame)
{
  nsISelfScrollingFrame *sf;
  if (NS_OK == aFrame->QueryInterface(NS_GET_IID(nsISelfScrollingFrame),
                                      (void**)&sf)) {
    return sf;
  }

  nsIFrame* parent;
  aFrame->GetParent(&parent);

  if (nsnull != parent) {
    return GetParentSelfScrollingFrame(parent);
  }

  return nsnull;
}

PRBool
nsEventStateManager::CheckDisabled(nsIContent* aContent)
{
  PRBool disabled = PR_FALSE;

  nsCOMPtr<nsIAtom> tag;
  aContent->GetTag(*getter_AddRefs(tag));

  if (nsHTMLAtoms::input == tag.get() ||
      nsHTMLAtoms::select == tag.get() ||
      nsHTMLAtoms::textarea == tag.get() ||
      nsHTMLAtoms::button == tag.get()) {
    nsAutoString empty;
    if (NS_CONTENT_ATTR_HAS_VALUE == aContent->GetAttribute(kNameSpaceID_HTML, 
                                                           nsHTMLAtoms::disabled,
                                                           empty)) {
      disabled = PR_TRUE;
    }
  }
  
  return disabled;
}

void
nsEventStateManager::UpdateCursor(nsIPresContext* aPresContext, nsEvent* aEvent, nsIFrame* aTargetFrame, 
                                  nsEventStatus* aStatus)
{
  PRInt32 cursor;
  nsCursor c;
  nsCOMPtr<nsIContent> targetContent;

  if (mCurrentTarget) {
    mCurrentTarget->GetContent(getter_AddRefs(targetContent));
  }

  //Check if the current target is disabled.  If so use the default pointer.
  if (targetContent && CheckDisabled(targetContent)) {
    cursor = NS_STYLE_CURSOR_DEFAULT;
  }
  //If not disabled, check for the right cursor.
  else {
    if ( aTargetFrame )
      aTargetFrame->GetCursor(aPresContext, aEvent->point, cursor);
  }

  switch (cursor) {
  default:
  case NS_STYLE_CURSOR_AUTO:
  case NS_STYLE_CURSOR_DEFAULT:
    c = eCursor_standard;
    break;
  case NS_STYLE_CURSOR_POINTER:
    c = eCursor_hyperlink;
    break;
  case NS_STYLE_CURSOR_CROSSHAIR:
    c = eCursor_crosshair;
    break;
  case NS_STYLE_CURSOR_MOVE:
    c = eCursor_move;
    break;
  case NS_STYLE_CURSOR_TEXT:
    c = eCursor_select;
    break;
  case NS_STYLE_CURSOR_WAIT:
    c = eCursor_wait;
    break;
  case NS_STYLE_CURSOR_HELP:
    c = eCursor_help;
    break;
  case NS_STYLE_CURSOR_N_RESIZE:
  case NS_STYLE_CURSOR_S_RESIZE:
    c = eCursor_sizeNS;
    break;
  case NS_STYLE_CURSOR_W_RESIZE:
  case NS_STYLE_CURSOR_E_RESIZE:
    c = eCursor_sizeWE;
    break;
  //These aren't in the CSS2 spec. Don't know what to do with them.
  case NS_STYLE_CURSOR_NE_RESIZE:
  case NS_STYLE_CURSOR_NW_RESIZE:
  case NS_STYLE_CURSOR_SE_RESIZE:
  case NS_STYLE_CURSOR_SW_RESIZE:
    c = eCursor_select;
    break;
  }

  if (NS_STYLE_CURSOR_AUTO != cursor) {
    *aStatus = nsEventStatus_eConsumeDoDefault;
  }

  if ( aTargetFrame ) {
    nsCOMPtr<nsIWidget> window;
    aTargetFrame->GetWindow(aPresContext, getter_AddRefs(window));
    window->SetCursor(c);
  }
}

void
nsEventStateManager::GenerateMouseEnterExit(nsIPresContext* aPresContext, nsGUIEvent* aEvent)
{
  //Hold onto old target content through the event and reset after.
  nsCOMPtr<nsIContent> targetBeforeEvent = mCurrentTargetContent;

  switch(aEvent->message) {
  case NS_MOUSE_MOVE:
    {
      nsCOMPtr<nsIContent> targetContent;
      if (mCurrentTarget) {
        mCurrentTarget->GetContentForEvent(aPresContext, aEvent, getter_AddRefs(targetContent));
      }

      if (mLastMouseOverContent.get() != targetContent.get()) {

        //Before firing mouseout, check for recursion
        if (mLastMouseOverContent.get() != mFirstMouseOutEventContent.get()) {
        
          //Store the first mouseOut event we fire and don't refire mouseOut
          //to that element while the first mouseOut is still ongoing.
          mFirstMouseOutEventContent = mLastMouseOverContent;

          if (mLastMouseOverFrame) {
            //fire mouseout
            nsEventStatus status = nsEventStatus_eIgnore;
            nsMouseEvent event;
            event.eventStructType = NS_MOUSE_EVENT;
            event.message = NS_MOUSE_EXIT;
            event.widget = aEvent->widget;
            event.clickCount = 0;
            event.point = aEvent->point;
            event.refPoint = aEvent->refPoint;
            event.isShift = ((nsMouseEvent*)aEvent)->isShift;
            event.isControl = ((nsMouseEvent*)aEvent)->isControl;
            event.isAlt = ((nsMouseEvent*)aEvent)->isAlt;
            event.isMeta = ((nsMouseEvent*)aEvent)->isMeta;

            mCurrentTargetContent = mLastMouseOverContent;
            NS_IF_ADDREF(mCurrentTargetContent);
            mCurrentRelatedContent = targetContent;
            NS_IF_ADDREF(mCurrentRelatedContent);

            //XXX This event should still go somewhere!!
            if (mLastMouseOverContent) {
              mLastMouseOverContent->HandleDOMEvent(aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status); 
            }

            //Now dispatch to the frame
            if (mLastMouseOverFrame) {
              //XXX Get the new frame
              mLastMouseOverFrame->HandleEvent(aPresContext, &event, &status);   
            }

            NS_IF_RELEASE(mCurrentTargetContent);
            NS_IF_RELEASE(mCurrentRelatedContent);

            mFirstMouseOutEventContent = nsnull;
          }
        }

        //Before firing mouseover, check for recursion
        if (targetContent.get() != mFirstMouseOverEventContent.get()) {
        
          //Store the first mouseOver event we fire and don't refire mouseOver
          //to that element while the first mouseOver is still ongoing.
          mFirstMouseOverEventContent = targetContent;

          //fire mouseover
          nsEventStatus status = nsEventStatus_eIgnore;
          nsMouseEvent event;
          event.eventStructType = NS_MOUSE_EVENT;
          event.message = NS_MOUSE_ENTER;
          event.widget = aEvent->widget;
          event.clickCount = 0;
          event.point = aEvent->point;
          event.refPoint = aEvent->refPoint;
          event.isShift = ((nsMouseEvent*)aEvent)->isShift;
          event.isControl = ((nsMouseEvent*)aEvent)->isControl;
          event.isAlt = ((nsMouseEvent*)aEvent)->isAlt;
          event.isMeta = ((nsMouseEvent*)aEvent)->isMeta;

          mCurrentTargetContent = targetContent;
          NS_IF_ADDREF(mCurrentTargetContent);
          mCurrentRelatedContent = mLastMouseOverContent;
          NS_IF_ADDREF(mCurrentRelatedContent);

          //XXX This event should still go somewhere!!
          if (targetContent) {
            targetContent->HandleDOMEvent(aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status); 
          }
        
          if (nsEventStatus_eConsumeNoDefault != status) {
            SetContentState(targetContent, NS_EVENT_STATE_HOVER);
          }

          //Now dispatch to the frame
          if (mCurrentTarget) {
            //XXX Get the new frame
            mCurrentTarget->HandleEvent(aPresContext, &event, &status);
          }

          NS_IF_RELEASE(mCurrentTargetContent);
          NS_IF_RELEASE(mCurrentRelatedContent);
          mLastMouseOverFrame = mCurrentTarget;
          mLastMouseOverContent = targetContent;

          mFirstMouseOverEventContent = nsnull;
        }
      }
    }
    break;
  case NS_MOUSE_EXIT:
    {
      //This is actually the window mouse exit event.
      if (nsnull != mLastMouseOverFrame) {
        //Before firing mouseout, check for recursion
        if (mLastMouseOverContent.get() != mFirstMouseOutEventContent.get()) {
    
          //Store the first mouseOut event we fire and don't refire mouseOut
          //to that element while the first mouseOut is still ongoing.
          mFirstMouseOutEventContent = mLastMouseOverContent;

          //fire mouseout
          nsEventStatus status = nsEventStatus_eIgnore;
          nsMouseEvent event;
          event.eventStructType = NS_MOUSE_EVENT;
          event.message = NS_MOUSE_EXIT;
          event.widget = aEvent->widget;
          event.clickCount = 0;
          event.point = aEvent->point;
          event.refPoint = aEvent->refPoint;
          event.isShift = ((nsMouseEvent*)aEvent)->isShift;
          event.isControl = ((nsMouseEvent*)aEvent)->isControl;
          event.isAlt = ((nsMouseEvent*)aEvent)->isAlt;
          event.isMeta = ((nsMouseEvent*)aEvent)->isMeta;

          mCurrentTargetContent = mLastMouseOverContent;
          NS_IF_ADDREF(mCurrentTargetContent);
          mCurrentRelatedContent = nsnull;

          if (mLastMouseOverContent) {
            mLastMouseOverContent->HandleDOMEvent(aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status); 

            if (nsEventStatus_eConsumeNoDefault != status) {
              SetContentState(nsnull, NS_EVENT_STATE_HOVER);
            }
          }


          //Now dispatch to the frame
          if (nsnull != mLastMouseOverFrame) {
            //XXX Get the new frame
            mLastMouseOverFrame->HandleEvent(aPresContext, &event, &status);   
            mLastMouseOverFrame = nsnull;
            mLastMouseOverContent = nsnull;
          }

          NS_IF_RELEASE(mCurrentTargetContent);

          mFirstMouseOutEventContent = nsnull;
        }
      }
    }
    break;
  }

  //reset mCurretTargetContent to what it was
  mCurrentTargetContent = targetBeforeEvent;
}

void
nsEventStateManager::GenerateDragDropEnterExit(nsIPresContext* aPresContext, nsGUIEvent* aEvent)
{
  //Hold onto old target content through the event and reset after.
  nsCOMPtr<nsIContent> targetBeforeEvent = mCurrentTargetContent;

  switch(aEvent->message) {
  case NS_DRAGDROP_OVER:
    {
      if (mLastDragOverFrame != mCurrentTarget) {
        //We'll need the content, too, to check if it changed separately from the frames.
        nsCOMPtr<nsIContent> lastContent;
        nsCOMPtr<nsIContent> targetContent;
        mCurrentTarget->GetContentForEvent(aPresContext, aEvent, getter_AddRefs(targetContent));

        if ( mLastDragOverFrame ) {
          //fire drag exit
          nsEventStatus status = nsEventStatus_eIgnore;
          nsMouseEvent event;
          event.eventStructType = NS_DRAGDROP_EVENT;
          event.message = NS_DRAGDROP_EXIT;
          event.widget = aEvent->widget;
          event.clickCount = 0;
          event.point = aEvent->point;
          event.refPoint = aEvent->refPoint;
          event.isShift = ((nsMouseEvent*)aEvent)->isShift;
          event.isControl = ((nsMouseEvent*)aEvent)->isControl;
          event.isAlt = ((nsMouseEvent*)aEvent)->isAlt;
          event.isMeta = ((nsMouseEvent*)aEvent)->isMeta;

          //The frame has change but the content may not have.  Check before dispatching to content
          mLastDragOverFrame->GetContentForEvent(aPresContext, aEvent, getter_AddRefs(lastContent));

          mCurrentTargetContent = lastContent;
          NS_IF_ADDREF(mCurrentTargetContent);
          mCurrentRelatedContent = targetContent;
          NS_IF_ADDREF(mCurrentRelatedContent);

          if ( lastContent != targetContent ) {
            //XXX This event should still go somewhere!!
            if (lastContent)
              lastContent->HandleDOMEvent(aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status); 

            // clear the drag hover
            if (status != nsEventStatus_eConsumeNoDefault )
              SetContentState(nsnull, NS_EVENT_STATE_DRAGOVER);
          }

          // Finally dispatch exit to the frame
          if ( mLastDragOverFrame ) {
            mLastDragOverFrame->HandleEvent(aPresContext, &event, &status);   

          NS_IF_RELEASE(mCurrentTargetContent);
          NS_IF_RELEASE(mCurrentRelatedContent);

          }
        }

        //fire drag enter
        nsEventStatus status = nsEventStatus_eIgnore;
        nsMouseEvent event;
        event.eventStructType = NS_DRAGDROP_EVENT;
        event.message = NS_DRAGDROP_ENTER;
        event.widget = aEvent->widget;
        event.clickCount = 0;
        event.point = aEvent->point;
        event.refPoint = aEvent->refPoint;
        event.isShift = ((nsMouseEvent*)aEvent)->isShift;
        event.isControl = ((nsMouseEvent*)aEvent)->isControl;
        event.isAlt = ((nsMouseEvent*)aEvent)->isAlt;
        event.isMeta = ((nsMouseEvent*)aEvent)->isMeta;

        mCurrentTargetContent = targetContent;
        NS_IF_ADDREF(mCurrentTargetContent);
        mCurrentRelatedContent = lastContent;
        NS_IF_ADDREF(mCurrentRelatedContent);

        //The frame has change but the content may not have.  Check before dispatching to content
        if ( lastContent != targetContent ) {
          //XXX This event should still go somewhere!!
          if ( targetContent ) 
            targetContent->HandleDOMEvent(aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status); 

          // set drag hover on this frame
          if ( status != nsEventStatus_eConsumeNoDefault )
            SetContentState(targetContent, NS_EVENT_STATE_DRAGOVER);
        }

        // Finally dispatch to the frame
        if (mCurrentTarget) {
          //XXX Get the new frame
          mCurrentTarget->HandleEvent(aPresContext, &event, &status);
        }

        NS_IF_RELEASE(mCurrentTargetContent);
        NS_IF_RELEASE(mCurrentRelatedContent);
        mLastDragOverFrame = mCurrentTarget;
      }
    }
    break;
    
  case NS_DRAGDROP_DROP:
  case NS_DRAGDROP_EXIT:
    {
      //This is actually the window mouse exit event.
      if ( mLastDragOverFrame ) {

        // fire mouseout
        nsEventStatus status = nsEventStatus_eIgnore;
        nsMouseEvent event;
        event.eventStructType = NS_DRAGDROP_EVENT;
        event.message = NS_DRAGDROP_EXIT;
        event.widget = aEvent->widget;
        event.clickCount = 0;
        event.point = aEvent->point;
        event.refPoint = aEvent->refPoint;
        event.isShift = ((nsMouseEvent*)aEvent)->isShift;
        event.isControl = ((nsMouseEvent*)aEvent)->isControl;
        event.isAlt = ((nsMouseEvent*)aEvent)->isAlt;
        event.isMeta = ((nsMouseEvent*)aEvent)->isMeta;

        // dispatch to content via DOM
        nsCOMPtr<nsIContent> lastContent;
        mLastDragOverFrame->GetContentForEvent(aPresContext, aEvent, getter_AddRefs(lastContent));

        mCurrentTargetContent = lastContent;
        NS_IF_ADDREF(mCurrentTargetContent);
        mCurrentRelatedContent = nsnull;

        if ( lastContent ) {
          lastContent->HandleDOMEvent(aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status); 
          if ( status != nsEventStatus_eConsumeNoDefault )
            SetContentState(nsnull, NS_EVENT_STATE_DRAGOVER);
        }

        // Finally dispatch to the frame
        if ( mLastDragOverFrame ) {
          //XXX Get the new frame
          mLastDragOverFrame->HandleEvent(aPresContext, &event, &status);   
          mLastDragOverFrame = nsnull;
        }

        NS_IF_RELEASE(mCurrentTargetContent);
     }
    }
    break;
  }

  //reset mCurretTargetContent to what it was
  mCurrentTargetContent = targetBeforeEvent;
}

NS_IMETHODIMP
nsEventStateManager::SetClickCount(nsIPresContext* aPresContext, 
                                   nsMouseEvent *aEvent,
                                   nsEventStatus* aStatus)
{
  nsresult ret = NS_OK;
  nsCOMPtr<nsIContent> mouseContent;

  mCurrentTarget->GetContentForEvent(aPresContext, aEvent, getter_AddRefs(mouseContent));

  switch (aEvent->message) {
  case NS_MOUSE_LEFT_BUTTON_DOWN:
    NS_IF_RELEASE(mLastLeftMouseDownContent);
    mLastLeftMouseDownContent = mouseContent;
    NS_IF_ADDREF(mLastLeftMouseDownContent);
    break;

  case NS_MOUSE_LEFT_BUTTON_UP:
    if (mLastLeftMouseDownContent == mouseContent.get()) {
      aEvent->clickCount = mLClickCount;
      mLClickCount = 0;
    }
    else {
      aEvent->clickCount = 0;
    }
    NS_IF_RELEASE(mLastLeftMouseDownContent);
    break;

  case NS_MOUSE_MIDDLE_BUTTON_DOWN:
    NS_IF_RELEASE(mLastMiddleMouseDownContent);
    mLastMiddleMouseDownContent = mouseContent;
    NS_IF_ADDREF(mLastMiddleMouseDownContent);
    break;

  case NS_MOUSE_MIDDLE_BUTTON_UP:
    if (mLastMiddleMouseDownContent == mouseContent.get()) {
      aEvent->clickCount = mMClickCount;
      mMClickCount = 0;
    }
    else {
      aEvent->clickCount = 0;
    }
    NS_IF_RELEASE(mLastMiddleMouseDownContent);
    break;

  case NS_MOUSE_RIGHT_BUTTON_DOWN:
    NS_IF_RELEASE(mLastRightMouseDownContent);
    mLastRightMouseDownContent = mouseContent;
    NS_IF_ADDREF(mLastRightMouseDownContent);
    break;

  case NS_MOUSE_RIGHT_BUTTON_UP:
    if (mLastRightMouseDownContent == mouseContent.get()) {
      aEvent->clickCount = mRClickCount;
      mRClickCount = 0;
    }
    else {
      aEvent->clickCount = 0;
    }
    NS_IF_RELEASE(mLastRightMouseDownContent);
    break;
  }

  return ret;
}

NS_IMETHODIMP
nsEventStateManager::CheckForAndDispatchClick(nsIPresContext* aPresContext, 
                                              nsMouseEvent *aEvent,
                                              nsEventStatus* aStatus)
{
  nsresult ret = NS_OK;
  nsMouseEvent event;
  nsCOMPtr<nsIContent> mouseContent;

  mCurrentTarget->GetContentForEvent(aPresContext, aEvent, getter_AddRefs(mouseContent));

  //If mouse is still over same element, clickcount will be > 1.
  //If it has moved it will be zero, so no click.
  if (0 != aEvent->clickCount) {
    //fire click
    switch (aEvent->message) {
    case NS_MOUSE_LEFT_BUTTON_UP:
      event.message = NS_MOUSE_LEFT_CLICK;
      break;
    case NS_MOUSE_MIDDLE_BUTTON_UP:
      event.message = NS_MOUSE_MIDDLE_CLICK;
      break;
    case NS_MOUSE_RIGHT_BUTTON_UP:
      event.message = NS_MOUSE_RIGHT_CLICK;
      break;
    }

    event.eventStructType = NS_MOUSE_EVENT;
    event.widget = aEvent->widget;
    event.point = aEvent->point;
    event.refPoint = aEvent->refPoint;
    event.clickCount = aEvent->clickCount;
    event.isShift = aEvent->isShift;
    event.isControl = aEvent->isControl;
    event.isAlt = aEvent->isAlt;
    event.isMeta = aEvent->isMeta;

    if (mouseContent) {
      ret = mouseContent->HandleDOMEvent(aPresContext, &event, nsnull,
                                         NS_EVENT_FLAG_INIT, aStatus); 
	  NS_ASSERTION(NS_SUCCEEDED(ret), "HandleDOMEvent failed");
    }

    if (nsnull != mCurrentTarget) {
      ret = mCurrentTarget->HandleEvent(aPresContext, &event, aStatus);
    }
  }
  return ret;
}

PRBool
nsEventStateManager::ChangeFocus(nsIContent* aFocusContent, nsIFrame* aTargetFrame, PRBool aSetFocus)
{
  aFocusContent->SetFocus(mPresContext);
  return PR_FALSE;
} 

void
nsEventStateManager::ShiftFocus(PRBool forward)
{
  PRBool topOfDoc = PR_FALSE;

  if (nsnull == mPresContext) {
    return;
  }

  if (nsnull == mDocument) {
    nsCOMPtr<nsIPresShell> presShell;
    mPresContext->GetShell(getter_AddRefs(presShell));
    if (presShell) {
      presShell->GetDocument(&mDocument);
      if (nsnull == mDocument) {
        return;
      }
    }
  }
  
  if (nsnull == mCurrentFocus) {
    mCurrentFocus = mDocument->GetRootContent();
    if (nsnull == mCurrentFocus) {  
      return;
    }
    mCurrentTabIndex = forward ? 1 : 0;
    topOfDoc = PR_TRUE;
  }

  nsIFrame* primaryFrame;
  nsCOMPtr<nsIPresShell> shell;
  if (mPresContext) {
    nsresult rv = mPresContext->GetShell(getter_AddRefs(shell));
    if (NS_SUCCEEDED(rv) && shell){
      shell->GetPrimaryFrameFor(mCurrentFocus, &primaryFrame);
    }
  }

  nsCOMPtr<nsIContent> rootContent = getter_AddRefs(mDocument->GetRootContent());

  nsCOMPtr<nsIContent> next;
  GetNextTabbableContent(rootContent, primaryFrame, forward, getter_AddRefs(next));

  if (!next) {
    PRBool focusTaken = PR_FALSE;

    SetContentState(nsnull, NS_EVENT_STATE_FOCUS);

    nsCOMPtr<nsISupports> container;
    mPresContext->GetContainer(getter_AddRefs(container));
    nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(container));
    if (docShellAsWin) {
      docShellAsWin->FocusAvailable(docShellAsWin, &focusTaken);
    }
      
    if (!focusTaken && !topOfDoc) {
      ShiftFocus(forward);
    }
    return;
  }

  // Now we need to get the content node's frame and reset 
  // the mCurrentTarget target.  Otherwise the focus
  // code will be slightly out of sync (with regards to
  // focusing widgets within the current target)
  if (shell)
    shell->GetPrimaryFrameFor(next, &mCurrentTarget);
  
  ChangeFocus(next, mCurrentTarget, PR_TRUE);

  NS_IF_RELEASE(mCurrentFocus);
  mCurrentFocus = next;
  NS_IF_ADDREF(mCurrentFocus);
}

/*
 * At some point this will need to be linked into HTML 4.0 tabindex
 */

NS_IMETHODIMP
nsEventStateManager::GetNextTabbableContent(nsIContent* aRootContent, nsIFrame* aFrame, PRBool forward,
                                            nsIContent** aResult)
{
  *aResult = nsnull;

  nsCOMPtr<nsIBidirectionalEnumerator> frameTraversal;
  nsresult result = NS_NewFrameTraversal(getter_AddRefs(frameTraversal), EXTENSIVE,
                                         mPresContext, aFrame);

  if (NS_FAILED(result))
    return NS_OK;

  if (forward)
    frameTraversal->Next();
  else frameTraversal->Prev();

  nsISupports* currentItem;
  frameTraversal->CurrentItem(&currentItem);
  nsIFrame* currentFrame = (nsIFrame*)currentItem;

  while (currentFrame) {
    nsCOMPtr<nsIContent> child;
    currentFrame->GetContent(getter_AddRefs(child));

    const nsStyleDisplay* disp;
    currentFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)disp));

    const nsStyleUserInterface* ui;
    currentFrame->GetStyleData(eStyleStruct_UserInterface, ((const nsStyleStruct*&)ui));;
    
    nsCOMPtr<nsIDOMElement> element(do_QueryInterface(child));

    // if collapsed or hidden, we don't get tabbed into.
    if ((disp->mVisible != NS_STYLE_VISIBILITY_COLLAPSE) &&
        (disp->mVisible != NS_STYLE_VISIBILITY_HIDDEN) && 
        (ui->mUserFocus != NS_STYLE_USER_FOCUS_IGNORE) &&
        (ui->mUserFocus != NS_STYLE_USER_FOCUS_NONE) && element) {
      nsCOMPtr<nsIAtom> tag;
      PRInt32 tabIndex = -1;
      PRBool disabled = PR_TRUE;
      PRBool hidden = PR_FALSE;

      child->GetTag(*getter_AddRefs(tag));
      nsCOMPtr<nsIDOMHTMLElement> htmlElement(do_QueryInterface(child));
      if (htmlElement) {
        if (nsHTMLAtoms::input==tag.get()) {
          nsCOMPtr<nsIDOMHTMLInputElement> nextInput(do_QueryInterface(child));
          if (nextInput) {
            nextInput->GetDisabled(&disabled);
            nextInput->GetTabIndex(&tabIndex);

            nsAutoString type;
            nextInput->GetType(type);
            if (type.EqualsIgnoreCase("hidden")) {
              hidden = PR_TRUE;
            }
          }
        }
        else if (nsHTMLAtoms::select==tag.get()) {
          nsCOMPtr<nsIDOMHTMLSelectElement> nextSelect(do_QueryInterface(child));
          if (nextSelect) {
            nextSelect->GetDisabled(&disabled);
            nextSelect->GetTabIndex(&tabIndex);
          }
        }
        else if (nsHTMLAtoms::textarea==tag.get()) {
          nsCOMPtr<nsIDOMHTMLTextAreaElement> nextTextArea(do_QueryInterface(child));
          if (nextTextArea) {
            nextTextArea->GetDisabled(&disabled);
            nextTextArea->GetTabIndex(&tabIndex);
          }
        }
        else if(nsHTMLAtoms::a==tag.get()) {
          nsCOMPtr<nsIDOMHTMLAnchorElement> nextAnchor(do_QueryInterface(child));
          if (nextAnchor)
            nextAnchor->GetTabIndex(&tabIndex);
          disabled = PR_FALSE;
        }
        else if(nsHTMLAtoms::button==tag.get()) {
          nsCOMPtr<nsIDOMHTMLButtonElement> nextButton(do_QueryInterface(child));
          if (nextButton) {
            nextButton->GetTabIndex(&tabIndex);
            nextButton->GetDisabled(&disabled);
          }
        }
        else if(nsHTMLAtoms::area==tag.get()) {
          nsCOMPtr<nsIDOMHTMLAreaElement> nextArea(do_QueryInterface(child));
          if (nextArea)
            nextArea->GetTabIndex(&tabIndex);
          disabled = PR_FALSE;
        }
        else if(nsHTMLAtoms::object==tag.get()) {
          nsCOMPtr<nsIDOMHTMLObjectElement> nextObject(do_QueryInterface(child));
          if (nextObject) 
            nextObject->GetTabIndex(&tabIndex);
          disabled = PR_FALSE;
        }
      }
      else {
        nsAutoString value;
        child->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::disabled, value);
        nsAutoString tabStr;
        child->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::tabindex, tabStr);
        if (!tabStr.IsEmpty()) {
          PRInt32 errorCode;
          tabIndex = tabStr.ToInteger(&errorCode);
        }
        if (!value.EqualsWithConversion("true"))
          disabled = PR_FALSE;
      }
      
      //TabIndex not set (-1) treated at same level as set to 0
      tabIndex = tabIndex < 0 ? 0 : tabIndex;

      if (!disabled && !hidden && mCurrentTabIndex == tabIndex) {
        *aResult = child;
        NS_IF_ADDREF(*aResult);
        return NS_OK;
      }
    }

    if (forward)
      frameTraversal->Next();
    else frameTraversal->Prev();

    frameTraversal->CurrentItem(&currentItem);
    currentFrame = (nsIFrame*)currentItem;
  }

  // Reached end or beginning of document
  //If already at lowest priority tab (0), end
  if (((forward) && (0 == mCurrentTabIndex)) ||
      ((!forward) && (1 == mCurrentTabIndex))) {
    return NS_OK;
  }
  //else continue looking for next highest priority tab
  mCurrentTabIndex = GetNextTabIndex(aRootContent, forward);
  return GetNextTabbableContent(aRootContent, nsnull, forward, aResult);
}

PRInt32
nsEventStateManager::GetNextTabIndex(nsIContent* aParent, PRBool forward)
{
  PRInt32 count, tabIndex, childTabIndex;
  nsCOMPtr<nsIContent> child;
  
  aParent->ChildCount(count);
 
  if (forward) {
    tabIndex = 0;
    for (PRInt32 index = 0; index < count; index++) {
      aParent->ChildAt(index, *getter_AddRefs(child));
      childTabIndex = GetNextTabIndex(child, forward);
      if (childTabIndex > mCurrentTabIndex && childTabIndex != tabIndex) {
        tabIndex = (tabIndex == 0 || childTabIndex < tabIndex) ? childTabIndex : tabIndex; 
      }
      
      nsAutoString tabIndexStr;
      child->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::tabindex, tabIndexStr);
      PRInt32 ec, val = tabIndexStr.ToInteger(&ec);
      if (NS_OK == ec && val > mCurrentTabIndex && val != tabIndex) {
        tabIndex = (tabIndex == 0 || val < tabIndex) ? val : tabIndex; 
      }
    }
  } 
  else { /* !forward */
    tabIndex = 1;
    for (PRInt32 index = 0; index < count; index++) {
      aParent->ChildAt(index, *getter_AddRefs(child));
      childTabIndex = GetNextTabIndex(child, forward);
      if ((mCurrentTabIndex==0 && childTabIndex > tabIndex) ||
          (childTabIndex < mCurrentTabIndex && childTabIndex > tabIndex)) {
        tabIndex = childTabIndex;
      }
      
      nsAutoString tabIndexStr;
      child->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::tabindex, tabIndexStr);
      PRInt32 ec, val = tabIndexStr.ToInteger(&ec);
      if (NS_OK == ec) {
        if ((mCurrentTabIndex==0 && val > tabIndex) ||
            (val < mCurrentTabIndex && val > tabIndex) ) {
          tabIndex = val;
        }
      }
    }
  }
  return tabIndex;
}


NS_IMETHODIMP
nsEventStateManager::GetEventTarget(nsIFrame **aFrame)
{
  if (!mCurrentTarget && mCurrentTargetContent) {
    nsCOMPtr<nsIPresShell> shell;
    if (mPresContext) {
      nsresult rv = mPresContext->GetShell(getter_AddRefs(shell));
      if (NS_SUCCEEDED(rv) && shell){
        shell->GetPrimaryFrameFor(mCurrentTargetContent, &mCurrentTarget);
      }
    }
  }

  *aFrame = mCurrentTarget;
  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::GetEventTargetContent(nsEvent* aEvent, nsIContent** aContent)
{
  if (aEvent &&
      (aEvent->message == NS_FOCUS_CONTENT ||
       aEvent->message == NS_BLUR_CONTENT)) {
    *aContent = mCurrentFocus;
    NS_IF_ADDREF(*aContent);
    return NS_OK;
  }

  if (mCurrentTargetContent) {
    *aContent = mCurrentTargetContent;
    NS_IF_ADDREF(*aContent);
    return NS_OK;      
  }
  
  if (mCurrentTarget) {
    mCurrentTarget->GetContentForEvent(mPresContext, aEvent, aContent);
    return NS_OK;
  }

  *aContent = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::GetEventRelatedContent(nsIContent** aContent)
{
  if (mCurrentRelatedContent) {
    *aContent = mCurrentRelatedContent;
    NS_IF_ADDREF(*aContent);
    return NS_OK;      
  }
  
  *aContent = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::GetContentState(nsIContent *aContent, PRInt32& aState)
{
  aState = NS_EVENT_STATE_UNSPECIFIED;

  if (aContent == mActiveContent) {
    aState |= NS_EVENT_STATE_ACTIVE;
  }
  if (aContent == mHoverContent) {
    aState |= NS_EVENT_STATE_HOVER;
  }
  if (aContent == mCurrentFocus) {
    aState |= NS_EVENT_STATE_FOCUS;
  }
  if (aContent == mDragOverContent) {
    aState |= NS_EVENT_STATE_DRAGOVER;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::SetContentState(nsIContent *aContent, PRInt32 aState)
{
  const PRInt32 maxNotify = 5;
  nsIContent  *notifyContent[maxNotify] = {nsnull, nsnull, nsnull, nsnull, nsnull};

  if ((aState & NS_EVENT_STATE_DRAGOVER) && (aContent != mDragOverContent)) {
    //transferring ref to notifyContent from mDragOverContent
    notifyContent[4] = mDragOverContent; // notify dragover first, since more common case
    mDragOverContent = aContent;
    NS_IF_ADDREF(mDragOverContent);
  }

  if ((aState & NS_EVENT_STATE_ACTIVE) && (aContent != mActiveContent)) {
    //transferring ref to notifyContent from mActiveContent
    notifyContent[2] = mActiveContent;
    mActiveContent = aContent;
    NS_IF_ADDREF(mActiveContent);
  }

  if ((aState & NS_EVENT_STATE_HOVER) && (aContent != mHoverContent)) {
    //transferring ref to notifyContent from mHoverContent
    notifyContent[1] = mHoverContent; // notify hover first, since more common case
    mHoverContent = aContent;
    NS_IF_ADDREF(mHoverContent);
  }

  if ((aState & NS_EVENT_STATE_FOCUS)) {
    if (aContent && (aContent == mCurrentFocus)) {
      // gLastFocusedDocument appears to always be correct, that is why
      // I'm not setting it here. This is to catch an edge case.
      NS_IF_RELEASE(gLastFocusedContent);
      gLastFocusedContent = mCurrentFocus;
      NS_IF_ADDREF(gLastFocusedContent);
    } else {
      notifyContent[3] = gLastFocusedContent;
      NS_IF_ADDREF(gLastFocusedContent);
      SendFocusBlur(mPresContext, aContent);
    }
  }

  if (aContent) { // notify about new content too
    notifyContent[0] = aContent;
    NS_ADDREF(aContent);  // everything in notify array has a ref
  }

  // remove duplicates
  if ((notifyContent[4] == notifyContent[3]) || (notifyContent[4] == notifyContent[2]) || (notifyContent[4] == notifyContent[1])) {
    NS_IF_RELEASE(notifyContent[4]);
  }
  // remove duplicates
  if ((notifyContent[3] == notifyContent[2]) || (notifyContent[3] == notifyContent[1])) {
    NS_IF_RELEASE(notifyContent[3]);
  }
  if (notifyContent[2] == notifyContent[1]) {
    NS_IF_RELEASE(notifyContent[2]);
  }

  // remove notifications for content not in document.
  // we may decide this is possible later but right now it has problems.
  nsIDocument* doc = nsnull;
  for  (int i = 0; i < maxNotify; i++) {
    if (notifyContent[i] && NS_SUCCEEDED(notifyContent[i]->GetDocument(doc)) && !doc) {
      NS_RELEASE(notifyContent[i]);
    }
    NS_IF_RELEASE(doc);
  }

  // compress the notify array to group notifications tighter
  nsIContent** from = &(notifyContent[0]);
  nsIContent** to   = &(notifyContent[0]);
  nsIContent** end  = &(notifyContent[maxNotify]);

  while (from < end) {
    if (! *from) {
      while (++from < end) {
        if (*from) {
          *to++ = *from;
          *from = nsnull;
          break;
        }
      }
    }
    else {
      if (from == to) {
        to++;
        from++;
      }
      else {
        *to++ = *from;
        *from++ = nsnull;
      }
    }
  }

  if (notifyContent[0]) { // have at least one to notify about
    nsIDocument *document;  // this presumes content can't get/lose state if not connected to doc
    notifyContent[0]->GetDocument(document);
    if (document) {
      document->BeginUpdate();
      document->ContentStatesChanged(notifyContent[0], notifyContent[1]);
      if (notifyContent[2]) {  // more that two notifications are needed (should be rare)
        // XXX a further optimization here would be to group the notification pairs
        // together by parent/child, only needed if more than two content changed
        // (ie: if [0] and [2] are parent/child, then notify (0,2) (1,3))
        document->ContentStatesChanged(notifyContent[2], notifyContent[3]);
        if (notifyContent[4]) {  // more that two notifications are needed (should be rare)
          document->ContentStatesChanged(notifyContent[4], nsnull);
        }
      }
      document->EndUpdate();
      NS_RELEASE(document);
    }

    from = &(notifyContent[0]);
    while (from < to) {  // release old refs now that we are through
      nsIContent* notify = *from++;
      NS_RELEASE(notify);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::SendFocusBlur(nsIPresContext* aPresContext, nsIContent *aContent)
{
  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));
  
  if (nsnull != gLastFocusedPresContext) {
    
    if (gLastFocusedContent && gLastFocusedContent != mFirstBlurEvent) {

      //Store the first blur event we fire and don't refire blur
      //to that element while the first blur is still ongoing.
      PRBool clearFirstBlurEvent = PR_FALSE;
      if (!mFirstBlurEvent) {
        mFirstBlurEvent = gLastFocusedContent;
        NS_ADDREF(mFirstBlurEvent);
        clearFirstBlurEvent = PR_TRUE;
      }

      // Retrieve this content node's pres context. it can be out of sync in
      // the Ender widget case.
      nsCOMPtr<nsIDocument> doc;
      gLastFocusedContent->GetDocument(*getter_AddRefs(doc));
      if (doc) {
        nsCOMPtr<nsIPresShell> shell = getter_AddRefs(doc->GetShellAt(0));
        if (shell) {
          nsCOMPtr<nsIPresContext> oldPresContext;
          shell->GetPresContext(getter_AddRefs(oldPresContext));

          //fire blur
          nsEventStatus status = nsEventStatus_eIgnore;
          nsEvent event;
          event.eventStructType = NS_EVENT;
          event.message = NS_BLUR_CONTENT;

          nsCOMPtr<nsIEventStateManager> esm;
          oldPresContext->GetEventStateManager(getter_AddRefs(esm));
          esm->SetFocusedContent(gLastFocusedContent);
          nsCOMPtr<nsIContent> temp = gLastFocusedContent;
          NS_RELEASE(gLastFocusedContent);
          gLastFocusedContent = nsnull;
          temp->HandleDOMEvent(oldPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status); 
          esm->SetFocusedContent(nsnull);
        }
      }

      if (clearFirstBlurEvent) {
        NS_RELEASE(mFirstBlurEvent);
      }
    }

    // Go ahead and fire a blur on the window.
    nsCOMPtr<nsIScriptGlobalObject> globalObject;

    if(gLastFocusedDocument)
      gLastFocusedDocument->GetScriptGlobalObject(getter_AddRefs(globalObject));
  
    if (!mDocument) {  
      if (presShell) {
        presShell->GetDocument(&mDocument);
      }
    }

    if (gLastFocusedDocument && (gLastFocusedDocument != mDocument) && globalObject) {  
      nsEventStatus status = nsEventStatus_eIgnore;
      nsEvent event;
      event.eventStructType = NS_EVENT;
      event.message = NS_BLUR_CONTENT;

      nsCOMPtr<nsIEventStateManager> esm;
      gLastFocusedPresContext->GetEventStateManager(getter_AddRefs(esm));
      esm->SetFocusedContent(nsnull);
      nsCOMPtr<nsIDocument> temp = gLastFocusedDocument;
      NS_RELEASE(gLastFocusedDocument);
      gLastFocusedDocument = nsnull;
      temp->HandleDOMEvent(gLastFocusedPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
      globalObject->HandleDOMEvent(gLastFocusedPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status); 
    }
  }
  
  NS_IF_RELEASE(gLastFocusedContent);
  NS_IF_RELEASE(mCurrentFocus);
  gLastFocusedContent = aContent;
  mCurrentFocus = aContent;
  NS_IF_ADDREF(aContent);
  NS_IF_ADDREF(aContent);
 
  if (nsnull != aContent && aContent != mFirstFocusEvent) {

    //Store the first focus event we fire and don't refire focus
    //to that element while the first focus is still ongoing.
    PRBool clearFirstFocusEvent = PR_FALSE;
    if (!mFirstFocusEvent) {
      mFirstFocusEvent = aContent;
      NS_ADDREF(mFirstFocusEvent);
      clearFirstFocusEvent = PR_TRUE;
    }

    //fire focus
    nsEventStatus status = nsEventStatus_eIgnore;
    nsEvent event;
    event.eventStructType = NS_EVENT;
    event.message = NS_FOCUS_CONTENT;

    if (nsnull != mPresContext) {
      aContent->HandleDOMEvent(mPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
    }
    
    nsAutoString tabIndex;
    aContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::tabindex, tabIndex);
    PRInt32 ec, val = tabIndex.ToInteger(&ec);
    if (NS_OK == ec) {
      mCurrentTabIndex = val;
    }

    if (clearFirstFocusEvent) {
      NS_RELEASE(mFirstFocusEvent);
    }
  }

  nsIFrame * currentFocusFrame = nsnull;
  if (mCurrentFocus)
    presShell->GetPrimaryFrameFor(mCurrentFocus, &currentFocusFrame);
  if (!currentFocusFrame)
    currentFocusFrame = mCurrentTarget;

  // Find the window that this frame is in and
  // make sure it has focus
  // GFX Text Control frames handle their own focus
  // and must be special-cased.
  nsCOMPtr<nsIGfxTextControlFrame> gfxFrame = do_QueryInterface(currentFocusFrame);
  if (gfxFrame) {
    gfxFrame->SetInnerFocus();
  } 
  else if (currentFocusFrame) {
    nsIFrame * parentFrame;
    currentFocusFrame->GetParentWithView(aPresContext, &parentFrame);
    if (nsnull != parentFrame) {
      nsIView * pView;
      parentFrame->GetView(aPresContext, &pView);
      if (nsnull != pView) {
        nsIWidget *window = nsnull;

        nsIView *ancestor = pView;
        while (nsnull != ancestor) {
          ancestor->GetWidget(window); // addrefs
          if (nsnull != window) {
              window->SetFocus();
              NS_RELEASE(window); // releases. Duh.
	          break;
	        }
	        ancestor->GetParent(ancestor);
        }
      }
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::GetFocusedContent(nsIContent** aContent)
{
  *aContent = mCurrentFocus;
  NS_IF_ADDREF(*aContent);
  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::SetFocusedContent(nsIContent* aContent)
{
  NS_IF_RELEASE(mCurrentFocus);
  mCurrentFocus = aContent;
  NS_IF_ADDREF(mCurrentFocus);
  return NS_OK;
}

//-------------------------------------------
// Access Key Registration
//-------------------------------------------
NS_IMETHODIMP
nsEventStateManager::RegisterAccessKey(nsIFrame * aFrame, PRUint32 aKey)
{
#ifdef DEBUG_rods
  printf("Obj: %p Registered %d [%c]accesskey\n", aFrame, aKey, (char)aKey);
#endif
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsEventStateManager::UnregisterAccessKey(nsIFrame * aFrame)
{
#ifdef DEBUG_rods
  printf("Obj: %p Unregistered accesskey\n", aFrame);
#endif
  return NS_ERROR_FAILURE;
}

#ifndef USE_FOCUS_FOR_MOUSEWHEEL

// This function MAY CHANGE the PresContext that you pass into it.  It
// will be changed to the PresContext for the main document.  If the
// new PresContext differs from the one you passed in, you should
// be sure to release the new one.

nsIFrame*
nsEventStateManager::GetDocumentFrame(nsIPresContext* &aPresContext)
{
  nsCOMPtr<nsIPresShell> presShell;
  nsCOMPtr<nsIDocument> aDocument;
  nsIFrame* aFrame;
  nsIView* aView;

  aPresContext->GetShell(getter_AddRefs(presShell));
    
  if (nsnull == presShell) {
    PR_LOG(MOUSEWHEEL, PR_LOG_DEBUG, ("GetDocumentFrame: Got a null PresShell\n"));
    return nsnull;
  }

  presShell->GetDocument(getter_AddRefs(aDocument));

  // Walk up the document parent chain.  This lets us scroll the main
  // document, even when the event is fired for an editor control.

  nsCOMPtr<nsIDocument> parentDoc(dont_AddRef(aDocument->GetParentDocument()));

  while(parentDoc) {
    aDocument = parentDoc;
    parentDoc = dont_AddRef(aDocument->GetParentDocument());
  }

  presShell = dont_AddRef(aDocument->GetShellAt(0));
  presShell->GetPresContext(&aPresContext);

  nsCOMPtr<nsIContent> rootContent(dont_AddRef(aDocument->GetRootContent()));
  presShell->GetPrimaryFrameFor(rootContent, &aFrame);

  aFrame->GetView(aPresContext, &aView);
  PR_LOG(MOUSEWHEEL, PR_LOG_DEBUG, ("GetDocumentFrame: got document view = %p\n", aView));

  if (!aView) {
    PR_LOG(MOUSEWHEEL, PR_LOG_DEBUG, ("GetDocumentFrame: looking for a parent with a view\n"));
    aFrame->GetParentWithView(aPresContext, &aFrame);
  }

  return aFrame;
}
#endif // !USE_FOCUS_FOR_MOUSEWHEEL

#ifdef USE_FOCUS_FOR_MOUSEWHEEL
// This is some work-in-progress code that uses only the focus
// to determine what to scroll

nsresult
nsEventStateManager::GetScrollableFrameOrView(nsIScrollableView* &sv,
                                              nsISelfScrollingFrame* &sf,
                                              nsIView* &focusView)
{
  NS_ASSERTION(mPresContext, "ESM has a null prescontext");
  PR_LOG(MOUSEWHEEL, PR_LOG_DEBUG, ("GetScrollableFrameOrView: gLastFocusedContent=%p\n", gLastFocusedContent));

  nsIFrame* focusFrame = nsnull;
  nsCOMPtr<nsIPresShell> presShell;
  mPresContext->GetShell(getter_AddRefs(presShell));
  if (!presShell || !gLastFocusedContent)
    {
      sv = nsnull;
      sf = nsnull;
      focusView = nsnull;
      return NS_OK;
    }

  presShell->GetPrimaryFrameFor(gLastFocusedContent, &focusFrame);
  if (focusFrame) {
    PR_LOG(MOUSEWHEEL, PR_LOG_DEBUG, ("GetScrollableFrameOrView: got focusFrame\n"));
    focusFrame->GetView(mPresContext, &focusView);
  }

  if (focusView) {
    PR_LOG(MOUSEWHEEL, PR_LOG_DEBUG, ("GetScrollableFrameOrView: got focusView\n"));
    sv = GetNearestScrollingView(focusView);
    if (sv) {
      PR_LOG(MOUSEWHEEL, PR_LOG_DEBUG, ("GetScrollableFrameOrView: got scrollingView\n"));
      sf = nsnull;
      return NS_OK; // success
    }
  }

  if (focusFrame)
    sf = GetParentSelfScrollingFrame(focusFrame);
  if (sf)
    PR_LOG(MOUSEWHEEL, PR_LOG_DEBUG, ("GetScrollableFrameOrView: got sf\n"));
  return NS_OK;
}

#else // USE_FOCUS_FOR_MOUSEWHEEL

// There are three posibilities for what this function returns:
//  sv and focusView non-null, sf null  (a view should be scrolled)
//  sv and focusView null, sf non-null  (a frame should be scrolled)
//  sv, focusView, and sf all null (nothing to scroll)
// The location works like this:
//  First, check for a focused frame and try to get its view.
//    If we can, and can get an nsIScrollableView for it, use that.
//  If there is a focused frame but it does not have a view, or it has
//   a view but no nsIScrollableView, check for an nsISelfScrollingFrame.
//    If we find one, use that.
//  If there is no focused frame, we first look for an nsISelfScrollingFrame
//   as an ancestor of the event target.  If there isn't one, we try to get
//   an nsIView corresponding to the main document.
// Confused yet?
// This function may call GetDocumentFrame, so read the warning above
// regarding the PresContext that you pass into this function.

nsresult
nsEventStateManager::GetScrollableFrameOrView(nsIPresContext* &aPresContext,
                                              nsIFrame* aTargetFrame,
                                              nsIView* aView,
                                              nsIScrollableView* &sv,
                                              nsISelfScrollingFrame* &sf,
                                              nsIView* &focusView)
{
  nsIFrame* focusFrame = nsnull;

  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));
  if (!presShell)    // this is bad
    {
      sv = nsnull;
      sf = nsnull;
      focusView = nsnull;
      return NS_OK;
    }

  PR_LOG(MOUSEWHEEL, PR_LOG_DEBUG, ("------------------------\n"));
  PR_LOG(MOUSEWHEEL, PR_LOG_DEBUG, ("GetScrollableFrameOrView: aTargetFrame = %p, aView = %p\n", aTargetFrame, aView));
          
  if (mCurrentFocus) {
    PR_LOG(MOUSEWHEEL, PR_LOG_DEBUG, ("GetScrollableFrameOrView: mCurrentFocus = %p\n", mCurrentFocus));
    
    presShell->GetPrimaryFrameFor(mCurrentFocus, &focusFrame);
    if (focusFrame)
      focusFrame->GetView(aPresContext, &focusView);
  }
          
  if (focusFrame) {
    if (!focusView) {
      // The focused frame doesn't have a view
      // Revert to the parameters passed in
      // XXX this might not be right

      PR_LOG(MOUSEWHEEL, PR_LOG_DEBUG, ("GetScrollableFrameOrView: Couldn't get a view for focused frame"));

      focusFrame = aTargetFrame;
      focusView = aView;
    }
  } else {
    // Focus is null.  This means the main document has the focus,
    // and we should scroll that.
    
    PR_LOG(MOUSEWHEEL, PR_LOG_DEBUG, ("GetScrollableFrameOrView: mCurrentFocus = NULL\n"));

    // If we can get an nsISelfScrollingFrame, that is preferable to getting
    // the document view

    sf = GetParentSelfScrollingFrame(aTargetFrame);
    if (sf)
      PR_LOG(MOUSEWHEEL, PR_LOG_DEBUG, ("GetScrollableFrameOrView: Found a SelfScrollingFrame: sf = %p\n", sf));
    else {
      focusFrame = GetDocumentFrame(aPresContext);
      focusFrame->GetView(aPresContext, &focusView);

      if (focusView)
        PR_LOG(MOUSEWHEEL, PR_LOG_DEBUG, ("GetScrollableFrameOrView: Got view for document frame!\n"));
      else
        PR_LOG(MOUSEWHEEL, PR_LOG_DEBUG, ("GetScrollableFrameOrView: Couldn't get view for document frame\n"));
      
    }
  }

  PR_LOG(MOUSEWHEEL, PR_LOG_DEBUG, ("GetScrollableFrameOrView: focusFrame=%p, focusView=%p\n", focusFrame, focusView));
  
  if (focusView)
    sv = GetNearestScrollingView(focusView);

  if (sv) {
    PR_LOG(MOUSEWHEEL, PR_LOG_DEBUG, ("GetScrollableFrameOrView: Found a ScrollingView\n"));

   // We can stop now
    sf = nsnull;
    return NS_OK;
  }
  else {
    PR_LOG(MOUSEWHEEL, PR_LOG_DEBUG, ("GetScrollableFrameOrView: No scrolling view, looking for scrolling frame\n"));
    if (!sf)
      sf = GetParentSelfScrollingFrame(aTargetFrame);

    if (sf)
      PR_LOG(MOUSEWHEEL, PR_LOG_DEBUG, ("GetScrollableFrameOrView: Found a scrolling frame\n"));

    sv = nsnull;
    focusView = nsnull;
    return NS_OK;
  }

  return NS_OK;  // should not be reached
}

#endif // USE_FOCUS_FOR_MOUSEWHEEL

void nsEventStateManager::ForceViewUpdate(nsIView* aView)
{
  // force the update to happen now, otherwise multiple scrolls can
  // occur before the update is processed. (bug #7354)

  nsIViewManager* vm = nsnull;
  if (NS_OK == aView->GetViewManager(vm) && nsnull != vm) {
    // I'd use Composite here, but it doesn't always work.
    // vm->Composite();
    vm->ForceUpdate();
    NS_RELEASE(vm);
  }
}

nsresult NS_NewEventStateManager(nsIEventStateManager** aInstancePtrResult)
{
  nsresult rv;

  NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIEventStateManager* manager = new nsEventStateManager();
  if (nsnull == manager) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  rv =  manager->QueryInterface(NS_GET_IID(nsIEventStateManager),
                                 (void **) aInstancePtrResult);
  if (NS_FAILED(rv)) return rv;

  return manager->Init();
}

