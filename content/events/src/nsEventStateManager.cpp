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
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLMapElement.h"
#include "nsIHTMLDocument.h"
#include "nsINameSpaceManager.h"  // for kNameSpaceID_HTML
#include "nsIWebShell.h"
#include "nsIBaseWindow.h"
#include "nsIScrollableView.h"
#include "nsISelection.h"
#include "nsIFrameSelection.h"
#include "nsIDeviceContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIGfxTextControlFrame.h"
#include "nsIDOMWindowInternal.h"
#include "nsPIDOMWindow.h"
#include "nsIEnumerator.h"
#include "nsFrameTraversal.h"
#include "nsIDocShellTreeItem.h"
#include "nsIWebNavigation.h"

#include "nsIServiceManager.h"
#include "nsIPref.h"

#include "nsXULAtoms.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIObserverService.h"
#include "prlog.h"
#include "nsIDocShell.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsITreeFrame.h"

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
 MOUSE_SCROLL_HISTORY,
 MOUSE_SCROLL_TEXTSIZE
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
  mLockCursor = 0;

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
  mAccessKeys = nsnull;
  hHover = PR_FALSE;
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
                  NS_OBSERVERSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
  {
    nsAutoString topic; topic.AssignWithConversion(NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    observerService->AddObserver(this, topic.GetUnicode());
  }

  rv = getPrefService();

  if (NS_SUCCEEDED(rv)) {
    mPrefService->GetBoolPref("nglayout.events.showHierarchicalHover", &hHover);
  }

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

  if (mAccessKeys) {
    delete mAccessKeys;
  }

  if (!m_haveShutdown) {
    Shutdown();

    // Don't remove from Observer service in Shutdown because Shutdown also
    // gets called from xpcom shutdown observer.  And we don't want to remove
    // from the service in that case.

    nsresult rv;

    NS_WITH_SERVICE (nsIObserverService, observerService,
                     NS_OBSERVERSERVICE_CONTRACTID, &rv);
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


NS_IMPL_ISUPPORTS3(nsEventStateManager, nsIEventStateManager, nsIObserver, nsISupportsWeakReference)

NS_IMETHODIMP
nsEventStateManager::PreHandleEvent(nsIPresContext* aPresContext, 
                                 nsEvent *aEvent,
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
    BeginTrackingDragGesture ( (nsGUIEvent*)aEvent, aTargetFrame );
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
    GenerateDragGesture(aPresContext, (nsGUIEvent*)aEvent);
    UpdateCursor(aPresContext, aEvent, mCurrentTarget, aStatus);
    GenerateMouseEnterExit(aPresContext, (nsGUIEvent*)aEvent);
    break;
  case NS_MOUSE_EXIT:
    GenerateMouseEnterExit(aPresContext, (nsGUIEvent*)aEvent);
    //This is a window level mouseenter event and should stop here
    aEvent->message = 0;
    break;
  case NS_DRAGDROP_OVER:
    GenerateDragDropEnterExit(aPresContext, (nsGUIEvent*)aEvent);
    break;
  case NS_GOTFOCUS:
    {
#ifdef DEBUG_hyatt
      printf("Got focus.\n");
#endif

      EnsureDocument(aPresContext);

      if (gLastFocusedDocument == mDocument)
         break;
        
      //fire focus
      nsEventStatus status = nsEventStatus_eIgnore;
      nsEvent focusevent;
      focusevent.eventStructType = NS_EVENT;
      focusevent.message = NS_FOCUS_CONTENT;

      if (mDocument) {
      
        if(gLastFocusedDocument) {
          // fire a blur, on the document only to keep ender happy
          nsEventStatus blurstatus = nsEventStatus_eIgnore;
          nsEvent blurevent;
          blurevent.eventStructType = NS_EVENT;
          blurevent.message = NS_BLUR_CONTENT;
      
          if(gLastFocusedPresContext) {
            nsCOMPtr<nsIDOMXULCommandDispatcher> commandDispatcher;

            nsCOMPtr<nsIScriptGlobalObject> ourGlobal;
            gLastFocusedDocument->GetScriptGlobalObject(getter_AddRefs(ourGlobal));
            nsCOMPtr<nsIDOMWindowInternal> rootWindow;
            nsCOMPtr<nsPIDOMWindow> ourWindow = do_QueryInterface(ourGlobal);
            if(ourWindow) {
              ourWindow->GetRootCommandDispatcher(getter_AddRefs(commandDispatcher));
              if (commandDispatcher)
                commandDispatcher->SetSuppressFocus(PR_TRUE);
            }
            
            gLastFocusedDocument->HandleDOMEvent(gLastFocusedPresContext, &blurevent, nsnull, NS_EVENT_FLAG_INIT, &blurstatus);
            if (!mCurrentFocus && gLastFocusedContent)// must send it to the element that is loosing focus. since SendFocusBlur wont be called
              gLastFocusedContent->HandleDOMEvent(gLastFocusedPresContext, &blurevent, nsnull, NS_EVENT_FLAG_INIT, &blurstatus);
              

            if (commandDispatcher) {
              commandDispatcher->SetSuppressFocus(PR_FALSE);
            }
          }
        }
        
        // fire focus on window, not document
        nsCOMPtr<nsIScriptGlobalObject> globalObject;
        mDocument->GetScriptGlobalObject(getter_AddRefs(globalObject));
        if (globalObject) {
          nsIContent* currentFocus = mCurrentFocus;
          mCurrentFocus = nsnull;
          if(gLastFocusedDocument != mDocument) {
            mDocument->HandleDOMEvent(aPresContext, &focusevent, nsnull, NS_EVENT_FLAG_INIT, &status);
            if (currentFocus && currentFocus != gLastFocusedContent)
              currentFocus->HandleDOMEvent(aPresContext, &focusevent, nsnull, NS_EVENT_FLAG_INIT, &status);
          }
          
          globalObject->HandleDOMEvent(aPresContext, &focusevent, nsnull, NS_EVENT_FLAG_INIT, &status); 
          mCurrentFocus = currentFocus;
          NS_IF_RELEASE(gLastFocusedContent);
          gLastFocusedContent = mCurrentFocus;
          NS_IF_ADDREF(gLastFocusedContent);
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
      EnsureDocument(aPresContext);

      nsCOMPtr<nsIDOMXULCommandDispatcher> commandDispatcher;
      nsCOMPtr<nsIDOMElement> focusedElement;
      nsCOMPtr<nsIDOMWindowInternal> focusedWindow;
      nsCOMPtr<nsIDOMXULDocument> xulDoc = do_QueryInterface(mDocument);

      if (xulDoc) {
        // See if we have a command dispatcher attached.
        xulDoc->GetCommandDispatcher(getter_AddRefs(commandDispatcher));
        if (commandDispatcher) {
          // Obtain focus info from the command dispatcher.
          commandDispatcher->GetFocusedWindow(getter_AddRefs(focusedWindow));
          commandDispatcher->GetFocusedElement(getter_AddRefs(focusedElement));
          
          commandDispatcher->SetSuppressFocusScroll(PR_TRUE);
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
		      nsCOMPtr<nsIDOMWindowInternal> privateRootWindow;
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
            
            commandDispatcher->SetSuppressFocusScroll(PR_TRUE);
          }
        }
	    }

      // Focus the DOM window.
      NS_WARN_IF_FALSE(focusedWindow,"check why focusedWindow is null!!!");
      if(focusedWindow) {
        focusedWindow->Focus();

        if (focusedElement) {
          nsCOMPtr<nsIContent> focusContent = do_QueryInterface(focusedElement);
          nsCOMPtr<nsIDOMDocument> domDoc;
          nsCOMPtr<nsIDocument> document;
          focusedWindow->GetDocument(getter_AddRefs(domDoc));
          if (domDoc) {
            document = do_QueryInterface(domDoc);
            nsCOMPtr<nsIPresShell> shell;
            nsCOMPtr<nsIPresContext> context;
            shell = getter_AddRefs(document->GetShellAt(0));
            shell->GetPresContext(getter_AddRefs(context));
            focusContent->SetFocus(context);
          }
        }  
      }

      if (commandDispatcher) {
        commandDispatcher->SetActive(PR_TRUE);
        
        PRBool isSuppressed;
        commandDispatcher->GetSuppressFocus(&isSuppressed);
        while(isSuppressed){
          commandDispatcher->SetSuppressFocus(PR_FALSE); // Unsuppress and let the command dispatcher listen again.
          commandDispatcher->GetSuppressFocus(&isSuppressed);
        }
        commandDispatcher->SetSuppressFocusScroll(PR_FALSE);
      }  
    }
  break;
    
 case NS_DEACTIVATE:
    {
      EnsureDocument(aPresContext);

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
      nsCOMPtr<nsIDOMWindowInternal> rootWindow;
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
        //commandDispatcher->SetSuppressFocus(PR_FALSE);
      }
    } 
    
    break;
    
  case NS_KEY_PRESS:
    {

      nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
#ifdef XP_MAC
    // (pinkerton, joki, saari) IE5 for mac uses Control for access keys. The HTML4 spec
    // suggests to use command on mac, but this really sucks (imagine someone having a "q"
    // as an access key and not letting you quit the app!). As a result, we've made a 
    // command decision 1 day before tree lockdown to change it to the control key.
    PRBool isSpecialAccessKeyDown = keyEvent->isControl;
#else
    PRBool isSpecialAccessKeyDown = keyEvent->isAlt;
#endif
      //This is to prevent keyboard scrolling while alt modifier in use.
      if (isSpecialAccessKeyDown) {
        //Alt key is down, we may need to do an accesskey
        if (mAccessKeys) {
          //Someone registered an accesskey.  Find and activate it.
          nsAutoString accKey((char)keyEvent->charCode);
          accKey.ToLowerCase();

          nsVoidKey key((void*)accKey.First());
          if (mAccessKeys->Exists(&key)) {
            nsCOMPtr<nsIContent> content = getter_AddRefs(NS_STATIC_CAST(nsIContent*, mAccessKeys->Get(&key)));

            //Its hard to say what HTML4 wants us to do in all cases.  So for now we'll settle for
            //A) Set focus
            ChangeFocus(content, nsnull, PR_TRUE);

            //B) Click on it.
            nsEventStatus status = nsEventStatus_eIgnore;
            nsMouseEvent event;
            event.eventStructType = NS_GUI_EVENT;
            event.message = NS_MOUSE_LEFT_CLICK;
            event.isShift = PR_FALSE;
            event.isControl = PR_FALSE;
            event.isAlt = PR_FALSE;
            event.isMeta = PR_FALSE;
            event.clickCount = 0;
            event.widget = nsnull;
            content->HandleDOMEvent(mPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);

            *aStatus = nsEventStatus_eConsumeNoDefault;
          }
        }
      }
    }
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

    // Check if selection is tracking drag gestures, if so
    // don't interfere!

    if (mGestureDownFrame) {
      nsCOMPtr<nsISelectionController> selCon;
      nsresult rv = mGestureDownFrame->GetSelectionController(aPresContext, getter_AddRefs(selCon));

      if (NS_SUCCEEDED(rv) && selCon) {
        nsCOMPtr<nsIFrameSelection> frameSel;

        frameSel = do_QueryInterface(selCon);

        if (! frameSel) {
          nsCOMPtr<nsIPresShell> shell;
          nsresult rv = aPresContext->GetShell(getter_AddRefs(shell));

          if (NS_SUCCEEDED(rv) && shell)
            rv = shell->GetFrameSelection(getter_AddRefs(frameSel));
        }

        if (NS_SUCCEEDED(rv) && frameSel) {
          PRBool mouseDownState = PR_TRUE;
          frameSel->GetMouseDownState(&mouseDownState);
          if (mouseDownState) {
            StopTrackingDragGesture();
            return;
          }
        }
      }
    }

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

      // Dispatch to the DOM. We have to fake out the ESM and tell it that the
      // current target frame is actually where the mouseDown occurred, otherwise it
      // will use the frame the mouse is currently over which may or may not be
      // the same. (Note: saari and I have decided that we don't have to reset |mCurrentTarget|
      // when we're through because no one else is doing anything more with this
      // event and it will get reset on the very next event to the correct frame).
      mCurrentTarget = mGestureDownFrame;
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

nsresult
nsEventStateManager::ChangeTextSize(PRInt32 change)
{
  if(!gLastFocusedDocument) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIScriptGlobalObject> ourGlobal;
  gLastFocusedDocument->GetScriptGlobalObject(getter_AddRefs(ourGlobal));
  if(!ourGlobal) return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsPIDOMWindow> ourWindow = do_QueryInterface(ourGlobal);
  if(!ourWindow) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMWindowInternal> rootWindow;
  ourWindow->GetPrivateRoot(getter_AddRefs(rootWindow));
  if(!rootWindow) return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIDOMWindowInternal> windowContent;
  rootWindow->Get_content(getter_AddRefs(windowContent));
  if(!windowContent) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMDocument> domDoc;
  windowContent->GetDocument(getter_AddRefs(domDoc));
  if(!domDoc) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  if(!doc) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresShell> presShell = getter_AddRefs(doc->GetShellAt(0));
  if(!presShell) return NS_ERROR_FAILURE;
  nsCOMPtr<nsIPresContext> presContext;
  presShell->GetPresContext(getter_AddRefs(presContext));
  if(!presContext) return NS_ERROR_FAILURE;

  nsCOMPtr<nsISupports> pcContainer;
  presContext->GetContainer(getter_AddRefs(pcContainer));
  if(!pcContainer) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocShell> docshell(do_QueryInterface(pcContainer));
  if(!docshell) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContentViewer> cv;
  docshell->GetContentViewer(getter_AddRefs(cv));
  if(!cv) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIMarkupDocumentViewer> mv(do_QueryInterface(cv));
  if(!mv) return NS_ERROR_FAILURE;

  float textzoom;
  mv->GetTextZoom(&textzoom);
  mv->SetTextZoom(textzoom + 0.1*change);

  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::PostHandleEvent(nsIPresContext* aPresContext, 
                                     nsEvent *aEvent,
                                     nsIFrame* aTargetFrame,
                                     nsEventStatus* aStatus,
                                     nsIView* aView)
{
  NS_ENSURE_ARG(aPresContext);
  NS_ENSURE_ARG_POINTER(aStatus);
  mCurrentTarget = aTargetFrame;
  NS_IF_RELEASE(mCurrentTargetContent);  
  nsresult ret = NS_OK;

  NS_ASSERTION(mCurrentTarget, "mCurrentTarget is null");
  if (!mCurrentTarget) return NS_ERROR_NULL_POINTER;

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
          mCurrentTarget->GetStyleData(eStyleStruct_UserInterface, ((const nsStyleStruct*&)ui));
          suppressBlur = (ui->mUserFocus == NS_STYLE_USER_FOCUS_IGNORE);
        }

        nsIFrame* currFrame = mCurrentTarget;
        nsCOMPtr<nsIContent> activeContent;
        if (mCurrentTarget)
          mCurrentTarget->GetContent(getter_AddRefs(activeContent));

        // Look for the nearest enclosing focusable frame.
        while (currFrame) {
          const nsStyleUserInterface* ui;
          currFrame->GetStyleData(eStyleStruct_UserInterface, ((const nsStyleStruct*&)ui));
          if ((ui->mUserFocus != NS_STYLE_USER_FOCUS_IGNORE) &&
              (ui->mUserFocus != NS_STYLE_USER_FOCUS_NONE)) {
            currFrame->GetContent(getter_AddRefs(newFocus));
            nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(newFocus));
            if (domElement)
              break;
          }
          currFrame->GetParent(&currFrame);
        }

        if (newFocus && currFrame)
          ChangeFocus(newFocus, currFrame, PR_TRUE);
        else if (!suppressBlur)
          SetContentState(nsnull, NS_EVENT_STATE_FOCUS);

        if (activeContent) {
          // The nearest enclosing element goes into the
          // :active state.  If we fail the QI to DOMElement,
          // then we know we're only a node, and that we need
          // to obtain our parent element and put it into :active
          // instead.
          nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(activeContent));
          if (!elt) {
            nsCOMPtr<nsIContent> par;
            activeContent->GetParent(*getter_AddRefs(par));
            activeContent = par;
          }
          if (activeContent)
            SetContentState(activeContent, NS_EVENT_STATE_ACTIVE);
        }
      }
      else {
        // if we're here, the event handler returned false, so stop
        // any of our own processing of a drag. Workaround for bug 43258.
        StopTrackingDragGesture();      
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
      case MOUSE_SCROLL_PAGE:
        {
          nsIView* focusView = nsnull;
          nsIScrollableView* sv = nsnull;
          nsIFrame* focusFrame = nsnull;

          // Special case for tree frames - they handle their own scrolling
          nsITreeFrame* treeFrame = nsnull;
          nsIFrame* curFrame = aTargetFrame;

          while (curFrame) {
            if (NS_OK == curFrame->QueryInterface(NS_GET_IID(nsITreeFrame), (void**) &treeFrame))
              break;
            curFrame->GetParent(&curFrame);
          }

          if (treeFrame) {
            PRInt32 scrollIndex, visibleRows;
            treeFrame->GetIndexOfFirstVisibleRow(&scrollIndex);
            treeFrame->GetNumberOfVisibleRows(&visibleRows);

            if (action == MOUSE_SCROLL_N_LINES)
              scrollIndex += numLines;
            else
              scrollIndex += ((numLines > 0) ? visibleRows : -visibleRows);

            if (scrollIndex < 0)
              scrollIndex = 0;
            else {
              PRInt32 numRows, lastPageTopRow;
              treeFrame->GetRowCount(&numRows);
              lastPageTopRow = numRows - visibleRows;
              if (scrollIndex > lastPageTopRow)
                scrollIndex = lastPageTopRow;
            }

            treeFrame->ScrollToIndex(scrollIndex);
            break;
          }

          nsCOMPtr<nsIPresShell> presShell;
          aPresContext->GetShell(getter_AddRefs(presShell));

          // Otherwise, check for a focused content element
          nsCOMPtr<nsIContent> focusContent;
          if (mCurrentFocus)
            focusContent = mCurrentFocus;
          else {
            // If there is no focused content, get the document content
            EnsureDocument(presShell);
            focusContent = dont_AddRef(mDocument->GetRootContent());
          }

          if (!focusContent)
            break;
          
          // Get the content's view and scroll it.
          presShell->GetPrimaryFrameFor(focusContent, &focusFrame);
          if (!focusFrame)
              break;

          focusFrame->GetView(aPresContext, &focusView);
          if (!focusView) {
            nsIFrame* frameWithView;
            focusFrame->GetParentWithView(aPresContext, &frameWithView);
            if (frameWithView)
              frameWithView->GetView(aPresContext, &focusView);
            else
              break;
          }

          sv = GetNearestScrollingView(focusView);
          if (sv) {
            if (action == MOUSE_SCROLL_N_LINES)
              sv->ScrollByLines(0, numLines);
            else
              sv->ScrollByPages((numLines > 0) ? 1 : -1);
            ForceViewUpdate(focusView);
          }
        }

        break;

      case MOUSE_SCROLL_HISTORY:
        {
          nsCOMPtr<nsISupports> pcContainer;
          mPresContext->GetContainer(getter_AddRefs(pcContainer));
          if (pcContainer) {
            nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(pcContainer));
            if (webNav) {
              if (msEvent->deltaLines > 0)
                 webNav->GoBack();
              else
                 webNav->GoForward();
            }
          }
        }
        break;

      case MOUSE_SCROLL_TEXTSIZE:
        ChangeTextSize((msEvent->deltaLines > 0) ? 1 : -1);
        break;
      }
      *aStatus = nsEventStatus_eConsumeNoDefault;

    }

    break;
  
  case NS_DRAGDROP_DROP:
  case NS_DRAGDROP_EXIT:
    // clean up after ourselves. make sure we do this _after_ the event, else we'll
    // clean up too early!
    GenerateDragDropEnterExit(aPresContext, (nsGUIEvent*)aEvent);
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
                sv->ScrollByLines(0, (keyEvent->keyCode == NS_VK_DOWN) ? 1 : -1);
              
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
          case NS_VK_LEFT: 
          case NS_VK_RIGHT:
            if (!mCurrentFocus) {
              nsIScrollableView* sv = GetNearestScrollingView(aView);
              if (sv) {
                nsKeyEvent * keyEvent = (nsKeyEvent *)aEvent;
                sv->ScrollByLines((keyEvent->keyCode == NS_VK_RIGHT) ? 1 : -1, 0);
              
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

  //If cursor is locked just use the locked one
  if (mLockCursor) {
    cursor = mLockCursor;
  }
  //If not locked, look for correct cursor
  else {
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
      if (aTargetFrame) {
        aTargetFrame->GetCursor(aPresContext, aEvent->point, cursor);
      }
    }
  }

  if (aTargetFrame) {
    nsCOMPtr<nsIWidget> window;
    aTargetFrame->GetWindow(aPresContext, getter_AddRefs(window));
    SetCursor(cursor, window, PR_FALSE);
  }

  if (mLockCursor || NS_STYLE_CURSOR_AUTO != cursor) {
    *aStatus = nsEventStatus_eConsumeDoDefault;
  }
}

NS_IMETHODIMP
nsEventStateManager::SetCursor(PRInt32 aCursor, nsIWidget* aWidget, PRBool aLockCursor)
{
  nsCursor c;

  NS_ENSURE_TRUE(aWidget, NS_ERROR_FAILURE);
  if (aLockCursor) {
    if (NS_STYLE_CURSOR_AUTO != aCursor) {
      mLockCursor = aCursor;
    }
    else {
      //If cursor style is set to auto we unlock the cursor again.
      mLockCursor = 0;
    }
  }
  switch (aCursor) {
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
  case NS_STYLE_CURSOR_NW_RESIZE:
    c = eCursor_sizeNW;
    break;
  case NS_STYLE_CURSOR_SE_RESIZE:
    c = eCursor_sizeSE;
    break;
  case NS_STYLE_CURSOR_NE_RESIZE:
    c = eCursor_sizeNE;
    break;
  case NS_STYLE_CURSOR_SW_RESIZE:
    c = eCursor_sizeSW;
    break;
  case NS_STYLE_CURSOR_COPY: // CSS3
    c = eCursor_copy;
    break;
  case NS_STYLE_CURSOR_ALIAS:
    c = eCursor_alias;
    break;
  case NS_STYLE_CURSOR_CONTEXT_MENU:
    c = eCursor_context_menu;
    break;
  case NS_STYLE_CURSOR_CELL:
    c = eCursor_cell;
    break;
  case NS_STYLE_CURSOR_GRAB:
    c = eCursor_grab;
    break;
  case NS_STYLE_CURSOR_GRABBING:
    c = eCursor_grabbing;
    break;
  case NS_STYLE_CURSOR_SPINNING:
    c = eCursor_spinning;
    break;
  case NS_STYLE_CURSOR_COUNT_UP:
    c = eCursor_count_up;
    break;
  case NS_STYLE_CURSOR_COUNT_DOWN:
    c = eCursor_count_down;
    break;
  case NS_STYLE_CURSOR_COUNT_UP_DOWN:
    c = eCursor_count_up_down;
    break;
  }

  aWidget->SetCursor(c);

  return NS_OK;
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
            event.message = NS_MOUSE_EXIT_SYNTH;
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
          event.message = NS_MOUSE_ENTER_SYNTH;
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
        
          if ( status != nsEventStatus_eConsumeNoDefault )
            SetContentState(targetContent, NS_EVENT_STATE_HOVER);

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
          event.message = NS_MOUSE_EXIT_SYNTH;
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

          if ( status != nsEventStatus_eConsumeNoDefault )
            SetContentState(nsnull, NS_EVENT_STATE_HOVER);
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
          event.message = NS_DRAGDROP_EXIT_SYNTH;
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
        event.message = NS_DRAGDROP_EXIT_SYNTH;
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

    nsCOMPtr<nsIPresShell> presShell;
    mPresContext->GetShell(getter_AddRefs(presShell));
    if (presShell) {
      ret = presShell->HandleEventWithTarget(&event, mCurrentTarget, mouseContent, NS_EVENT_FLAG_INIT, aStatus);
      if (NS_SUCCEEDED(ret) && aEvent->clickCount == 2) {
        nsMouseEvent event2;
        //fire double click
        switch (aEvent->message) {
        case NS_MOUSE_LEFT_BUTTON_UP:
          event2.message = NS_MOUSE_LEFT_DOUBLECLICK;
          break;
        case NS_MOUSE_MIDDLE_BUTTON_UP:
          event2.message = NS_MOUSE_MIDDLE_DOUBLECLICK;
          break;
        case NS_MOUSE_RIGHT_BUTTON_UP:
          event2.message = NS_MOUSE_RIGHT_DOUBLECLICK;
          break;
        }
        
        event2.eventStructType = NS_MOUSE_EVENT;
        event2.widget = aEvent->widget;
        event2.point = aEvent->point;
        event2.refPoint = aEvent->refPoint;
        event2.clickCount = aEvent->clickCount;
        event2.isShift = aEvent->isShift;
        event2.isControl = aEvent->isControl;
        event2.isAlt = aEvent->isAlt;
        event2.isMeta = aEvent->isMeta;

        ret = presShell->HandleEventWithTarget(&event2, mCurrentTarget, mouseContent, NS_EVENT_FLAG_INIT, aStatus);
      }
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

  EnsureDocument(mPresContext);
  if (nsnull == mDocument) {
    return;
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
      if (topOfDoc) {
          primaryFrame = nsnull;
      }
      else {
        shell->GetPrimaryFrameFor(mCurrentFocus, &primaryFrame);
      }
    }
  }

  nsCOMPtr<nsIContent> rootContent = getter_AddRefs(mDocument->GetRootContent());

  nsCOMPtr<nsIContent> next;
  //Get the next tab item.  This takes tabIndex into account
  GetNextTabbableContent(rootContent, primaryFrame, forward, getter_AddRefs(next));

  //Either no tabbable items or the end of the document
  if (!next) {
    PRBool focusTaken = PR_FALSE;

    SetContentState(nsnull, NS_EVENT_STATE_FOCUS);

    //Offer focus upwards to allow shifting focus to UI controls
    nsCOMPtr<nsISupports> container;
    mPresContext->GetContainer(getter_AddRefs(container));
    nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(container));
    if (docShellAsWin) {
      docShellAsWin->FocusAvailable(docShellAsWin, &focusTaken);
    }
      
    //No one took focus and we're not already at the top of the doc
    //so calling ShiftFocus will start at the top of the doc again.
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

NS_IMETHODIMP
nsEventStateManager::GetNextTabbableContent(nsIContent* aRootContent, nsIFrame* aFrame, PRBool forward,
                                            nsIContent** aResult)
{
  *aResult = nsnull;
  PRBool keepFirstFrame = PR_FALSE;

  nsCOMPtr<nsIBidirectionalEnumerator> frameTraversal;

  if (!aFrame) {
    //No frame means we need to start with the root content again.
    nsCOMPtr<nsIPresShell> presShell;
    if (mPresContext) {
      nsIFrame* result = nsnull;
      if (NS_SUCCEEDED(mPresContext->GetShell(getter_AddRefs(presShell))) && presShell) {
        presShell->GetPrimaryFrameFor(aRootContent, &result);
      }
      if (result) {
        while(NS_SUCCEEDED(result->FirstChild(mPresContext, nsnull, &result)) && result) {
          aFrame = result;
        }
      }
    }
    if (!aFrame) {
      return NS_ERROR_FAILURE;
    }
    keepFirstFrame = PR_TRUE;
  }

  //Need to do special check in case we're in an imagemap which has multiple content per frame
  if (mCurrentFocus) {
    nsCOMPtr<nsIAtom> tag;
    mCurrentFocus->GetTag(*getter_AddRefs(tag));
    if(nsHTMLAtoms::area==tag.get()) {
      //Focus is in an imagemap area
      nsCOMPtr<nsIPresShell> presShell;
      if (mPresContext) {
        nsIFrame* result = nsnull;
        if (NS_SUCCEEDED(mPresContext->GetShell(getter_AddRefs(presShell))) && presShell) {
          presShell->GetPrimaryFrameFor(mCurrentFocus, &result);
        }
        if (result == aFrame) {
          //The current focus map area is in the current frame, don't skip over it.
          keepFirstFrame = PR_TRUE;
        }
      }
    }
  }

  nsresult result = NS_NewFrameTraversal(getter_AddRefs(frameTraversal), EXTENSIVE,
                                         mPresContext, aFrame);

  if (NS_FAILED(result))
    return NS_OK;

  if (!keepFirstFrame) {
    if (forward)
      frameTraversal->Next();
    else frameTraversal->Prev();
  }

  nsISupports* currentItem;
  frameTraversal->CurrentItem(&currentItem);
  nsIFrame* currentFrame = (nsIFrame*)currentItem;

  while (currentFrame) {
    nsCOMPtr<nsIContent> child;
    currentFrame->GetContent(getter_AddRefs(child));

    const nsStyleDisplay* disp;
    currentFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)disp));

    const nsStyleUserInterface* ui;
    currentFrame->GetStyleData(eStyleStruct_UserInterface, ((const nsStyleStruct*&)ui));
    
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
          nsAutoString href;
          nextAnchor->GetAttribute(NS_ConvertASCIItoUCS2("href"), href);
          if (!href.Length()) {
            disabled = PR_TRUE; // Don't tab unless href, bug 17605
          } else {
            disabled = PR_FALSE;
          }
        }
        else if(nsHTMLAtoms::button==tag.get()) {
          nsCOMPtr<nsIDOMHTMLButtonElement> nextButton(do_QueryInterface(child));
          if (nextButton) {
            nextButton->GetTabIndex(&tabIndex);
            nextButton->GetDisabled(&disabled);
          }
        }
        else if(nsHTMLAtoms::img==tag.get()) {
          nsCOMPtr<nsIDOMHTMLImageElement> nextImage(do_QueryInterface(child));
          nsAutoString usemap;
          if (nextImage) {
            nextImage->GetAttribute(NS_ConvertASCIItoUCS2("usemap"), usemap);
            if (usemap.Length()) {
              //Image is an imagemap.  We need to get its maps and walk its children.
              usemap.StripWhitespace();

              nsCOMPtr<nsIDocument> doc;
              if (NS_SUCCEEDED(child->GetDocument(*getter_AddRefs(doc))) && doc) {
                if (usemap.First() == '#') {
                  usemap.Cut(0, 1);
                }

                nsCOMPtr<nsIHTMLDocument> hdoc(do_QueryInterface(doc));
                if (hdoc) {
                  nsCOMPtr<nsIDOMHTMLMapElement> hmap;
                  if (NS_SUCCEEDED(hdoc->GetImageMap(usemap, getter_AddRefs(hmap))) && hmap) {
                    nsCOMPtr<nsIContent> map(do_QueryInterface(hmap));
                    if (map) {
                      nsCOMPtr<nsIContent> childArea;
                      PRInt32 count, index;
                      map->ChildCount(count);
                      //First see if mCurrentFocus is in this map
                      for (index = 0; index < count; index++) {
                        map->ChildAt(index, *getter_AddRefs(childArea));
                        if (childArea.get() == mCurrentFocus) {
                          nsAutoString tabIndexStr;
                          childArea->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::tabindex, tabIndexStr);
                          PRInt32 ec, val = tabIndexStr.ToInteger(&ec);
                          if (NS_OK == ec && mCurrentTabIndex == val) {
                            //mCurrentFocus is in this map so we must start iterating past it.
                            //We skip the case where mCurrentFocus has the same tab index
                            //as mCurrentTabIndex since the next tab ordered element might
                            //be before it (or after for backwards) in the child list.
                            break;
                          }
                        }
                      }
                      PRInt32 increment = forward ? 1 : - 1;
                      PRInt32 start = index < count ? index + increment : (forward ? 0 : count - 1);
                      for (index = start; index < count && index >= 0; index += increment) {
                        //Iterate over the children.
                        map->ChildAt(index, *getter_AddRefs(childArea));

                        //Got the map area, check its tabindex.
                        nsAutoString tabIndexStr;
                        childArea->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::tabindex, tabIndexStr);
                        PRInt32 ec, val = tabIndexStr.ToInteger(&ec);
                        if (NS_OK == ec && mCurrentTabIndex == val) {
                          //tabindex == the current one, use it.
                          *aResult = childArea;
                          NS_IF_ADDREF(*aResult);
                          return NS_OK;
                        }
                      }
                    }
                  }
                }
              }
            }
          }
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

  if (!mCurrentTarget) {
	  nsCOMPtr<nsIPresShell> presShell;
	  mPresContext->GetShell(getter_AddRefs(presShell));
	  if (presShell) {
	    presShell->GetEventTargetFrame(&mCurrentTarget);
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

  if (!mCurrentTarget) {
	  nsCOMPtr<nsIPresShell> presShell;
	  mPresContext->GetShell(getter_AddRefs(presShell));
	  if (presShell) {
	    presShell->GetEventTargetFrame(&mCurrentTarget);
	  }
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
  if (hHover) {
    //If using hierchical hover check the ancestor chain of mHoverContent
    //to see if we are on it.
    nsCOMPtr<nsIContent> parent = mHoverContent;
    nsIContent* child;
    while (parent) {
	    if (aContent == parent.get()) {
	      aState |= NS_EVENT_STATE_HOVER;
        break;
	    }
      child = parent;
      child->GetParent(*getter_AddRefs(parent));
    }
  }
  else {
    if (aContent == mHoverContent) {
      aState |= NS_EVENT_STATE_HOVER;
    }
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

  // check to see that this state is allowed by style. Check dragover too?
  if (mCurrentTarget && (aState == NS_EVENT_STATE_ACTIVE || aState == NS_EVENT_STATE_HOVER))
  {
    const nsStyleUserInterface* ui;
    mCurrentTarget->GetStyleData(eStyleStruct_UserInterface, ((const nsStyleStruct*&)ui));
    if (ui->mUserInput == NS_STYLE_USER_INPUT_NONE)
      return NS_OK;
  }
  
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

  nsCOMPtr<nsIContent> newHover = nsnull;
  nsCOMPtr<nsIContent> oldHover = nsnull;
  nsCOMPtr<nsIContent> commonHoverParent = nsnull;
  if ((aState & NS_EVENT_STATE_HOVER) && (aContent != mHoverContent)) {
    if (hHover) {
      newHover = aContent;
      oldHover = mHoverContent;

      //Find closest common parent
      nsCOMPtr<nsIContent> parent1 = mHoverContent;
      nsCOMPtr<nsIContent> parent2;
      if (mHoverContent && aContent) {
        while (parent1) {
          parent2 = aContent;
          while (parent2) {
	          if (parent1 == parent2) {
              commonHoverParent = parent1;
              break;
            }
            nsIContent* child2 = parent2;
            child2->GetParent(*getter_AddRefs(parent2));
	        }
          if (commonHoverParent) {
            break;
          }
          nsIContent* child1 = parent1;
          child1->GetParent(*getter_AddRefs(parent1));
        }
      }
      //If new hover content is null we get the top parent of mHoverContent
      else if (mHoverContent) {
        nsCOMPtr<nsIContent> parent = mHoverContent;
        nsIContent* child = nsnull;
        while (parent) {
          child = parent;
          child->GetParent(*getter_AddRefs(parent));
        }
        commonHoverParent = child;
      }
      //Else if old hover content is null we get the top parent of aContent
      else {
        nsCOMPtr<nsIContent> parent = aContent;
        nsIContent* child = nsnull;
        while (parent) {
          child = parent;
          child->GetParent(*getter_AddRefs(parent));
        }
        commonHoverParent = child;
      }
      NS_IF_RELEASE(mHoverContent);
    }
    else {
      //transferring ref to notifyContent from mHoverContent
      notifyContent[1] = mHoverContent; // notify hover first, since more common case
    }
    mHoverContent = aContent;
    NS_IF_ADDREF(mHoverContent);
  }

  if ((aState & NS_EVENT_STATE_FOCUS)) {
    if (aContent && (aContent == mCurrentFocus) && gLastFocusedDocument == mDocument) {
      // gLastFocusedDocument appears to always be correct, that is why
      // I'm not setting it here. This is to catch an edge case.
      NS_IF_RELEASE(gLastFocusedContent);
      gLastFocusedContent = mCurrentFocus;
      NS_IF_ADDREF(gLastFocusedContent);
      aContent = nsnull;
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

  if (notifyContent[0] || newHover || oldHover) { // have at least one to notify about
    nsIDocument *document;  // this presumes content can't get/lose state if not connected to doc
    if (notifyContent[0]) {
      notifyContent[0]->GetDocument(document);
    }
    else if (newHover) {
      newHover->GetDocument(document);
    }
    else {
      oldHover->GetDocument(document);
    }
    if (document) {
      document->BeginUpdate();

      //Notify all content from newHover to the commonHoverParent
      if (newHover) {
        nsCOMPtr<nsIContent> parent;
        newHover->GetParent(*getter_AddRefs(parent));
        document->ContentStatesChanged(newHover, parent);
        while (parent && parent != commonHoverParent) {
          parent->GetParent(*getter_AddRefs(newHover));
          if (newHover && newHover != commonHoverParent) {
            newHover->GetParent(*getter_AddRefs(parent));
            if (parent == commonHoverParent) {
              document->ContentStatesChanged(newHover, nsnull);
            }
            else {
              document->ContentStatesChanged(newHover, parent);
            }
          }
          else {
            break;
          }
        }
      }
      //Notify all content from oldHover to the commonHoverParent
      if (oldHover) {
        nsCOMPtr<nsIContent> parent;
        oldHover->GetParent(*getter_AddRefs(parent));
        document->ContentStatesChanged(oldHover, parent);
        while (parent && parent != commonHoverParent) {
          parent->GetParent(*getter_AddRefs(oldHover));
          if (oldHover && oldHover != commonHoverParent) {
            oldHover->GetParent(*getter_AddRefs(parent));
            if (parent == commonHoverParent) {
              document->ContentStatesChanged(oldHover, nsnull);
            }
            else {
              document->ContentStatesChanged(oldHover, parent);
            }
          }
          else {
            break;
          }
        }
      }

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
          
          EnsureDocument(presShell);
          
          // Make sure we're not switching command dispatchers, if so, surpress the blurred one
          if(gLastFocusedDocument && mDocument) {
            nsCOMPtr<nsIDOMXULCommandDispatcher> newCommandDispatcher;
            nsCOMPtr<nsIDOMXULCommandDispatcher> oldCommandDispatcher;
            nsCOMPtr<nsPIDOMWindow> oldPIDOMWindow;
            nsCOMPtr<nsPIDOMWindow> newPIDOMWindow;
            nsCOMPtr<nsIScriptGlobalObject> oldGlobal;
            nsCOMPtr<nsIScriptGlobalObject> newGlobal;
            gLastFocusedDocument->GetScriptGlobalObject(getter_AddRefs(oldGlobal));
            mDocument->GetScriptGlobalObject(getter_AddRefs(newGlobal));
            nsCOMPtr<nsPIDOMWindow> newWindow = do_QueryInterface(newGlobal);
            nsCOMPtr<nsPIDOMWindow> oldWindow = do_QueryInterface(oldGlobal);
            if(newWindow)
              newWindow->GetRootCommandDispatcher(getter_AddRefs(newCommandDispatcher));
            if(oldWindow)
			  oldWindow->GetRootCommandDispatcher(getter_AddRefs(oldCommandDispatcher));
            if(oldCommandDispatcher && oldCommandDispatcher != newCommandDispatcher)
              oldCommandDispatcher->SetSuppressFocus(PR_TRUE);
          }
          
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

    EnsureDocument(presShell);

    if (gLastFocusedDocument && (gLastFocusedDocument != mDocument) && globalObject) {  
      nsEventStatus status = nsEventStatus_eIgnore;
      nsEvent event;
      event.eventStructType = NS_EVENT;
      event.message = NS_BLUR_CONTENT;

	  	  // Make sure we're not switching command dispatchers, if so, surpress the blurred one
		  if(mDocument) {
		    nsCOMPtr<nsIDOMXULCommandDispatcher> newCommandDispatcher;
            nsCOMPtr<nsIDOMXULCommandDispatcher> oldCommandDispatcher;
		    nsCOMPtr<nsPIDOMWindow> oldPIDOMWindow;
		    nsCOMPtr<nsPIDOMWindow> newPIDOMWindow;
		    nsCOMPtr<nsIScriptGlobalObject> oldGlobal;
		    nsCOMPtr<nsIScriptGlobalObject> newGlobal;
		    gLastFocusedDocument->GetScriptGlobalObject(getter_AddRefs(oldGlobal));
		    mDocument->GetScriptGlobalObject(getter_AddRefs(newGlobal));
            nsCOMPtr<nsPIDOMWindow> newWindow = do_QueryInterface(newGlobal);
		    nsCOMPtr<nsPIDOMWindow> oldWindow = do_QueryInterface(oldGlobal);

		    newWindow->GetRootCommandDispatcher(getter_AddRefs(newCommandDispatcher));
		    oldWindow->GetRootCommandDispatcher(getter_AddRefs(oldCommandDispatcher));
            if(oldCommandDispatcher && oldCommandDispatcher != newCommandDispatcher)
			  oldCommandDispatcher->SetSuppressFocus(PR_TRUE);
		  }

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
nsEventStateManager::RegisterAccessKey(nsIFrame * aFrame, nsIContent* aContent, PRUint32 aKey)
{
  if (!mAccessKeys) {
    mAccessKeys = new nsSupportsHashtable();
    if (!mAccessKeys) {
      return NS_ERROR_FAILURE;
    }
  }

  nsCOMPtr<nsIContent> content;
  if (!aContent) {
    aFrame->GetContent(getter_AddRefs(content));
  }
  else {
    content = aContent;
  }

  if (content) {
    nsAutoString accKey((char)aKey);
    accKey.ToLowerCase();

    nsVoidKey key((void*)accKey.First());

    mAccessKeys->Put(&key, content);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::UnregisterAccessKey(nsIFrame * aFrame, nsIContent* aContent, PRUint32 aKey)
{
  if (!mAccessKeys) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIContent> content;
  if (!aContent) {
    aFrame->GetContent(getter_AddRefs(content));
  }
  else {
    content = aContent;
  }
  if (content) {
    nsAutoString accKey((char)aKey);
    accKey.ToLowerCase();

    nsVoidKey key((void*)accKey.First());

    nsCOMPtr<nsIContent> oldContent = getter_AddRefs(NS_STATIC_CAST(nsIContent*, mAccessKeys->Get(&key)));
    if (oldContent != content) {
      return NS_OK;
    }
    mAccessKeys->Remove(&key);
  }
  return NS_OK;
}

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

NS_IMETHODIMP
nsEventStateManager::DispatchNewEvent(nsISupports* aTarget, nsIDOMEvent* aEvent)
{
  nsresult ret = NS_OK;

  nsCOMPtr<nsIPrivateDOMEvent> privEvt(do_QueryInterface(aEvent));
  if (aEvent) {
    nsEvent* innerEvent;
    privEvt->GetInternalNSEvent(&innerEvent);
    if (innerEvent) {
      nsEventStatus status;
      nsCOMPtr<nsIScriptGlobalObject> target(do_QueryInterface(aTarget));
      if (target) {
        ret = target->HandleDOMEvent(mPresContext, innerEvent, &aEvent, NS_EVENT_FLAG_INIT, &status);
      }
      else {
        nsCOMPtr<nsIDocument> target(do_QueryInterface(aTarget));
        if (target) {
          ret = target->HandleDOMEvent(mPresContext, innerEvent, &aEvent, NS_EVENT_FLAG_INIT, &status);
        }
        else {
          nsCOMPtr<nsIContent> target(do_QueryInterface(aTarget));
          if (target) {
            ret = target->HandleDOMEvent(mPresContext, innerEvent, &aEvent, NS_EVENT_FLAG_INIT, &status);
          }
        }
      }
    }
  }
  return ret;
}

void nsEventStateManager::EnsureDocument(nsIPresContext* aPresContext) {
  if (!mDocument) {
    nsCOMPtr<nsIPresShell> presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));
    EnsureDocument(presShell);
  }
}

void nsEventStateManager::EnsureDocument(nsIPresShell* aPresShell) {
  if (!mDocument && aPresShell)
    aPresShell->GetDocument(&mDocument);
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

