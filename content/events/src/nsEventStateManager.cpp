/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Makoto Kato  <m_kato@ga2.so-net.ne.jp>
 *   Dean Tessman <dean_tessman@hotmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "nsEventStateManager.h"
#include "nsEventListenerManager.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsIWidget.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsDOMEvent.h"
#include "nsHTMLAtoms.h"
#include "nsIFormControl.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLButtonElement.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLMapElement.h"
#include "nsIDOMHTMLBodyElement.h"
#include "nsImageMapUtils.h"
#include "nsIHTMLDocument.h"
#include "nsINameSpaceManager.h"
#include "nsIWebShell.h"
#include "nsIBaseWindow.h"
#include "nsIScrollableView.h"
#include "nsISelection.h"
#include "nsIFrameSelection.h"
#include "nsIDeviceContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMWindowInternal.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMEventTarget.h"
#include "nsIEnumerator.h"
#include "nsFrameTraversal.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIWebNavigation.h"
#include "nsIContentViewer.h"

#include "nsIServiceManager.h"
#include "nsIPrefService.h"
#include "nsIScriptSecurityManager.h"

#include "nsIChromeEventHandler.h"
#include "nsIFocusController.h"

#include "nsXULAtoms.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMKeyEvent.h"
#include "nsIObserverService.h"
#include "nsIDocShell.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIScrollableViewProvider.h"
#include "nsIDOMDocumentRange.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMAbstractView.h"
#include "nsIDOMNSUIEvent.h"

#include "nsIDOMRange.h"
#include "nsICaret.h"
#include "nsILookAndFeel.h"
#include "nsWidgetsCID.h"

#include "nsIFrameTraversal.h"
#include "nsLayoutCID.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsUnicharUtils.h"
#include "nsContentUtils.h"

#if defined (XP_MAC) || defined(XP_MACOSX)
#include <Events.h>
#endif

#if defined(DEBUG_rods) || defined(DEBUG_bryner)
//#define DEBUG_DOCSHELL_FOCUS
#endif

#ifdef DEBUG_DOCSHELL_FOCUS
static char* gDocTypeNames[] = {"eChrome", "eGenericContent", "eFrameSet", "eFrame", "eIFrame"};
#endif

static NS_DEFINE_CID(kFrameTraversalCID, NS_FRAMETRAVERSAL_CID);


//we will use key binding by default now. this wil lbreak viewer for now
#define NON_KEYBINDING 0

static NS_DEFINE_CID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);

nsIContent * gLastFocusedContent = 0; // Strong reference
nsIDocument * gLastFocusedDocument = 0; // Strong reference
nsIPresContext* gLastFocusedPresContext = 0; // Weak reference

PRInt8 nsEventStateManager::sTextfieldSelectModel = eTextfieldSelect_unset;

PRUint32 nsEventStateManager::mInstanceCount = 0;
PRInt32 nsEventStateManager::gGeneralAccesskeyModifier = -1; // magic value of -1 means uninitialized

enum {
 MOUSE_SCROLL_N_LINES,
 MOUSE_SCROLL_PAGE,
 MOUSE_SCROLL_HISTORY,
 MOUSE_SCROLL_TEXTSIZE
};

nsEventStateManager::nsEventStateManager()
  : mGestureDownPoint(0,0),
    m_haveShutdown(PR_FALSE),
    mDOMEventLevel(0),
    mClearedFrameRefsDuringEvent(PR_FALSE)
{
  mLastMouseOverFrame = nsnull;
  mLastDragOverFrame = nsnull;
  mCurrentTarget = nsnull;

  mConsumeFocusEvents = PR_FALSE;
  mLockCursor = 0;

  // init d&d gesture state machine variables
  mIsTrackingDragGesture = PR_FALSE;
  mGestureDownFrame = nsnull;

  mLClickCount = 0;
  mMClickCount = 0;
  mRClickCount = 0;
  mLastFocusedWith = eEventFocusedByUnknown;
  mPresContext = nsnull;
  mCurrentTabIndex = 0;
  mAccessKeys = nsnull;
  mBrowseWithCaret = PR_FALSE;
  mLeftClickOnly = PR_TRUE;
  mNormalLMouseEventInProcess = PR_FALSE;


#ifdef CLICK_HOLD_CONTEXT_MENUS
  mEventDownWidget = nsnull;
#endif
  
  ++mInstanceCount;
}

NS_IMETHODIMP
nsEventStateManager::Init()
{
  nsresult rv;
  nsCOMPtr<nsIObserverService> observerService = 
           do_GetService("@mozilla.org/observer-service;1", &rv);
  if (NS_SUCCEEDED(rv))
  {
    observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_TRUE);
  }

  rv = getPrefBranch();

  if (NS_SUCCEEDED(rv)) {
    mPrefBranch->GetBoolPref("nglayout.events.dispatchLeftClickOnly",
                             &mLeftClickOnly);

    // magic value of -1 means uninitialized
    if (nsEventStateManager::gGeneralAccesskeyModifier == -1) {
      mPrefBranch->GetIntPref("ui.key.generalAccessKey",
                              &nsEventStateManager::gGeneralAccesskeyModifier);
    }
  }

  if (nsEventStateManager::sTextfieldSelectModel == eTextfieldSelect_unset) {
    nsCOMPtr<nsILookAndFeel> lookNFeel(do_GetService(kLookAndFeelCID));
    PRInt32 selectTextfieldsOnKeyFocus = 0;
    lookNFeel->GetMetric(nsILookAndFeel::eMetric_SelectTextfieldsOnKeyFocus,
                         selectTextfieldsOnKeyFocus);
    sTextfieldSelectModel = selectTextfieldsOnKeyFocus ? eTextfieldSelect_auto:
                                                         eTextfieldSelect_manual;
  }

  return rv;
}

nsEventStateManager::~nsEventStateManager()
{
#if CLICK_HOLD_CONTEXT_MENUS
  if ( mClickHoldTimer ) {
    mClickHoldTimer->Cancel();
    mClickHoldTimer = nsnull;
  }
#endif

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

    nsCOMPtr<nsIObserverService> observerService = 
             do_GetService("@mozilla.org/observer-service;1", &rv);
    if (NS_SUCCEEDED(rv))
      {
        observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
      }
  }
  
}

nsresult
nsEventStateManager::Shutdown()
{
  mPrefBranch = nsnull;

  m_haveShutdown = PR_TRUE;
  return NS_OK;
}

nsresult
nsEventStateManager::getPrefBranch()
{
  nsresult rv = NS_OK;

  if (!mPrefBranch) {
    mPrefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  }

  if (NS_FAILED(rv)) return rv;

  if (!mPrefBranch) return NS_ERROR_FAILURE;

  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::Observe(nsISupports *aSubject, 
                             const char *aTopic,
                             const PRUnichar *someData)
{
  if (!nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID))
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
  mCurrentTargetContent = nsnull;

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
#ifndef XP_OS2
    BeginTrackingDragGesture ( aPresContext, (nsGUIEvent*)aEvent, aTargetFrame );
#endif
    mLClickCount = ((nsMouseEvent*)aEvent)->clickCount;
    SetClickCount(aPresContext, (nsMouseEvent*)aEvent, aStatus);
    mNormalLMouseEventInProcess = PR_TRUE;
    break;
  case NS_MOUSE_MIDDLE_BUTTON_DOWN:
    mMClickCount = ((nsMouseEvent*)aEvent)->clickCount;
    SetClickCount(aPresContext, (nsMouseEvent*)aEvent, aStatus);
    break;
  case NS_MOUSE_RIGHT_BUTTON_DOWN:
#ifdef XP_OS2
    BeginTrackingDragGesture ( aPresContext, (nsGUIEvent*)aEvent, aTargetFrame );
#endif
    mRClickCount = ((nsMouseEvent*)aEvent)->clickCount;
    SetClickCount(aPresContext, (nsMouseEvent*)aEvent, aStatus);
    break;
  case NS_MOUSE_LEFT_BUTTON_UP:
#ifdef CLICK_HOLD_CONTEXT_MENUS
    KillClickHoldTimer();
#endif
#ifndef XP_OS2
    StopTrackingDragGesture();
#endif
    mNormalLMouseEventInProcess = PR_FALSE;
  case NS_MOUSE_RIGHT_BUTTON_UP:
#ifdef XP_OS2
    StopTrackingDragGesture();
#endif
  case NS_MOUSE_MIDDLE_BUTTON_UP:
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
    // Flush reflows and invalidates to eliminate flicker when both a reflow
    // and visual change occur in an event callback. See bug  #36849
    FlushPendingEvents(aPresContext); 
    break;
  case NS_MOUSE_EXIT:
    GenerateMouseEnterExit(aPresContext, (nsGUIEvent*)aEvent);
    //This is a window level mouseenter event and should stop here
    aEvent->message = 0;
    break;
#ifdef CLICK_HOLD_CONTEXT_MENUS
  case NS_DRAGDROP_GESTURE:
    // an external drag gesture event came in, not generated internally
    // by Gecko. Make sure we get rid of the click-hold timer.
    KillClickHoldTimer();
    break;
#endif
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
            nsCOMPtr<nsIFocusController> focusController;

            nsCOMPtr<nsIScriptGlobalObject> ourGlobal;
            gLastFocusedDocument->GetScriptGlobalObject(getter_AddRefs(ourGlobal));
            nsCOMPtr<nsIDOMWindowInternal> rootWindow;
            nsCOMPtr<nsPIDOMWindow> ourWindow = do_QueryInterface(ourGlobal);

            // If the focus controller is already suppressed, it means that we are
            // in the middle of an activate sequence. In this case, we do _not_
            // want to fire a blur on the previously focused content, since we
            // will be focusing it again later when we receive the NS_ACTIVATE
            // event.  See bug 120209.

            PRBool isAlreadySuppressed = PR_FALSE;
            if(ourWindow) {
              ourWindow->GetRootFocusController(getter_AddRefs(focusController));
              if (focusController) {
                focusController->GetSuppressFocus(&isAlreadySuppressed);
                focusController->SetSuppressFocus(PR_TRUE, "NS_GOTFOCUS ESM Suppression");
              }
            }
            
            if (!isAlreadySuppressed) {
              gLastFocusedDocument->HandleDOMEvent(gLastFocusedPresContext, &blurevent, nsnull, NS_EVENT_FLAG_INIT, &blurstatus);
              if (!mCurrentFocus && gLastFocusedContent) {
                // must send it to the element that is losing focus,
                // since SendFocusBlur wont be called
                gLastFocusedContent->HandleDOMEvent(gLastFocusedPresContext, &blurevent, nsnull, NS_EVENT_FLAG_INIT, &blurstatus);
                
                nsCOMPtr<nsIDocument> doc;
                if (gLastFocusedContent) // could have changed in HandleDOMEvent
                  gLastFocusedContent->GetDocument(*getter_AddRefs(doc));
                if (doc) {
                  nsCOMPtr<nsIPresShell> shell;
                  doc->GetShellAt(0, getter_AddRefs(shell));
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
            }

            if (focusController) {
              focusController->SetSuppressFocus(PR_FALSE, "NS_GOTFOCUS ESM Suppression");
            }
          }
        }
        
        // fire focus on window, not document
        nsCOMPtr<nsIScriptGlobalObject> globalObject;
        mDocument->GetScriptGlobalObject(getter_AddRefs(globalObject));
        if (globalObject) {
          nsCOMPtr<nsIContent> currentFocus = mCurrentFocus;
          mCurrentFocus = nsnull;
          if(gLastFocusedDocument != mDocument) {
            mDocument->HandleDOMEvent(aPresContext, &focusevent, nsnull, NS_EVENT_FLAG_INIT, &status);
            if (currentFocus && currentFocus != gLastFocusedContent)
              currentFocus->HandleDOMEvent(aPresContext, &focusevent, nsnull, NS_EVENT_FLAG_INIT, &status);
          }
          
          globalObject->HandleDOMEvent(aPresContext, &focusevent, nsnull, NS_EVENT_FLAG_INIT, &status);
          mCurrentFocus = currentFocus; // we kept this reference above
          NS_IF_RELEASE(gLastFocusedContent);
          gLastFocusedContent = mCurrentFocus;
          NS_IF_ADDREF(gLastFocusedContent);
        }
        
        // Try to keep the focus controllers and the globals in synch
        if(gLastFocusedDocument != mDocument && gLastFocusedDocument && mDocument) {
          nsCOMPtr<nsIScriptGlobalObject> lastGlobal;
          gLastFocusedDocument->GetScriptGlobalObject(getter_AddRefs(lastGlobal));
          nsCOMPtr<nsPIDOMWindow> lastWindow;
          nsCOMPtr<nsIFocusController> lastController;
          if(lastGlobal) {
            lastWindow = do_QueryInterface(lastGlobal);

            if(lastWindow)
              lastWindow->GetRootFocusController(getter_AddRefs(lastController));
          }
          
          nsCOMPtr<nsIScriptGlobalObject> nextGlobal;
          mDocument->GetScriptGlobalObject(getter_AddRefs(nextGlobal));
          nsCOMPtr<nsIFocusController> nextController;
          if(nextGlobal) {
            nsCOMPtr<nsPIDOMWindow> nextWindow = do_QueryInterface(nextGlobal);
            if(nextWindow) {
              nextWindow->GetRootFocusController(getter_AddRefs(nextController));
            }
          }
          
          if(lastController != nextController && lastController && nextController) {
            lastController->SetActive(PR_FALSE);
          }
        }
        
        NS_IF_RELEASE(gLastFocusedDocument);
        gLastFocusedDocument = mDocument;
        gLastFocusedPresContext = aPresContext;
        NS_IF_ADDREF(gLastFocusedDocument);
      }

      ResetBrowseWithCaret(&mBrowseWithCaret);
    }

    break;

  case NS_LOSTFOCUS:
    {
      // Hide the caret used in "browse with caret mode" 
      if (mBrowseWithCaret && mPresContext) {
        nsCOMPtr<nsIPresShell> presShell;
        mPresContext->GetShell(getter_AddRefs(presShell));
        if (presShell)
           SetContentCaretVisible(presShell, mCurrentFocus, PR_FALSE);
      }

      // Hold the blur, wait for the focus so we can query the style of the focus
      // target as to what to do with the event. If appropriate we fire the blur
      // at that time.
#if defined(XP_WIN) || defined(XP_OS2)
      if(! NS_STATIC_CAST(nsFocusEvent*, aEvent)->isMozWindowTakingFocus) {
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
              nsCOMPtr<nsIPresShell> shell;
              doc->GetShellAt(0, getter_AddRefs(shell));
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
      } 
#endif
    }
    break;

 case NS_ACTIVATE:
    {
      // If we have a command dispatcher, and if it has a focused window and a
      // focused element in its focus memory, then restore the focus to those
      // objects.
      EnsureDocument(aPresContext);
#ifdef DEBUG_hyatt
      printf("ESM: GOT ACTIVATE.\n");
#endif
      nsCOMPtr<nsIFocusController> focusController;
      nsCOMPtr<nsIDOMElement> focusedElement;
      nsCOMPtr<nsIDOMWindowInternal> focusedWindow;
      nsCOMPtr<nsIDOMXULDocument> xulDoc = do_QueryInterface(mDocument);

      nsCOMPtr<nsIScriptGlobalObject> globalObj;
      mDocument->GetScriptGlobalObject(getter_AddRefs(globalObj));
      nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(globalObj));
      NS_ASSERTION(win, "win is null.  this happens [often on xlib builds].  see bug #79213");
      if (!win) return NS_ERROR_NULL_POINTER;
      win->GetRootFocusController(getter_AddRefs(focusController));

      if (focusController) {
        // Obtain focus info from the command dispatcher.
        focusController->GetFocusedWindow(getter_AddRefs(focusedWindow));
        focusController->GetFocusedElement(getter_AddRefs(focusedElement));

        focusController->SetSuppressFocusScroll(PR_TRUE);
        focusController->SetActive(PR_TRUE);
      }

      if (!focusedWindow) {
        nsCOMPtr<nsIScriptGlobalObject> globalObject;
        mDocument->GetScriptGlobalObject(getter_AddRefs(globalObject));
        focusedWindow = do_QueryInterface(globalObject);
      }

      // Focus the DOM window.
      NS_WARN_IF_FALSE(focusedWindow,"check why focusedWindow is null!!!");
      if(focusedWindow) {
        focusedWindow->Focus();

        nsCOMPtr<nsIDOMDocument> domDoc;
        nsCOMPtr<nsIDocument> document;
        focusedWindow->GetDocument(getter_AddRefs(domDoc));
        if (domDoc) {
          document = do_QueryInterface(domDoc);
          nsCOMPtr<nsIPresShell> shell;
          document->GetShellAt(0, getter_AddRefs(shell));
          NS_ASSERTION(shell, "Focus events should not be getting thru when this is null!");
          if (shell) {
            if (focusedElement) {
              nsCOMPtr<nsIContent> focusContent = do_QueryInterface(focusedElement);
              nsCOMPtr<nsIPresContext> context;
              shell->GetPresContext(getter_AddRefs(context));
              focusContent->SetFocus(context);
            }

            //disable selection mousedown state on activation
            nsCOMPtr<nsIFrameSelection> frameSel;
            shell->GetFrameSelection(getter_AddRefs(frameSel));
            if (frameSel) {
              frameSel->SetMouseDownState(PR_FALSE);
            }
          }
        }  
      }
      
      if (focusController) {
        PRBool isSuppressed;
        focusController->GetSuppressFocus(&isSuppressed);
        while(isSuppressed){
          focusController->SetSuppressFocus(PR_FALSE, "Activation Suppression"); // Unsuppress and let the command dispatcher listen again.
          focusController->GetSuppressFocus(&isSuppressed);
        }
        focusController->SetSuppressFocusScroll(PR_FALSE);
      }
    }
  break;
    
 case NS_DEACTIVATE:
    {
      EnsureDocument(aPresContext);

      nsCOMPtr<nsIScriptGlobalObject> ourGlobal;
      mDocument->GetScriptGlobalObject(getter_AddRefs(ourGlobal));

      // Suppress the command dispatcher for the duration of the
      // de-activation.  This will cause it to remember the last
      // focused sub-window and sub-element for this top-level
      // window.
      nsCOMPtr<nsIFocusController> focusController = getter_AddRefs(GetFocusControllerForDocument(mDocument));
        if (focusController) {
          // Suppress the command dispatcher.
          focusController->SetSuppressFocus(PR_TRUE, "Deactivate Suppression");
        }

      // Now fire blurs.  We have to fire a blur on the focused window
      // and on the focused element if there is one.
      if (gLastFocusedDocument && gLastFocusedDocument == mDocument) {
        if (gLastFocusedContent) {
          // Blur the element.
          nsCOMPtr<nsIPresShell> shell;
          
          nsCOMPtr<nsIDOMElement> focusedElement;
          if (focusController)
            focusController->GetFocusedElement(getter_AddRefs(focusedElement));
          nsCOMPtr<nsIContent> focusedContent = do_QueryInterface(focusedElement);
          
          gLastFocusedDocument->GetShellAt(0, getter_AddRefs(shell));
          if (shell) {
            nsCOMPtr<nsIPresContext> oldPresContext;
            shell->GetPresContext(getter_AddRefs(oldPresContext));
            
            nsEventStatus status = nsEventStatus_eIgnore;
            nsEvent event;
            event.eventStructType = NS_EVENT;
            event.message = NS_BLUR_CONTENT;
            event.flags = 0;
            nsCOMPtr<nsIEventStateManager> esm;
            oldPresContext->GetEventStateManager(getter_AddRefs(esm));
            esm->SetFocusedContent(gLastFocusedContent);
            if(focusedContent)
              focusedContent->HandleDOMEvent(oldPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status); 
            esm->SetFocusedContent(nsnull);
            NS_IF_RELEASE(gLastFocusedContent);
          }
        }

        nsEventStatus status = nsEventStatus_eIgnore;
        nsEvent event;
        event.eventStructType = NS_EVENT;
        event.message = NS_BLUR_CONTENT;
    
        // fire blur on document and window
        mDocument->HandleDOMEvent(aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
        if (ourGlobal)
          ourGlobal->HandleDOMEvent(aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
        else {
          // If the document is being torn down, we can't fire a blur on
          // the window, but we still need to tell the focus controller
          // that it isn't active.
          
          nsCOMPtr<nsIFocusController> fc = getter_AddRefs(GetFocusControllerForDocument(gLastFocusedDocument));
          if (fc)
            fc->SetActive(PR_FALSE);
        }

        // Now clear our our global variables
        mCurrentTarget = nsnull;
        NS_IF_RELEASE(gLastFocusedDocument);
        gLastFocusedPresContext = nsnull;
      }             

      if (focusController) {
        focusController->SetActive(PR_FALSE);
        focusController->SetSuppressFocus(PR_FALSE, "Deactivate Suppression");
      }
    } 
    
    break;
    
  case NS_KEY_PRESS:
    {

      nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;

      PRBool isSpecialAccessKeyDown = PR_FALSE;
      switch (gGeneralAccesskeyModifier) {
        case nsIDOMKeyEvent::DOM_VK_CONTROL: isSpecialAccessKeyDown = keyEvent->isControl; break;
        case nsIDOMKeyEvent::DOM_VK_ALT: isSpecialAccessKeyDown = keyEvent->isAlt; break;
        case nsIDOMKeyEvent::DOM_VK_META: isSpecialAccessKeyDown = keyEvent->isMeta; break;
      }

      //This is to prevent keyboard scrolling while alt or other accesskey modifier in use.
      if (isSpecialAccessKeyDown)
        HandleAccessKey(aPresContext, keyEvent, aStatus, -1, eAccessKeyProcessingNormal);
    }
  case NS_KEY_DOWN:
  case NS_KEY_UP:
  case NS_MOUSE_SCROLL:
    {
      if (mCurrentFocus) {
        mCurrentTargetContent = mCurrentFocus;
      }
    }
    break;
  }
  return NS_OK;
}

// Note: for the in parameter aChildOffset,
// -1 stands for not bubbling from the child docShell
// 0 -- childCount - 1 stands for the child docShell's offset
// which bubbles up the access key handling
void
nsEventStateManager::HandleAccessKey(nsIPresContext* aPresContext,
                                     nsKeyEvent *aEvent,
                                     nsEventStatus* aStatus,
                                     PRInt32 aChildOffset,
                                     ProcessingAccessKeyState aAccessKeyState)
{

  // Alt or other accesskey modifier is down, we may need to do an accesskey
  if (mAccessKeys) {
    // Someone registered an accesskey.  Find and activate it.
    PRUnichar accKey = nsCRT::ToLower((char)aEvent->charCode);

    nsVoidKey key(NS_INT32_TO_PTR(accKey));
    if (mAccessKeys->Exists(&key)) {
      nsCOMPtr<nsIContent> content = dont_AddRef(NS_STATIC_CAST(nsIContent*, mAccessKeys->Get(&key)));

      PRBool isXUL = content->IsContentOfType(nsIContent::eXUL);

      // if it's a XUL element...
      if (isXUL) {

        // find out what type of content node this is
        nsCOMPtr<nsIAtom> atom;
        nsresult rv = content->GetTag(*getter_AddRefs(atom));
        if (NS_SUCCEEDED(rv) && atom) {
          if (atom == nsXULAtoms::label) {
            // If anything fails, this will be null ...
            nsCOMPtr<nsIDOMElement> element;

            nsAutoString control;
            content->GetAttr(kNameSpaceID_None, nsXULAtoms::control, control);
            if (!control.IsEmpty()) {
              nsCOMPtr<nsIDocument> document;
              content->GetDocument(*getter_AddRefs(document));
              nsCOMPtr<nsIDOMDocument> domDocument = do_QueryInterface(document);
              if (domDocument)
                domDocument->GetElementById(control, getter_AddRefs(element));
            }
            // ... that here we'll either change |content| to the element
            // referenced by |element|, or clear it.
            content = do_QueryInterface(element);
          }
        }

        if (!content)
          return;

        nsCOMPtr<nsIPresShell> presShell;
        aPresContext->GetShell(getter_AddRefs(presShell));

        nsIFrame* frame = nsnull;
        presShell->GetPrimaryFrameFor(content, &frame);

        if (frame) {
          const nsStyleVisibility* vis;
          frame->GetStyleData(eStyleStruct_Visibility,
            ((const nsStyleStruct *&)vis));
          PRBool viewShown = PR_TRUE;

          nsIView* frameView = nsnull;
          frame->GetView(mPresContext, &frameView);

          if (!frameView) {
            nsIFrame* parentWithView = nsnull;
            frame->GetParentWithView(mPresContext, &parentWithView);

            if (parentWithView)
              parentWithView->GetView(mPresContext, &frameView);
          }

          while (frameView) {
            nsViewVisibility visib;
            frameView->GetVisibility(visib);

            if (visib == nsViewVisibility_kHide) {
              viewShown = PR_FALSE;
              break;
            }

            frameView->GetParent(frameView);
          }

          // get the XUL element
          nsCOMPtr<nsIDOMXULElement> element = do_QueryInterface(content);

          // if collapsed or hidden, we don't get tabbed into.
          if (viewShown &&
            vis->mVisible != NS_STYLE_VISIBILITY_COLLAPSE &&
            vis->mVisible != NS_STYLE_VISIBILITY_HIDDEN &&
            element) {

            // find out what type of content node this is
            nsCOMPtr<nsIAtom> atom;
            nsresult rv = content->GetTag(*getter_AddRefs(atom));

            if (NS_SUCCEEDED(rv) && atom) {
              // define behavior for each type of XUL element:
              if (atom == nsXULAtoms::textbox || atom == nsXULAtoms::menulist) {
                // if it's a text box or menulist, give it focus
                element->Focus();
              } else if (atom == nsXULAtoms::toolbarbutton) {
                // if it's a toolbar button, just click
                element->Click();
              } else {
                // otherwise, focus and click in it
                element->Focus();
                element->Click();
              }
            }
          }
        }
      } else { // otherwise, it must be HTML
        // It's hard to say what HTML4 wants us to do in all cases.
        // So for now we'll settle for A) Set focus
        ChangeFocus(content, eEventFocusedByKey);

        nsresult rv = getPrefBranch();
        PRBool activate = PR_TRUE;

        if (NS_SUCCEEDED(rv)) {
          mPrefBranch->GetBoolPref("accessibility.accesskeycausesactivation", &activate);
        }

        if (activate) {
          // B) Click on it if the users prefs indicate to do so.
          nsEventStatus status = nsEventStatus_eIgnore;
          nsMouseEvent event;
          event.eventStructType = NS_MOUSE_EVENT;
          event.message = NS_MOUSE_LEFT_CLICK;
          event.isShift = PR_FALSE;
          event.isControl = PR_FALSE;
          event.isAlt = PR_FALSE;
          event.isMeta = PR_FALSE;
          event.clickCount = 0;
          event.widget = nsnull;
          content->HandleDOMEvent(mPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
        }

      }

      *aStatus = nsEventStatus_eConsumeNoDefault;
    }
  }
  // after the local accesskey handling
  if (nsEventStatus_eConsumeNoDefault != *aStatus) {
    // checking all sub docshells

    nsCOMPtr<nsISupports> pcContainer;
    aPresContext->GetContainer(getter_AddRefs(pcContainer));
    NS_ASSERTION(pcContainer, "no container for presContext");

    nsCOMPtr<nsIDocShellTreeNode> docShell(do_QueryInterface(pcContainer));
    NS_ASSERTION(docShell, "no docShellTreeNode for presContext");

    PRInt32 childCount;
    docShell->GetChildCount(&childCount);
    for (PRInt32 counter = 0; counter < childCount; counter++) {
      // Not processing the child which bubbles up the handling
      if (aAccessKeyState == eAccessKeyProcessingUp && counter == aChildOffset)
        continue;

      nsCOMPtr<nsIDocShellTreeItem> subShellItem;
      nsCOMPtr<nsIPresShell> subPS;
      nsCOMPtr<nsIPresContext> subPC;
      nsCOMPtr<nsIEventStateManager> subESM;


      docShell->GetChildAt(counter, getter_AddRefs(subShellItem));
      nsCOMPtr<nsIDocShell> subDS = do_QueryInterface(subShellItem);
      if (subDS && IsShellVisible(subDS)) {
        subDS->GetPresShell(getter_AddRefs(subPS));

        // Docshells need not have a presshell (eg. display:none
        // iframes, docshells in transition between documents, etc).
        if (!subPS) {
          // Oh, well.  Just move on to the next child
          continue;
        }

        subPS->GetPresContext(getter_AddRefs(subPC));
        NS_ASSERTION(subPC, "PresShell without PresContext");

        subPC->GetEventStateManager(getter_AddRefs(subESM));

        nsEventStateManager* esm = NS_STATIC_CAST(nsEventStateManager *, NS_STATIC_CAST(nsIEventStateManager *, subESM.get()));
        if (esm)
          esm->HandleAccessKey(subPC, aEvent, aStatus, -1, eAccessKeyProcessingDown);

        if (nsEventStatus_eConsumeNoDefault == *aStatus)
          break;
      }
    }
  }// if end . checking all sub docshell ends here.

  // bubble up the process to the parent docShell if necesary
  if (eAccessKeyProcessingDown != aAccessKeyState && nsEventStatus_eConsumeNoDefault != *aStatus) {
    nsCOMPtr<nsISupports> pcContainer;
    aPresContext->GetContainer(getter_AddRefs(pcContainer));
    NS_ASSERTION(pcContainer, "no container for presContext");

    nsCOMPtr<nsIDocShellTreeItem> docShell(do_QueryInterface(pcContainer));
    NS_ASSERTION(docShell, "no docShellTreeItem for presContext");
    
    nsCOMPtr<nsIDocShellTreeItem> parentShellItem;
    docShell->GetParent(getter_AddRefs(parentShellItem));
    nsCOMPtr<nsIDocShell> parentDS = do_QueryInterface(parentShellItem);
    if (parentDS) {
      PRInt32 myOffset;
      docShell->GetChildOffset(&myOffset);

      nsCOMPtr<nsIPresShell> parentPS;
      nsCOMPtr<nsIPresContext> parentPC;
      nsCOMPtr<nsIEventStateManager> parentESM;

      parentDS->GetPresShell(getter_AddRefs(parentPS));
      NS_ASSERTION(parentPS, "Our PresShell exists but the parent's does not?");

      parentPS->GetPresContext(getter_AddRefs(parentPC));
      NS_ASSERTION(parentPC, "PresShell without PresContext");

      parentPC->GetEventStateManager(getter_AddRefs(parentESM));

      nsEventStateManager* esm = NS_STATIC_CAST(nsEventStateManager *, NS_STATIC_CAST(nsIEventStateManager *, parentESM.get()));
      if (esm)
        esm->HandleAccessKey(parentPC, aEvent, aStatus, myOffset, eAccessKeyProcessingUp);
    }
  }// if end. bubble up process
}// end of HandleAccessKey


#ifdef CLICK_HOLD_CONTEXT_MENUS


//
// CreateClickHoldTimer
//
// Fire off a timer for determining if the user wants click-hold. This timer
// is a one-shot that will be cancelled when the user moves enough to fire
// a drag.
//
void
nsEventStateManager :: CreateClickHoldTimer ( nsIPresContext* inPresContext, nsGUIEvent* inMouseDownEvent )
{
  // just to be anal (er, safe)
  if ( mClickHoldTimer ) {
    mClickHoldTimer->Cancel();
    mClickHoldTimer = nsnull;
  }
    
  // if content clicked on has a popup, don't even start the timer
  // since we'll end up conflicting and both will show.
  nsCOMPtr<nsIContent> clickedContent;
  if ( mGestureDownFrame ) {
    mGestureDownFrame->GetContent(getter_AddRefs(clickedContent));
    if ( clickedContent ) {
      // check for the |popup| attribute
      nsAutoString popup;
      clickedContent->GetAttr(kNameSpaceID_None, nsXULAtoms::popup, popup);
      if ( popup != NS_LITERAL_STRING("") )
        return;
      
      // check for a <menubutton> like bookmarks
      nsCOMPtr<nsIAtom> tag;
      clickedContent->GetTag ( *getter_AddRefs(tag) );
      if ( tag == nsXULAtoms::menubutton )
        return;
    }
  }

  mClickHoldTimer = do_CreateInstance("@mozilla.org/timer;1");
  if ( mClickHoldTimer )
    mClickHoldTimer->InitWithFuncCallback(sClickHoldCallback, this, kClickHoldDelay, 
                                          nsITimer::TYPE_ONE_SHOT);

  mEventPoint = inMouseDownEvent->point;
  mEventRefPoint = inMouseDownEvent->refPoint;
  mEventDownWidget = inMouseDownEvent->widget;
  
  mEventPresContext = inPresContext;
  
} // CreateClickHoldTimer


//
// KillClickHoldTimer
//
// Stop the timer that would show the context menu dead in its tracks
//
void
nsEventStateManager :: KillClickHoldTimer ( )
{
  if ( mClickHoldTimer ) {
    mClickHoldTimer->Cancel();
    mClickHoldTimer = nsnull;
  }

  mEventDownWidget = nsnull;
  mEventPresContext = nsnull;

} // KillTooltipTimer


//
// sClickHoldCallback
//
// This fires after the mouse has been down for a certain length of time. 
//
void
nsEventStateManager :: sClickHoldCallback ( nsITimer *aTimer, void* aESM )
{
  nsEventStateManager* self = NS_STATIC_CAST(nsEventStateManager*, aESM);
  if ( self )
    self->FireContextClick();

  // NOTE: |aTimer| and |self->mAutoHideTimer| are invalid after calling ClosePopup();
  
} // sAutoHideCallback


//
// FireContextClick
//
// If we're this far, our timer has fired, which means the mouse has been down
// for a certain period of time and has not moved enough to generate a dragGesture.
// We can be certain the user wants a context-click at this stage, so generate
// a dom event and fire it in.
//
// After the event fires, check if PreventDefault() has been set on the event which
// means that someone either ate the event or put up a context menu. This is our cue
// to stop tracking the drag gesture. If we always did this, draggable items w/out
// a context menu wouldn't be draggable after a certain length of time, which is
// _not_ what we want.
//
void
nsEventStateManager :: FireContextClick ( )
{
  if ( !mEventDownWidget || !mEventPresContext )
    return;

#if defined (XP_MAC) || defined(XP_MACOSX)
  // hacky OS call to ensure that we don't show a context menu when the user
  // let go of the mouse already, after a long, cpu-hogging operation prevented
  // us from handling any OS events. See bug 117589.
  if (!::StillDown())
    return;
#endif

  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event;
  event.eventStructType = NS_MOUSE_EVENT;
  event.message = NS_CONTEXTMENU;
  event.widget = mEventDownWidget;
  event.clickCount = 1;
  event.point = mEventPoint;
  event.refPoint = mEventRefPoint;
  event.isShift = PR_FALSE;
  event.isControl = PR_FALSE;
  event.isAlt = PR_FALSE;
  event.isMeta = PR_FALSE;

  // Dispatch to the DOM. We have to fake out the ESM and tell it that the
  // current target frame is actually where the mouseDown occurred, otherwise it
  // will use the frame the mouse is currently over which may or may not be
  // the same. (Note: saari and I have decided that we don't have to reset |mCurrentTarget|
  // when we're through because no one else is doing anything more with this
  // event and it will get reset on the very next event to the correct frame).
  mCurrentTarget = mGestureDownFrame;
  nsCOMPtr<nsIContent> lastContent;
  if ( mGestureDownFrame ) {
    mGestureDownFrame->GetContent(getter_AddRefs(lastContent));
    
    if ( lastContent ) {
      // before dispatching, check that we're not on something that doesn't get a context menu
      PRBool allowedToDispatch = PR_TRUE;

      nsCOMPtr<nsIAtom> tag;
      lastContent->GetTag ( *getter_AddRefs(tag) );
      nsCOMPtr<nsIDOMHTMLInputElement> inputElm ( do_QueryInterface(lastContent) );
      PRBool isFormControl =
        lastContent->IsContentOfType(nsIContent::eHTML_FORM_CONTROL);
      if ( inputElm ) {
        // of all input elements, only ones dealing with text are allowed to have context menus
        if ( tag == nsHTMLAtoms::input ) {
          nsAutoString type;
          lastContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::type, type);
          if ( type != NS_LITERAL_STRING("") && type != NS_LITERAL_STRING("text") &&
                type != NS_LITERAL_STRING("password") && type != NS_LITERAL_STRING("file") )
            allowedToDispatch = PR_FALSE;
        }
      }
      else if ( isFormControl && tag != nsHTMLAtoms::textarea )
        // catches combo-boxes, <object>
        allowedToDispatch = PR_FALSE;
      else if ( tag == nsXULAtoms::scrollbar || tag == nsXULAtoms::scrollbarbutton || tag == nsXULAtoms::button )
        allowedToDispatch = PR_FALSE;
      else if ( tag == nsHTMLAtoms::applet || tag == nsHTMLAtoms::embed )
        allowedToDispatch = PR_FALSE;
      else if ( tag == nsXULAtoms::toolbarbutton ) {
        // a <toolbarbutton> that has the container attribute set will already have its
        // own dropdown. 
        nsAutoString container;
        lastContent->GetAttr(kNameSpaceID_None, nsXULAtoms::container, container);
        if ( container.Length() )
          allowedToDispatch = PR_FALSE;
      }
    
      if ( allowedToDispatch ) {
        // stop selection tracking, we're in control now
        nsCOMPtr<nsIFrameSelection> frameSel;
        GetSelection ( mGestureDownFrame, mEventPresContext, getter_AddRefs(frameSel) );
        if ( frameSel ) {
          PRBool mouseDownState = PR_TRUE;
          frameSel->GetMouseDownState(&mouseDownState);
          if (mouseDownState)
            frameSel->SetMouseDownState(PR_FALSE);
        }
        
        // dispatch to DOM
        lastContent->HandleDOMEvent(mEventPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
        
        // Firing the DOM event could have caused mGestureDownFrame to
        // be destroyed.  So, null-check it again.

        if (mGestureDownFrame) {
          // dispatch to the frame
          mGestureDownFrame->HandleEvent(mEventPresContext, &event, &status);   
        }
      }
    }
  }
  
  // now check if the event has been handled. If so, stop tracking a drag
  if ( status == nsEventStatus_eConsumeNoDefault )
    StopTrackingDragGesture();

  KillClickHoldTimer();
  
} // FireContextClick

#endif


// 
// BeginTrackingDragGesture
//
// Record that the mouse has gone down and that we should move to TRACKING state
// of d&d gesture tracker.
//
// We also use this to track click-hold context menus on mac. When the mouse goes down,
// fire off a short timer. If the timer goes off and we have yet to fire the
// drag gesture (ie, the mouse hasn't moved a certain distance), then we can
// assume the user wants a click-hold, so fire a context-click event. We only
// want to cancel the drag gesture if the context-click event is handled.
//
void
nsEventStateManager :: BeginTrackingDragGesture ( nsIPresContext* aPresContext, nsGUIEvent* inDownEvent, nsIFrame* inDownFrame )
{
  mIsTrackingDragGesture = PR_TRUE;
  mGestureDownPoint = inDownEvent->point;
  mGestureDownFrame = inDownFrame;
  
#ifdef CLICK_HOLD_CONTEXT_MENUS
  // fire off a timer to track click-hold
  CreateClickHoldTimer ( aPresContext, inDownEvent );
#endif

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
// GetSelection
//
// Helper routine to get an nsIFrameSelection from the given frame
//
void
nsEventStateManager :: GetSelection ( nsIFrame* inFrame, nsIPresContext* inPresContext, nsIFrameSelection** outSelection )
{
  *outSelection = nsnull;
  
  if (inFrame) {
    nsCOMPtr<nsISelectionController> selCon;
    nsresult rv = inFrame->GetSelectionController(inPresContext, getter_AddRefs(selCon));

    if (NS_SUCCEEDED(rv) && selCon) {
      nsCOMPtr<nsIFrameSelection> frameSel;

      frameSel = do_QueryInterface(selCon);

      if (! frameSel) {
        nsCOMPtr<nsIPresShell> shell;
        rv = inPresContext->GetShell(getter_AddRefs(shell));

        if (NS_SUCCEEDED(rv) && shell)
          rv = shell->GetFrameSelection(getter_AddRefs(frameSel));
      }
      
      *outSelection = frameSel.get();
      NS_IF_ADDREF(*outSelection);
    }
  }

} // GetSelection


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
// a drag too early, or very similar which might not trigger a drag. 
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
    nsCOMPtr<nsIFrameSelection> frameSel;
    GetSelection ( mGestureDownFrame, aPresContext, getter_AddRefs(frameSel) );
    if ( frameSel ) {
      PRBool mouseDownState = PR_TRUE;
      frameSel->GetMouseDownState(&mouseDownState);
      if (mouseDownState) {
        StopTrackingDragGesture();
        return;
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
#ifdef CLICK_HOLD_CONTEXT_MENUS
      // stop the click-hold before we fire off the drag gesture, in case
      // it takes a long time
      KillClickHoldTimer();
#endif

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

  // Now flush all pending notifications.
  FlushPendingEvents(aPresContext);
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
  
  nsCOMPtr<nsIDOMWindow> windowContent;
  rootWindow->GetContent(getter_AddRefs(windowContent));
  if(!windowContent) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMDocument> domDoc;
  windowContent->GetDocument(getter_AddRefs(domDoc));
  if(!domDoc) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  if(!doc) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresShell> presShell;
  doc->GetShellAt(0, getter_AddRefs(presShell));
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
  textzoom += ((float)change) / 10;
  if (textzoom > 0 && textzoom <= 20)
    mv->SetTextZoom(textzoom);

  return NS_OK;
}


//
nsresult
nsEventStateManager::DoWheelScroll(nsIPresContext* aPresContext,
                                   nsIFrame* aTargetFrame,
                                   nsMouseScrollEvent* msEvent,
                                   PRInt32 numLines, PRBool scrollPage,
                                   PRBool aUseTargetFrame)
{
  nsCOMPtr<nsIContent> targetContent;
  aTargetFrame->GetContent(getter_AddRefs(targetContent));
  if (!targetContent)
    GetFocusedContent(getter_AddRefs(targetContent));
  if (!targetContent) return NS_OK;
  nsCOMPtr<nsIDocument> targetDoc;
  targetContent->GetDocument(*getter_AddRefs(targetDoc));
  if (!targetDoc) return NS_OK;
  nsCOMPtr<nsIDOMDocumentEvent> targetDOMDoc(do_QueryInterface(targetDoc));

  nsCOMPtr<nsIDOMEvent> event;
  targetDOMDoc->CreateEvent(NS_LITERAL_STRING("MouseScrollEvents"), getter_AddRefs(event));
  if (event) {
    nsCOMPtr<nsIDOMMouseEvent> mouseEvent(do_QueryInterface(event));
    nsCOMPtr<nsIDOMDocumentView> docView = do_QueryInterface(targetDoc);
    if (!docView) return nsnull;
    nsCOMPtr<nsIDOMAbstractView> view;
    docView->GetDefaultView(getter_AddRefs(view));
    
    if (scrollPage) {
      if (numLines > 0) {
        numLines = nsIDOMNSUIEvent::SCROLL_PAGE_DOWN;
      } else {
        numLines = nsIDOMNSUIEvent::SCROLL_PAGE_UP;
      }
    }

    mouseEvent->InitMouseEvent(NS_LITERAL_STRING("DOMMouseScroll"), PR_TRUE, PR_TRUE, 
                               view, numLines,
                               msEvent->refPoint.x, msEvent->refPoint.y,
                               msEvent->point.x, msEvent->point.y,
                               msEvent->isControl, msEvent->isAlt, msEvent->isShift, msEvent->isMeta,
                               0, nsnull);
    PRBool allowDefault;
    nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(targetContent));
    if (target) {
      target->DispatchEvent(event, &allowDefault);
      if (!allowDefault)
        return NS_OK;
    }
  }

  targetDOMDoc->CreateEvent(NS_LITERAL_STRING("MouseScrollEvents"), getter_AddRefs(event));
  if (event) {
    nsCOMPtr<nsIDOMMouseEvent> mouseEvent(do_QueryInterface(event));
  }

  nsIView* focusView = nsnull;
  nsIScrollableView* sv = nsnull;
  nsIFrame* focusFrame = nsnull;
  
  // Create a mouseout event that we fire to the content before
  // scrolling, to allow tooltips to disappear, etc.

  nsMouseEvent mouseOutEvent;
  mouseOutEvent.eventStructType = NS_MOUSE_EVENT;
  mouseOutEvent.message = NS_MOUSE_EXIT;
  mouseOutEvent.widget = msEvent->widget;
  mouseOutEvent.clickCount = 0;
  mouseOutEvent.point = nsPoint(0,0);
  mouseOutEvent.refPoint = nsPoint(0,0);
  mouseOutEvent.isShift = PR_FALSE;
  mouseOutEvent.isControl = PR_FALSE;
  mouseOutEvent.isAlt = PR_FALSE;
  mouseOutEvent.isMeta = PR_FALSE;

  // initialize nativeMsg field otherwise plugin code will crash when trying to access it
  mouseOutEvent.nativeMsg = nsnull;

  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));

  // Otherwise, check for a focused content element
  nsCOMPtr<nsIContent> focusContent;
  if (mCurrentFocus) {
    focusContent = mCurrentFocus;
  }
  else {
    // If there is no focused content, get the document content
    EnsureDocument(presShell);
    mDocument->GetRootContent(getter_AddRefs(focusContent));
  }
  
  if (!focusContent)
    return NS_ERROR_FAILURE;

  if (aUseTargetFrame)
    focusFrame = aTargetFrame;
  else
    presShell->GetPrimaryFrameFor(focusContent, &focusFrame);

  if (!focusFrame)
    return NS_ERROR_FAILURE;

  // Now check whether this frame wants to provide us with an
  // nsIScrollableView to use for scrolling.

  nsCOMPtr<nsIScrollableViewProvider> svp = do_QueryInterface(focusFrame);
  if (svp) {
    svp->GetScrollableView(&sv);
    if (sv)
      CallQueryInterface(sv, &focusView);
  } else {
    focusFrame->GetView(aPresContext, &focusView);
    if (!focusView) {
      nsIFrame* frameWithView;
      focusFrame->GetParentWithView(aPresContext, &frameWithView);
      if (frameWithView)
        frameWithView->GetView(aPresContext, &focusView);
      else
        return NS_ERROR_FAILURE;
    }
    
    sv = GetNearestScrollingView(focusView);
  }

  PRBool passToParent = PR_FALSE;

  if (sv) {
    GenerateMouseEnterExit(aPresContext, &mouseOutEvent);

    // Check the scroll position before and after calling ScrollBy[Page|Line]s.
    // This allows us to detect whether the view is not scrollable
    // (for whatever reason) and bubble up the scroll to the parent document.

    nscoord xPos, yPos;
    sv->GetScrollPosition(xPos, yPos);

    if (scrollPage)
      sv->ScrollByPages((numLines > 0) ? 1 : -1);
    else {
      PRInt32 vertLines = 0, horizLines = 0;
      
      if (msEvent->scrollFlags & nsMouseScrollEvent::kIsHorizontal)
        horizLines = numLines;

      if (msEvent->scrollFlags & nsMouseScrollEvent::kIsVertical)
        vertLines = numLines;
        
      sv->ScrollByLines(horizLines, vertLines);
    }

    nscoord newXPos, newYPos;
    sv->GetScrollPosition(newXPos, newYPos);

    if (newYPos != yPos) {
      if (focusView)
        ForceViewUpdate(focusView);
    } else
      passToParent = PR_TRUE;
  } else
    passToParent = PR_TRUE;

  if (passToParent) {
    nsresult rv;
    nsIFrame* newFrame = nsnull;
    nsCOMPtr<nsIPresContext> newPresContext;

    rv = GetParentScrollingView(msEvent, aPresContext, newFrame,
                                *getter_AddRefs(newPresContext));
    if (NS_SUCCEEDED(rv) && newFrame)
      return DoWheelScroll(newPresContext, newFrame, msEvent, numLines,
                           scrollPage, PR_TRUE);
    else
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
nsEventStateManager::GetParentScrollingView(nsMouseScrollEvent *aEvent,
                                            nsIPresContext* aPresContext,
                                            nsIFrame* &targetOuterFrame,
                                            nsIPresContext* &presCtxOuter)
{
  targetOuterFrame = nsnull;

  if (!aEvent) return NS_ERROR_FAILURE;
  if (!aPresContext) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocument> doc;

  {
    nsCOMPtr<nsIPresShell> presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));
    NS_ASSERTION(presShell, "No presshell in prescontext!");

    presShell->GetDocument(getter_AddRefs(doc));
  }

  NS_ASSERTION(doc, "No document in prescontext!");

  nsCOMPtr<nsIDocument> parentDoc;
  doc->GetParentDocument(getter_AddRefs(parentDoc));

  if (!parentDoc) {
    return NS_OK;
  }

  nsCOMPtr<nsIPresShell> pPresShell;
  parentDoc->GetShellAt(0, getter_AddRefs(pPresShell));
  NS_ENSURE_TRUE(pPresShell, NS_ERROR_FAILURE);

  /* now find the content node in our parent docshell's document that
     corresponds to our docshell */
  nsCOMPtr<nsIContent> frameContent;

  parentDoc->FindContentForSubDocument(doc, getter_AddRefs(frameContent));
  NS_ENSURE_TRUE(frameContent, NS_ERROR_FAILURE);

  /*
    get this content node's frame, and use it as the new event target,
    so the event can be processed in the parent docshell.
    Note that we don't actually need to translate the event coordinates
    because they are not used by DoWheelScroll().
  */

  nsIFrame* frameFrame = nsnull;
  pPresShell->GetPrimaryFrameFor(frameContent, &frameFrame);
  if (!frameFrame) return NS_ERROR_FAILURE;

  pPresShell->GetPresContext(&presCtxOuter); //addrefs
  targetOuterFrame = frameFrame;

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
  mCurrentTargetContent = nsnull;
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
      if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN && !mNormalLMouseEventInProcess) {
        //Our state is out of whack.  We got a mouseup while still processing
        //the mousedown.  Kill View-level mouse capture or it'll stay stuck
        nsCOMPtr<nsIViewManager> viewMan;
        if (aView) {
          aView->GetViewManager(*getter_AddRefs(viewMan));
          if (viewMan) {
            nsIView* grabbingView;
            viewMan->GetMouseEventGrabber(grabbingView);
            if (grabbingView == aView) {
              PRBool result;
              viewMan->GrabMouseEvents(nsnull, result);
            }
          }
        }
        break;
      }
        
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
          ChangeFocus(newFocus, eEventFocusedByMouse);
        else if (!suppressBlur) {
          SetContentState(nsnull, NS_EVENT_STATE_FOCUS);
        }

        // The rest is left button-specific.
        if (aEvent->message != NS_MOUSE_LEFT_BUTTON_DOWN)
          break;

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
            if (par)
              activeContent = par;
          }
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
      SetContentState(nsnull, NS_EVENT_STATE_ACTIVE);
      if (!mCurrentTarget) {
        nsIFrame* targ;
        GetEventTarget(&targ);
        if (!targ) return NS_ERROR_FAILURE;
      }
      ret = CheckForAndDispatchClick(aPresContext, (nsMouseEvent*)aEvent, aStatus);
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
      rv = getPrefBranch();
      if (NS_FAILED(rv)) return rv;

      nsMouseScrollEvent *msEvent = (nsMouseScrollEvent*) aEvent;
      PRInt32 action = 0;
      PRInt32 numLines = 0;
      PRBool aBool;
      if (msEvent->isShift) {
        mPrefBranch->GetIntPref("mousewheel.withshiftkey.action", &action);
        mPrefBranch->GetBoolPref("mousewheel.withshiftkey.sysnumlines",
                                 &aBool);
        if (aBool) {
          numLines = msEvent->delta;
          if (msEvent->scrollFlags & nsMouseScrollEvent::kIsFullPage)
            action = MOUSE_SCROLL_PAGE;
        }
        else
          mPrefBranch->GetIntPref("mousewheel.withshiftkey.numlines",
                                  &numLines);
      } else if (msEvent->isControl) {
        mPrefBranch->GetIntPref("mousewheel.withcontrolkey.action", &action);
        mPrefBranch->GetBoolPref("mousewheel.withcontrolkey.sysnumlines",
                                 &aBool);
        if (aBool) {
          numLines = msEvent->delta;
          if (msEvent->scrollFlags & nsMouseScrollEvent::kIsFullPage)
            action = MOUSE_SCROLL_PAGE;          
        }
        else
          mPrefBranch->GetIntPref("mousewheel.withcontrolkey.numlines",
                                  &numLines);
      } else if (msEvent->isAlt) {
        mPrefBranch->GetIntPref("mousewheel.withaltkey.action", &action);
        mPrefBranch->GetBoolPref("mousewheel.withaltkey.sysnumlines", &aBool);
        if (aBool) {
          numLines = msEvent->delta;
          if (msEvent->scrollFlags & nsMouseScrollEvent::kIsFullPage)
            action = MOUSE_SCROLL_PAGE;
        }
        else
          mPrefBranch->GetIntPref("mousewheel.withaltkey.numlines",
                                  &numLines);
      } else {
        mPrefBranch->GetIntPref("mousewheel.withnokey.action", &action);
        mPrefBranch->GetBoolPref("mousewheel.withnokey.sysnumlines", &aBool);
        if (aBool) {
          numLines = msEvent->delta;
          if (msEvent->scrollFlags & nsMouseScrollEvent::kIsFullPage)
            action = MOUSE_SCROLL_PAGE;
        }
        else
          mPrefBranch->GetIntPref("mousewheel.withnokey.numlines", &numLines);
      }

      if ((msEvent->delta < 0) && (numLines > 0))
        numLines = -numLines;

      switch (action) {
      case MOUSE_SCROLL_N_LINES:
      case MOUSE_SCROLL_PAGE:
        {
          DoWheelScroll(aPresContext, aTargetFrame, msEvent, numLines,
                        (action == MOUSE_SCROLL_PAGE), PR_FALSE);

        }

        break;

      case MOUSE_SCROLL_HISTORY:
        {
          nsCOMPtr<nsISupports> pcContainer;
          mPresContext->GetContainer(getter_AddRefs(pcContainer));
          if (pcContainer) {
            nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(pcContainer));
            if (webNav) {
              if (msEvent->delta > 0)
                 webNav->GoBack();
              else
                 webNav->GoForward();
            }
          }
        }
        break;

      case MOUSE_SCROLL_TEXTSIZE:
        {
          // Exclude form controls and XUL content.
          nsCOMPtr<nsIContent> content;
          aTargetFrame->GetContent(getter_AddRefs(content));
          if (content &&
              !content->IsContentOfType(nsIContent::eHTML_FORM_CONTROL) &&
              !content->IsContentOfType(nsIContent::eXUL))
            {
              ChangeTextSize((msEvent->delta > 0) ? 1 : -1);
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
    GenerateDragDropEnterExit(aPresContext, (nsGUIEvent*)aEvent);
    break;

  case NS_KEY_UP:
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
            if (!((nsInputEvent*)aEvent)->isControl) {
              //Shift focus forward or back depending on shift key
              ShiftFocus(!((nsInputEvent*)aEvent)->isShift);
            } else {
              ShiftFocusByDoc(!((nsInputEvent*)aEvent)->isShift);
            }
            *aStatus = nsEventStatus_eConsumeNoDefault;
            break;

          case NS_VK_F6:
            if (mConsumeFocusEvents) {
              mConsumeFocusEvents = PR_FALSE;
              break;
            }
            //Shift focus forward or back depending on shift key
            ShiftFocusByDoc(!((nsInputEvent*)aEvent)->isShift);
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
    break;

  case NS_MOUSE_ENTER:
    if (mCurrentTarget) {
      nsCOMPtr<nsIContent> targetContent;
      mCurrentTarget->GetContentForEvent(aPresContext, aEvent,
                                         getter_AddRefs(targetContent));
      SetContentState(targetContent, NS_EVENT_STATE_HOVER);
    }
    break;

  //
  // OS custom application event, such as from MS IntelliMouse
  //
  case NS_APPCOMMAND:
    // by default, tell the driver we're not handling the event
    ret = PR_FALSE;

    nsAppCommandEvent* appCommandEvent = (nsAppCommandEvent*) aEvent;

    nsCOMPtr<nsISupports> pcContainer;
    switch (appCommandEvent->appCommand) {
      case NS_APPCOMMAND_BACK:
      case NS_APPCOMMAND_FORWARD:
      case NS_APPCOMMAND_REFRESH:
      case NS_APPCOMMAND_STOP:
        // handle these commands using nsIWebNavigation
        mPresContext->GetContainer(getter_AddRefs(pcContainer));
        if (pcContainer) {
          nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(pcContainer));
          if (webNav) {
            // tell the driver we're handling the event
            ret = PR_TRUE;

            switch (appCommandEvent->appCommand) {
              case NS_APPCOMMAND_BACK:
                webNav->GoBack();
                break;

              case NS_APPCOMMAND_FORWARD:
                webNav->GoForward();
                break;

              case NS_APPCOMMAND_REFRESH:
                webNav->Reload(nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE);
                break;
      
              case NS_APPCOMMAND_STOP:
                webNav->Stop(nsIWebNavigation::STOP_ALL);
                break;

            }  // switch (appCommandEvent->appCommand)
          }  // if (webNav)
        }  // if (pcContainer)
        break;

      // XXX todo: handle these commands
      // case NS_APPCOMMAND_SEARCH:
      // case NS_APPCOMMAND_FAVORITES:
      // case NS_APPCOMMAND_HOME:
          
        // tell the driver we're handling the event
        // ret = PR_TRUE;
        // break;

    }  // switch (appCommandEvent->appCommand)

    break;
  }

  //Reset target frame to null to avoid mistargeting after reentrant event
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
  if (aFrame == mGestureDownFrame) {
    mGestureDownFrame = nsnull;
 #if CLICK_HOLD_CONTEXT_MENUS
    mEventDownWidget = nsnull;
    mEventPresContext = nsnull;
#endif
  }
  if (aFrame == mCurrentTarget) {
    if (aFrame) {
      aFrame->GetContent(getter_AddRefs(mCurrentTargetContent));
    }
    mCurrentTarget = nsnull;
  }
  if (mDOMEventLevel > 0) {
    mClearedFrameRefsDuringEvent = PR_TRUE;
  }
  

  return NS_OK;
}

nsIScrollableView*
nsEventStateManager::GetNearestScrollingView(nsIView* aView)
{
  nsIScrollableView* sv = nsnull;
  CallQueryInterface(aView, &sv);
  if (sv) {
    return sv;
  }

  nsIView* parent;
  aView->GetParent(parent);

  if (parent) {
    return GetNearestScrollingView(parent);
  }

  return nsnull;
}

PRBool
nsEventStateManager::CheckDisabled(nsIContent* aContent)
{
  nsCOMPtr<nsIAtom> tag;
  aContent->GetTag(*getter_AddRefs(tag));

  if (tag == nsHTMLAtoms::input    ||
      tag == nsHTMLAtoms::select   ||
      tag == nsHTMLAtoms::textarea ||
      tag == nsHTMLAtoms::button) {
    return aContent->HasAttr(kNameSpaceID_None, nsHTMLAtoms::disabled);
  }
  
  return PR_FALSE;
}

void
nsEventStateManager::UpdateCursor(nsIPresContext* aPresContext,
                                  nsEvent* aEvent, nsIFrame* aTargetFrame, 
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
        if (NS_FAILED(aTargetFrame->GetCursor(aPresContext, aEvent->point, cursor)))
          return;  // don't update the cursor if we failed to get it from the frame see bug 118877
      }
    }
  }

  // Check whether or not to show the busy cursor
  nsCOMPtr<nsISupports> pcContainer;
  aPresContext->GetContainer(getter_AddRefs(pcContainer));    
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(pcContainer));    
  if (!docShell) return;
  PRUint32 busyFlags = nsIDocShell::BUSY_FLAGS_NONE;
  docShell->GetBusyFlags(&busyFlags);

  // Show busy cursor everywhere before page loads
  // and just replace the arrow cursor after page starts loading
  if (busyFlags & nsIDocShell::BUSY_FLAGS_BUSY &&
        (cursor == NS_STYLE_CURSOR_AUTO || cursor == NS_STYLE_CURSOR_DEFAULT))
  {
    cursor = NS_STYLE_CURSOR_SPINNING;
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
nsEventStateManager::AfterDispatchEvent()
{
  mDOMEventLevel--;
  if (mClearedFrameRefsDuringEvent && mDOMEventLevel == 0) {
    mClearedFrameRefsDuringEvent = PR_FALSE;
  }
}

void
nsEventStateManager::DispatchMouseEvent(nsIPresContext* aPresContext,
                                        nsGUIEvent* aEvent, PRUint32 aMessage,
                                        nsIContent* aTargetContent,
                                        nsIFrame* aTargetFrame,
                                        nsIContent* aRelatedContent)
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event;
  event.eventStructType = NS_MOUSE_EVENT;
  event.message = aMessage;
  event.widget = aEvent->widget;
  event.clickCount = 0;
  event.point = aEvent->point;
  event.refPoint = aEvent->refPoint;
  event.isShift = ((nsMouseEvent*)aEvent)->isShift;
  event.isControl = ((nsMouseEvent*)aEvent)->isControl;
  event.isAlt = ((nsMouseEvent*)aEvent)->isAlt;
  event.isMeta = ((nsMouseEvent*)aEvent)->isMeta;
  event.nativeMsg = ((nsMouseEvent*)aEvent)->nativeMsg;

  mCurrentTargetContent = aTargetContent;
  mCurrentRelatedContent = aRelatedContent;

  mDOMEventLevel++;
  BeforeDispatchEvent();
  nsIFrame* targetFrame = aTargetFrame;
  if (aTargetContent) {
    aTargetContent->HandleDOMEvent(aPresContext, &event, nsnull,
                                   NS_EVENT_FLAG_INIT, &status); 
    // If frame refs were cleared, we need to re-resolve the frame
    if (mClearedFrameRefsDuringEvent) {
      nsCOMPtr<nsIPresShell> shell;
      aPresContext->GetShell(getter_AddRefs(shell));
      shell->GetPrimaryFrameFor(aTargetContent, &targetFrame);
    }
  }
  if (targetFrame) {
    targetFrame->HandleEvent(aPresContext, &event, &status);   
  }
  AfterDispatchEvent();

  mCurrentTargetContent = nsnull;
  mCurrentRelatedContent = nsnull;
}

void
nsEventStateManager::MaybeDispatchMouseEventToIframe(
    nsIPresContext* aPresContext, nsGUIEvent* aEvent, PRUint32 aMessage)
{
  // Check to see if we're an IFRAME and if so dispatch the given event
  // (mouseover / mouseout) to the IFRAME element above us.  This will result
  // in over-out-over combo to the IFRAME but as long as IFRAMEs are native
  // windows this will serve as a workaround to maintain IFRAME mouseover state.
  nsCOMPtr<nsIDocument> parentDoc;
  // If this is the first event in this window then mDocument might not be set
  // yet.  Call EnsureDocument to set it.
  EnsureDocument(aPresContext);
  mDocument->GetParentDocument(getter_AddRefs(parentDoc));
  if (parentDoc) {
    nsCOMPtr<nsIContent> docContent;
    parentDoc->FindContentForSubDocument(mDocument, getter_AddRefs(docContent));
    if (docContent) {
      nsCOMPtr<nsIAtom> tag;
      docContent->GetTag(*getter_AddRefs(tag));
      if (tag == nsHTMLAtoms::iframe) {
        // We're an IFRAME.  Send an event to our IFRAME tag.
        nsCOMPtr<nsIPresShell> parentShell;
        parentDoc->GetShellAt(0, getter_AddRefs(parentShell));
        if (parentShell) {
          nsEventStatus status = nsEventStatus_eIgnore;
          nsMouseEvent event;
          event.eventStructType = NS_MOUSE_EVENT;
          event.message = aMessage;
          event.widget = aEvent->widget;
          event.clickCount = 0;
          event.point = aEvent->point;
          event.refPoint = aEvent->refPoint;
          event.isShift = ((nsMouseEvent*)aEvent)->isShift;
          event.isControl = ((nsMouseEvent*)aEvent)->isControl;
          event.isAlt = ((nsMouseEvent*)aEvent)->isAlt;
          event.isMeta = ((nsMouseEvent*)aEvent)->isMeta;
          event.nativeMsg = ((nsMouseEvent*)aEvent)->nativeMsg;

          parentShell->HandleDOMEventWithTarget(docContent, &event, &status);
        }
      }
    }
  }
}


void
nsEventStateManager::GenerateMouseEnterExit(nsIPresContext* aPresContext, nsGUIEvent* aEvent)
{
  // Hold onto old target content through the event and reset after.
  nsCOMPtr<nsIContent> targetBeforeEvent = mCurrentTargetContent;

  switch(aEvent->message) {
  case NS_MOUSE_MOVE:
    {
      // Optimization if we're over the same frame as before
      if (mCurrentTarget == mLastMouseOverFrame) {
        break;
      }

      // Check whether we are over the same element as before
      nsCOMPtr<nsIContent> targetElement;
      nsIFrame* targetFrame;
      if (mCurrentTarget) {
        mCurrentTarget->GetContentForEvent(aPresContext, aEvent,
                                           getter_AddRefs(targetElement));
        targetFrame = mCurrentTarget;
      }

      // Bug 103055: mouseout and mouseover apply to *elements*, not all nodes
      // Get the nearest element parent and check it
      while (targetElement &&
             !targetElement->IsContentOfType(nsIContent::eELEMENT)) {
        // Yes, this is weak, but it will stick around for this short time :)
        nsIContent* temp = targetElement;
        temp->GetParent(*getter_AddRefs(targetElement));
        targetFrame = nsnull;
      }

      if (mLastMouseOverElement == targetElement) {
        break;
      }

      // Before firing mouseout, check for recursion
      // XXX is it wise to fire mouseover / mouseout when the target is null?
      if (mLastMouseOverElement != mFirstMouseOutEventElement ||
          !mFirstMouseOutEventElement) {
      
        // Store the first mouseOut event we fire and don't refire mouseOut
        // to that element while the first mouseOut is still ongoing.
        mFirstMouseOutEventElement = mLastMouseOverElement;

        if (mLastMouseOverFrame) {
          DispatchMouseEvent(aPresContext, aEvent, NS_MOUSE_EXIT_SYNTH,
                             mLastMouseOverElement, mLastMouseOverFrame,
                             targetElement);

          // Turn off recursion protection
          mFirstMouseOutEventElement = nsnull;
        }
        else {
          // If there is no previous frame, we are entering this widget
          MaybeDispatchMouseEventToIframe(aPresContext, aEvent,
                                          NS_MOUSE_ENTER_SYNTH);
        }
      }

      // Before firing mouseover, check for recursion
      if (targetElement != mFirstMouseOverEventElement) {
      
        // If we actually went and got a parent element, we need to fetch its
        // frame so we can send it events
        if (!targetFrame) {
          nsCOMPtr<nsIPresShell> shell;
          aPresContext->GetShell(getter_AddRefs(shell));
          shell->GetPrimaryFrameFor(targetElement, &targetFrame);
        }

        // Store the first mouseOver event we fire and don't refire mouseOver
        // to that element while the first mouseOver is still ongoing.
        mFirstMouseOverEventElement = targetElement;

        if (targetElement) {
          SetContentState(targetElement, NS_EVENT_STATE_HOVER);
        }

        // Fire mouseover
        DispatchMouseEvent(aPresContext, aEvent, NS_MOUSE_ENTER_SYNTH,
                           targetElement, targetFrame,
                           mLastMouseOverElement);

        mLastMouseOverFrame = targetFrame;
        mLastMouseOverElement = targetElement;

        // Turn recursion protection back off
        mFirstMouseOverEventElement = nsnull;
      }
    }
    break;
  case NS_MOUSE_EXIT:
    {
      // This is actually the window mouse exit event.
      if (mLastMouseOverFrame) {
        // Before firing mouseout, check for recursion
        if (mLastMouseOverElement != mFirstMouseOutEventElement) {
    
          // Store the first mouseOut event we fire and don't refire mouseOut
          // to that element while the first mouseOut is still ongoing.
          mFirstMouseOutEventElement = mLastMouseOverElement;

          // Unset :hover
          if (mLastMouseOverElement) {
            SetContentState(nsnull, NS_EVENT_STATE_HOVER);
          }

          // Fire mouseout
          DispatchMouseEvent(aPresContext, aEvent, NS_MOUSE_EXIT_SYNTH,
                             mLastMouseOverElement, mLastMouseOverFrame,
                             nsnull);

          // XXX Get the new frame
          mLastMouseOverFrame = nsnull;
          mLastMouseOverElement = nsnull;

          // Turn recursion protection back off
          mFirstMouseOutEventElement = nsnull;
        }
      }

      // If we are over an iframe and got this event, fire mouseout at the
      // iframe's content
      MaybeDispatchMouseEventToIframe(aPresContext, aEvent,
                                      NS_MOUSE_EXIT_SYNTH);
    }
    break;
  }

  // reset mCurretTargetContent to what it was
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
          mCurrentRelatedContent = targetContent;

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
        mCurrentRelatedContent = lastContent;

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
     }
    }
    break;
  }

  //reset mCurretTargetContent to what it was
  mCurrentTargetContent = targetBeforeEvent;

  // Now flush all pending notifications.
  FlushPendingEvents(aPresContext);
}

nsresult
nsEventStateManager::SetClickCount(nsIPresContext* aPresContext, 
                                   nsMouseEvent *aEvent,
                                   nsEventStatus* aStatus)
{
  nsresult ret = NS_OK;
  nsCOMPtr<nsIContent> mouseContent;

  mCurrentTarget->GetContentForEvent(aPresContext, aEvent, getter_AddRefs(mouseContent));

  switch (aEvent->message) {
  case NS_MOUSE_LEFT_BUTTON_DOWN:
    mLastLeftMouseDownContent = mouseContent;
    break;

  case NS_MOUSE_LEFT_BUTTON_UP:
    if (mLastLeftMouseDownContent == mouseContent) {
      aEvent->clickCount = mLClickCount;
      mLClickCount = 0;
    }
    else {
      aEvent->clickCount = 0;
    }
    mLastLeftMouseDownContent = nsnull;
    break;

  case NS_MOUSE_MIDDLE_BUTTON_DOWN:
    mLastMiddleMouseDownContent = mouseContent;
    break;

  case NS_MOUSE_MIDDLE_BUTTON_UP:
    if (mLastMiddleMouseDownContent == mouseContent) {
      aEvent->clickCount = mMClickCount;
      mMClickCount = 0;
    }
    else {
      aEvent->clickCount = 0;
    }
    break;

  case NS_MOUSE_RIGHT_BUTTON_DOWN:
    mLastRightMouseDownContent = mouseContent;
    break;

  case NS_MOUSE_RIGHT_BUTTON_UP:
    if (mLastRightMouseDownContent == mouseContent) {
      aEvent->clickCount = mRClickCount;
      mRClickCount = 0;
    }
    else {
      aEvent->clickCount = 0;
    }
    break;
  }

  return ret;
}

nsresult
nsEventStateManager::CheckForAndDispatchClick(nsIPresContext* aPresContext, 
                                              nsMouseEvent *aEvent,
                                              nsEventStatus* aStatus)
{
  nsresult ret = NS_OK;
  nsMouseEvent event;
  nsCOMPtr<nsIContent> mouseContent;
  PRInt32 flags = NS_EVENT_FLAG_INIT;

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
      flags |= mLeftClickOnly ? NS_EVENT_FLAG_NO_CONTENT_DISPATCH : NS_EVENT_FLAG_NONE;
      break;
    case NS_MOUSE_RIGHT_BUTTON_UP:
      event.message = NS_MOUSE_RIGHT_CLICK;
      flags |= mLeftClickOnly ? NS_EVENT_FLAG_NO_CONTENT_DISPATCH : NS_EVENT_FLAG_NONE;
      break;
    }

    event.eventStructType = NS_MOUSE_EVENT;
    event.widget = aEvent->widget;
    event.nativeMsg = nsnull;
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
      ret = presShell->HandleEventWithTarget(&event, mCurrentTarget, mouseContent, flags, aStatus);
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
        event2.nativeMsg = nsnull;
        event2.point = aEvent->point;
        event2.refPoint = aEvent->refPoint;
        event2.clickCount = aEvent->clickCount;
        event2.isShift = aEvent->isShift;
        event2.isControl = aEvent->isControl;
        event2.isAlt = aEvent->isAlt;
        event2.isMeta = aEvent->isMeta;

        ret = presShell->HandleEventWithTarget(&event2, mCurrentTarget, mouseContent, flags, aStatus);
      }
    }
  }
  return ret;
}

PRBool
nsEventStateManager::ChangeFocus(nsIContent* aFocusContent, PRInt32 aFocusedWith)
{
  aFocusContent->SetFocus(mPresContext);
  if (aFocusedWith != eEventFocusedByMouse) {
    MoveCaretToFocus();
    // Select text fields when focused via keyboard (tab or accesskey)
    if (sTextfieldSelectModel == eTextfieldSelect_auto && 
        mCurrentFocus && 
        mCurrentFocus->IsContentOfType(nsIContent::eHTML_FORM_CONTROL)) {
      nsCOMPtr<nsIFormControl> formControl(do_QueryInterface(mCurrentFocus));
      PRInt32 controlType = formControl->GetType();
      if (controlType == NS_FORM_INPUT_TEXT ||
          controlType == NS_FORM_INPUT_PASSWORD) {
        nsCOMPtr<nsIDOMHTMLInputElement> inputElement = 
          do_QueryInterface(mCurrentFocus);
        if (inputElement) {
          inputElement->Select();
        }
      }
    }
  }

  mLastFocusedWith = aFocusedWith;
  return PR_FALSE;
}

//---------------------------------------------------------
// Debug Helpers
#ifdef DEBUG_DOCSHELL_FOCUS
static void 
PrintDocTree(nsIDocShellTreeNode * aParentNode, int aLevel)
{
  for (PRInt32 i=0;i<aLevel;i++) printf("  ");

  PRInt32 childWebshellCount;
  aParentNode->GetChildCount(&childWebshellCount);
  nsCOMPtr<nsIDocShell> parentAsDocShell(do_QueryInterface(aParentNode));
  nsCOMPtr<nsIDocShellTreeItem> parentAsItem(do_QueryInterface(aParentNode));
  PRInt32 type;
  parentAsItem->GetItemType(&type);
  nsCOMPtr<nsIPresShell> presShell;
  parentAsDocShell->GetPresShell(getter_AddRefs(presShell));
  nsCOMPtr<nsIPresContext> presContext;
  parentAsDocShell->GetPresContext(getter_AddRefs(presContext));
  nsCOMPtr<nsIDocument> doc;
  presShell->GetDocument(getter_AddRefs(doc));

  nsCOMPtr<nsIScriptGlobalObject> sgo;
  doc->GetScriptGlobalObject(getter_AddRefs(sgo));
  nsCOMPtr<nsIDOMWindowInternal> domwin(do_QueryInterface(sgo));

  nsCOMPtr<nsIWidget> widget;
  nsCOMPtr<nsIViewManager> vm;
  presShell->GetViewManager(getter_AddRefs(vm));
  if (vm) {
    vm->GetWidget(getter_AddRefs(widget));
  }
  nsCOMPtr<nsIEventStateManager> esm;
  presContext->GetEventStateManager(getter_AddRefs(esm));

  printf("DS %p  Type %s  Cnt %d  Doc %p  DW %p  EM %p\n", 
    parentAsDocShell.get(), 
    type==nsIDocShellTreeItem::typeChrome?"Chrome":"Content", 
    childWebshellCount, doc.get(), domwin.get(), esm.get());

  if (childWebshellCount > 0) {
    for (PRInt32 i=0;i<childWebshellCount;i++) {
      nsCOMPtr<nsIDocShellTreeItem> child;
      aParentNode->GetChildAt(i, getter_AddRefs(child));
      nsCOMPtr<nsIDocShellTreeNode> childAsNode(do_QueryInterface(child));
      PrintDocTree(childAsNode, aLevel+1);
    }
  }
}
#endif // end debug helpers

NS_IMETHODIMP
nsEventStateManager::ShiftFocus(PRBool aForward, nsIContent* aStart)
{
  // We use mTabbedThroughDocument to indicate that we have passed
  // the end (or beginning) of the document we started tabbing from,
  // without finding anything else to focus.  If we pass the end of
  // the same document again (and the flag is set), we know that there
  // is no focusable content anywhere in the tree, and should stop.

  mTabbedThroughDocument = PR_FALSE;
  return ShiftFocusInternal(aForward, aStart);
}

nsresult
nsEventStateManager::ShiftFocusInternal(PRBool aForward, nsIContent* aStart)
{
#ifdef DEBUG_DOCSHELL_FOCUS
  printf("[%p] ShiftFocus: aForward=%d, aStart=%p, mCurrentFocus=%p\n",
         this, aForward, aStart, mCurrentFocus);
#endif
  NS_ASSERTION(mPresContext, "no pres context");
  EnsureDocument(mPresContext);
  NS_ASSERTION(mDocument, "no document");

  nsCOMPtr<nsIContent> rootContent;
  mDocument->GetRootContent(getter_AddRefs(rootContent));

  nsCOMPtr<nsISupports> pcContainer;
  mPresContext->GetContainer(getter_AddRefs(pcContainer));
  NS_ASSERTION(pcContainer, "no container for presContext");

  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(pcContainer));
  PRBool docHasFocus = PR_FALSE;

  // ignoreTabIndex allows the user to tab to the next link after clicking before it link in the page
  // or using find text to get to the link. Without ignoreTabIndex in those cases, pages that 
  // use tabindex would still enforce that order in those situations.
  PRBool ignoreTabIndex = PR_FALSE;

  if (aStart) {
    mCurrentFocus = aStart;

    TabIndexFrom(mCurrentFocus, &mCurrentTabIndex);
  } else if (!mCurrentFocus) {  
    // mCurrentFocus is ambiguous for determining whether
    // we're in document-focus mode, because it's nulled out
    // when the document is blurred, and it's also nulled out
    // when the document/canvas has focus.
    //
    // So, use the docshell focus state to disambiguate.

    docShell->GetHasFocus(&docHasFocus);
  }

  nsIFrame* selectionFrame = nsnull;
  nsIFrame* curFocusFrame = nsnull;   // This will hold the location we're moving away from

  // If in content, navigate from last cursor position rather than last focus
  // If we're in UI, selection location will return null
  nsCOMPtr<nsIPresShell> presShell;
  mPresContext->GetShell(getter_AddRefs(presShell));

  // We might use the selection position, rather than mCurrentFocus, as our position to shift focus from
  PRInt32 itemType;
  nsCOMPtr<nsIDocShellTreeItem> shellItem(do_QueryInterface(docShell));
  shellItem->GetItemType(&itemType);
  
  if (itemType != nsIDocShellTreeItem::typeChrome && mLastFocusedWith != eEventFocusedByMouse) {  
    // We're going to tab from the selection position 
    nsCOMPtr<nsIDOMHTMLAreaElement> areaElement(do_QueryInterface(mCurrentFocus));
    if (!areaElement) {
      nsCOMPtr<nsIContent> selectionContent, endSelectionContent;  // We won't be using this, need arg for method call
      PRUint32 selectionOffset; // We won't be using this either, need arg for method call
      GetDocSelectionLocation(getter_AddRefs(selectionContent), getter_AddRefs(endSelectionContent), &selectionFrame, &selectionOffset);
      if (selectionContent == rootContent)  // If selection on rootContent, same as null -- we have no selection yet
        selectionFrame = nsnull;
      // Only use tabindex if selection is synchronized with focus
      // That way, if the user clicks in content, or does a find text that lands between focusable elements,
      // they can then tab relative to that selection
      if (selectionFrame) {
        PRBool selectionWithFocus;
        MoveFocusToCaret(PR_FALSE, &selectionWithFocus);
        ignoreTabIndex = !selectionWithFocus;
      }
    }
  }

  if (!mCurrentFocus)   // Get tabindex ready
    if (aForward) {
      if (docHasFocus && selectionFrame)
        mCurrentTabIndex = 0;
      else {
        mCurrentFocus = rootContent;
        mCurrentTabIndex = 1;
      }
    } 
    else if (!docHasFocus) 
      mCurrentTabIndex = 0;
    else if (selectionFrame)
      mCurrentTabIndex = 1;   // will keep it from wrapping around to end

  if (selectionFrame) 
    curFocusFrame = selectionFrame;
  else if (mCurrentFocus && !docHasFocus)   // If selection is not found in doc, use mCurrentFocus 
    presShell->GetPrimaryFrameFor(mCurrentFocus, &curFocusFrame);

  nsCOMPtr<nsIContent> nextFocus;
  if (aForward || !docHasFocus || selectionFrame)
    GetNextTabbableContent(rootContent, curFocusFrame, aForward, ignoreTabIndex,
                         getter_AddRefs(nextFocus));

  // Clear out mCurrentTabIndex. It has a garbage value because of GetNextTabbableContent()'s side effects
  // It will be set correctly when focus is changed via ChangeFocus()
  mCurrentTabIndex = 0; 

  if (nextFocus) {
    // Check to see if the next focused element has a subshell.
    // This is the case for an IFRAME or FRAME element.  If it
    // does, we send focus into the subshell.

    nsCOMPtr<nsIDocShell> sub_shell;

    nsCOMPtr<nsIDocument> doc, sub_doc;
    nextFocus->GetDocument(*getter_AddRefs(doc));

    if (doc) {
      doc->GetSubDocumentFor(nextFocus, getter_AddRefs(sub_doc));

      if (sub_doc) {
        nsCOMPtr<nsISupports> container;
        sub_doc->GetContainer(getter_AddRefs(container));

        sub_shell = do_QueryInterface(container);
      }
    }

    if (sub_shell) {
      SetContentState(nsnull, NS_EVENT_STATE_FOCUS);

      nsIFrame* nextFocusFrame = nsnull;
      presShell->GetPrimaryFrameFor(nextFocus, &nextFocusFrame);
      presShell->ScrollFrameIntoView(nextFocusFrame,
                                     NS_PRESSHELL_SCROLL_ANYWHERE,
                                     NS_PRESSHELL_SCROLL_ANYWHERE);
      TabIntoDocument(sub_shell, aForward);
    } else {
      // there is no subshell, so just focus nextFocus
#ifdef DEBUG_DOCSHELL_FOCUS
      printf("focusing next focusable content: %p\n", nextFocus.get());
#endif
      presShell->GetPrimaryFrameFor(nextFocus, &mCurrentTarget);

      //This may be new frame that hasn't been through the ESM so we
      //must set its NS_FRAME_EXTERNAL_REFERENCE bit.
      if (mCurrentTarget) {
        nsFrameState state;
        mCurrentTarget->GetFrameState(&state);
        state |= NS_FRAME_EXTERNAL_REFERENCE;
        mCurrentTarget->SetFrameState(state);
      }

      ChangeFocus(nextFocus, eEventFocusedByKey);
      
      mCurrentFocus = nextFocus;

      if (docHasFocus)
        docShell->SetCanvasHasFocus(PR_FALSE);
      else
        docShell->SetHasFocus(PR_TRUE);
    }
  } else {
    
    // If we're going backwards past the first content,
    // focus the document.
    
    PRBool focusDocument;
    if (itemType == nsIDocShellTreeItem::typeChrome)
      focusDocument = PR_FALSE;
    else {
      // Check for a frameset document
      focusDocument = !(IsFrameSetDoc(docShell));
    }

    if (!aForward && !docHasFocus && focusDocument) {
#ifdef DEBUG_DOCSHELL_FOCUS
      printf("Focusing document\n");
#endif
      SetContentState(nsnull, NS_EVENT_STATE_FOCUS);
      docShell->SetHasFocus(PR_TRUE);
      docShell->SetCanvasHasFocus(PR_TRUE);
      // Next time forward we start at the beginning of the document
      // Next time backward we go to URL bar
      // We need to move the caret to the document root, so that we don't 
      // tab from the most recently focused element next time around
      mCurrentFocus = rootContent;
      MoveCaretToFocus();
      mCurrentFocus = nsnull;

    } else {
      // If there's nothing left to focus in this document,
      // pop out to our parent document, and have it shift focus
      // in the same direction starting at the content element
      // corresponding to our docshell.
      // Guard against infinite recursion (see explanation in ShiftFocus)

      if (mTabbedThroughDocument)
        return NS_OK;

      mCurrentFocus = rootContent;
      mCurrentTabIndex = 0;
      MoveCaretToFocus();
      mCurrentFocus = nsnull;

      mTabbedThroughDocument = PR_TRUE;

      nsCOMPtr<nsIDocShellTreeItem> treeItem = do_QueryInterface(pcContainer);
      nsCOMPtr<nsIDocShellTreeItem> treeParent;
      treeItem->GetParent(getter_AddRefs(treeParent));
      if (treeParent) {
        nsCOMPtr<nsIDocShell> parentDS = do_QueryInterface(treeParent);
        if (parentDS) {
          nsCOMPtr<nsIPresShell> parentShell;
          parentDS->GetPresShell(getter_AddRefs(parentShell));

          nsCOMPtr<nsIContent> docContent;
          nsCOMPtr<nsIDocument> parent_doc;

          parentShell->GetDocument(getter_AddRefs(parent_doc));

          parent_doc->FindContentForSubDocument(mDocument,
                                                getter_AddRefs(docContent));

          nsCOMPtr<nsIPresContext> parentPC;
          parentShell->GetPresContext(getter_AddRefs(parentPC));

          nsCOMPtr<nsIEventStateManager> parentESM;
          parentPC->GetEventStateManager(getter_AddRefs(parentESM));

          SetContentState(nsnull, NS_EVENT_STATE_FOCUS);

#ifdef DEBUG_DOCSHELL_FOCUS
          printf("popping out focus to parent docshell\n");
#endif
          parentESM->MoveCaretToFocus();
          parentESM->ShiftFocus(aForward, docContent);
        }
      } else {      
        PRBool tookFocus = PR_FALSE;
        nsCOMPtr<nsIDocShell> subShell = do_QueryInterface(pcContainer);
        if (subShell) {
          subShell->TabToTreeOwner(aForward, &tookFocus);
        }
        
#ifdef DEBUG_DOCSHEL_FOCUS
        printf("offered focus to tree owner, tookFocus=%d\n",
               tookFocus);
#endif

        if (tookFocus) {
          SetContentState(nsnull, NS_EVENT_STATE_FOCUS);
          docShell->SetHasFocus(PR_FALSE);
        } else {
          // there is nowhere else to send the focus, so
          // refocus ourself.
          // Next time forward we start at the beginning of the document

#ifdef DEBUG_DOCSHELL_FOCUS
          printf("wrapping around within this document\n");
#endif

          mCurrentFocus = nsnull;
          docShell->SetHasFocus(PR_FALSE);
          ShiftFocusInternal(aForward);
        }
      }
    }
  }

  return NS_OK;
}

void nsEventStateManager::TabIndexFrom(nsIContent *aFrom, PRInt32 *aOutIndex)
{
  if (aFrom->IsContentOfType(nsIContent::eHTML)) {
    nsCOMPtr<nsIAtom> tag;
    aFrom->GetTag(*getter_AddRefs(tag));
    if (nsHTMLAtoms::a != tag && nsHTMLAtoms::area != tag && nsHTMLAtoms::button != tag && 
      nsHTMLAtoms::input != tag && nsHTMLAtoms::object != tag && nsHTMLAtoms::select != tag && nsHTMLAtoms::textarea != tag)
      return;
  }

  nsAutoString tabIndexStr;
  aFrom->GetAttr(kNameSpaceID_None, nsHTMLAtoms::tabindex, tabIndexStr);
  if (!tabIndexStr.IsEmpty()) {
    PRInt32 ec, tabIndexVal = tabIndexStr.ToInteger(&ec);
    if (NS_SUCCEEDED(ec))
      *aOutIndex = tabIndexVal;
  }
}


nsresult
nsEventStateManager::GetNextTabbableContent(nsIContent* aRootContent, nsIFrame* aFrame, PRBool forward, PRBool aIgnoreTabIndex, 
                                            nsIContent** aResult)
{
  *aResult = nsnull;
  PRBool keepFirstFrame = PR_FALSE;
  PRBool findLastFrame = PR_FALSE;

  nsCOMPtr<nsIBidirectionalEnumerator> frameTraversal;

  if (!aFrame) {
    //No frame means we need to start with the root content again.
    nsCOMPtr<nsIPresShell> presShell;
    if (mPresContext) {
      nsIFrame* result = nsnull;
      if (NS_SUCCEEDED(mPresContext->GetShell(getter_AddRefs(presShell))) && presShell) {
        presShell->GetPrimaryFrameFor(aRootContent, &result);
      }

      aFrame = result;

      if (!forward)
        findLastFrame = PR_TRUE;
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
    if(nsHTMLAtoms::area==tag) {
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

  nsresult result;
  nsCOMPtr<nsIFrameTraversal> trav(do_CreateInstance(kFrameTraversalCID,&result));
  if (NS_FAILED(result))
    return result;

  result = trav->NewFrameTraversal(getter_AddRefs(frameTraversal), FOCUS,
                                   mPresContext, aFrame);
  if (NS_FAILED(result))
    return NS_OK;

  if (!keepFirstFrame) {
    if (forward)
      frameTraversal->Next();
    else frameTraversal->Prev();
  } else if (findLastFrame)
    frameTraversal->Last();

  nsISupports* currentItem;
  frameTraversal->CurrentItem(&currentItem);
  nsIFrame* currentFrame = (nsIFrame*)currentItem;

  while (currentFrame) {
    nsCOMPtr<nsIContent> child;
    currentFrame->GetContent(getter_AddRefs(child));

    const nsStyleVisibility* vis;
    currentFrame->GetStyleData(eStyleStruct_Visibility, ((const nsStyleStruct *&)vis));

    const nsStyleUserInterface* ui;
    currentFrame->GetStyleData(eStyleStruct_UserInterface, ((const nsStyleStruct*&)ui));

    PRBool viewShown = PR_TRUE;

    nsIView* frameView = nsnull;
    currentFrame->GetView(mPresContext, &frameView);
    if (!frameView) {
      nsIFrame* parentWithView = nsnull;
      currentFrame->GetParentWithView(mPresContext, &parentWithView);
      if (parentWithView)
        parentWithView->GetView(mPresContext, &frameView);
    }
    while (frameView) {
      nsViewVisibility visib;
      frameView->GetVisibility(visib);
      if (visib == nsViewVisibility_kHide) {
        viewShown = PR_FALSE;
        break;
      }
      frameView->GetParent(frameView);
    }

    nsCOMPtr<nsIDOMElement> element(do_QueryInterface(child));

    // if collapsed or hidden, we don't get tabbed into.
    if (viewShown &&
        (vis->mVisible != NS_STYLE_VISIBILITY_COLLAPSE) &&
        (vis->mVisible != NS_STYLE_VISIBILITY_HIDDEN) && 
        (ui->mUserFocus != NS_STYLE_USER_FOCUS_IGNORE) &&
        (ui->mUserFocus != NS_STYLE_USER_FOCUS_NONE) && element) {
      nsCOMPtr<nsIAtom> tag;
      PRInt32 tabIndex = -1;
      PRBool disabled = PR_TRUE;
      PRBool hidden = PR_FALSE;

      PRInt32 tabFocusModel = eTabFocus_any;
      if (mPrefBranch) {
        // This could be done via a pref observer, but because there are 
        // no static pref callbacks we'd have to create a singleton object
        // just to observe this pref. Since mPrefBranch is already cached, and
        // GetIntPref() is fairly fast, that would probably be overkill.
        // This only happens once per tab press.
        mPrefBranch->GetIntPref("accessibility.tabfocus", &tabFocusModel);
      }

      child->GetTag(*getter_AddRefs(tag));
      if (child->IsContentOfType(nsIContent::eHTML)) {
        if (nsHTMLAtoms::input==tag) {
          nsCOMPtr<nsIDOMHTMLInputElement> nextInput(do_QueryInterface(child));
          if (nextInput) {
            nextInput->GetDisabled(&disabled);
            nextInput->GetTabIndex(&tabIndex);

            nsAutoString type;
            nextInput->GetType(type);
            if (type.EqualsIgnoreCase("text") 
                || type.EqualsIgnoreCase("autocomplete")
                || type.EqualsIgnoreCase("password")) {
              // It's a text field or password field
              disabled = PR_FALSE;
            }
            else if (type.EqualsIgnoreCase("hidden")) {
              hidden = PR_TRUE;
            }
            else if (type.EqualsIgnoreCase("file")) {
              disabled = PR_TRUE;
            }
            else {
              // it's some other type of form element
              disabled =
                disabled || !(tabFocusModel & eTabFocus_formElementsMask);
            }
          }
        }
        else if (nsHTMLAtoms::select==tag) {
          // Select counts as form but not as text
          disabled = !(tabFocusModel & eTabFocus_formElementsMask);
          if (!disabled) {
            nsCOMPtr<nsIDOMHTMLSelectElement> nextSelect(do_QueryInterface(child));
            if (nextSelect) {
              nextSelect->GetDisabled(&disabled);
              nextSelect->GetTabIndex(&tabIndex);
            }
          }
        }
        else if (nsHTMLAtoms::textarea==tag) {
          // it's a textarea
          disabled = PR_FALSE;
          if (!disabled) {
            nsCOMPtr<nsIDOMHTMLTextAreaElement> nextTextArea(do_QueryInterface(child));
            if (nextTextArea) {
              nextTextArea->GetDisabled(&disabled);
              nextTextArea->GetTabIndex(&tabIndex);
            }
          }
        }
        else if (nsHTMLAtoms::a==tag) {
          // it's a link
          disabled = !(tabFocusModel & eTabFocus_linksMask);
          nsCOMPtr<nsIDOMHTMLAnchorElement> nextAnchor(do_QueryInterface(child));
          if (!disabled) {
            if (nextAnchor)
              nextAnchor->GetTabIndex(&tabIndex);
            nsAutoString href;
            nextAnchor->GetAttribute(NS_LITERAL_STRING("href"), href);
            if (href.IsEmpty()) {
              disabled = PR_TRUE; // Don't tab unless href, bug 17605
            } else {
              disabled = PR_FALSE;
            }
          }
        }
        else if (nsHTMLAtoms::button==tag) {
          // Button counts as a form element but not as text
          disabled = !(tabFocusModel & eTabFocus_formElementsMask);
          if (!disabled) {
            nsCOMPtr<nsIDOMHTMLButtonElement> nextButton(do_QueryInterface(child));
            if (nextButton) {
              nextButton->GetTabIndex(&tabIndex);
              nextButton->GetDisabled(&disabled);
            }
          }
        }
        else if (nsHTMLAtoms::img==tag) {
          // Don't need to set disabled here, because if we
          // match an imagemap, we'll return from there.
          if (tabFocusModel & eTabFocus_linksMask) {
            nsCOMPtr<nsIDOMHTMLImageElement> nextImage(do_QueryInterface(child));
            nsAutoString usemap;
            if (nextImage) {
              nsCOMPtr<nsIDocument> doc;
              if (NS_SUCCEEDED(child->GetDocument(*getter_AddRefs(doc))) && doc) {
                nextImage->GetAttribute(NS_LITERAL_STRING("usemap"), usemap);
                nsCOMPtr<nsIDOMHTMLMapElement> imageMap;
                if (NS_SUCCEEDED(nsImageMapUtils::FindImageMap(doc,usemap,getter_AddRefs(imageMap))) && imageMap) {
                  nsCOMPtr<nsIContent> map(do_QueryInterface(imageMap));
                  if (map) {
                    nsCOMPtr<nsIContent> childArea;
                    PRInt32 count, index;
                    map->ChildCount(count);
                    //First see if mCurrentFocus is in this map
                    for (index = 0; index < count; index++) {
                      map->ChildAt(index, *getter_AddRefs(childArea));
                      if (childArea == mCurrentFocus) {
                        PRInt32 val = 0;
                        TabIndexFrom(childArea, &val);
                        if (mCurrentTabIndex == val) {
                          // mCurrentFocus is in this map so we must start
                          // iterating past it.
                          // We skip the case where mCurrentFocus has the
                          // same tab index as mCurrentTabIndex since the
                          // next tab ordered element might be before it
                          // (or after for backwards) in the child list.
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
                      PRInt32 val = 0;
                      TabIndexFrom(childArea, &val);
                      if (mCurrentTabIndex == val) {
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
        else if (nsHTMLAtoms::object==tag) {
          // OBJECT is treated as a form element.
          disabled = !(tabFocusModel & eTabFocus_formElementsMask);
          if (!disabled) {
            nsCOMPtr<nsIDOMHTMLObjectElement> nextObject(do_QueryInterface(child));
            if (nextObject) 
              nextObject->GetTabIndex(&tabIndex);
            disabled = PR_FALSE;
          }
        }
        else if (nsHTMLAtoms::iframe==tag || nsHTMLAtoms::frame==tag) {
          disabled = PR_TRUE;
          if (child) {
            nsCOMPtr<nsIDocument> doc;
            child->GetDocument(*getter_AddRefs(doc));
            if (doc) {
              nsCOMPtr<nsIDocument> subDoc;
              doc->GetSubDocumentFor(child, getter_AddRefs(subDoc));
              nsCOMPtr<nsISupports> container;
              subDoc->GetContainer(getter_AddRefs(container));
              nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(container));
              if (docShell) {
                nsCOMPtr<nsIContentViewer> contentViewer;
                docShell->GetContentViewer(getter_AddRefs(contentViewer));
                if (contentViewer) {
                  nsCOMPtr<nsIContentViewer> zombieViewer;
                  contentViewer->GetPreviousViewer(getter_AddRefs(zombieViewer));
                  if (!zombieViewer) {
                    // If there are 2 viewers for the current docshell, that 
                    // means the current document is a zombie document.
                    // Only navigate into the frame/iframe if it's not a zombie
                    disabled = PR_FALSE;
                  }
                }
              }
            }
          }
        } 
      }
      else {
        // Is it disabled?
        nsAutoString value;
        child->GetAttr(kNameSpaceID_None, nsHTMLAtoms::disabled, value);
        // Check the tabindex attribute
        nsAutoString tabStr;
        child->GetAttr(kNameSpaceID_None, nsHTMLAtoms::tabindex, tabStr);
        if (!tabStr.IsEmpty()) {
          PRInt32 errorCode;
          tabIndex = tabStr.ToInteger(&errorCode);
        }
        if (!value.Equals(NS_LITERAL_STRING("true")))
          disabled = PR_FALSE;
      }
      
      //TabIndex not set (-1) treated at same level as set to 0
      tabIndex = tabIndex < 0 ? 0 : tabIndex;

      if (!disabled && !hidden && (aIgnoreTabIndex ||
                                   mCurrentTabIndex == tabIndex) &&
          child != mCurrentFocus) {
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
  return GetNextTabbableContent(aRootContent, nsnull, forward, aIgnoreTabIndex, aResult);
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
      child->GetAttr(kNameSpaceID_None, nsHTMLAtoms::tabindex, tabIndexStr);
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
      child->GetAttr(kNameSpaceID_None, nsHTMLAtoms::tabindex, tabIndexStr);
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

        //This may be new frame that hasn't been through the ESM so we
        //must set its NS_FRAME_EXTERNAL_REFERENCE bit.
        if (mCurrentTarget) {
          nsFrameState state;
          mCurrentTarget->GetFrameState(&state);
          state |= NS_FRAME_EXTERNAL_REFERENCE;
          mCurrentTarget->SetFrameState(state);
        }
      }
    }
  }

  if (!mCurrentTarget) {
    nsCOMPtr<nsIPresShell> presShell;
    mPresContext->GetShell(getter_AddRefs(presShell));
    if (presShell) {
      presShell->GetEventTargetFrame(&mCurrentTarget);

      //This may be new frame that hasn't been through the ESM so we
      //must set its NS_FRAME_EXTERNAL_REFERENCE bit.
      if (mCurrentTarget) {
        nsFrameState state;
        mCurrentTarget->GetFrameState(&state);
        state |= NS_FRAME_EXTERNAL_REFERENCE;
        mCurrentTarget->SetFrameState(state);
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

  if (!mCurrentTarget) {
    nsCOMPtr<nsIPresShell> presShell;
    mPresContext->GetShell(getter_AddRefs(presShell));
    if (presShell) {
      presShell->GetEventTargetFrame(&mCurrentTarget);

      //This may be new frame that hasn't been through the ESM so we
      //must set its NS_FRAME_EXTERNAL_REFERENCE bit.
      if (mCurrentTarget) {
        nsFrameState state;
        mCurrentTarget->GetFrameState(&state);
        state |= NS_FRAME_EXTERNAL_REFERENCE;
        mCurrentTarget->SetFrameState(state);
      }
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
  // Hierchical hover:  Check the ancestor chain of mHoverContent to see
  // if we are on it.
  nsCOMPtr<nsIContent> hoverContent = mHoverContent;
  while (hoverContent) {
    if (aContent == hoverContent) {
      aState |= NS_EVENT_STATE_HOVER;
      break;
    }
    nsIContent *parent;
    hoverContent->GetParent(parent);
    hoverContent = dont_AddRef(parent);
  }

  if (aContent == mCurrentFocus) {
    aState |= NS_EVENT_STATE_FOCUS;
  }
  if (aContent == mDragOverContent) {
    aState |= NS_EVENT_STATE_DRAGOVER;
  }
  if (aContent == mURLTargetContent) {
    aState |= NS_EVENT_STATE_URLTARGET;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::SetContentState(nsIContent *aContent, PRInt32 aState)
{
  const PRInt32 maxNotify = 6;
  // We must initialize this array with memset for the sake of the boneheaded
  // OS X compiler.  See bug 134934.
  nsIContent  *notifyContent[maxNotify];
  memset(notifyContent, 0, sizeof(notifyContent));

  // check to see that this state is allowed by style. Check dragover too?
  // XXX This doesn't consider that |aState| is a bitfield.
  if (mCurrentTarget && (aState == NS_EVENT_STATE_ACTIVE || aState == NS_EVENT_STATE_HOVER))
  {
    const nsStyleUserInterface* ui;
    mCurrentTarget->GetStyleData(eStyleStruct_UserInterface, ((const nsStyleStruct*&)ui));
    if (ui->mUserInput == NS_STYLE_USER_INPUT_NONE)
      return NS_OK;
  }
  
  if ((aState & NS_EVENT_STATE_DRAGOVER) && (aContent != mDragOverContent)) {
    notifyContent[4] = mDragOverContent; // notify dragover first, since more common case
    NS_IF_ADDREF(notifyContent[4]);
    mDragOverContent = aContent;
  }

  if ((aState & NS_EVENT_STATE_URLTARGET) && (aContent != mURLTargetContent)) {
    notifyContent[5] = mURLTargetContent;
    NS_IF_ADDREF(notifyContent[5]);
    mURLTargetContent = aContent;
  }

  if ((aState & NS_EVENT_STATE_ACTIVE) && (aContent != mActiveContent)) {
    //transferring ref to notifyContent from mActiveContent
    notifyContent[2] = mActiveContent;
    NS_IF_ADDREF(notifyContent[2]);
    mActiveContent = aContent;
  }

  nsCOMPtr<nsIContent> commonHoverAncestor, oldHover, newHover;
  if ((aState & NS_EVENT_STATE_HOVER) && (aContent != mHoverContent)) {
    oldHover = mHoverContent;
    newHover = aContent;
    // Find closest common ancestor (commonHoverAncestor)
    if (mHoverContent && aContent) {
      // Find the nearest common ancestor by counting the distance to the
      // root and then walking up again, in pairs.
      PRInt32 offset = 0;
      nsCOMPtr<nsIContent> oldAncestor = mHoverContent;
      for (;;) {
        ++offset;
        nsIContent *parent;
        oldAncestor->GetParent(parent);
        if (!parent)
          break;
        oldAncestor = dont_AddRef(parent);
      }
      nsCOMPtr<nsIContent> newAncestor = aContent;
      for (;;) {
        --offset;
        nsIContent *parent;
        newAncestor->GetParent(parent);
        if (!parent)
          break;
        newAncestor = dont_AddRef(parent);
      }
#ifdef DEBUG
      if (oldAncestor != newAncestor) {
        // This could be a performance problem.
        nsCOMPtr<nsIDocument> oldDoc;
        oldAncestor->GetDocument(*getter_AddRefs(oldDoc));
        // The |!oldDoc| case (that the old hover node has been removed
        // from the document) could be a slight performance problem.
        // It's rare enough that it shouldn't be an issue, but common
        // enough that we don't want to assert..
        NS_ASSERTION(!oldDoc,
                     "moved hover between nodes in different documents");
        // XXX Why don't we ever hit this code because we're not using
        // |GetBindingParent|?
      }
#endif
      if (oldAncestor == newAncestor) {
        oldAncestor = mHoverContent;
        newAncestor = aContent;
        while (offset > 0) {
          nsIContent *parent;
          oldAncestor->GetParent(parent);
          oldAncestor = dont_AddRef(parent);
          --offset;
        }
        while (offset < 0) {
          nsIContent *parent;
          newAncestor->GetParent(parent);
          newAncestor = dont_AddRef(parent);
          ++offset;
        }
        while (oldAncestor != newAncestor) {
          nsIContent *parent;
          oldAncestor->GetParent(parent);
          oldAncestor = dont_AddRef(parent);
          newAncestor->GetParent(parent);
          newAncestor = dont_AddRef(parent);
        }
        commonHoverAncestor = oldAncestor;
      }
    }

    mHoverContent = aContent;
  }

  if ((aState & NS_EVENT_STATE_FOCUS)) {
    if (aContent && (aContent == mCurrentFocus) && gLastFocusedDocument == mDocument) {
      // gLastFocusedDocument appears to always be correct, that is why
      // I'm not setting it here. This is to catch an edge case.
      NS_IF_RELEASE(gLastFocusedContent);
      gLastFocusedContent = mCurrentFocus;
      NS_IF_ADDREF(gLastFocusedContent);
      //If this notification was for focus alone then get rid of aContent
      //ref to avoid unnecessary notification.
      if (!(aState & ~NS_EVENT_STATE_FOCUS)) {
        aContent = nsnull;
      }
    } else {
      notifyContent[3] = gLastFocusedContent;
      NS_IF_ADDREF(gLastFocusedContent);
      SendFocusBlur(mPresContext, aContent, PR_TRUE);
    }
  }

  if (aContent && aContent != newHover) { // notify about new content too
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
    nsCOMPtr<nsIDocument> doc1, doc2;  // this presumes content can't get/lose state if not connected to doc
    if (notifyContent[0]) {
      notifyContent[0]->GetDocument(*getter_AddRefs(doc1));
      if (notifyContent[1]) {
        //For :focus this might be a different doc so check
        notifyContent[1]->GetDocument(*getter_AddRefs(doc2));
        if (doc1 == doc2) {
          doc2 = nsnull;
        }
      }
    }
    else if (newHover) {
      newHover->GetDocument(*getter_AddRefs(doc1));
    }
    else {
      oldHover->GetDocument(*getter_AddRefs(doc1));
    }
    if (doc1) {
      doc1->BeginUpdate();

      // Notify all content from newHover to the commonHoverAncestor
      while (newHover && newHover != commonHoverAncestor) {
        doc1->ContentStatesChanged(newHover, nsnull, NS_EVENT_STATE_HOVER);
        nsIContent *parent;
        newHover->GetParent(parent);
        newHover = dont_AddRef(parent);
      }
      // Notify all content from oldHover to the commonHoverAncestor
      while (oldHover && oldHover != commonHoverAncestor) {
        doc1->ContentStatesChanged(oldHover, nsnull, NS_EVENT_STATE_HOVER);
        nsIContent *parent;
        oldHover->GetParent(parent);
        oldHover = dont_AddRef(parent);
      }

      if (notifyContent[0]) {
        doc1->ContentStatesChanged(notifyContent[0], notifyContent[1],
                                   aState & ~NS_EVENT_STATE_HOVER);
        if (notifyContent[2]) {
          // more that two notifications are needed (should be rare)
          // XXX a further optimization here would be to group the
          // notification pairs together by parent/child, only needed if
          // more than two content changed (ie: if [0] and [2] are
          // parent/child, then notify (0,2) (1,3))
          doc1->ContentStatesChanged(notifyContent[2], notifyContent[3],
                                     aState & ~NS_EVENT_STATE_HOVER);
          if (notifyContent[4]) {
            // more that four notifications are needed (should be rare)
            doc1->ContentStatesChanged(notifyContent[4], nsnull,
                                       aState & ~NS_EVENT_STATE_HOVER);
          }
        }
      }
      doc1->EndUpdate();

      if (doc2) {
        doc2->BeginUpdate();
        doc2->ContentStatesChanged(notifyContent[1], notifyContent[2],
                                   aState & ~NS_EVENT_STATE_HOVER);
        if (notifyContent[3]) {
          doc1->ContentStatesChanged(notifyContent[3], notifyContent[4],
                                     aState & ~NS_EVENT_STATE_HOVER);
        }
        doc2->EndUpdate();
      }
    }

    from = &(notifyContent[0]);
    while (from < to) {  // release old refs now that we are through
      nsIContent* notify = *from++;
      NS_RELEASE(notify);
    }
  }

  return NS_OK;
}

nsresult
nsEventStateManager::SendFocusBlur(nsIPresContext* aPresContext, nsIContent *aContent, PRBool aEnsureWindowHasFocus)
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
        clearFirstBlurEvent = PR_TRUE;
      }

      // Retrieve this content node's pres context. it can be out of sync in
      // the Ender widget case.
      nsCOMPtr<nsIDocument> doc;
      gLastFocusedContent->GetDocument(*getter_AddRefs(doc));
      if (doc) {
        // The order of the nsIViewManager and nsIPresShell COM pointers is
        // important below.  We want the pres shell to get released before the
        // associated view manager on exit from this function.
        // See bug 53763.
        nsCOMPtr<nsIViewManager> kungFuDeathGrip;
        nsCOMPtr<nsIPresShell> shell;
        doc->GetShellAt(0, getter_AddRefs(shell));
        if (shell) {
          shell->GetViewManager(getter_AddRefs(kungFuDeathGrip));

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
            nsCOMPtr<nsIFocusController> newFocusController;
            nsCOMPtr<nsIFocusController> oldFocusController;
            nsCOMPtr<nsPIDOMWindow> oldPIDOMWindow;
            nsCOMPtr<nsPIDOMWindow> newPIDOMWindow;
            nsCOMPtr<nsIScriptGlobalObject> oldGlobal;
            nsCOMPtr<nsIScriptGlobalObject> newGlobal;
            gLastFocusedDocument->GetScriptGlobalObject(getter_AddRefs(oldGlobal));
            mDocument->GetScriptGlobalObject(getter_AddRefs(newGlobal));
            nsCOMPtr<nsPIDOMWindow> newWindow = do_QueryInterface(newGlobal);
            nsCOMPtr<nsPIDOMWindow> oldWindow = do_QueryInterface(oldGlobal);
            if(newWindow)
              newWindow->GetRootFocusController(getter_AddRefs(newFocusController));
            if(oldWindow)
              oldWindow->GetRootFocusController(getter_AddRefs(oldFocusController));
            if(oldFocusController && oldFocusController != newFocusController)
              oldFocusController->SetSuppressFocus(PR_TRUE, "SendFocusBlur Window Switch");
          }
          
          nsCOMPtr<nsIEventStateManager> esm;
          oldPresContext->GetEventStateManager(getter_AddRefs(esm));
          esm->SetFocusedContent(gLastFocusedContent);
          nsCOMPtr<nsIContent> temp = gLastFocusedContent;
          NS_RELEASE(gLastFocusedContent);
          gLastFocusedContent = nsnull;

          nsCxPusher pusher(temp);
          temp->HandleDOMEvent(oldPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status); 
          pusher.Pop();

          esm->SetFocusedContent(nsnull);
        }
      }

      if (clearFirstBlurEvent) {
        mFirstBlurEvent = nsnull;
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
      if (mDocument) {
        nsCOMPtr<nsIFocusController> newFocusController;
        nsCOMPtr<nsIFocusController> oldFocusController;
        nsCOMPtr<nsPIDOMWindow> oldPIDOMWindow;
        nsCOMPtr<nsPIDOMWindow> newPIDOMWindow;
        nsCOMPtr<nsIScriptGlobalObject> oldGlobal;
        nsCOMPtr<nsIScriptGlobalObject> newGlobal;
        gLastFocusedDocument->GetScriptGlobalObject(getter_AddRefs(oldGlobal));
        mDocument->GetScriptGlobalObject(getter_AddRefs(newGlobal));
        nsCOMPtr<nsPIDOMWindow> newWindow = do_QueryInterface(newGlobal);
        nsCOMPtr<nsPIDOMWindow> oldWindow = do_QueryInterface(oldGlobal);

        if (newWindow)
          newWindow->GetRootFocusController(getter_AddRefs(newFocusController));
        oldWindow->GetRootFocusController(getter_AddRefs(oldFocusController));
        if(oldFocusController && oldFocusController != newFocusController)
          oldFocusController->SetSuppressFocus(PR_TRUE, "SendFocusBlur Window Switch #2");
      }

      nsCOMPtr<nsIEventStateManager> esm;
      gLastFocusedPresContext->GetEventStateManager(getter_AddRefs(esm));
      esm->SetFocusedContent(nsnull);
      nsCOMPtr<nsIDocument> temp = gLastFocusedDocument;
      NS_RELEASE(gLastFocusedDocument);
      gLastFocusedDocument = nsnull;

      nsCxPusher pusher(temp);
      temp->HandleDOMEvent(gLastFocusedPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
      pusher.Pop();

      pusher.Push(globalObject);
      globalObject->HandleDOMEvent(gLastFocusedPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status); 
    }
  }
  
  NS_IF_RELEASE(gLastFocusedContent);
  gLastFocusedContent = aContent;
  NS_IF_ADDREF(gLastFocusedContent);
  mCurrentFocus = aContent;
 
  // Moved widget focusing code here, from end of SendFocusBlur
  // This fixes the order of accessibility focus events, so that 
  // the window focus event goes first, and then the focus event for the control
  if (aEnsureWindowHasFocus) {
    // This raises the window that has both content and scroll bars in it
    // instead of the child window just below it that contains only the content
    // That way we focus the same window that gets focused by a mouse click
    nsCOMPtr<nsIViewManager> vm;
    presShell->GetViewManager(getter_AddRefs(vm));
    if (vm) {
      nsCOMPtr<nsIWidget> widget;
      vm->GetWidget(getter_AddRefs(widget));
      if (widget)
        widget->SetFocus(PR_TRUE);
    }
  }

  if (nsnull != aContent && aContent != mFirstFocusEvent) {

    //Store the first focus event we fire and don't refire focus
    //to that element while the first focus is still ongoing.
    PRBool clearFirstFocusEvent = PR_FALSE;
    if (!mFirstFocusEvent) {
      mFirstFocusEvent = aContent;
      clearFirstFocusEvent = PR_TRUE;
    }

    //fire focus
    nsEventStatus status = nsEventStatus_eIgnore;
    nsEvent event;
    event.eventStructType = NS_EVENT;
    event.message = NS_FOCUS_CONTENT;

    if (nsnull != mPresContext) {
      nsCxPusher pusher(aContent);
      aContent->HandleDOMEvent(mPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
    }
    
    nsAutoString tabIndex;
    aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::tabindex, tabIndex);
    PRInt32 ec, val = tabIndex.ToInteger(&ec);
    if (NS_OK == ec) {
      mCurrentTabIndex = val;
    }

    if (clearFirstFocusEvent) {
      mFirstFocusEvent = nsnull;
	  }
  } else if (!aContent) {
    //fire focus on document even if the content isn't focusable (ie. text)
    //see bugzilla bug 93521
    nsEventStatus status = nsEventStatus_eIgnore;
    nsEvent event;
    event.eventStructType = NS_EVENT;
    event.message = NS_FOCUS_CONTENT;
    if (nsnull != mPresContext && mDocument) {
      nsCxPusher pusher(mDocument);
      mDocument->HandleDOMEvent(mPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
    }
  }

  if (mBrowseWithCaret) 
    SetContentCaretVisible(presShell, aContent, PR_TRUE); 

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
  mCurrentFocus = aContent;
  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::ContentRemoved(nsIContent* aContent)
{
  if (aContent == mCurrentFocus) {
    // Note that we don't use SetContentState() here because
    // we don't want to fire a blur.  Blurs should only be fired
    // in response to clicks or tabbing.

    mCurrentFocus = nsnull;
  }

  if (aContent == mHoverContent) {
    // Since hover is hierarchical, set the current hover to the
    // content's parent node.
    aContent->GetParent(*getter_AddRefs(mHoverContent));
  }

  if (aContent == mActiveContent) {
    mActiveContent = nsnull;
  }

  if (aContent == mDragOverContent) {
    mDragOverContent = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::EventStatusOK(nsGUIEvent* aEvent, PRBool *aOK)
{
  *aOK = PR_TRUE;
  if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN) {
    if (!mNormalLMouseEventInProcess) {
      *aOK = PR_FALSE;
    }
  }
  return NS_OK;
}

//-------------------------------------------
// Access Key Registration
//-------------------------------------------
NS_IMETHODIMP
nsEventStateManager::RegisterAccessKey(nsIContent* aContent, PRUint32 aKey)
{
  if (!mAccessKeys) {
    mAccessKeys = new nsSupportsHashtable();
    if (!mAccessKeys) {
      return NS_ERROR_FAILURE;
    }
  }

  if (aContent) {
    PRUnichar accKey = nsCRT::ToLower((char)aKey);

    nsVoidKey key(NS_INT32_TO_PTR(accKey));

#ifdef DEBUG_jag
    nsCOMPtr<nsIContent> oldContent = dont_AddRef(NS_STATIC_CAST(nsIContent*,  mAccessKeys->Get(&key)));
    NS_ASSERTION(!oldContent, "Overwriting accesskey registration");
#endif
    mAccessKeys->Put(&key, aContent);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::UnregisterAccessKey(nsIContent* aContent, PRUint32 aKey)
{
  if (!mAccessKeys) {
    return NS_ERROR_FAILURE;
  }

  if (aContent) {
    PRUnichar accKey = nsCRT::ToLower((char)aKey);

    nsVoidKey key(NS_INT32_TO_PTR(accKey));

    nsCOMPtr<nsIContent> oldContent = dont_AddRef(NS_STATIC_CAST(nsIContent*, mAccessKeys->Get(&key)));
#ifdef DEBUG_jag
    NS_ASSERTION(oldContent == aContent, "Trying to unregister wrong content");
#endif
    if (oldContent != aContent)
      return NS_OK;

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
nsEventStateManager::DispatchNewEvent(nsISupports* aTarget, nsIDOMEvent* aEvent, PRBool *aPreventDefault)
{
  nsresult ret = NS_OK;

  nsCOMPtr<nsIPrivateDOMEvent> privEvt(do_QueryInterface(aEvent));
  if (privEvt) {
    nsCOMPtr<nsIDOMEventTarget> eventTarget(do_QueryInterface(aTarget));
    privEvt->SetTarget(eventTarget);

    //Key and mouse events have additional security to prevent event spoofing
    nsEvent * innerEvent;
    privEvt->GetInternalNSEvent(&innerEvent);
    if (innerEvent && (innerEvent->eventStructType == NS_KEY_EVENT ||
        innerEvent->eventStructType == NS_MOUSE_EVENT)) {
      //Check security state to determine if dispatcher is trusted
      nsCOMPtr<nsIScriptSecurityManager>
      securityManager(do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID));
      NS_ENSURE_TRUE(securityManager, NS_ERROR_FAILURE);

      PRBool enabled;
      nsresult res = securityManager->IsCapabilityEnabled("UniversalBrowserWrite", &enabled);
      if (NS_SUCCEEDED(res) && enabled) {
        privEvt->SetTrusted(PR_TRUE);
      }
      else {
        privEvt->SetTrusted(PR_FALSE);
      } 
    }
    else {
      privEvt->SetTrusted(PR_TRUE);
    }

    if (innerEvent) {
      nsEventStatus status = nsEventStatus_eIgnore;
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
          else {
            nsCOMPtr<nsIChromeEventHandler> target(do_QueryInterface(aTarget));
            if (target) {
              ret = target->HandleChromeEvent(mPresContext, innerEvent, &aEvent, NS_EVENT_FLAG_INIT, &status);
            }
          }
        }
      }

      *aPreventDefault = status == nsEventStatus_eConsumeNoDefault ? PR_FALSE : PR_TRUE;
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
    aPresShell->GetDocument(getter_AddRefs(mDocument));
}

void nsEventStateManager::FlushPendingEvents(nsIPresContext* aPresContext) {
  NS_PRECONDITION(nsnull != aPresContext, "nsnull ptr");
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  if (nsnull != shell) {
    shell->FlushPendingNotifications(PR_FALSE);
    nsCOMPtr<nsIViewManager> viewManager;
    shell->GetViewManager(getter_AddRefs(viewManager));
    if (viewManager) {
      viewManager->FlushPendingInvalidates();
    }
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
  rv = CallQueryInterface(manager, aInstancePtrResult);
  if (NS_FAILED(rv)) return rv;

  return manager->Init();
}


nsresult nsEventStateManager::GetDocSelectionLocation(nsIContent **aStartContent, nsIContent **aEndContent,
    nsIFrame **aStartFrame, PRUint32* aStartOffset)
{
  // In order to return the nsIContent and nsIFrame of the caret's position,
  // we need to get a pres shell, and then get the selection from it

  *aStartOffset = 0;
  *aStartFrame = nsnull; 
  *aStartContent = *aEndContent = nsnull;
  nsresult rv = NS_ERROR_FAILURE;
  
  if (!mDocument)
    return rv;
  nsCOMPtr<nsIPresShell> shell;
  if (mPresContext)
    rv = mPresContext->GetShell(getter_AddRefs(shell));

  nsCOMPtr<nsIFrameSelection> frameSelection;
  if (shell) 
    rv = shell->GetFrameSelection(getter_AddRefs(frameSelection));

  nsCOMPtr<nsISelection> domSelection;
  if (frameSelection)
    rv = frameSelection->GetSelection(nsISelectionController::SELECTION_NORMAL,
      getter_AddRefs(domSelection));

  nsCOMPtr<nsIDOMNode> startNode, endNode;
  PRBool isCollapsed = PR_FALSE;
  nsCOMPtr<nsIContent> startContent, endContent;
  if (domSelection) {
    domSelection->GetIsCollapsed(&isCollapsed);
    nsCOMPtr<nsIDOMRange> domRange;
    rv = domSelection->GetRangeAt(0, getter_AddRefs(domRange));
    if (domRange) {
      domRange->GetStartContainer(getter_AddRefs(startNode));
      domRange->GetEndContainer(getter_AddRefs(endNode));
      typedef PRInt32* PRInt32_ptr;
      domRange->GetStartOffset(PRInt32_ptr(aStartOffset));

      nsCOMPtr<nsIContent> childContent;
      PRBool canContainChildren;

      startContent = do_QueryInterface(startNode);
      if (NS_SUCCEEDED(startContent->CanContainChildren(canContainChildren)) &&
          canContainChildren) {
        startContent->ChildAt(*aStartOffset, *getter_AddRefs(childContent));
        if (childContent)
          startContent = childContent;
      }

      endContent = do_QueryInterface(endNode);
      if (NS_SUCCEEDED(endContent->CanContainChildren(canContainChildren)) &&
          canContainChildren) {
        PRInt32 endOffset = 0;
        domRange->GetEndOffset(&endOffset);
        endContent->ChildAt(endOffset, *getter_AddRefs(childContent));
        if (childContent)
          endContent = childContent;
      }
    }
  }

  nsIFrame *startFrame = nsnull;
  if (startContent) {
    rv = shell->GetPrimaryFrameFor(startContent, &startFrame);
    if (isCollapsed && NS_SUCCEEDED(rv)) {
      // First check to see if our caret is at the very end of a node
      // If so, the caret is actually sitting in front of the next
      // logical frame's primary node - so for this case we need to
      // change caretContent to that node.

      nsCOMPtr<nsIDOMNode> domNode(do_QueryInterface(startContent));
      PRUint16 nodeType;
      domNode->GetNodeType(&nodeType);

      if (nodeType == nsIDOMNode::TEXT_NODE) {      
        nsCOMPtr<nsIContent> origStartContent(startContent), rootContent;
        mDocument->GetRootContent(getter_AddRefs(rootContent));
        nsAutoString nodeValue;
        domNode->GetNodeValue(nodeValue);

        PRBool isFormControl =
          startContent->IsContentOfType(nsIContent::eHTML_FORM_CONTROL);

        if (nodeValue.Length() == *aStartOffset && !isFormControl &&
            startContent != rootContent) {
          // Yes, indeed we were at the end of the last node
          nsCOMPtr<nsIBidirectionalEnumerator> frameTraversal;

          nsCOMPtr<nsIFrameTraversal> trav(do_CreateInstance(kFrameTraversalCID,
                                                             &rv));
          NS_ENSURE_SUCCESS(rv, rv);

          rv = trav->NewFrameTraversal(getter_AddRefs(frameTraversal), LEAF,
                                       mPresContext, startFrame);
          NS_ENSURE_SUCCESS(rv, rv);

          do {   
            // Get the next logical frame, and set the start of
            // focusable elements. Search for focusable elements from there.
            // Continue getting next frame until the primary node for the frame
            // we are on changes - we don't want to be stuck in the same place
            frameTraversal->Next();
            nsISupports* currentItem;
            frameTraversal->CurrentItem(&currentItem);
            startFrame = NS_STATIC_CAST(nsIFrame*, currentItem);
            if (startFrame) {
              PRBool endEqualsStart(startContent == endContent);
              startFrame->GetContent(getter_AddRefs(startContent));
              if (endEqualsStart)            
                endContent = startContent;
            }
            else break;
          }
          while (startContent == origStartContent);
        }
      }
    }
  }

  *aStartFrame = startFrame;
  *aStartContent = startContent;
  *aEndContent = endContent;
  NS_IF_ADDREF(*aStartContent);
  NS_IF_ADDREF(*aEndContent);

  return rv;
}

void nsEventStateManager::FocusElementButNotDocument(nsIContent *aContent)
{
  // Focus an element in the current document, but don't switch document/window focus!

  if (gLastFocusedDocument == mDocument || !gLastFocusedContent) {
    // If we're already focused in this document, 
    // or if there was no last focus
    // use normal focus method
    if (mCurrentFocus != aContent) {
      if (aContent) 
        aContent->SetFocus(mPresContext);
      else
        SetContentState(nsnull, NS_EVENT_STATE_FOCUS);
    }
    return;
  }

  // The last focus wasn't in this document, so we may be getting our position from the selection
  // while the window focus is currently somewhere else such as the find dialog

  // Temporarily save the current focus globals so we can leave them undisturbed after this method
  nsCOMPtr<nsIContent> lastFocusedContent(gLastFocusedContent);
  nsCOMPtr<nsIDocument> lastFocusedDocument(gLastFocusedDocument);
  nsCOMPtr<nsIContent> lastFocusInThisDoc(mCurrentFocus);
  NS_IF_RELEASE(gLastFocusedDocument);
  NS_IF_RELEASE(gLastFocusedContent);
  gLastFocusedContent = mCurrentFocus;
  gLastFocusedDocument = mDocument;
  NS_IF_ADDREF(gLastFocusedDocument);
  NS_IF_ADDREF(gLastFocusedContent);

  // Focus the content, but don't change the document
  SendFocusBlur(mPresContext, aContent, PR_FALSE);
  mDocument->BeginUpdate();
  if (!lastFocusInThisDoc)
    lastFocusInThisDoc = mCurrentFocus;
  if (mCurrentFocus)
    mDocument->ContentStatesChanged(lastFocusInThisDoc, mCurrentFocus,
                                    NS_EVENT_STATE_FOCUS);
  mDocument->EndUpdate();
  FlushPendingEvents(mPresContext);

  // Restore the global focus state
  NS_IF_RELEASE(gLastFocusedDocument);
  NS_IF_RELEASE(gLastFocusedContent);
  gLastFocusedContent = lastFocusedContent;
  gLastFocusedDocument = lastFocusedDocument;
  NS_IF_ADDREF(gLastFocusedDocument);
  NS_IF_ADDREF(gLastFocusedContent);

  // Make sure our document's window's focus controller knows the focus has changed 
  // That way when our window is activated (PreHandleEvent get NS_ACTIVATE), the correct element & doc get focused 
  nsCOMPtr<nsIFocusController> focusController;
  nsCOMPtr<nsIDOMElement> focusedElement(do_QueryInterface(mCurrentFocus));

  nsCOMPtr<nsIScriptGlobalObject> globalObj;
  mDocument->GetScriptGlobalObject(getter_AddRefs(globalObj));
  nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(globalObj));
  NS_ASSERTION(win, "win is null.  this happens [often on xlib builds].  see bug #79213");
  if (win) {
    win->GetRootFocusController(getter_AddRefs(focusController));
    if (focusController && focusedElement) 
      focusController->SetFocusedElement(focusedElement);
  }

  if (mCurrentFocus)
    TabIndexFrom(mCurrentFocus, &mCurrentTabIndex);
}

NS_IMETHODIMP nsEventStateManager::MoveFocusToCaret(PRBool aCanFocusDoc, PRBool *aIsSelectionWithFocus)
{
  // mBrowseWithCaret equals the pref accessibility.browsewithcaret
  // When it's true, the user can arrow around the browser as if it's a
  // read only text editor.

  // If the user cursors over a focusable element or gets there with find text, then send focus to it

  *aIsSelectionWithFocus= PR_FALSE;
  nsCOMPtr<nsIContent> selectionContent, endSelectionContent;
  nsIFrame *selectionFrame;
  PRUint32 selectionOffset;
  GetDocSelectionLocation(getter_AddRefs(selectionContent), getter_AddRefs(endSelectionContent),
    &selectionFrame, &selectionOffset);

  if (!selectionContent) 
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> testContent(selectionContent);
  nsCOMPtr<nsIContent> nextTestContent(endSelectionContent);

  // We now have the correct start node in selectionContent!
  // Search for focusable elements, starting with selectionContent

  // Method #1: Keep going up while we look - an ancestor might be focusable
  // We could end the loop earlier, such as when we're no longer
  // in the same frame, by comparing getPrimaryFrameFor(selectionContent)
  // with a variable holding the starting selectionContent
  nsCOMPtr<nsIAtom> tag;
  while (testContent) {
    // Keep testing while selectionContent is equal to something,
    // eventually we'll run out of ancestors

    if (testContent == mCurrentFocus) {
      *aIsSelectionWithFocus = PR_TRUE;
      return NS_OK;  // already focused on this node, this whole thing's moot
    }

    testContent->GetTag(*getter_AddRefs(tag));

    // Add better focusable test here later if necessary ... 
    if (nsHTMLAtoms::a == tag.get()) {
      *aIsSelectionWithFocus = PR_TRUE;
    }
    else {
      *aIsSelectionWithFocus = testContent->HasAttr(kNameSpaceID_XLink, nsHTMLAtoms::href);
      if (*aIsSelectionWithFocus) {
        nsAutoString xlinkType;
        testContent->GetAttr(kNameSpaceID_XLink, nsHTMLAtoms::type, xlinkType);
        if (!xlinkType.Equals(NS_LITERAL_STRING("simple"))) {
          *aIsSelectionWithFocus = PR_FALSE;  // Xlink must be type="simple"
        }
      }
    }

    if (*aIsSelectionWithFocus) {
      FocusElementButNotDocument(testContent);
      return NS_OK;
    }

    // Get the parent
    nsIContent* parent;
    testContent->GetParent(parent);
    testContent = dont_AddRef(parent);

    if (!testContent) {
      // We run this loop again, checking the ancestor chain of the selection's end point
      testContent = nextTestContent;
      nextTestContent = nsnull;
    }
  }

  // We couldn't find an anchor that was an ancestor of the selection start
  // Method #2: look for anchor in selection's primary range (depth first search)
  
  // Turn into nodes so that we can use GetNextSibling() and GetFirstChild()
  nsCOMPtr<nsIDOMNode> selectionNode(do_QueryInterface(selectionContent));
  nsCOMPtr<nsIDOMNode> endSelectionNode(do_QueryInterface(endSelectionContent));
  nsCOMPtr<nsIDOMNode> testNode;

  do {
    testContent = do_QueryInterface(selectionNode);

    // We're looking for any focusable item that could be part of the
    // main document's selection.
    // Right now we only look for elements with the <a> tag.
    // Add better focusable test here later if necessary ... 
    if (testContent) {
      testContent->GetTag(*getter_AddRefs(tag));
      if (nsHTMLAtoms::a == tag.get()) {
        *aIsSelectionWithFocus = PR_TRUE;
        FocusElementButNotDocument(testContent);
        return NS_OK;
      }
    }

    selectionNode->GetFirstChild(getter_AddRefs(testNode));
    if (testNode) {
      selectionNode = testNode;
      continue;
    }

    if (selectionNode == endSelectionNode)
      break;
    selectionNode->GetNextSibling(getter_AddRefs(testNode));
    if (testNode) {
      selectionNode = testNode;
      continue;
    }

    do {
      selectionNode->GetParentNode(getter_AddRefs(testNode));
      if (!testNode || testNode == endSelectionNode) {
        selectionNode = nsnull;
        break;
      }
      testNode->GetNextSibling(getter_AddRefs(selectionNode));
      if (selectionNode)
        break;
      selectionNode = testNode;
    } while (PR_TRUE);
  }
  while (selectionNode && selectionNode != endSelectionNode);

  if (aCanFocusDoc)
    FocusElementButNotDocument(nsnull);

  return NS_OK; // no errors, but caret not inside focusable element other than doc itself
}



NS_IMETHODIMP nsEventStateManager::MoveCaretToFocus()
{
  // If in HTML content and the pref accessibility.browsewithcaret is TRUE,
  // then always move the caret to beginning of a new focus

  PRInt32 itemType = nsIDocShellTreeItem::typeChrome;

  if (mPresContext) {
    nsCOMPtr<nsISupports> pcContainer;
    mPresContext->GetContainer(getter_AddRefs(pcContainer));
    nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(pcContainer));
    if (treeItem) 
      treeItem->GetItemType(&itemType);
  }

  if (itemType != nsIDocShellTreeItem::typeChrome) {
    nsCOMPtr<nsIContent> selectionContent, endSelectionContent;
    nsIFrame *selectionFrame;
    PRUint32 selectionOffset;
    GetDocSelectionLocation(getter_AddRefs(selectionContent),
                            getter_AddRefs(endSelectionContent),
                            &selectionFrame, &selectionOffset);
    while (selectionContent) {
      nsCOMPtr<nsIContent> parentContent;
      selectionContent->GetParent(*getter_AddRefs(parentContent));
      if (mCurrentFocus == selectionContent && parentContent)
        return NS_OK; // selection is already within focus node that isn't the root content
      selectionContent = parentContent; // Keep checking up chain of parents, focus may be in a link above us
    }

    nsCOMPtr<nsIPresShell> shell;
    mPresContext->GetShell(getter_AddRefs(shell));
    if (shell) {
      // rangeDoc is a document interface we can create a range with
      nsCOMPtr<nsIDOMDocumentRange> rangeDoc(do_QueryInterface(mDocument));
      nsCOMPtr<nsIDOMNode> currentFocusNode(do_QueryInterface(mCurrentFocus));
      nsCOMPtr<nsIFrameSelection> frameSelection;
      shell->GetFrameSelection(getter_AddRefs(frameSelection));

      if (frameSelection && rangeDoc) {
        nsCOMPtr<nsISelection> domSelection;
        frameSelection->GetSelection(nsISelectionController::SELECTION_NORMAL, 
          getter_AddRefs(domSelection));
        if (domSelection) {
          // First clear the selection
          domSelection->RemoveAllRanges();
          nsCOMPtr<nsIDOMRange> newRange;
          if (currentFocusNode) {
            nsresult rv = rangeDoc->CreateRange(getter_AddRefs(newRange));
            if (NS_SUCCEEDED(rv)) {
              // If we could create a new range, then set it to the current focus node
              // And then collapse the selection
              newRange->SelectNodeContents(currentFocusNode);
              domSelection->AddRange(newRange);
              domSelection->CollapseToStart();
            }
          }
        }
      }
    }
  }
  return NS_OK;
}

nsresult nsEventStateManager::SetCaretEnabled(nsIPresShell *aPresShell, PRBool aEnabled)
{
  nsCOMPtr<nsICaret> caret;
  aPresShell->GetCaret(getter_AddRefs(caret));

  nsCOMPtr<nsISelectionController> selCon(do_QueryInterface(aPresShell));
  if (!selCon || !caret)
    return NS_ERROR_FAILURE;

  selCon->SetCaretEnabled(aEnabled);
  caret->SetCaretVisible(aEnabled);

  if (aEnabled) {
    PRInt32 pixelWidth = 1;
    nsCOMPtr<nsILookAndFeel> lookNFeel(do_GetService(kLookAndFeelCID));
    if (lookNFeel)
      lookNFeel->GetMetric(nsILookAndFeel::eMetric_MultiLineCaretWidth, pixelWidth);
    caret->SetCaretWidth(pixelWidth);
  }

  return NS_OK;
}

nsresult nsEventStateManager::SetContentCaretVisible(nsIPresShell* aPresShell, nsIContent *aFocusedContent, PRBool aVisible)
{ 
  // When browsing with caret, make sure caret is visible after new focus
  nsCOMPtr<nsICaret> caret;
  aPresShell->GetCaret(getter_AddRefs(caret));

  nsCOMPtr<nsIFrameSelection> frameSelection, docFrameSelection;
  if (aFocusedContent) {
    nsIFrame *focusFrame = nsnull;
    aPresShell->GetPrimaryFrameFor(aFocusedContent, &focusFrame);

    GetSelection(focusFrame, mPresContext, getter_AddRefs(frameSelection));
  }
  aPresShell->GetFrameSelection(getter_AddRefs(docFrameSelection));

  if (docFrameSelection && caret && 
     (frameSelection == docFrameSelection || !aFocusedContent)) {     
    nsCOMPtr<nsISelection> domSelection;
    docFrameSelection->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSelection));
    if (domSelection) {
      // First, tell the caret which selection to use
      caret->SetCaretDOMSelection(domSelection);

      // In content, we need to set the caret
      // the only other case is edit fields, where they have a different frame selection from the doc's
      // in that case they'll take care of making the caret visible themselves

      // Then make sure it's visible
      return SetCaretEnabled(aPresShell, aVisible);
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::ResetBrowseWithCaret(PRBool *aBrowseWithCaret)
{
  // This is called when browse with caret changes on the fly
  // or when a document gets focused 

  *aBrowseWithCaret = PR_FALSE;

  nsCOMPtr<nsISupports> pcContainer;
  mPresContext->GetContainer(getter_AddRefs(pcContainer));
  PRInt32 itemType;
  nsCOMPtr<nsIDocShellTreeItem> shellItem(do_QueryInterface(pcContainer));
  if (!shellItem)
    return NS_ERROR_FAILURE;

  shellItem->GetItemType(&itemType);

  if (itemType == nsIDocShellTreeItem::typeChrome) 
    return NS_OK;  // Never browse with caret in chrome

  mPrefBranch->GetBoolPref("accessibility.browsewithcaret", aBrowseWithCaret);

  if (mBrowseWithCaret == *aBrowseWithCaret)
    return NS_OK; // already set this way, don't change caret at all

  mBrowseWithCaret = *aBrowseWithCaret;

  nsCOMPtr<nsIPresShell> presShell;
  mPresContext->GetShell(getter_AddRefs(presShell));

  // Make caret visible or not, depending on what's appropriate
  if (presShell) {
    return SetContentCaretVisible(presShell, mCurrentFocus,
                                  *aBrowseWithCaret &&
                                  (!gLastFocusedDocument ||
                                   gLastFocusedDocument == mDocument));
  }

  return NS_ERROR_FAILURE;
}


//--------------------------------------------------------------------------------
//-- DocShell Focus Traversal Methods
//--------------------------------------------------------------------------------

//----------------------------------------
// Returns PR_TRUE if this doc contains a frameset
PRBool
nsEventStateManager::IsFrameSetDoc(nsIDocShell* aDocShell)
{
  NS_ASSERTION(aDocShell, "docshell is null");
  PRBool isFrameSet = PR_FALSE;

  // a frameset element will always be the immediate child
  // of the root content (the HTML tag)
  nsCOMPtr<nsIPresShell> presShell;
  aDocShell->GetPresShell(getter_AddRefs(presShell));
  if (presShell) {
    nsCOMPtr<nsIDocument> doc;
    presShell->GetDocument(getter_AddRefs(doc));
    nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(doc);
    if (htmlDoc) {
      nsCOMPtr<nsIContent> rootContent;
      doc->GetRootContent(getter_AddRefs(rootContent));
      if (rootContent) {
        PRInt32 childCount;
        rootContent->ChildCount(childCount);
        for (PRInt32 i = 0; i < childCount; ++i) {
          nsCOMPtr<nsIContent> childContent;
          rootContent->ChildAt(i, *getter_AddRefs(childContent));
          nsCOMPtr<nsIAtom> childTag;
          childContent->GetTag(*getter_AddRefs(childTag));
          if (childTag == nsHTMLAtoms::frameset) {
            isFrameSet = PR_TRUE;
            break;
          }
        }
      }
    }
  }

  return isFrameSet;
}

//----------------------------------------
// Returns PR_TRUE if this doc is an IFRAME

PRBool
nsEventStateManager::IsIFrameDoc(nsIDocShell* aDocShell)
{
  NS_ASSERTION(aDocShell, "docshell is null");

  nsCOMPtr<nsIDocShellTreeItem> treeItem = do_QueryInterface(aDocShell);
  nsCOMPtr<nsIDocShellTreeItem> parentItem;
  treeItem->GetParent(getter_AddRefs(parentItem));
  if (!parentItem)
    return PR_FALSE;
  
  nsCOMPtr<nsIDocShell> parentDS = do_QueryInterface(parentItem);
  nsCOMPtr<nsIPresShell> parentPresShell;
  parentDS->GetPresShell(getter_AddRefs(parentPresShell));
  NS_ASSERTION(parentPresShell, "presshell is null");

  nsCOMPtr<nsIDocument> parentDoc;
  parentPresShell->GetDocument(getter_AddRefs(parentDoc));

  // The current docshell may not have a presshell, eg if it's a display:none
  // iframe or if the presshell just hasn't been created yet. Get the
  // document off the docshell directly.
  nsCOMPtr<nsIDOMDocument> domDoc = do_GetInterface(aDocShell);
  if (!domDoc) {
    NS_ERROR("No document in docshell!  How are we to decide whether this"
             " is an iframe?  Arbitrarily deciding it's not.");
    return PR_FALSE;
  }

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  NS_ASSERTION(doc, "DOM document not implementing nsIDocument");

  nsCOMPtr<nsIContent> docContent;

  parentDoc->FindContentForSubDocument(doc, getter_AddRefs(docContent));

  if (!docContent)
    return PR_FALSE;
  
  nsCOMPtr<nsIAtom> tag;
  docContent->GetTag(*getter_AddRefs(tag));
  return (tag == nsHTMLAtoms::iframe);
}

//-------------------------------------------------------
// Return PR_TRUE if the docshell is visible

PRBool
nsEventStateManager::IsShellVisible(nsIDocShell* aShell)
{
  NS_ASSERTION(aShell, "docshell is null");

  nsCOMPtr<nsIBaseWindow> basewin = do_QueryInterface(aShell);
  if (!basewin)
    return PR_TRUE;
  
  PRBool isVisible = PR_TRUE;
  basewin->GetVisibility(&isVisible);

  // We should be doing some additional checks here so that
  // we don't tab into hidden tabs of tabbrowser.  -bryner

  return isVisible;
}

//------------------------------------------------
// This method should be called when tab or F6/ctrl-tab
// traversal wants to focus a new document.  It will focus
// the docshell, traverse into the document if this type 
// of document does not get document focus (i.e. framsets 
// and chrome), and update the canvas focus state on the docshell.

void
nsEventStateManager::TabIntoDocument(nsIDocShell* aDocShell,
                                     PRBool aForward)
{
  NS_ASSERTION(aDocShell, "null docshell");
  nsCOMPtr<nsIDOMWindowInternal> domwin = do_GetInterface(aDocShell);
  if (domwin)
    domwin->Focus();

  PRInt32 itemType;
  nsCOMPtr<nsIDocShellTreeItem> treeItem = do_QueryInterface(aDocShell);
  treeItem->GetItemType(&itemType);

  PRBool focusDocument;
  if (!aForward || (itemType == nsIDocShellTreeItem::typeChrome))
    focusDocument = PR_FALSE;
  else {
    // Check for a frameset document
    focusDocument = !(IsFrameSetDoc(aDocShell));
  }

  if (focusDocument) {
    // make sure we're in view
    aDocShell->SetCanvasHasFocus(PR_TRUE);
  }
  else {
    aDocShell->SetHasFocus(PR_FALSE);

    nsCOMPtr<nsIPresContext> pc;
    aDocShell->GetPresContext(getter_AddRefs(pc));
    if (pc) {
      nsCOMPtr<nsIEventStateManager> docESM;
      pc->GetEventStateManager(getter_AddRefs(docESM));
      if (docESM) {
        // clear out any existing focus state
        docESM->SetContentState(nsnull, NS_EVENT_STATE_FOCUS);
        // now focus the first (or last) focusable content
        docESM->ShiftFocus(aForward, nsnull);
      }
    }
  }
}

void
nsEventStateManager::GetLastChildDocShell(nsIDocShellTreeItem* aItem,
                                          nsIDocShellTreeItem** aResult)
{
  NS_ASSERTION(aItem, "null docshell");
  NS_ASSERTION(aResult, "null out pointer");
  
  nsCOMPtr<nsIDocShellTreeItem> curItem = do_QueryInterface(aItem);
  while (1) {
    nsCOMPtr<nsIDocShellTreeNode> curNode = do_QueryInterface(curItem);
    PRInt32 childCount = 0;
    curNode->GetChildCount(&childCount);
    if (!childCount) {
      *aResult = curItem;
      NS_ADDREF(*aResult);
      return;
    }
    
    curNode->GetChildAt(childCount - 1, getter_AddRefs(curItem));
  }
}

void
nsEventStateManager::GetNextDocShell(nsIDocShellTreeNode* aNode,
                                     nsIDocShellTreeItem** aResult)
{
  NS_ASSERTION(aNode, "null docshell");
  NS_ASSERTION(aResult, "null out pointer");
  PRInt32 numChildren = 0;
  
  *aResult = nsnull;

  aNode->GetChildCount(&numChildren);
  if (numChildren) {
    aNode->GetChildAt(0, aResult);
    if (*aResult)
      return;
  }
  
  nsCOMPtr<nsIDocShellTreeNode> curNode = aNode;
  while (curNode) {
    nsCOMPtr<nsIDocShellTreeItem> curItem = do_QueryInterface(curNode);
    nsCOMPtr<nsIDocShellTreeItem> parentItem;
    curItem->GetParent(getter_AddRefs(parentItem));
    if (!parentItem) {
      *aResult = nsnull;
      return;
    }
    
    PRInt32 childOffset = 0;
    curItem->GetChildOffset(&childOffset);
    nsCOMPtr<nsIDocShellTreeNode> parentNode = do_QueryInterface(parentItem);
    numChildren = 0;
    parentNode->GetChildCount(&numChildren);
    if (childOffset+1 < numChildren) {
      parentNode->GetChildAt(childOffset+1, aResult);
      if (*aResult)
        return;
    }
    
    curNode = do_QueryInterface(parentItem);
  }
}

void
nsEventStateManager::GetPrevDocShell(nsIDocShellTreeNode* aNode,
                                     nsIDocShellTreeItem** aResult)
{
  NS_ASSERTION(aNode, "null docshell");
  NS_ASSERTION(aResult, "null out pointer");
  
  nsCOMPtr<nsIDocShellTreeNode> curNode = aNode;
  nsCOMPtr<nsIDocShellTreeItem> curItem = do_QueryInterface(curNode);
  nsCOMPtr<nsIDocShellTreeItem> parentItem;

  curItem->GetParent(getter_AddRefs(parentItem));
  if (!parentItem) {
    *aResult = nsnull;
    return;
  }
  
  PRInt32 childOffset = 0;
  curItem->GetChildOffset(&childOffset);
  if (childOffset) {
    nsCOMPtr<nsIDocShellTreeNode> parentNode = do_QueryInterface(parentItem);
    parentNode->GetChildAt(childOffset - 1, getter_AddRefs(curItem));
    
    // get the last child recursively of this node
    while (1) {
      PRInt32 childCount = 0;
      curNode = do_QueryInterface(curItem);
      curNode->GetChildCount(&childCount);
      if (!childCount)
        break;
      
      curNode->GetChildAt(childCount - 1, getter_AddRefs(curItem));
    }
    
    *aResult = curItem;
    NS_ADDREF(*aResult);
    return;
  }
  
  *aResult = parentItem;
  NS_ADDREF(*aResult);
  return;
}

//-------------------------------------------------
// Traversal by document/DocShell only
// this does not include any content inside the doc
// or IFrames
void
nsEventStateManager::ShiftFocusByDoc(PRBool aForward)
{
  // Note that we use the docshell tree here instead of iteratively calling
  // ShiftFocus.  The docshell tree should be kept in depth-first frame tree
  // order, the same as we use for tabbing, so the effect should be the same,
  // but this is much faster.
  
  NS_ASSERTION(mPresContext, "no prescontext");

  nsCOMPtr<nsISupports> pcContainer;
  mPresContext->GetContainer(getter_AddRefs(pcContainer));
  nsCOMPtr<nsIDocShellTreeNode> curNode = do_QueryInterface(pcContainer);
  
  // perform a depth first search (preorder) of the docshell tree
  // looking for an HTML Frame or a chrome document
  
  nsCOMPtr<nsIDocShellTreeItem> nextItem;
  nsCOMPtr<nsIDocShell> nextShell;
  do {
    if (aForward) {
      GetNextDocShell(curNode, getter_AddRefs(nextItem));
      if (!nextItem) {
        nsCOMPtr<nsIDocShellTreeItem> curItem = do_QueryInterface(pcContainer);
        // wrap around to the beginning, which is the top of the tree
        curItem->GetRootTreeItem(getter_AddRefs(nextItem));
      }
    }
    else {
      GetPrevDocShell(curNode, getter_AddRefs(nextItem));
      if (!nextItem) {
        nsCOMPtr<nsIDocShellTreeItem> curItem = do_QueryInterface(pcContainer);
        // wrap around to the end, which is the last node in the tree
        nsCOMPtr<nsIDocShellTreeItem> rootItem;
        curItem->GetRootTreeItem(getter_AddRefs(rootItem));
        GetLastChildDocShell(rootItem, getter_AddRefs(nextItem));
      }
    }

    curNode = do_QueryInterface(nextItem);
    nextShell = do_QueryInterface(nextItem);
  } while (IsFrameSetDoc(nextShell) || IsIFrameDoc(nextShell) || !IsShellVisible(nextShell));

  if (nextShell) {
    // NOTE: always tab forward into the document, this ensures that we
    // focus the document itself, not its last focusable content.
    // chrome documents will get their first focusable content focused.
    SetContentState(nsnull, NS_EVENT_STATE_FOCUS);
    TabIntoDocument(nextShell, PR_TRUE);
  }
}

// Get the FocusController given an nsIDocument
nsIFocusController*
nsEventStateManager::GetFocusControllerForDocument(nsIDocument* aDocument)
{
  nsCOMPtr<nsISupports> container;
  aDocument->GetContainer(getter_AddRefs(container));
  nsCOMPtr<nsPIDOMWindow> windowPrivate = do_GetInterface(container);
  nsIFocusController* fc;
  if (windowPrivate)
    windowPrivate->GetRootFocusController(&fc);
  else
    fc = nsnull;

  return fc;
}
