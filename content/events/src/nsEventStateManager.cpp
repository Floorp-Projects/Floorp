/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsCOMPtr.h"
#include "nsEventStateManager.h"
#include "nsEventListenerManager.h"
#include "nsIContent.h"
#include "nsINodeInfo.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsIWidget.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsDOMEvent.h"
#include "nsHTMLAtoms.h"
#include "nsIEditorDocShell.h"
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
#include "nsIDOMXULControlElement.h"
#include "nsImageMapUtils.h"
#include "nsIHTMLDocument.h"
#include "nsINameSpaceManager.h"
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
#include "nsIPrefBranchInternal.h"

#include "nsIServiceManager.h"
#include "nsIScriptSecurityManager.h"

#include "nsIChromeEventHandler.h"
#include "nsIFocusController.h"

#include "nsXULAtoms.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMDocument.h"
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
#include "nsLayoutAtoms.h"
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

// Tab focus model bit field:
enum nsTabFocusModel {
//eTabFocus_textControlsMask = (1<<0),  // unused - textboxes always tabbable
  eTabFocus_formElementsMask = (1<<1),  // non-text form elements
  eTabFocus_linksMask = (1<<2),         // links
  eTabFocus_any = 1 + (1<<1) + (1<<2)   // everything that can be focused
};

enum nsTextfieldSelectModel {
  eTextfieldSelect_unset = -1,
  eTextfieldSelect_manual = 0,
  eTextfieldSelect_auto = 1   // select textfields when focused with keyboard
};

// Tab focus policy (static, constant across the app):
// Which types of elements are in the tab order?
static PRInt8 sTextfieldSelectModel = eTextfieldSelect_unset;
static PRBool sLeftClickOnly = PR_TRUE;
static PRBool sKeyCausesActivation = PR_TRUE;
static PRUint32 sESMInstanceCount = 0;
static PRInt32 sGeneralAccesskeyModifier = -1; // magic value of -1 means uninitialized
static PRInt32 sTabFocusModel = eTabFocus_any;

enum {
 MOUSE_SCROLL_N_LINES,
 MOUSE_SCROLL_PAGE,
 MOUSE_SCROLL_HISTORY,
 MOUSE_SCROLL_TEXTSIZE
};

/******************************************************************/
/* CurrentEventShepherd pushes a new current event onto           */
/* nsEventStateManager and pops it when destroyed                 */
/******************************************************************/

class CurrentEventShepherd {
public:
  CurrentEventShepherd(nsEventStateManager *aManager, nsEvent *aEvent);
  CurrentEventShepherd(nsEventStateManager *aManager);
  ~CurrentEventShepherd();
  void SetRestoreEvent(nsEvent *aEvent) { mRestoreEvent = aEvent; }
  void SetCurrentEvent(nsEvent *aEvent) { mManager->mCurrentEvent = aEvent; }

private:
  nsEvent             *mRestoreEvent;
  nsEventStateManager *mManager;
};
CurrentEventShepherd::CurrentEventShepherd(nsEventStateManager *aManager,
                                           nsEvent *aEvent)
{
  mRestoreEvent = aManager->mCurrentEvent;
  mManager = aManager;
  SetCurrentEvent(aEvent);
}
CurrentEventShepherd::CurrentEventShepherd(nsEventStateManager *aManager)
{
  mRestoreEvent = aManager->mCurrentEvent;
  mManager = aManager;
}
CurrentEventShepherd::~CurrentEventShepherd()
{
  mManager->mCurrentEvent = mRestoreEvent;
}

/******************************************************************/
/* nsEventStateManager                                            */
/******************************************************************/

nsEventStateManager::nsEventStateManager()
  : mLockCursor(0),
    mCurrentTarget(nsnull),
    mLastMouseOverFrame(nsnull),
    mLastDragOverFrame(nsnull),
    // init d&d gesture state machine variables
    mIsTrackingDragGesture(PR_FALSE),
    mGestureDownPoint(0,0),
    mGestureDownRefPoint(0,0),
    mGestureDownFrame(nsnull),
    mCurrentFocusFrame(nsnull),
    mLastFocusedWith(eEventFocusedByUnknown),
    mCurrentTabIndex(0),
    mCurrentEvent(0),
    mPresContext(nsnull),
    mLClickCount(0),
    mMClickCount(0),
    mRClickCount(0),
    mConsumeFocusEvents(PR_FALSE),
    mNormalLMouseEventInProcess(PR_FALSE),
    m_haveShutdown(PR_FALSE),
    mClearedFrameRefsDuringEvent(PR_FALSE),
    mBrowseWithCaret(PR_FALSE),
    mTabbedThroughDocument(PR_FALSE),
    mDOMEventLevel(0),
    mAccessKeys(nsnull)
#ifdef CLICK_HOLD_CONTEXT_MENUS
    ,
    mEventDownWidget(nsnull),
    mEventPresContext(nsnull)
#endif
{
  ++sESMInstanceCount;
}

NS_IMETHODIMP
nsEventStateManager::Init()
{
  nsresult rv;
  nsCOMPtr<nsIObserverService> observerService = 
           do_GetService("@mozilla.org/observer-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_TRUE);

  nsCOMPtr<nsIPrefBranchInternal> prefBranch =
    do_QueryInterface(nsContentUtils::GetPrefBranch());

  if (prefBranch) {
    if (sESMInstanceCount == 1) {
      sLeftClickOnly =
        nsContentUtils::GetBoolPref("nglayout.events.dispatchLeftClickOnly",
                                    sLeftClickOnly);

      sGeneralAccesskeyModifier =
        nsContentUtils::GetIntPref("ui.key.generalAccessKey",
                                   sGeneralAccesskeyModifier);

      sTabFocusModel = nsContentUtils::GetIntPref("accessibility.tabfocus",
                                                  sTabFocusModel);
    }
    prefBranch->AddObserver("accessibility.accesskeycausesactivation", this, PR_TRUE);
    prefBranch->AddObserver("accessibility.browsewithcaret", this, PR_TRUE);
    prefBranch->AddObserver("accessibility.tabfocus", this, PR_TRUE);
    prefBranch->AddObserver("nglayout.events.dispatchLeftClickOnly", this, PR_TRUE);
    prefBranch->AddObserver("ui.key.generalAccessKey", this, PR_TRUE);
#if 0
    prefBranch->AddObserver("mousewheel.withaltkey.action", this, PR_TRUE);
    prefBranch->AddObserver("mousewheel.withaltkey.numlines", this, PR_TRUE);
    prefBranch->AddObserver("mousewheel.withaltkey.sysnumlines", this, PR_TRUE);
    prefBranch->AddObserver("mousewheel.withcontrolkey.action", this, PR_TRUE);
    prefBranch->AddObserver("mousewheel.withcontrolkey.numlines", this, PR_TRUE);
    prefBranch->AddObserver("mousewheel.withcontrolkey.sysnumlines", this, PR_TRUE);
    prefBranch->AddObserver("mousewheel.withnokey.action", this, PR_TRUE);
    prefBranch->AddObserver("mousewheel.withnokey.numlines", this, PR_TRUE);
    prefBranch->AddObserver("mousewheel.withnokey.sysnumlines", this, PR_TRUE);
    prefBranch->AddObserver("mousewheel.withshiftkey.action", this, PR_TRUE);
    prefBranch->AddObserver("mousewheel.withshiftkey.numlines", this, PR_TRUE);
    prefBranch->AddObserver("mousewheel.withshiftkey.sysnumlines", this, PR_TRUE);
#endif
  }

  if (sTextfieldSelectModel == eTextfieldSelect_unset) {
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

  --sESMInstanceCount;
  if(sESMInstanceCount == 0) {
    NS_IF_RELEASE(gLastFocusedContent);
    NS_IF_RELEASE(gLastFocusedDocument);
  }

  delete mAccessKeys;

  if (!m_haveShutdown) {
    Shutdown();

    // Don't remove from Observer service in Shutdown because Shutdown also
    // gets called from xpcom shutdown observer.  And we don't want to remove
    // from the service in that case.

    nsresult rv;

    nsCOMPtr<nsIObserverService> observerService = 
             do_GetService("@mozilla.org/observer-service;1", &rv);
    if (NS_SUCCEEDED(rv)) {
      observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    }
  }
  
}

nsresult
nsEventStateManager::Shutdown()
{
  nsCOMPtr<nsIPrefBranchInternal> prefBranch =
    do_QueryInterface(nsContentUtils::GetPrefBranch());

  if (prefBranch) {
    prefBranch->RemoveObserver("accessibility.accesskeycausesactivation", this);
    prefBranch->RemoveObserver("accessibility.browsewithcaret", this);
    prefBranch->RemoveObserver("accessibility.tabfocus", this);
    prefBranch->RemoveObserver("nglayout.events.dispatchLeftClickOnly", this);
    prefBranch->RemoveObserver("ui.key.generalAccessKey", this);
#if 0
    prefBranch->RemoveObserver("mousewheel.withshiftkey.action", this);
    prefBranch->RemoveObserver("mousewheel.withshiftkey.numlines", this);
    prefBranch->RemoveObserver("mousewheel.withshiftkey.sysnumlines", this);
    prefBranch->RemoveObserver("mousewheel.withcontrolkey.action", this);
    prefBranch->RemoveObserver("mousewheel.withcontrolkey.numlines", this);
    prefBranch->RemoveObserver("mousewheel.withcontrolkey.sysnumlines", this);
    prefBranch->RemoveObserver("mousewheel.withaltkey.action", this);
    prefBranch->RemoveObserver("mousewheel.withaltkey.numlines", this);
    prefBranch->RemoveObserver("mousewheel.withaltkey.sysnumlines", this);
    prefBranch->RemoveObserver("mousewheel.withnokey.action", this);
    prefBranch->RemoveObserver("mousewheel.withnokey.numlines", this);
    prefBranch->RemoveObserver("mousewheel.withnokey.sysnumlines", this);
#endif
  }

  m_haveShutdown = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::Observe(nsISupports *aSubject, 
                             const char *aTopic,
                             const PRUnichar *someData)
{
  if (!nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID))
    Shutdown();
  else if (!nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    if (!someData)
      return NS_OK;

    nsDependentString data(someData);
    if (data.EqualsLiteral("accessibility.accesskeycausesactivation")) {
      sKeyCausesActivation =
        nsContentUtils::GetBoolPref("accessibility.accesskeycausesactivation",
                                    sKeyCausesActivation);
    } else if (data.EqualsLiteral("accessibility.browsewithcaret")) {
      ResetBrowseWithCaret();
    } else if (data.EqualsLiteral("accessibility.tabfocus")) {
      sTabFocusModel = nsContentUtils::GetIntPref("accessibility.tabfocus",
                                                  sTabFocusModel);
    } else if (data.EqualsLiteral("nglayout.events.dispatchLeftClickOnly")) {
      sLeftClickOnly =
        nsContentUtils::GetBoolPref("nglayout.events.dispatchLeftClickOnly",
                                    sLeftClickOnly);
    } else if (data.EqualsLiteral("ui.key.generalAccessKey")) {
      sGeneralAccesskeyModifier =
        nsContentUtils::GetIntPref("ui.key.generalAccessKey",
                                   sGeneralAccesskeyModifier);
#if 0
    } else if (data.EqualsLiteral("mousewheel.withaltkey.action")) {
    } else if (data.EqualsLiteral("mousewheel.withaltkey.numlines")) {
    } else if (data.EqualsLiteral("mousewheel.withaltkey.sysnumlines")) {
    } else if (data.EqualsLiteral("mousewheel.withcontrolkey.action")) {
    } else if (data.EqualsLiteral("mousewheel.withcontrolkey.numlines")) {
    } else if (data.EqualsLiteral("mousewheel.withcontrolkey.sysnumlines")) {
    } else if (data.EqualsLiteral("mousewheel.withshiftkey.action")) {
    } else if (data.EqualsLiteral("mousewheel.withshiftkey.numlines")) {
    } else if (data.EqualsLiteral("mousewheel.withshiftkey.sysnumlines")) {
    } else if (data.EqualsLiteral("mousewheel.withnokey.action")) {
    } else if (data.EqualsLiteral("mousewheel.withnokey.numlines")) {
    } else if (data.EqualsLiteral("mousewheel.withnokey.sysnumlines")) {
#endif
    }
  }
  
  return NS_OK;
}


NS_IMPL_ISUPPORTS3(nsEventStateManager, nsIEventStateManager, nsIObserver, nsISupportsWeakReference)

inline void
SetFrameExternalReference(nsIFrame* aFrame)
{
  aFrame->AddStateBits(NS_FRAME_EXTERNAL_REFERENCE);
}

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

  // Focus events don't necessarily need a frame.
  if (NS_EVENT_NEEDS_FRAME(aEvent)) {
    NS_ASSERTION(mCurrentTarget, "mCurrentTarget is null.  this should not happen.  see bug #13007");
    if (!mCurrentTarget) return NS_ERROR_NULL_POINTER;
  }

  if (mCurrentTarget)
    SetFrameExternalReference(mCurrentTarget);

  *aStatus = nsEventStatus_eIgnore;

  if (!aEvent) {
    NS_ERROR("aEvent is null.  This should never happen.");
    return NS_ERROR_NULL_POINTER;
  }

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
    if (IsTrackingDragGesture() &&
        NS_STATIC_CAST(nsMouseEvent*, aEvent)->reason ==
          nsMouseEvent::eSynthesized) {
      // This is not a real mouse move, it's synthesized.  Reset the
      // starting point to the current point (since things moved), but
      // otherwise let the user continue the drag.
      StopTrackingDragGesture();
      BeginTrackingDragGesture(aPresContext,
                               NS_STATIC_CAST(nsGUIEvent*, aEvent),
                               aTargetFrame);
    }
    // on the Mac, GenerateDragGesture() may not return until the drag
    // has completed and so |aTargetFrame| may have been deleted (moving
    // a bookmark, for example).  If this is the case, however, we know
    // that ClearFrameRefs() has been called and it cleared out
    // |mCurrentTarget|. As a result, we should pass |mCurrentTarget|
    // into UpdateCursor().
    GenerateDragGesture(aPresContext, (nsGUIEvent*)aEvent);
    UpdateCursor(aPresContext, aEvent, mCurrentTarget, aStatus);
    GenerateMouseEnterExit(aPresContext, (nsGUIEvent*)aEvent);
    // Flush reflows and invalidates to eliminate flicker when both a reflow
    // and visual change occur in an event callback. See bug  #36849
    // XXXbz eeeew.  Why not fix viewmanager to flush reflows before painting??
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
      // This is called when a child widget has received focus.
      // We need to take care of sending a blur event for the previously
      // focused content and document, then dispatching a focus
      // event to the target content, its document, and its window.

      EnsureDocument(aPresContext);

      // If the document didn't change, then the only thing that could have
      // changed is the focused content node.  That's handled elsewhere
      // (SetContentState and SendFocusBlur).

      if (gLastFocusedDocument == mDocument)
        break;

      if (mDocument) {
        if (gLastFocusedDocument && gLastFocusedPresContext) {
          nsCOMPtr<nsPIDOMWindow> ourWindow = do_QueryInterface(gLastFocusedDocument->GetScriptGlobalObject());

          // If the focus controller is already suppressed, it means that we
          // are in the middle of an activate sequence. In this case, we do
          // _not_ want to fire a blur on the previously focused content, since
          // we will be focusing it again later when we receive the NS_ACTIVATE
          // event.  See bug 120209.

          nsIFocusController *focusController = nsnull;
          PRBool isAlreadySuppressed = PR_FALSE;

          if (ourWindow) {
            focusController = ourWindow->GetRootFocusController();
            if (focusController) {
              focusController->GetSuppressFocus(&isAlreadySuppressed);
              focusController->SetSuppressFocus(PR_TRUE,
                                                "NS_GOTFOCUS ESM Suppression");
            }
          }

          if (!isAlreadySuppressed) {

            // Fire the blur event on the previously focused document.

            nsEventStatus blurstatus = nsEventStatus_eIgnore;
            nsEvent blurevent(NS_BLUR_CONTENT);

            gLastFocusedDocument->HandleDOMEvent(gLastFocusedPresContext,
                                                 &blurevent,
                                                 nsnull, NS_EVENT_FLAG_INIT,
                                                 &blurstatus);

            if (!mCurrentFocus && gLastFocusedContent) {
              // We also need to blur the previously focused content node here,
              // if we don't have a focused content node in this document.
              // (SendFocusBlur isn't called in this case).

              nsCOMPtr<nsIContent> blurContent = gLastFocusedContent;
              gLastFocusedContent->HandleDOMEvent(gLastFocusedPresContext,
                                                  &blurevent, nsnull,
                                                  NS_EVENT_FLAG_INIT,
                                                  &blurstatus);

              // XXX bryner this isn't quite right -- it can result in
              // firing two blur events on the content.

              nsCOMPtr<nsIDocument> doc;
              if (gLastFocusedContent) // could have changed in HandleDOMEvent
                doc = gLastFocusedContent->GetDocument();
              if (doc) {
                nsIPresShell *shell = doc->GetShellAt(0);
                if (shell) {
                  nsCOMPtr<nsIPresContext> oldPresContext;
                  shell->GetPresContext(getter_AddRefs(oldPresContext));
                  
                  nsCOMPtr<nsIEventStateManager> esm;
                  esm = oldPresContext->EventStateManager();
                  esm->SetFocusedContent(gLastFocusedContent);
                  gLastFocusedContent->HandleDOMEvent(oldPresContext,
                                                      &blurevent, nsnull,
                                                      NS_EVENT_FLAG_INIT,
                                                      &blurstatus); 
                  esm->SetFocusedContent(nsnull);
                  NS_IF_RELEASE(gLastFocusedContent);
                }
              }
            }
          }

          if (focusController)
            focusController->SetSuppressFocus(PR_FALSE,
                                              "NS_GOTFOCUS ESM Suppression");
        }
        
        // Now we should fire the focus event.  We fire it on the document,
        // then the content node, then the window.

        nsEventStatus status = nsEventStatus_eIgnore;
        nsEvent focusevent(NS_FOCUS_CONTENT);

        nsCOMPtr<nsIScriptGlobalObject> globalObject = mDocument->GetScriptGlobalObject();
        if (globalObject) {
          // We don't want there to be a focused content node while we're
          // dispatching the focus event.

          nsCOMPtr<nsIContent> currentFocus = mCurrentFocus;
          // "leak" this reference, but we take it back later
          SetFocusedContent(nsnull);

          if (gLastFocusedDocument != mDocument) {
            mDocument->HandleDOMEvent(aPresContext, &focusevent, nsnull,
                                      NS_EVENT_FLAG_INIT, &status);
            if (currentFocus && currentFocus != gLastFocusedContent)
              currentFocus->HandleDOMEvent(aPresContext, &focusevent, nsnull,
                                           NS_EVENT_FLAG_INIT, &status);
          }

          globalObject->HandleDOMEvent(aPresContext, &focusevent, nsnull,
                                       NS_EVENT_FLAG_INIT, &status);

          SetFocusedContent(currentFocus); // we kept this reference above
          NS_IF_RELEASE(gLastFocusedContent);
          gLastFocusedContent = mCurrentFocus;
          NS_IF_ADDREF(gLastFocusedContent);
        }
        
        // Try to keep the focus controllers and the globals in synch
        if (gLastFocusedDocument && gLastFocusedDocument != mDocument) {

          nsIFocusController *lastController = nsnull;
          nsCOMPtr<nsPIDOMWindow> lastWindow = do_QueryInterface(gLastFocusedDocument->GetScriptGlobalObject());
          if (lastWindow)
            lastController = lastWindow->GetRootFocusController();

          nsIFocusController *nextController = nsnull;
          nsCOMPtr<nsPIDOMWindow> nextWindow = do_QueryInterface(mDocument->GetScriptGlobalObject());
          if (nextWindow)
            nextController = nextWindow->GetRootFocusController();

          if (lastController != nextController && lastController && nextController)
            lastController->SetActive(PR_FALSE);
        }

        NS_IF_RELEASE(gLastFocusedDocument);
        gLastFocusedDocument = mDocument;
        gLastFocusedPresContext = aPresContext;
        NS_IF_ADDREF(gLastFocusedDocument);
      }

      ResetBrowseWithCaret();
    }

    break;

  case NS_LOSTFOCUS:
    {
      // Hide the caret used in "browse with caret mode" 
      if (mBrowseWithCaret && mPresContext) {
        nsIPresShell *presShell = mPresContext->GetPresShell();
        if (presShell)
           SetContentCaretVisible(presShell, mCurrentFocus, PR_FALSE);
      }

      // If focus is going to another mozilla window, we wait for the
      // focus event and fire a blur on the old focused content at that time.
      // This allows "-moz-user-focus: ignore" to work.

#if defined(XP_WIN) || defined(XP_OS2)
      if (!NS_STATIC_CAST(nsFocusEvent*, aEvent)->isMozWindowTakingFocus) {

        // This situation occurs when focus goes to a non-gecko child window
        // in an embedding application.  In this case we do fire a blur
        // immediately.

        EnsureDocument(aPresContext);

        // We can get a deactivate on an Ender (editor) widget.  In this
        // case, we would like to obtain the DOM Window to start
        // with by looking at gLastFocusedContent.
        nsCOMPtr<nsIScriptGlobalObject> ourGlobal;
        if (gLastFocusedContent) {
          nsIDocument* doc = gLastFocusedContent->GetDocument();
          if (doc)
            ourGlobal = doc->GetScriptGlobalObject();
          else {
            ourGlobal = mDocument->GetScriptGlobalObject();
            NS_RELEASE(gLastFocusedContent);
          }
        }
        else
          ourGlobal = mDocument->GetScriptGlobalObject();

        // Now fire blurs.  We fire a blur on the focused document, element,
        // and window.

        nsEventStatus status = nsEventStatus_eIgnore;
        nsEvent event(NS_BLUR_CONTENT);

        if (gLastFocusedDocument && gLastFocusedPresContext) {
          if (gLastFocusedContent) {
            // Retrieve this content node's pres context. it can be out of sync
            // in the Ender widget case.
            nsCOMPtr<nsIDocument> doc = gLastFocusedContent->GetDocument();
            if (doc) {
              nsIPresShell *shell = doc->GetShellAt(0);
              if (shell) {
                nsCOMPtr<nsIPresContext> oldPresContext;
                shell->GetPresContext(getter_AddRefs(oldPresContext));

                nsCOMPtr<nsIEventStateManager> esm =
                  oldPresContext->EventStateManager();
                esm->SetFocusedContent(gLastFocusedContent);
                gLastFocusedContent->HandleDOMEvent(oldPresContext, &event,
                                                    nsnull, NS_EVENT_FLAG_INIT,
                                                    &status); 
                esm->SetFocusedContent(nsnull);
                NS_IF_RELEASE(gLastFocusedContent);
              }
            }
          }

          // fire blur on document and window
          if (gLastFocusedDocument) {
            // get the window here, in case the event causes
            // gLastFocusedDocument to change.

            nsCOMPtr<nsIScriptGlobalObject> globalObject = gLastFocusedDocument->GetScriptGlobalObject();

            gLastFocusedDocument->HandleDOMEvent(gLastFocusedPresContext,
                                                 &event, nsnull,
                                                 NS_EVENT_FLAG_INIT, &status);

            if (globalObject)
              globalObject->HandleDOMEvent(gLastFocusedPresContext, &event,
                                           nsnull, NS_EVENT_FLAG_INIT,
                                           &status);
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
      // If we have a focus controller, and if it has a focused window and a
      // focused element in its focus memory, then restore the focus to those
      // objects.

      EnsureDocument(aPresContext);

      nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(mDocument->GetScriptGlobalObject()));

      if (!win) {
        NS_ERROR("win is null.  this happens [often on xlib builds].  see bug #79213");
        return NS_ERROR_NULL_POINTER;
      }

      nsIFocusController *focusController = win->GetRootFocusController();

      nsCOMPtr<nsIDOMElement> focusedElement;
      nsCOMPtr<nsIDOMWindowInternal> focusedWindow;

      if (focusController) {
        // Obtain focus info from the focus controller.
        focusController->GetFocusedWindow(getter_AddRefs(focusedWindow));
        focusController->GetFocusedElement(getter_AddRefs(focusedElement));

        focusController->SetSuppressFocusScroll(PR_TRUE);
        focusController->SetActive(PR_TRUE);
      }

      if (!focusedWindow)
        focusedWindow = win;

      NS_WARN_IF_FALSE(focusedWindow,"check why focusedWindow is null!!!");

      // Focus the DOM window.
      if (focusedWindow) {
        focusedWindow->Focus();

        nsCOMPtr<nsIDOMDocument> domDoc;
        focusedWindow->GetDocument(getter_AddRefs(domDoc));

        if (domDoc) {
          nsCOMPtr<nsIDocument> document = do_QueryInterface(domDoc);
          nsIPresShell *shell = document->GetShellAt(0);
          NS_ASSERTION(shell, "Focus events should not be getting thru when this is null!");
          if (shell) {
            if (focusedElement) {
              nsCOMPtr<nsIContent> focusContent = do_QueryInterface(focusedElement);
              nsCOMPtr<nsIPresContext> context;
              shell->GetPresContext(getter_AddRefs(context));
              focusContent->SetFocus(context);
            }

            // disable selection mousedown state on activation
            nsCOMPtr<nsIFrameSelection> frameSel;
            shell->GetFrameSelection(getter_AddRefs(frameSel));
            if (frameSel)
              frameSel->SetMouseDownState(PR_FALSE);
          }
        }  
      }

      if (focusController) {

        // Make sure the focus controller is up-to-date, since restoring
        // focus memory may have caused focus to go elsewhere.

        if (gLastFocusedDocument && gLastFocusedDocument == mDocument) {
          nsCOMPtr<nsIDOMElement> focusElement = do_QueryInterface(mCurrentFocus);
          focusController->SetFocusedElement(focusElement);
        }

        PRBool isSuppressed;
        focusController->GetSuppressFocus(&isSuppressed);
        while (isSuppressed) {
          // Unsuppress and let the focus controller listen again.
          focusController->SetSuppressFocus(PR_FALSE,
                                            "Activation Suppression");

          focusController->GetSuppressFocus(&isSuppressed);
        }
        focusController->SetSuppressFocusScroll(PR_FALSE);
      }
    }
    break;
    
 case NS_DEACTIVATE:
    {
      EnsureDocument(aPresContext);

      nsCOMPtr<nsIScriptGlobalObject> ourGlobal = mDocument->GetScriptGlobalObject();

      // Suppress the focus controller for the duration of the
      // de-activation.  This will cause it to remember the last
      // focused sub-window and sub-element for this top-level
      // window.

      nsCOMPtr<nsIFocusController> focusController =
        GetFocusControllerForDocument(mDocument);

      if (focusController)
        focusController->SetSuppressFocus(PR_TRUE, "Deactivate Suppression");

      // Now fire blurs.  Blur the content, then the document, then the window.

      if (gLastFocusedDocument && gLastFocusedDocument == mDocument) {

        nsEventStatus status = nsEventStatus_eIgnore;
        nsEvent event(NS_BLUR_CONTENT);

        if (gLastFocusedContent) {
          nsIPresShell *shell = gLastFocusedDocument->GetShellAt(0);
          if (shell) {
            nsCOMPtr<nsIPresContext> oldPresContext;
            shell->GetPresContext(getter_AddRefs(oldPresContext));
            
            nsCOMPtr<nsIDOMElement> focusedElement;
            if (focusController)
              focusController->GetFocusedElement(getter_AddRefs(focusedElement));

            nsCOMPtr<nsIEventStateManager> esm;
            esm = oldPresContext->EventStateManager();
            esm->SetFocusedContent(gLastFocusedContent);

            nsCOMPtr<nsIContent> focusedContent = do_QueryInterface(focusedElement);
            if (focusedContent) {
              // Blur the element.
              focusedContent->HandleDOMEvent(oldPresContext, &event, nsnull,
                                             NS_EVENT_FLAG_INIT, &status); 
            }

            esm->SetFocusedContent(nsnull);
            NS_IF_RELEASE(gLastFocusedContent);
          }
        }

        // fire blur on document and window
        mDocument->HandleDOMEvent(aPresContext, &event, nsnull,
                                  NS_EVENT_FLAG_INIT, &status);

        if (ourGlobal)
          ourGlobal->HandleDOMEvent(aPresContext, &event, nsnull,
                                    NS_EVENT_FLAG_INIT, &status);

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
      switch (sGeneralAccesskeyModifier) {
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

      // if it's a XUL element...
      if (content->IsContentOfType(nsIContent::eXUL)) {
        // find out what type of content node this is
        if (content->Tag() == nsXULAtoms::label) {
          // If anything fails, this will be null ...
          nsCOMPtr<nsIDOMElement> element;

          nsAutoString control;
          content->GetAttr(kNameSpaceID_None, nsXULAtoms::control, control);
          if (!control.IsEmpty()) {
            nsCOMPtr<nsIDOMDocument> domDocument =
              do_QueryInterface(content->GetDocument());
            if (domDocument)
              domDocument->GetElementById(control, getter_AddRefs(element));
          }
          // ... that here we'll either change |content| to the element
          // referenced by |element|, or clear it.
          content = do_QueryInterface(element);
        }

        if (!content)
          return;

        nsIFrame* frame = nsnull;
        aPresContext->PresShell()->GetPrimaryFrameFor(content, &frame);

        if (frame) {
          const nsStyleVisibility* vis = frame->GetStyleVisibility();
          PRBool viewShown = frame->AreAncestorViewsVisible();

          // get the XUL element
          nsCOMPtr<nsIDOMXULElement> element = do_QueryInterface(content);

          // if collapsed or hidden, we don't get tabbed into.
          if (viewShown &&
            vis->mVisible != NS_STYLE_VISIBILITY_COLLAPSE &&
            vis->mVisible != NS_STYLE_VISIBILITY_HIDDEN &&
            element) {

            // find out what type of content node this is
            nsIAtom *atom = content->Tag();

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
      } else { // otherwise, it must be HTML
        // It's hard to say what HTML4 wants us to do in all cases.
        // So for now we'll settle for A) Set focus
        ChangeFocus(content, eEventFocusedByKey);

        if (sKeyCausesActivation) {
          // B) Click on it if the users prefs indicate to do so.
          nsEventStatus status = nsEventStatus_eIgnore;
          nsMouseEvent event(NS_MOUSE_LEFT_CLICK);
          content->HandleDOMEvent(mPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
        }

      }

      *aStatus = nsEventStatus_eConsumeNoDefault;
    }
  }
  // after the local accesskey handling
  if (nsEventStatus_eConsumeNoDefault != *aStatus) {
    // checking all sub docshells

    nsCOMPtr<nsISupports> pcContainer = aPresContext->GetContainer();
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

        nsEventStateManager* esm =
          NS_STATIC_CAST(nsEventStateManager *, subPC->EventStateManager());

        if (esm)
          esm->HandleAccessKey(subPC, aEvent, aStatus, -1, eAccessKeyProcessingDown);

        if (nsEventStatus_eConsumeNoDefault == *aStatus)
          break;
      }
    }
  }// if end . checking all sub docshell ends here.

  // bubble up the process to the parent docShell if necesary
  if (eAccessKeyProcessingDown != aAccessKeyState && nsEventStatus_eConsumeNoDefault != *aStatus) {
    nsCOMPtr<nsISupports> pcContainer = aPresContext->GetContainer();
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

      parentDS->GetPresShell(getter_AddRefs(parentPS));
      NS_ASSERTION(parentPS, "Our PresShell exists but the parent's does not?");

      parentPS->GetPresContext(getter_AddRefs(parentPC));
      NS_ASSERTION(parentPC, "PresShell without PresContext");

      nsEventStateManager* esm =
        NS_STATIC_CAST(nsEventStateManager *, parentPC->EventStateManager());

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
nsEventStateManager::CreateClickHoldTimer(nsIPresContext* inPresContext,
                                          nsGUIEvent* inMouseDownEvent)
{
  // just to be anal (er, safe)
  if (mClickHoldTimer) {
    mClickHoldTimer->Cancel();
    mClickHoldTimer = nsnull;
  }

  // if content clicked on has a popup, don't even start the timer
  // since we'll end up conflicting and both will show.
  if (mGestureDownFrame) {
    nsIContent* clickedContent = mGestureDownFrame->GetContent();
    if (clickedContent) {
      // check for the |popup| attribute
      nsAutoString popup;
      clickedContent->GetAttr(kNameSpaceID_None, nsXULAtoms::popup, popup);
      if (!popup.IsEmpty())
        return;

      // check for a <menubutton> like bookmarks
      if (clickedContent->Tag() == nsXULAtoms::menubutton)
        return;
    }
  }

  mClickHoldTimer = do_CreateInstance("@mozilla.org/timer;1");
  if ( mClickHoldTimer )
    mClickHoldTimer->InitWithFuncCallback(sClickHoldCallback, this,
                                          kClickHoldDelay, 
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
nsEventStateManager::KillClickHoldTimer()
{
  if (mClickHoldTimer) {
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
nsEventStateManager::sClickHoldCallback(nsITimer *aTimer, void* aESM)
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
nsEventStateManager::FireContextClick()
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
  nsMouseEvent event(NS_CONTEXTMENU, mEventDownWidget);
  event.clickCount = 1;
  event.point = mEventPoint;
  event.refPoint = mEventRefPoint;

  // Dispatch to the DOM. We have to fake out the ESM and tell it that the
  // current target frame is actually where the mouseDown occurred, otherwise it
  // will use the frame the mouse is currently over which may or may not be
  // the same. (Note: saari and I have decided that we don't have to reset |mCurrentTarget|
  // when we're through because no one else is doing anything more with this
  // event and it will get reset on the very next event to the correct frame).
  mCurrentTarget = mGestureDownFrame;
  if ( mGestureDownFrame ) {
    nsIContent* lastContent = mGestureDownFrame->GetContent();

    if ( lastContent ) {
      // before dispatching, check that we're not on something that
      // doesn't get a context menu
      nsIAtom *tag = lastContent->Tag();
      PRBool allowedToDispatch = PR_TRUE;

      if (lastContent->IsContentOfType(nsIContent::eXUL)) {
        if (tag == nsXULAtoms::scrollbar ||
            tag == nsXULAtoms::scrollbarbutton ||
            tag == nsXULAtoms::button)
          allowedToDispatch = PR_FALSE;
        else if (tag == nsXULAtoms::toolbarbutton) {
          // a <toolbarbutton> that has the container attribute set
          // will already have its own dropdown.
          nsAutoString container;
          lastContent->GetAttr(kNameSpaceID_None, nsXULAtoms::container,
                               container);
          if (!container.IsEmpty())
            allowedToDispatch = PR_FALSE;
        }
      }
      else if (lastContent->IsContentOfType(nsIContent::eHTML)) {
        nsCOMPtr<nsIFormControl> formCtrl(do_QueryInterface(lastContent));

        if (formCtrl) {
          // of all form controls, only ones dealing with text are
          // allowed to have context menus
          PRInt32 type = formCtrl->GetType();

          allowedToDispatch = (type == NS_FORM_INPUT_TEXT ||
                               type == NS_FORM_INPUT_PASSWORD ||
                               type == NS_FORM_INPUT_FILE ||
                               type == NS_FORM_TEXTAREA);
        }
        else if (tag == nsHTMLAtoms::applet ||
                 tag == nsHTMLAtoms::embed  ||
                 tag == nsHTMLAtoms::object) {
          allowedToDispatch = PR_FALSE;
        }
      }

      if (allowedToDispatch) {
        // stop selection tracking, we're in control now
        nsCOMPtr<nsIFrameSelection> frameSel;
        GetSelection(mGestureDownFrame, mEventPresContext,
                     getter_AddRefs(frameSel));
        if (frameSel) {
          PRBool mouseDownState = PR_TRUE;
          frameSel->GetMouseDownState(&mouseDownState);
          if (mouseDownState)
            frameSel->SetMouseDownState(PR_FALSE);
        }
        
        // dispatch to DOM
        lastContent->HandleDOMEvent(mEventPresContext, &event, nsnull,
                                    NS_EVENT_FLAG_INIT, &status);

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
nsEventStateManager::BeginTrackingDragGesture(nsIPresContext* aPresContext,
                                              nsGUIEvent* inDownEvent,
                                              nsIFrame* inDownFrame)
{
  // Note that |inDownEvent| could be either a mouse down event or a
  // synthesized mouse move event.
  mIsTrackingDragGesture = PR_TRUE;
  mGestureDownPoint = inDownEvent->point;
  mGestureDownRefPoint = inDownEvent->refPoint;
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
nsEventStateManager::StopTrackingDragGesture()
{
  mIsTrackingDragGesture = PR_FALSE;
  mGestureDownPoint = nsPoint(0,0);
  mGestureDownRefPoint = nsPoint(0,0);
  mGestureDownFrame = nsnull;
}


//
// GetSelection
//
// Helper routine to get an nsIFrameSelection from the given frame
//
void
nsEventStateManager::GetSelection(nsIFrame* inFrame,
                                  nsIPresContext* inPresContext,
                                  nsIFrameSelection** outSelection)
{
  *outSelection = nsnull;
  
  if (inFrame) {
    nsCOMPtr<nsISelectionController> selCon;
    nsresult rv = inFrame->GetSelectionController(inPresContext, getter_AddRefs(selCon));

    if (NS_SUCCEEDED(rv) && selCon) {
      nsCOMPtr<nsIFrameSelection> frameSel;

      frameSel = do_QueryInterface(selCon);

      if (! frameSel) {
        nsIPresShell *shell = inPresContext->GetPresShell();
        if (shell)
          shell->GetFrameSelection(getter_AddRefs(frameSel));
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
nsEventStateManager::GenerateDragGesture(nsIPresContext* aPresContext,
                                         nsGUIEvent *aEvent)
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

    static PRInt32 pixelThresholdX = 0;
    static PRInt32 pixelThresholdY = 0;

    if (!pixelThresholdX) {
      nsILookAndFeel *lf = aPresContext->LookAndFeel();
      lf->GetMetric(nsILookAndFeel::eMetric_DragThresholdX, pixelThresholdX);
      lf->GetMetric(nsILookAndFeel::eMetric_DragThresholdY, pixelThresholdY);
      if (!pixelThresholdX)
        pixelThresholdX = 5;
      if (!pixelThresholdY)
        pixelThresholdY = 5;
    }

    // figure out the delta in twips, since that is how it is in the event.
    // Do we need to do this conversion every time?
    // Will the pres context really change on us or can we cache it?
    float pixelsToTwips;
    pixelsToTwips = aPresContext->DeviceContext()->DevUnitsToTwips();

    nscoord thresholdX = NSIntPixelsToTwips(pixelThresholdX, pixelsToTwips);
    nscoord thresholdY = NSIntPixelsToTwips(pixelThresholdY, pixelsToTwips);
 
    // fire drag gesture if mouse has moved enough
    if (abs(aEvent->point.x - mGestureDownPoint.x) > thresholdX ||
        abs(aEvent->point.y - mGestureDownPoint.y) > thresholdY) {
#ifdef CLICK_HOLD_CONTEXT_MENUS
      // stop the click-hold before we fire off the drag gesture, in case
      // it takes a long time
      KillClickHoldTimer();
#endif

      // get the widget from the target frame
      nsEventStatus status = nsEventStatus_eIgnore;
      nsMouseEvent event(NS_DRAGDROP_GESTURE, mGestureDownFrame->GetWindow());
      event.point = mGestureDownPoint;
      event.refPoint = mGestureDownRefPoint;
      // ideally, we should get the modifiers from the original event too, 
      // but the drag code looks at modifiers at the end of the drag, so this
      // is probably OK.
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

      if ( mGestureDownFrame ) {
        // Get the content for our synthesised event, not the mouse move event
        nsCOMPtr<nsIContent> lastContent;
        mGestureDownFrame->GetContentForEvent(aPresContext, &event, getter_AddRefs(lastContent));

        // Hold onto old target content through the event and reset after.
        nsCOMPtr<nsIContent> targetBeforeEvent = mCurrentTargetContent;
        // Set the current target to the content for the mouse down
        mCurrentTargetContent = lastContent;

        // Dispatch to DOM
        if ( lastContent )
          lastContent->HandleDOMEvent(aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
      
        // Firing the DOM event could have caused mGestureDownFrame to
        // be destroyed.  So, null-check it again.
        if ( mGestureDownFrame )
          mGestureDownFrame->HandleEvent(aPresContext, &event, &status);   

        // Reset mCurretTargetContent to what it was
        mCurrentTargetContent = targetBeforeEvent;
      }
      
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

  nsCOMPtr<nsPIDOMWindow> ourWindow = do_QueryInterface(gLastFocusedDocument->GetScriptGlobalObject());
  if(!ourWindow) return NS_ERROR_FAILURE;

  nsIDOMWindowInternal *rootWindow = ourWindow->GetPrivateRoot();
  if(!rootWindow) return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIDOMWindow> windowContent;
  rootWindow->GetContent(getter_AddRefs(windowContent));
  if(!windowContent) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMDocument> domDoc;
  windowContent->GetDocument(getter_AddRefs(domDoc));
  if(!domDoc) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  if(!doc) return NS_ERROR_FAILURE;

  nsIPresShell *presShell = doc->GetShellAt(0);
  if(!presShell) return NS_ERROR_FAILURE;
  nsCOMPtr<nsIPresContext> presContext;
  presShell->GetPresContext(getter_AddRefs(presContext));
  if(!presContext) return NS_ERROR_FAILURE;

  nsCOMPtr<nsISupports> pcContainer = presContext->GetContainer();
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

void
nsEventStateManager::DoScrollHistory(PRInt32 direction)
{
  nsCOMPtr<nsISupports> pcContainer(mPresContext->GetContainer());
  if (pcContainer) {
    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(pcContainer));
    if (webNav) {
      // positive direction to go back one step, nonpositive to go forward
      if (direction > 0)
        webNav->GoBack();
      else
        webNav->GoForward();
    }
  }
}

void
nsEventStateManager::DoScrollTextsize(nsIFrame *aTargetFrame,
                                      PRInt32 adjustment)
{
  // Exclude form controls and XUL content.
  nsIContent *content = aTargetFrame->GetContent();
  if (content &&
      !content->IsContentOfType(nsIContent::eHTML_FORM_CONTROL) &&
      !content->IsContentOfType(nsIContent::eXUL))
    {
      // positive adjustment to increase text size, non-positive to decrease
      ChangeTextSize((adjustment > 0) ? 1 : -1);
    }
}

//
nsresult
nsEventStateManager::DoScrollText(nsIPresContext* aPresContext,
                                  nsIFrame* aTargetFrame,
                                  nsInputEvent* aEvent,
                                  PRInt32 aNumLines,
                                  PRBool aScrollHorizontal,
                                  PRBool aScrollPage,
                                  PRBool aUseTargetFrame)
{
  nsCOMPtr<nsIContent> targetContent = aTargetFrame->GetContent();
  if (!targetContent)
    GetFocusedContent(getter_AddRefs(targetContent));
  if (!targetContent) return NS_OK;
  nsCOMPtr<nsIDOMDocumentEvent> targetDOMDoc(
                    do_QueryInterface(targetContent->GetDocument()));
  if (!targetDOMDoc) return NS_OK;

  nsCOMPtr<nsIDOMEvent> event;
  targetDOMDoc->CreateEvent(NS_LITERAL_STRING("MouseScrollEvents"), getter_AddRefs(event));
  if (event) {
    nsCOMPtr<nsIDOMMouseEvent> mouseEvent(do_QueryInterface(event));
    nsCOMPtr<nsIDOMDocumentView> docView = do_QueryInterface(targetDOMDoc);
    if (!docView) return NS_ERROR_FAILURE;
    nsCOMPtr<nsIDOMAbstractView> view;
    docView->GetDefaultView(getter_AddRefs(view));
    
    if (aScrollPage) {
      if (aNumLines > 0) {
        aNumLines = nsIDOMNSUIEvent::SCROLL_PAGE_DOWN;
      } else {
        aNumLines = nsIDOMNSUIEvent::SCROLL_PAGE_UP;
      }
    }

    mouseEvent->InitMouseEvent(NS_LITERAL_STRING("DOMMouseScroll"),
                               PR_TRUE, PR_TRUE, 
                               view, aNumLines,
                               aEvent->refPoint.x, aEvent->refPoint.y,
                               aEvent->point.x,    aEvent->point.y,
                               aEvent->isControl,  aEvent->isAlt,
                               aEvent->isShift,    aEvent->isMeta,
                               0, nsnull);
    PRBool allowDefault;
    nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(targetContent));
    if (target) {
      target->DispatchEvent(event, &allowDefault);
      if (!allowDefault)
        return NS_OK;
    }
  }

  nsIView* focusView = nsnull;
  nsIScrollableView* sv = nsnull;
  nsIFrame* focusFrame = nsnull;
  
  nsIPresShell *presShell = aPresContext->PresShell();

  // Otherwise, check for a focused content element
  nsCOMPtr<nsIContent> focusContent;
  if (mCurrentFocus) {
    GetFocusedFrame(&focusFrame);
  }
  else {
    // If there is no focused content, get the document content
    EnsureDocument(presShell);
    focusContent = mDocument->GetRootContent();
    if (!focusContent)
      return NS_ERROR_FAILURE;
  }
  
  if (aUseTargetFrame)
    focusFrame = aTargetFrame;
  else if (!focusFrame)
    presShell->GetPrimaryFrameFor(focusContent, &focusFrame);

  if (!focusFrame)
    return NS_ERROR_FAILURE;

  // Now check whether this frame wants to provide us with an
  // nsIScrollableView to use for scrolling.

  nsCOMPtr<nsIScrollableViewProvider> svp = do_QueryInterface(focusFrame);
  if (svp) {
    svp->GetScrollableView(aPresContext, &sv);
    if (sv)
      CallQueryInterface(sv, &focusView);
  } else {
    focusView = focusFrame->GetClosestView();
    if (!focusView)
      return NS_ERROR_FAILURE;
    
    sv = GetNearestScrollingView(focusView);
  }

  PRBool passToParent;
  if (sv) {
    // If we're already at the scroll limit for this view, scroll the
    // parent view instead.

    // If the view has a 0 line height, it will not scroll.
    nscoord lineHeight;
    sv->GetLineHeight(&lineHeight);

    if (lineHeight == 0) {
      passToParent = PR_TRUE;
    } else {
      nscoord xPos, yPos;
      sv->GetScrollPosition(xPos, yPos);

      if (aNumLines < 0) {
        passToParent = aScrollHorizontal ? (xPos <= 0) : (yPos <= 0);
      } else {
        nsSize scrolledSize;
        sv->GetContainerSize(&scrolledSize.width, &scrolledSize.height);

        nsIView* portView = nsnull;
        CallQueryInterface(sv, &portView);
        if (!portView)
          return NS_ERROR_FAILURE;
        nsRect portRect = portView->GetBounds();

        passToParent = (aScrollHorizontal ?
                        (xPos + portRect.width >= scrolledSize.width) :
                        (yPos + portRect.height >= scrolledSize.height));
      }
    }

    if (!passToParent) {
      PRInt32 scrollX = 0;
      PRInt32 scrollY = aNumLines;

      if (aScrollPage)
        scrollY = (scrollY > 0) ? 1 : -1;
      
      if (aScrollHorizontal)
        {
          scrollX = scrollY;
          scrollY = 0;
        }
      
      if (aScrollPage)
        sv->ScrollByPages(scrollX, scrollY);
      else
        sv->ScrollByLines(scrollX, scrollY);
      
      if (focusView)
        ForceViewUpdate(focusView);
    }
  } else
    passToParent = PR_TRUE;

  if (passToParent) {
    nsresult rv;
    nsIFrame* newFrame = nsnull;
    nsCOMPtr<nsIPresContext> newPresContext;

    rv = GetParentScrollingView(aEvent, aPresContext, newFrame,
                                *getter_AddRefs(newPresContext));
    if (NS_SUCCEEDED(rv) && newFrame)
      return DoScrollText(newPresContext, newFrame, aEvent, aNumLines,
                          aScrollHorizontal, aScrollPage, PR_TRUE);
    else
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
nsEventStateManager::GetParentScrollingView(nsInputEvent *aEvent,
                                            nsIPresContext* aPresContext,
                                            nsIFrame* &targetOuterFrame,
                                            nsIPresContext* &presCtxOuter)
{
  targetOuterFrame = nsnull;

  if (!aEvent) return NS_ERROR_FAILURE;
  if (!aPresContext) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocument> doc;
  aPresContext->PresShell()->GetDocument(getter_AddRefs(doc));

  NS_ASSERTION(doc, "No document in prescontext!");

  nsIDocument *parentDoc = doc->GetParentDocument();

  if (!parentDoc) {
    return NS_OK;
  }

  nsIPresShell *pPresShell = parentDoc->GetShellAt(0);
  NS_ENSURE_TRUE(pPresShell, NS_ERROR_FAILURE);

  /* now find the content node in our parent docshell's document that
     corresponds to our docshell */

  nsIContent *frameContent = parentDoc->FindContentForSubDocument(doc);
  NS_ENSURE_TRUE(frameContent, NS_ERROR_FAILURE);

  /*
    get this content node's frame, and use it as the new event target,
    so the event can be processed in the parent docshell.
    Note that we don't actually need to translate the event coordinates
    because they are not used by DoScrollText().
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

  SetFrameExternalReference(mCurrentTarget);

  switch (aEvent->message) {
  case NS_MOUSE_LEFT_BUTTON_DOWN:
  case NS_MOUSE_MIDDLE_BUTTON_DOWN:
  case NS_MOUSE_RIGHT_BUTTON_DOWN: 
    {
      if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN && !mNormalLMouseEventInProcess) {
        //Our state is out of whack.  We got a mouseup while still processing
        //the mousedown.  Kill View-level mouse capture or it'll stay stuck
        if (aView) {
          nsIViewManager* viewMan = aView->GetViewManager();
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
          const nsStyleUserInterface* ui = mCurrentTarget->GetStyleUserInterface();
          suppressBlur = (ui->mUserFocus == NS_STYLE_USER_FOCUS_IGNORE);
        }

        nsIFrame* currFrame = mCurrentTarget;
        nsIContent* activeContent = nsnull;
        if (mCurrentTarget)
          activeContent = mCurrentTarget->GetContent();

        // Look for the nearest enclosing focusable frame.
        while (currFrame) {
          // If the mousedown happened inside a popup, don't
          // try to set focus on one of its containing elements
          const nsStyleDisplay* display = currFrame->GetStyleDisplay();
          if (display->mDisplay == NS_STYLE_DISPLAY_POPUP) {
            newFocus = nsnull;
            break;
          }

          const nsStyleUserInterface* ui = currFrame->GetStyleUserInterface();
          if ((ui->mUserFocus != NS_STYLE_USER_FOCUS_IGNORE) &&
              (ui->mUserFocus != NS_STYLE_USER_FOCUS_NONE)) {
            newFocus = currFrame->GetContent();
            nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(newFocus));
            if (domElement)
              break;
          }
          currFrame = currFrame->GetParent();
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
            nsIContent* par = activeContent->GetParent();
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
      nsIPresShell *shell = aPresContext->GetPresShell();
      if (shell) {
        nsCOMPtr<nsIFrameSelection> frameSel;
        nsresult rv = shell->GetFrameSelection(getter_AddRefs(frameSel));
        if (NS_SUCCEEDED(rv) && frameSel){
            frameSel->SetMouseDownState(PR_FALSE);
        }
      }
    }
    break;
  case NS_MOUSE_SCROLL:
    if (nsEventStatus_eConsumeNoDefault != *aStatus) {

      // Build the preference keys, based on the event properties.
      nsMouseScrollEvent *msEvent = (nsMouseScrollEvent*) aEvent;

      NS_NAMED_LITERAL_CSTRING(prefbase,        "mousewheel");
      NS_NAMED_LITERAL_CSTRING(horizscroll,     ".horizscroll");
      NS_NAMED_LITERAL_CSTRING(withshift,       ".withshiftkey");
      NS_NAMED_LITERAL_CSTRING(withalt,         ".withaltkey");
      NS_NAMED_LITERAL_CSTRING(withcontrol,     ".withcontrolkey");
      NS_NAMED_LITERAL_CSTRING(withno,          ".withnokey");
      NS_NAMED_LITERAL_CSTRING(actionslot,      ".action");
      NS_NAMED_LITERAL_CSTRING(numlinesslot,    ".numlines");
      NS_NAMED_LITERAL_CSTRING(sysnumlinesslot, ".sysnumlines");

      nsCAutoString baseKey(prefbase);
      if (msEvent->scrollFlags & nsMouseScrollEvent::kIsHorizontal) {
        baseKey.Append(horizscroll);
      }
      if (msEvent->isShift) {
        baseKey.Append(withshift);
      } else if (msEvent->isControl) {
        baseKey.Append(withcontrol);
      } else if (msEvent->isAlt) {
        baseKey.Append(withalt);
      } else {
        baseKey.Append(withno);
      }

      // Extract the preferences
      nsCAutoString actionKey(baseKey);
      actionKey.Append(actionslot);

      nsCAutoString sysNumLinesKey(baseKey);
      sysNumLinesKey.Append(sysnumlinesslot);

      PRInt32 action = nsContentUtils::GetIntPref(actionKey.get());
      PRInt32 numLines = 0;
      PRBool useSysNumLines =
        nsContentUtils::GetBoolPref(sysNumLinesKey.get());

      if (useSysNumLines) {
        numLines = msEvent->delta;
        if (msEvent->scrollFlags & nsMouseScrollEvent::kIsFullPage)
          action = MOUSE_SCROLL_PAGE;
      }
      else
        {
          // If the scroll event's delta isn't to our liking, we can
          // override it with the "numlines" parameter.  There are two
          // things we can do:
          //
          // (1) Pick a different number.  Instead of scrolling 3
          //     lines ("delta" in Gtk2), we would scroll 1 line.
          // (2) Swap directions.  Instead of scrolling down, scroll up.
          //
          // For the first item, the magnitude of the parameter is
          // used instead of the magnitude of the delta.  For the
          // second item, if the parameter is negative we swap
          // directions.

          nsCAutoString numLinesKey(baseKey);
          numLinesKey.Append(numlinesslot);

          numLines = nsContentUtils::GetIntPref(numLinesKey.get());

          bool swapDirs = (numLines < 0);
          PRInt32 userSize = swapDirs ? -numLines : numLines;

          PRInt32 deltaUp = (msEvent->delta < 0);
          if (swapDirs) {
            deltaUp = ! deltaUp;
          }

          numLines = deltaUp ? -userSize : userSize;
        }

      switch (action) {
      case MOUSE_SCROLL_N_LINES:
      case MOUSE_SCROLL_PAGE:
        {
          DoScrollText(aPresContext, aTargetFrame, msEvent, numLines,
                       (msEvent->scrollFlags & nsMouseScrollEvent::kIsHorizontal),
                       (action == MOUSE_SCROLL_PAGE), PR_FALSE);
        }

        break;

      case MOUSE_SCROLL_HISTORY:
        {
          DoScrollHistory(numLines);
        }
        break;

      case MOUSE_SCROLL_TEXTSIZE:
        {
          DoScrollTextsize(aTargetFrame, numLines);
        }
        break;

      default:  // Including -1 (do nothing)
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
                sv->ScrollByPages(0, (keyEvent->keyCode != NS_VK_PAGE_UP) ? 1 : -1);
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
                nsIViewManager* vm = aView->GetViewManager();
                if (vm) {
                  // I'd use Composite here, but it doesn't always work.
                  // vm->Composite();
                  vm->ForceUpdate();
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
                nsIViewManager* vm = aView->GetViewManager();
                if (vm) {
                  // I'd use Composite here, but it doesn't always work.
                  // vm->Composite();
                  vm->ForceUpdate();
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
                  sv->ScrollByPages(0, 1);
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
        pcContainer = mPresContext->GetContainer();
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
    StopTrackingDragGesture();
 #if CLICK_HOLD_CONTEXT_MENUS
    mEventDownWidget = nsnull;
    mEventPresContext = nsnull;
#endif
  }
  if (aFrame == mCurrentTarget) {
    if (aFrame) {
      mCurrentTargetContent = aFrame->GetContent();
    }
    mCurrentTarget = nsnull;
  }
  if (aFrame == mCurrentFocusFrame)
    mCurrentFocusFrame = nsnull;
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

  nsIView* parent = aView->GetParent();

  if (parent) {
    return GetNearestScrollingView(parent);
  }

  return nsnull;
}

PRBool
nsEventStateManager::CheckDisabled(nsIContent* aContent)
{
  nsIAtom *tag = aContent->Tag();

  if (((tag == nsHTMLAtoms::input    ||
        tag == nsHTMLAtoms::select   ||
        tag == nsHTMLAtoms::textarea ||
        tag == nsHTMLAtoms::button) &&
       (aContent->IsContentOfType(nsIContent::eHTML))) ||
      (tag == nsHTMLAtoms::button &&
       aContent->IsContentOfType(nsIContent::eXUL))) {
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
    nsIContent* targetContent = nsnull;
    if (mCurrentTarget) {
      targetContent = mCurrentTarget->GetContent();
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
  nsCOMPtr<nsISupports> pcContainer = aPresContext->GetContainer();
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
    SetCursor(cursor, aTargetFrame->GetWindow(), PR_FALSE);
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
  case NS_STYLE_CURSOR_MOZ_ZOOM_IN:
    c = eCursor_zoom_in;
    break;
  case NS_STYLE_CURSOR_MOZ_ZOOM_OUT:
    c = eCursor_zoom_out;
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
                                        nsIFrame*& aTargetFrame,
                                        nsIContent* aRelatedContent)
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event(aMessage, aEvent->widget);
  event.point = aEvent->point;
  event.refPoint = aEvent->refPoint;
  event.isShift = ((nsMouseEvent*)aEvent)->isShift;
  event.isControl = ((nsMouseEvent*)aEvent)->isControl;
  event.isAlt = ((nsMouseEvent*)aEvent)->isAlt;
  event.isMeta = ((nsMouseEvent*)aEvent)->isMeta;
  event.nativeMsg = ((nsMouseEvent*)aEvent)->nativeMsg;

  mCurrentTargetContent = aTargetContent;
  mCurrentRelatedContent = aRelatedContent;

  BeforeDispatchEvent();
  CurrentEventShepherd shepherd(this, &event);
  if (aTargetContent) {
    aTargetContent->HandleDOMEvent(aPresContext, &event, nsnull,
                                   NS_EVENT_FLAG_INIT, &status); 
    // If frame refs were cleared, we need to re-resolve the frame
    if (mClearedFrameRefsDuringEvent) {
      nsIPresShell *shell = aPresContext->GetPresShell();
      if (shell) {
        shell->GetPrimaryFrameFor(aTargetContent, &aTargetFrame);
      } else {
        aTargetFrame = nsnull;
      }
    }
  }
  if (aTargetFrame) {
    aTargetFrame->HandleEvent(aPresContext, &event, &status);   
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
  // If this is the first event in this window then mDocument might not be set
  // yet.  Call EnsureDocument to set it.
  EnsureDocument(aPresContext);
  nsIDocument *parentDoc = mDocument->GetParentDocument();
  if (parentDoc) {
    nsIContent *docContent = parentDoc->FindContentForSubDocument(mDocument);
    if (docContent) {
      if (docContent->Tag() == nsHTMLAtoms::iframe) {
        // We're an IFRAME.  Send an event to our IFRAME tag.
        nsIPresShell *parentShell = parentDoc->GetShellAt(0);
        if (parentShell) {
          nsEventStatus status = nsEventStatus_eIgnore;
          nsMouseEvent event(aMessage, aEvent->widget);
          event.point = aEvent->point;
          event.refPoint = aEvent->refPoint;
          event.isShift = ((nsMouseEvent*)aEvent)->isShift;
          event.isControl = ((nsMouseEvent*)aEvent)->isControl;
          event.isAlt = ((nsMouseEvent*)aEvent)->isAlt;
          event.isMeta = ((nsMouseEvent*)aEvent)->isMeta;
          event.nativeMsg = ((nsMouseEvent*)aEvent)->nativeMsg;

          CurrentEventShepherd shepherd(this, &event);
          parentShell->HandleDOMEventWithTarget(docContent, &event, &status);
        }
      }
    }
  }
}


void
nsEventStateManager::GenerateMouseEnterExit(nsIPresContext* aPresContext,
                                            nsGUIEvent* aEvent)
{
  // Hold onto old target content through the event and reset after.
  nsCOMPtr<nsIContent> targetBeforeEvent = mCurrentTargetContent;

  switch(aEvent->message) {
  case NS_MOUSE_MOVE:
    {
      // Get the target content target (mousemove target == mouseover target)
      nsCOMPtr<nsIContent> targetElement;
      GetEventTargetContent(aEvent, getter_AddRefs(targetElement));
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
          // frame may have changed during the call; make sure bit is set
          if (mLastMouseOverFrame) {
            SetFrameExternalReference(mLastMouseOverFrame);
          }

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
      
        // Store the first mouseOver event we fire and don't refire mouseOver
        // to that element while the first mouseOver is still ongoing.
        mFirstMouseOverEventElement = targetElement;

        if (targetElement) {
          SetContentState(targetElement, NS_EVENT_STATE_HOVER);
        }

        // Fire mouseover
        nsIFrame* targetFrame = nsnull;
        GetEventTarget(&targetFrame);
        DispatchMouseEvent(aPresContext, aEvent, NS_MOUSE_ENTER_SYNTH,
                           targetElement, targetFrame, mLastMouseOverElement);

        mLastMouseOverFrame = targetFrame;
        // This may be a different frame than the one we started with, so we
        // need to ensure it has its external reference bit set.
        if (mLastMouseOverFrame) {
          SetFrameExternalReference(mLastMouseOverFrame);
        }
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
nsEventStateManager::GenerateDragDropEnterExit(nsIPresContext* aPresContext,
                                               nsGUIEvent* aEvent)
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
          nsMouseEvent event(NS_DRAGDROP_EXIT_SYNTH, aEvent->widget);
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
        nsMouseEvent event(NS_DRAGDROP_ENTER, aEvent->widget);
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
        nsMouseEvent event(NS_DRAGDROP_EXIT_SYNTH, aEvent->widget);
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
  PRUint32 eventMsg = 0;
  PRInt32 flags = NS_EVENT_FLAG_INIT;

  //If mouse is still over same element, clickcount will be > 1.
  //If it has moved it will be zero, so no click.
  if (0 != aEvent->clickCount) {
    //fire click
    switch (aEvent->message) {
    case NS_MOUSE_LEFT_BUTTON_UP:
      eventMsg = NS_MOUSE_LEFT_CLICK;
      break;
    case NS_MOUSE_MIDDLE_BUTTON_UP:
      eventMsg = NS_MOUSE_MIDDLE_CLICK;
      flags |= sLeftClickOnly ? NS_EVENT_FLAG_NO_CONTENT_DISPATCH : NS_EVENT_FLAG_NONE;
      break;
    case NS_MOUSE_RIGHT_BUTTON_UP:
      eventMsg = NS_MOUSE_RIGHT_CLICK;
      flags |= sLeftClickOnly ? NS_EVENT_FLAG_NO_CONTENT_DISPATCH : NS_EVENT_FLAG_NONE;
      break;
    }

    nsMouseEvent event(eventMsg, aEvent->widget);
    event.point = aEvent->point;
    event.refPoint = aEvent->refPoint;
    event.clickCount = aEvent->clickCount;
    event.isShift = aEvent->isShift;
    event.isControl = aEvent->isControl;
    event.isAlt = aEvent->isAlt;
    event.isMeta = aEvent->isMeta;

    nsCOMPtr<nsIPresShell> presShell = mPresContext->GetPresShell();
    if (presShell) {
      nsCOMPtr<nsIContent> mouseContent;
      GetEventTargetContent(aEvent, getter_AddRefs(mouseContent));

      ret = presShell->HandleEventWithTarget(&event, mCurrentTarget, mouseContent, flags, aStatus);
      if (NS_SUCCEEDED(ret) && aEvent->clickCount == 2) {
        eventMsg = 0;
        //fire double click
        switch (aEvent->message) {
        case NS_MOUSE_LEFT_BUTTON_UP:
          eventMsg = NS_MOUSE_LEFT_DOUBLECLICK;
          break;
        case NS_MOUSE_MIDDLE_BUTTON_UP:
          eventMsg = NS_MOUSE_MIDDLE_DOUBLECLICK;
          break;
        case NS_MOUSE_RIGHT_BUTTON_UP:
          eventMsg = NS_MOUSE_RIGHT_DOUBLECLICK;
          break;
        }
        
        nsMouseEvent event2(eventMsg, aEvent->widget);
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
nsEventStateManager::ChangeFocus(nsIContent* aFocusContent,
                                 PRInt32 aFocusedWith)
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

  nsCOMPtr<nsIDOMWindowInternal> domwin(do_QueryInterface(doc->GetScriptGlobalObject));

  nsCOMPtr<nsIWidget> widget;
  nsIViewManager* vm = presShell->GetViewManager();
  if (vm) {
    vm->GetWidget(getter_AddRefs(widget));
  }

  printf("DS %p  Type %s  Cnt %d  Doc %p  DW %p  EM %p\n", 
    parentAsDocShell.get(), 
    type==nsIDocShellTreeItem::typeChrome?"Chrome":"Content", 
    childWebshellCount, doc.get(), domwin.get(),
    presContext->EventStateManager());

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
  printf("[%p] ShiftFocusInternal: aForward=%d, aStart=%p, mCurrentFocus=%p\n",
         this, aForward, aStart, mCurrentFocus.get());
#endif
  NS_ASSERTION(mPresContext, "no pres context");
  EnsureDocument(mPresContext);
  NS_ASSERTION(mDocument, "no document");

  nsCOMPtr<nsIContent> rootContent = mDocument->GetRootContent();

  nsCOMPtr<nsISupports> pcContainer = mPresContext->GetContainer();
  NS_ASSERTION(pcContainer, "no container for presContext");

  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(pcContainer));
  PRBool docHasFocus = PR_FALSE;

  // ignoreTabIndex allows the user to tab to the next link after clicking before it link in the page
  // or using find text to get to the link. Without ignoreTabIndex in those cases, pages that 
  // use tabindex would still enforce that order in those situations.
  PRBool ignoreTabIndex = PR_FALSE;

  if (!aStart && !mCurrentFocus) {  
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
  nsIPresShell *presShell = mPresContext->PresShell();

  // We might use the selection position, rather than mCurrentFocus, as our position to shift focus from
  PRInt32 itemType;
  nsCOMPtr<nsIDocShellTreeItem> shellItem(do_QueryInterface(docShell));
  shellItem->GetItemType(&itemType);
  
  // Tab from the selection if it exists, but not if we're in chrome or an explicit starting
  // point was given.
  if (!aStart && itemType != nsIDocShellTreeItem::typeChrome &&
      mLastFocusedWith != eEventFocusedByMouse) {
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

  nsIContent *startContent = nsnull;

  if (aStart) {
    presShell->GetPrimaryFrameFor(aStart, &curFocusFrame);

    // If there is no frame, we can't navigate from this content node, and we
    // fall back to navigating from the document root.
    if (curFocusFrame)
      startContent = aStart;
  } else if (selectionFrame) {
    // We moved focus to the caret location above, so mCurrentFocus
    // reflects the starting content node.
    startContent = mCurrentFocus;
    curFocusFrame = selectionFrame;
  } else if (!docHasFocus) {
    startContent = mCurrentFocus;
    GetFocusedFrame(&curFocusFrame);
  }

  if (aStart) {
    TabIndexFrom(aStart, &mCurrentTabIndex);
  } else if (!mCurrentFocus) {  // Get tabindex ready
    if (aForward) {
      mCurrentTabIndex = docHasFocus && selectionFrame ? 0 : 1;
    } else if (!docHasFocus) {
      mCurrentTabIndex = 0;
    } else if (selectionFrame) {
      mCurrentTabIndex = 1;   // will keep it from wrapping around to end
    }
  }

  nsCOMPtr<nsIContent> nextFocus;
  nsIFrame* nextFocusFrame;
  if (aForward || !docHasFocus || selectionFrame)
    GetNextTabbableContent(rootContent, startContent, curFocusFrame,
                           aForward, ignoreTabIndex,
                           getter_AddRefs(nextFocus), &nextFocusFrame);

  // Clear out mCurrentTabIndex. It has a garbage value because of GetNextTabbableContent()'s side effects
  // It will be set correctly when focus is changed via ChangeFocus()
  mCurrentTabIndex = 0; 

  if (nextFocus) {
    // Check to see if the next focused element has a subshell.
    // This is the case for an IFRAME or FRAME element.  If it
    // does, we send focus into the subshell.

    nsCOMPtr<nsIDocShell> sub_shell;
    nsCOMPtr<nsIDocument> doc = nextFocus->GetDocument();

    if (doc) {
      nsIDocument *sub_doc = doc->GetSubDocumentFor(nextFocus);

      if (sub_doc) {
        nsCOMPtr<nsISupports> container = sub_doc->GetContainer();
        sub_shell = do_QueryInterface(container);
      }
    }

    if (sub_shell) {
      SetContentState(nsnull, NS_EVENT_STATE_FOCUS);

      presShell->ScrollFrameIntoView(nextFocusFrame,
                                     NS_PRESSHELL_SCROLL_ANYWHERE,
                                     NS_PRESSHELL_SCROLL_ANYWHERE);
      
      // if we are in the middle of tabbing into 
      // sub_shell, bail out, to avoid recursion
      // see bug #195011 and bug #137191
      if (mTabbingFromDocShells.IndexOf(sub_shell) != -1)
        return NS_OK;

      TabIntoDocument(sub_shell, aForward); 
    } else {
      // there is no subshell, so just focus nextFocus
#ifdef DEBUG_DOCSHELL_FOCUS
      printf("focusing next focusable content: %p\n", nextFocus.get());
#endif
      mCurrentTarget = nextFocusFrame;

      //This may be new frame that hasn't been through the ESM so we
      //must set its NS_FRAME_EXTERNAL_REFERENCE bit.
      if (mCurrentTarget)
        SetFrameExternalReference(mCurrentTarget);

      nsCOMPtr<nsIContent> oldFocus(mCurrentFocus);
      ChangeFocus(nextFocus, eEventFocusedByKey);
      if (!mCurrentFocus && oldFocus) {
        // ChangeFocus failed to move focus to nextFocus because a blur handler
        // made it unfocusable. (bug #118685)
        // Try again unless it's from the same point, bug 232368.
        if (oldFocus != aStart) {
          mCurrentTarget = nsnull;
          return ShiftFocusInternal(aForward, oldFocus);
        } else {
          return NS_OK;
        }
      } else {
        if (mCurrentFocus != nextFocus) {
          // A focus or blur handler switched the focus from one of
          // its focus/blur/change handlers, don't mess with what the
          // page wanted...

          return NS_OK;
        }
        GetFocusedFrame(&mCurrentTarget);
        if (mCurrentTarget)
          SetFrameExternalReference(mCurrentTarget);
      }

      // It's possible that the act of removing focus from our previously
      // focused element caused nextFocus to be removed from the document.
      // In this case, we can restart the frame traversal from our previously
      // focused content.

      if (oldFocus && doc != nextFocus->GetDocument()) {
        mCurrentTarget = nsnull;
        return ShiftFocusInternal(aForward, oldFocus);
      }

      if (!docHasFocus)
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
      SetFocusedContent(rootContent);
      MoveCaretToFocus();
      SetFocusedContent(nsnull);

    } else {
      // If there's nothing left to focus in this document,
      // pop out to our parent document, and have it shift focus
      // in the same direction starting at the content element
      // corresponding to our docshell.
      // Guard against infinite recursion (see explanation in ShiftFocus)

      if (mTabbedThroughDocument)
        return NS_OK;

      SetFocusedContent(rootContent);
      mCurrentTabIndex = 0;
      MoveCaretToFocus();
      SetFocusedContent(nsnull);

      mTabbedThroughDocument = PR_TRUE;

      nsCOMPtr<nsIDocShellTreeItem> treeItem = do_QueryInterface(pcContainer);
      nsCOMPtr<nsIDocShellTreeItem> treeParent;
      treeItem->GetParent(getter_AddRefs(treeParent));
      if (treeParent) {
        nsCOMPtr<nsIDocShell> parentDS = do_QueryInterface(treeParent);
        if (parentDS) {
          nsCOMPtr<nsIPresShell> parentShell;
          parentDS->GetPresShell(getter_AddRefs(parentShell));

          nsCOMPtr<nsIDocument> parent_doc;
          parentShell->GetDocument(getter_AddRefs(parent_doc));

          nsIContent *docContent = parent_doc->FindContentForSubDocument(mDocument);

          nsCOMPtr<nsIPresContext> parentPC;
          parentShell->GetPresContext(getter_AddRefs(parentPC));

          nsIEventStateManager *parentESM = parentPC->EventStateManager();

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

          SetFocusedContent(nsnull);
          docShell->SetHasFocus(PR_FALSE);
          ShiftFocusInternal(aForward);
        }
      }
    }
  }

  return NS_OK;
}

void
nsEventStateManager::TabIndexFrom(nsIContent *aFrom, PRInt32 *aOutIndex)
{
  if (aFrom->IsContentOfType(nsIContent::eHTML)) {
    nsIAtom *tag = aFrom->Tag();

    if (tag != nsHTMLAtoms::a &&
        tag != nsHTMLAtoms::area &&
        tag != nsHTMLAtoms::button &&
        tag != nsHTMLAtoms::input &&
        tag != nsHTMLAtoms::object &&
        tag != nsHTMLAtoms::select &&
        tag != nsHTMLAtoms::textarea)
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

PRBool
nsEventStateManager::HasFocusableAncestor(nsIFrame *aFrame)
{
  // This method helps prevent a situation where a link or other element
  // with -moz-user-focus is focused twice, because the parent link
  // would get focused, and all of the children also get focus.

  nsIFrame *ancestorFrame = aFrame;
  while ((ancestorFrame = ancestorFrame->GetParent()) != nsnull) {
    nsIContent *ancestorContent = ancestorFrame->GetContent();
    if (!ancestorContent) {
      break;
    }
    nsIAtom *ancestorTag = ancestorContent->Tag();
    if (ancestorTag == nsHTMLAtoms::frame || ancestorTag == nsHTMLAtoms::iframe) {
      break; // The only focusable containers that can also have focusable children
    }
    // Any other parent that's focusable can't have focusable children
    const nsStyleUserInterface *ui = ancestorFrame->GetStyleUserInterface();
    if (ui->mUserFocus != NS_STYLE_USER_FOCUS_IGNORE &&
        ui->mUserFocus != NS_STYLE_USER_FOCUS_NONE) {
      // Inside a focusable parent -- let parent get focus
      // instead of child (to avoid links within links etc.)
      return PR_TRUE;   
    }
  }
  return PR_FALSE;
}

nsresult
nsEventStateManager::GetNextTabbableContent(nsIContent* aRootContent,
                                            nsIContent* aStartContent,
                                            nsIFrame* aStartFrame, 
                                            PRBool forward,
                                            PRBool aIgnoreTabIndex, 
                                            nsIContent** aResultNode,
                                            nsIFrame** aResultFrame)
{
  *aResultNode = nsnull;
  *aResultFrame = nsnull;
  PRBool keepFirstFrame = PR_FALSE;
  PRBool findLastFrame = PR_FALSE;

  nsCOMPtr<nsIBidirectionalEnumerator> frameTraversal;

  if (!aStartFrame) {
    //No frame means we need to start with the root content again.
    if (mPresContext) {
      nsIFrame* result = nsnull;
      nsIPresShell *presShell = mPresContext->GetPresShell();
      if (presShell) {
        presShell->GetPrimaryFrameFor(aRootContent, &result);
      }

      aStartFrame = result;

      if (!forward)
        findLastFrame = PR_TRUE;
    }
    if (!aStartFrame) {
      return NS_ERROR_FAILURE;
    }
    keepFirstFrame = PR_TRUE;
  }

  // Need to do special check in case we're in an imagemap which has multiple content per frame
  if (aStartContent) {
    if (aStartContent->Tag() == nsHTMLAtoms::area &&
        aStartContent->IsContentOfType(nsIContent::eHTML)) {
      // We're starting from an imagemap area, so don't skip over the starting frame.
      keepFirstFrame = PR_TRUE;
    }
  }

  nsresult result;
  nsCOMPtr<nsIFrameTraversal> trav(do_CreateInstance(kFrameTraversalCID,&result));
  if (NS_FAILED(result))
    return result;

  result = trav->NewFrameTraversal(getter_AddRefs(frameTraversal), FOCUS,
                                   mPresContext, aStartFrame);
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
    const nsStyleVisibility* vis = currentFrame->GetStyleVisibility();
    const nsStyleUserInterface* ui = currentFrame->GetStyleUserInterface();

    PRBool viewShown = currentFrame->AreAncestorViewsVisible();

    nsIContent* child = currentFrame->GetContent();
    nsCOMPtr<nsIDOMElement> element(do_QueryInterface(child));

    // if collapsed or hidden, we don't get tabbed into.
    if (viewShown &&
        (vis->mVisible != NS_STYLE_VISIBILITY_COLLAPSE) &&
        (vis->mVisible != NS_STYLE_VISIBILITY_HIDDEN) && 
        (ui->mUserFocus != NS_STYLE_USER_FOCUS_IGNORE) &&
        (ui->mUserFocus != NS_STYLE_USER_FOCUS_NONE) && element) {
      PRInt32 tabIndex = -1;
      PRBool disabled = PR_TRUE;
      PRBool hidden = PR_FALSE;

      nsIAtom *tag = child->Tag();
      if (child->IsContentOfType(nsIContent::eHTML)) {
        if (tag == nsHTMLAtoms::input) {
          nsCOMPtr<nsIDOMHTMLInputElement> nextInput(do_QueryInterface(child));
          if (nextInput) {
            nextInput->GetDisabled(&disabled);
            nextInput->GetTabIndex(&tabIndex);

            // The type attribute is unreliable; use the actual input type
            nsCOMPtr<nsIFormControl> formControl(do_QueryInterface(child));
            NS_ASSERTION(formControl, "DOMHTMLInputElement must QI to nsIFormControl!");
            switch (formControl->GetType()) {
              case NS_FORM_INPUT_TEXT:
              case NS_FORM_INPUT_PASSWORD:
                disabled = PR_FALSE;
                break;
              case NS_FORM_INPUT_HIDDEN:
                hidden = PR_TRUE;
                break;
              case NS_FORM_INPUT_FILE:
                disabled = PR_TRUE;
                break;
              default:
                disabled =
                  disabled || !(sTabFocusModel & eTabFocus_formElementsMask);
                break;
            }
          }
        }
        else if (tag == nsHTMLAtoms::select) {
          // Select counts as form but not as text
          disabled = !(sTabFocusModel & eTabFocus_formElementsMask);
          if (!disabled) {
            nsCOMPtr<nsIDOMHTMLSelectElement> nextSelect(do_QueryInterface(child));
            if (nextSelect) {
              nextSelect->GetDisabled(&disabled);
              nextSelect->GetTabIndex(&tabIndex);
            }
          }
        }
        else if (tag == nsHTMLAtoms::textarea) {
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
        else if (tag == nsHTMLAtoms::a) {
          // it's a link
          disabled = !(sTabFocusModel & eTabFocus_linksMask);
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
        else if (tag == nsHTMLAtoms::button) {
          // Button counts as a form element but not as text
          disabled = !(sTabFocusModel & eTabFocus_formElementsMask);
          if (!disabled) {
            nsCOMPtr<nsIDOMHTMLButtonElement> nextButton(do_QueryInterface(child));
            if (nextButton) {
              nextButton->GetTabIndex(&tabIndex);
              nextButton->GetDisabled(&disabled);
            }
          }
        }
        else if (tag == nsHTMLAtoms::img) {
          PRBool hasImageMap = PR_FALSE;
          // Don't need to set disabled here, because if we
          // match an imagemap, we'll return from there.
          nsCOMPtr<nsIDOMHTMLImageElement> nextImage(do_QueryInterface(child));
          nsAutoString usemap;
          if (nextImage) {
            nsCOMPtr<nsIDocument> doc = child->GetDocument();
            if (doc) {
              nextImage->GetAttribute(NS_LITERAL_STRING("usemap"), usemap);
              nsCOMPtr<nsIDOMHTMLMapElement> imageMap = nsImageMapUtils::FindImageMap(doc,usemap);
              if (imageMap) {
                hasImageMap = PR_TRUE;
                if (sTabFocusModel & eTabFocus_linksMask) {
                  nsCOMPtr<nsIContent> map(do_QueryInterface(imageMap));
                  if (map) {
                    nsIContent *childArea;
                    PRUint32 index, count = map->GetChildCount();
                    // First see if mCurrentFocus is in this map
                    for (index = 0; index < count; index++) {
                      childArea = map->GetChildAt(index);
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
                    PRInt32 increment = forward ? 1 : -1;
                    // In the following two lines we might substract 1 from zero,
                    // the |index < count| loop condition will be false in that case too.
                    index = index < count ? index + increment : (forward ? 0 : count - 1);
                    for (; index < count; index += increment) {
                      //Iterate over the children.
                      childArea = map->GetChildAt(index);

                      //Got the map area, check its tabindex.
                      PRInt32 val = 0;
                      TabIndexFrom(childArea, &val);
                      if (mCurrentTabIndex == val) {
                        //tabindex == the current one, use it.
                        *aResultNode = childArea;
                        NS_IF_ADDREF(*aResultNode);
                        *aResultFrame = currentFrame;
                        return NS_OK;
                      }
                    }
                  }
                }
              }
            }
          }

          // Might be using -moz-user-focus and imitating a control.
          // If already inside link, -moz-user-focus'd element, or already has 
          // image map with tab stops, then don't use the
          // image frame itself as a tab stop.
          disabled = HasFocusableAncestor(currentFrame) || hasImageMap ||
                     !(sTabFocusModel & eTabFocus_formElementsMask);
        }
        else if (tag == nsHTMLAtoms::object) {
          // OBJECT is treated as a form element.
          disabled = !(sTabFocusModel & eTabFocus_formElementsMask);
          if (!disabled) {
            nsCOMPtr<nsIDOMHTMLObjectElement> nextObject(do_QueryInterface(child));
            if (nextObject) 
              nextObject->GetTabIndex(&tabIndex);
            disabled = PR_FALSE;
          }
        }
        else if (tag == nsHTMLAtoms::iframe || tag == nsHTMLAtoms::frame) {
          disabled = PR_TRUE;
          if (child) {
            nsCOMPtr<nsIDocument> doc = child->GetDocument();
            if (doc) {
              nsIDocument *subDoc = doc->GetSubDocumentFor(child);
              if (subDoc) {
                nsCOMPtr<nsISupports> container = subDoc->GetContainer();
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
          // Let any HTML element with -moz-user-focus: normal be tabbable
          // Without this rule, DHTML form controls made from div or span
          // cannot be put in the tab order.
          disabled = !(sTabFocusModel & eTabFocus_formElementsMask) ||
            HasFocusableAncestor(currentFrame);
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
        if (!value.EqualsLiteral("true")) {
          nsCOMPtr<nsIDOMXULControlElement> control(do_QueryInterface(child));
          if (control)
            control->GetDisabled(&disabled);
          else
            disabled = PR_FALSE;
        }
      }
      
      //TabIndex not set (-1) treated at same level as set to 0
      tabIndex = tabIndex < 0 ? 0 : tabIndex;

      if (!disabled && !hidden && (aIgnoreTabIndex ||
                                   mCurrentTabIndex == tabIndex) &&
          child != aStartContent) {
        *aResultNode = child;
        NS_IF_ADDREF(*aResultNode);
        *aResultFrame = currentFrame;
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
  return GetNextTabbableContent(aRootContent, aStartContent, nsnull, forward,
                                aIgnoreTabIndex, aResultNode, aResultFrame);
}

PRInt32
nsEventStateManager::GetNextTabIndex(nsIContent* aParent, PRBool forward)
{
  PRInt32 tabIndex, childTabIndex;
  nsIContent *child;

  PRUint32 count = aParent->GetChildCount();
 
  if (forward) {
    tabIndex = 0;
    for (PRUint32 index = 0; index < count; index++) {
      child = aParent->GetChildAt(index);
      childTabIndex = GetNextTabIndex(child, forward);
      if (childTabIndex > mCurrentTabIndex && childTabIndex != tabIndex) {
        tabIndex = (tabIndex == 0 || childTabIndex < tabIndex) ? childTabIndex : tabIndex; 
      }
      
      nsAutoString tabIndexStr;
      child->GetAttr(kNameSpaceID_None, nsHTMLAtoms::tabindex, tabIndexStr);
      PRInt32 ec, val = tabIndexStr.ToInteger(&ec);
      if (NS_SUCCEEDED (ec) && val > mCurrentTabIndex && val != tabIndex) {
        tabIndex = (tabIndex == 0 || val < tabIndex) ? val : tabIndex; 
      }
    }
  } 
  else { /* !forward */
    tabIndex = 1;
    for (PRUint32 index = 0; index < count; index++) {
      child = aParent->GetChildAt(index);
      childTabIndex = GetNextTabIndex(child, forward);
      if ((mCurrentTabIndex == 0 && childTabIndex > tabIndex) ||
          (childTabIndex < mCurrentTabIndex && childTabIndex > tabIndex)) {
        tabIndex = childTabIndex;
      }
      
      nsAutoString tabIndexStr;
      child->GetAttr(kNameSpaceID_None, nsHTMLAtoms::tabindex, tabIndexStr);
      PRInt32 ec, val = tabIndexStr.ToInteger(&ec);
      if (NS_SUCCEEDED (ec)) {
        if ((mCurrentTabIndex == 0 && val > tabIndex) ||
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
    if (mPresContext) {
      nsIPresShell *shell = mPresContext->GetPresShell();
      if (shell) {
        shell->GetPrimaryFrameFor(mCurrentTargetContent, &mCurrentTarget);

        //This may be new frame that hasn't been through the ESM so we
        //must set its NS_FRAME_EXTERNAL_REFERENCE bit.
        if (mCurrentTarget)
          SetFrameExternalReference(mCurrentTarget);
      }
    }
  }

  if (!mCurrentTarget) {
    nsIPresShell *presShell = mPresContext->GetPresShell();
    if (presShell) {
      presShell->GetEventTargetFrame(&mCurrentTarget);

      //This may be new frame that hasn't been through the ESM so we
      //must set its NS_FRAME_EXTERNAL_REFERENCE bit.
      if (mCurrentTarget)
        SetFrameExternalReference(mCurrentTarget);
    }
  }

  *aFrame = mCurrentTarget;
  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::GetEventTargetContent(nsEvent* aEvent,
                                           nsIContent** aContent)
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

  *aContent = nsnull;

  nsIPresShell *presShell = mPresContext->GetPresShell();
  if (presShell) {
    presShell->GetEventTargetContent(aEvent, aContent);
  }

  // Some events here may set mCurrentTarget but not set the corresponding
  // event target in the PresShell.
  if (!*aContent && mCurrentTarget) {
    mCurrentTarget->GetContentForEvent(mPresContext, aEvent, aContent);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::GetEventRelatedContent(nsIContent** aContent)
{
  *aContent = mCurrentRelatedContent;
  NS_IF_ADDREF(*aContent);
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
  for (nsIContent* hoverContent = mHoverContent; hoverContent;
       hoverContent = hoverContent->GetParent()) {
    if (aContent == hoverContent) {
      aState |= NS_EVENT_STATE_HOVER;
      break;
    }
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
    const nsStyleUserInterface* ui = mCurrentTarget->GetStyleUserInterface();
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
        nsIContent* parent = oldAncestor->GetParent();
        if (!parent)
          break;
        oldAncestor = parent;
      }
      nsCOMPtr<nsIContent> newAncestor = aContent;
      for (;;) {
        --offset;
        nsIContent* parent = newAncestor->GetParent();
        if (!parent)
          break;
        newAncestor = parent;
      }
#ifdef DEBUG
      if (oldAncestor != newAncestor) {
        // This could be a performance problem.
        
        // The |!oldAncestor->GetDocument()| case (that the old hover node has
        // been removed from the document) could be a slight performance
        // problem.  It's rare enough that it shouldn't be an issue, but common
        // enough that we don't want to assert..
        NS_ASSERTION(!oldAncestor->GetDocument(),
                     "moved hover between nodes in different documents");
        // XXX Why don't we ever hit this code because we're not using
        // |GetBindingParent|?
      }
#endif
      if (oldAncestor == newAncestor) {
        oldAncestor = mHoverContent;
        newAncestor = aContent;
        while (offset > 0) {
          oldAncestor = oldAncestor->GetParent();
          --offset;
        }
        while (offset < 0) {
          newAncestor = newAncestor->GetParent();
          ++offset;
        }
        while (oldAncestor != newAncestor) {
          oldAncestor = oldAncestor->GetParent();
          newAncestor = newAncestor->GetParent();
        }
        commonHoverAncestor = oldAncestor;
      }
    }

    mHoverContent = aContent;
  }

  if ((aState & NS_EVENT_STATE_FOCUS)) {
    EnsureDocument(mPresContext);
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
      // see comments in ShiftFocusInternal on mCurrentFocus overloading
      PRBool fcActive = PR_FALSE;
      if (mDocument) {
        nsIFocusController *fc = GetFocusControllerForDocument(mDocument);
        if (fc)
          fc->GetActive(&fcActive);
      }
      notifyContent[3] = gLastFocusedContent;
      NS_IF_ADDREF(gLastFocusedContent);
      // only raise window if the the focus controller is active
      SendFocusBlur(mPresContext, aContent, fcActive); 

      // If we now have focused content, ensure that the canvas focus ring
      // is removed.
      if (mDocument) {
        nsCOMPtr<nsIDocShell> docShell =
          do_QueryInterface(nsCOMPtr<nsISupports>(mDocument->GetContainer()));

        if (docShell && mCurrentFocus)
          docShell->SetCanvasHasFocus(PR_FALSE);
      }
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
  for  (int i = 0; i < maxNotify; i++) {
    if (notifyContent[i] &&
        !notifyContent[i]->GetDocument()) {
      NS_RELEASE(notifyContent[i]);
    }
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
      doc1 = notifyContent[0]->GetDocument();
      if (notifyContent[1]) {
        //For :focus this might be a different doc so check
        doc2 = notifyContent[1]->GetDocument();
        if (doc1 == doc2) {
          doc2 = nsnull;
        }
      }
    }
    else if (newHover) {
      doc1 = newHover->GetDocument();
    }
    else {
      doc1 = oldHover->GetDocument();
    }
    if (doc1) {
      doc1->BeginUpdate(UPDATE_CONTENT_STATE);

      // Notify all content from newHover to the commonHoverAncestor
      while (newHover && newHover != commonHoverAncestor) {
        doc1->ContentStatesChanged(newHover, nsnull, NS_EVENT_STATE_HOVER);
        newHover = newHover->GetParent();
      }
      // Notify all content from oldHover to the commonHoverAncestor
      while (oldHover && oldHover != commonHoverAncestor) {
        doc1->ContentStatesChanged(oldHover, nsnull, NS_EVENT_STATE_HOVER);
        oldHover = oldHover->GetParent();
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
      doc1->EndUpdate(UPDATE_CONTENT_STATE);

      if (doc2) {
        doc2->BeginUpdate(UPDATE_CONTENT_STATE);
        doc2->ContentStatesChanged(notifyContent[1], notifyContent[2],
                                   aState & ~NS_EVENT_STATE_HOVER);
        if (notifyContent[3]) {
          doc1->ContentStatesChanged(notifyContent[3], notifyContent[4],
                                     aState & ~NS_EVENT_STATE_HOVER);
        }
        doc2->EndUpdate(UPDATE_CONTENT_STATE);
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
nsEventStateManager::SendFocusBlur(nsIPresContext* aPresContext,
                                   nsIContent *aContent,
                                   PRBool aEnsureWindowHasFocus)
{
  // Keep a ref to presShell since dispatching the DOM event may cause
  // the document to be destroyed.
  nsCOMPtr<nsIPresShell> presShell = aPresContext->PresShell();

  nsCOMPtr<nsIContent> previousFocus = mCurrentFocus;

  // Make sure previousFocus is in a document.  If it's not, then
  // we should never abort firing events based on what happens when we
  // send it a blur.

  if (previousFocus && !previousFocus->GetDocument())
    previousFocus = nsnull;

  CurrentEventShepherd shepherd(this);

  if (nsnull != gLastFocusedPresContext) {

    nsCOMPtr<nsIContent> focusAfterBlur;

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
      nsCOMPtr<nsIDocument> doc = gLastFocusedContent->GetDocument();
      if (doc) {
        // The order of the nsIViewManager and nsIPresShell COM pointers is
        // important below.  We want the pres shell to get released before the
        // associated view manager on exit from this function.
        // See bug 53763.
        nsCOMPtr<nsIViewManager> kungFuDeathGrip;
        nsIPresShell *shell = doc->GetShellAt(0);
        if (shell) {
          kungFuDeathGrip = shell->GetViewManager();

          nsCOMPtr<nsIPresContext> oldPresContext;
          shell->GetPresContext(getter_AddRefs(oldPresContext));

          //fire blur
          nsEventStatus status = nsEventStatus_eIgnore;
          nsEvent event(NS_BLUR_CONTENT);
          shepherd.SetCurrentEvent(&event);

          EnsureDocument(presShell);
          
          // Make sure we're not switching command dispatchers, if so, surpress the blurred one
          if(gLastFocusedDocument && mDocument) {
            nsIFocusController *newFocusController = nsnull;
            nsIFocusController *oldFocusController = nsnull;
            nsCOMPtr<nsPIDOMWindow> newWindow = do_QueryInterface(mDocument->GetScriptGlobalObject());
            nsCOMPtr<nsPIDOMWindow> oldWindow = do_QueryInterface(gLastFocusedDocument->GetScriptGlobalObject());
            if(newWindow)
              newFocusController = newWindow->GetRootFocusController();
            if(oldWindow)
              oldFocusController = oldWindow->GetRootFocusController();
            if(oldFocusController && oldFocusController != newFocusController)
              oldFocusController->SetSuppressFocus(PR_TRUE, "SendFocusBlur Window Switch");
          }
          
          nsCOMPtr<nsIEventStateManager> esm;
          esm = oldPresContext->EventStateManager();
          esm->SetFocusedContent(gLastFocusedContent);
          nsCOMPtr<nsIContent> temp = gLastFocusedContent;
          NS_RELEASE(gLastFocusedContent); // nulls out gLastFocusedContent

          nsCxPusher pusher(temp);
          temp->HandleDOMEvent(oldPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status); 
          pusher.Pop();

          focusAfterBlur = mCurrentFocus;
          if (!previousFocus || previousFocus == focusAfterBlur)
            esm->SetFocusedContent(nsnull);
        }
      }

      if (clearFirstBlurEvent) {
        mFirstBlurEvent = nsnull;
      }

      if (previousFocus && previousFocus != focusAfterBlur) {
        // The content node's blur handler focused something else.
        // In this case, abort firing any more blur or focus events.
        return NS_OK;
      }
    }

    // Go ahead and fire a blur on the window.
    nsCOMPtr<nsIScriptGlobalObject> globalObject;

    if(gLastFocusedDocument)
      globalObject = gLastFocusedDocument->GetScriptGlobalObject();

    EnsureDocument(presShell);

    if (gLastFocusedDocument && (gLastFocusedDocument != mDocument) && globalObject) {  
      nsEventStatus status = nsEventStatus_eIgnore;
      nsEvent event(NS_BLUR_CONTENT);
      shepherd.SetCurrentEvent(&event);

      // Make sure we're not switching command dispatchers, if so, surpress the blurred one
      if (mDocument) {
        nsIFocusController *newFocusController = nsnull;
        nsIFocusController *oldFocusController = nsnull;
        nsCOMPtr<nsPIDOMWindow> newWindow = do_QueryInterface(mDocument->GetScriptGlobalObject());
        nsCOMPtr<nsPIDOMWindow> oldWindow = do_QueryInterface(gLastFocusedDocument->GetScriptGlobalObject());

        if (newWindow)
          newFocusController = newWindow->GetRootFocusController();
        oldFocusController = oldWindow->GetRootFocusController();
        if (oldFocusController && oldFocusController != newFocusController)
          oldFocusController->SetSuppressFocus(PR_TRUE, "SendFocusBlur Window Switch #2");
      }

      gLastFocusedPresContext->EventStateManager()->SetFocusedContent(nsnull);
      nsCOMPtr<nsIDocument> temp = gLastFocusedDocument;
      NS_RELEASE(gLastFocusedDocument);
      gLastFocusedDocument = nsnull;

      nsCxPusher pusher(temp);
      temp->HandleDOMEvent(gLastFocusedPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
      pusher.Pop();

      if (previousFocus && mCurrentFocus != previousFocus) {
        // The document's blur handler focused something else.
        // Abort firing any additional blur or focus events.
        return NS_OK;
      }

      pusher.Push(globalObject);
      globalObject->HandleDOMEvent(gLastFocusedPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status); 

      if (previousFocus && mCurrentFocus != previousFocus) {
        // The window's blur handler focused something else.
        // Abort firing any additional blur or focus events.
        return NS_OK;
      }
    }
  }
  
  if (aContent) {
    // Check if the HandleDOMEvent calls above destroyed our frame (bug #118685)
    nsIFrame* frame = nsnull;
    presShell->GetPrimaryFrameFor(aContent, &frame);
    if (!frame) {
      aContent = nsnull;
    }
  }

  NS_IF_RELEASE(gLastFocusedContent);
  gLastFocusedContent = aContent;
  NS_IF_ADDREF(gLastFocusedContent);
  SetFocusedContent(aContent);

  // Moved widget focusing code here, from end of SendFocusBlur
  // This fixes the order of accessibility focus events, so that 
  // the window focus event goes first, and then the focus event for the control
  if (aEnsureWindowHasFocus) {
    // This raises the window that has both content and scroll bars in it
    // instead of the child window just below it that contains only the content
    // That way we focus the same window that gets focused by a mouse click
    nsIViewManager* vm = presShell->GetViewManager();
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
    nsEvent event(NS_FOCUS_CONTENT);
    shepherd.SetCurrentEvent(&event);

    if (nsnull != mPresContext) {
      nsCxPusher pusher(aContent);
      aContent->HandleDOMEvent(mPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
    }
    
    nsAutoString tabIndex;
    aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::tabindex, tabIndex);
    PRInt32 ec, val = tabIndex.ToInteger(&ec);
    if (NS_SUCCEEDED (ec)) {
      mCurrentTabIndex = val;
    }

    if (clearFirstFocusEvent) {
      mFirstFocusEvent = nsnull;
    }
  } else if (!aContent) {
    //fire focus on document even if the content isn't focusable (ie. text)
    //see bugzilla bug 93521
    nsEventStatus status = nsEventStatus_eIgnore;
    nsEvent event(NS_FOCUS_CONTENT);
    shepherd.SetCurrentEvent(&event);

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
  mCurrentFocusFrame = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::GetFocusedFrame(nsIFrame** aFrame)
{
  if (!mCurrentFocusFrame && mCurrentFocus) {
    nsIDocument* doc = mCurrentFocus->GetDocument();
    if (doc) {
      nsIPresShell *shell = doc->GetShellAt(0);
      if (shell) {
        shell->GetPrimaryFrameFor(mCurrentFocus, &mCurrentFocusFrame);
        if (mCurrentFocusFrame)
          SetFrameExternalReference(mCurrentFocusFrame);
      }
    }
  }

  *aFrame = mCurrentFocusFrame;
  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::ContentRemoved(nsIContent* aContent)
{
  if (aContent == mCurrentFocus) {
    // Note that we don't use SetContentState() here because
    // we don't want to fire a blur.  Blurs should only be fired
    // in response to clicks or tabbing.

    SetFocusedContent(nsnull);
  }

  if (aContent == mHoverContent) {
    // Since hover is hierarchical, set the current hover to the
    // content's parent node.
    mHoverContent = aContent->GetParent();
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

void
nsEventStateManager::ForceViewUpdate(nsIView* aView)
{
  // force the update to happen now, otherwise multiple scrolls can
  // occur before the update is processed. (bug #7354)

  nsIViewManager* vm = aView->GetViewManager();
  if (vm) {
    // I'd use Composite here, but it doesn't always work.
    // vm->Composite();
    vm->ForceUpdate();
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
      nsIScriptSecurityManager *securityManager =
        nsContentUtils::GetSecurityManager();

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

            // Dispatch to the system event group.  Make sure to clear the
            // STOP_DISPATCH flag since this resets for each event group
            // per DOM3 Events.

            innerEvent->flags &= ~NS_EVENT_FLAG_STOP_DISPATCH;
            ret = target->HandleDOMEvent(mPresContext, innerEvent, &aEvent,
                                         NS_EVENT_FLAG_INIT | NS_EVENT_FLAG_SYSTEM_EVENT,
                                         &status);
          }
          else {
            nsCOMPtr<nsIChromeEventHandler> target(do_QueryInterface(aTarget));
            if (target) {
              ret = target->HandleChromeEvent(mPresContext, innerEvent, &aEvent, NS_EVENT_FLAG_INIT, &status);
            }
          }
        }
      }

      *aPreventDefault = status != nsEventStatus_eConsumeNoDefault;
    }
  }

  return ret;
}

void
nsEventStateManager::EnsureDocument(nsIPresContext* aPresContext)
{
  if (!mDocument)
    EnsureDocument(aPresContext->PresShell());
}

void
nsEventStateManager::EnsureDocument(nsIPresShell* aPresShell)
{
  if (!mDocument && aPresShell)
    aPresShell->GetDocument(getter_AddRefs(mDocument));
}

void
nsEventStateManager::FlushPendingEvents(nsIPresContext* aPresContext)
{
  NS_PRECONDITION(nsnull != aPresContext, "nsnull ptr");
  nsIPresShell *shell = aPresContext->GetPresShell();
  if (shell) {
    // This is not flushing _Display because of the mess that is bug 36849
    shell->FlushPendingNotifications(Flush_Layout);
    nsIViewManager* viewManager = shell->GetViewManager();
    if (viewManager) {
      viewManager->FlushPendingInvalidates();
    }
  }
}

nsresult
nsEventStateManager::GetDocSelectionLocation(nsIContent **aStartContent,
                                             nsIContent **aEndContent,
                                             nsIFrame **aStartFrame,
                                             PRUint32* aStartOffset)
{
  // In order to return the nsIContent and nsIFrame of the caret's position,
  // we need to get a pres shell, and then get the selection from it

  *aStartOffset = 0;
  *aStartFrame = nsnull; 
  *aStartContent = *aEndContent = nsnull;
  nsresult rv = NS_ERROR_FAILURE;
  
  if (!mDocument)
    return rv;
  nsIPresShell *shell = nsnull;
  if (mPresContext)
    shell = mPresContext->GetPresShell();

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
      domRange->GetStartOffset(NS_REINTERPRET_CAST(PRInt32 *, aStartOffset));

      nsIContent *childContent = nsnull;

      startContent = do_QueryInterface(startNode);
      if (startContent->IsContentOfType(nsIContent::eELEMENT)) {
        childContent = startContent->GetChildAt(*aStartOffset);
        if (childContent)
          startContent = childContent;
      }

      endContent = do_QueryInterface(endNode);
      if (endContent->IsContentOfType(nsIContent::eELEMENT)) {
        PRInt32 endOffset = 0;
        domRange->GetEndOffset(&endOffset);
        childContent = endContent->GetChildAt(endOffset);
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
        nsCOMPtr<nsIContent> origStartContent(startContent);
        nsAutoString nodeValue;
        domNode->GetNodeValue(nodeValue);

        PRBool isFormControl =
          startContent->IsContentOfType(nsIContent::eHTML_FORM_CONTROL);

        if (nodeValue.Length() == *aStartOffset && !isFormControl &&
            startContent != mDocument->GetRootContent()) {
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
              startContent = startFrame->GetContent();
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

void
nsEventStateManager::FocusElementButNotDocument(nsIContent *aContent)
{
  // Focus an element in the current document, but don't switch document/window focus!

  if (gLastFocusedDocument == mDocument) {
    // If we're already focused in this document, 
    // use normal focus method
    if (mCurrentFocus != aContent) {
      if (aContent) 
        aContent->SetFocus(mPresContext);
      else
        SetContentState(nsnull, NS_EVENT_STATE_FOCUS);
    }
    return;
  }

  /**
   * The last focus wasn't in this document, so we may be getting our position from the selection
   * while the window focus is currently somewhere else such as the find dialog
   */

  nsIFocusController *focusController =
    GetFocusControllerForDocument(mDocument);
  if (!focusController)
      return;

  // Get previous focus
  nsCOMPtr<nsIDOMElement> oldFocusedElement;
  focusController->GetFocusedElement(getter_AddRefs(oldFocusedElement));
  nsCOMPtr<nsIContent> oldFocusedContent(do_QueryInterface(oldFocusedElement));

  // Notify focus controller of new focus for this document
  nsCOMPtr<nsIDOMElement> newFocusedElement(do_QueryInterface(aContent));
  focusController->SetFocusedElement(newFocusedElement);

  // Temporarily set mCurrentFocus so that esm::GetContentState() tells 
  // layout system to show focus on this element. 
  SetFocusedContent(aContent);  // Reset back to null at the end of this method.
  mDocument->BeginUpdate(UPDATE_CONTENT_STATE);
  mDocument->ContentStatesChanged(oldFocusedContent, aContent, 
                                  NS_EVENT_STATE_FOCUS);
  mDocument->EndUpdate(UPDATE_CONTENT_STATE);

  // Reset mCurrentFocus = nsnull for this doc, so when this document 
  // does get focus next time via preHandleEvent() NS_GOTFOCUS,
  // the old document gets blurred
  SetFocusedContent(nsnull);

}

NS_IMETHODIMP
nsEventStateManager::MoveFocusToCaret(PRBool aCanFocusDoc,
                                      PRBool *aIsSelectionWithFocus)
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
  while (testContent) {
    // Keep testing while selectionContent is equal to something,
    // eventually we'll run out of ancestors

    if (testContent == mCurrentFocus) {
      *aIsSelectionWithFocus = PR_TRUE;
      return NS_OK;  // already focused on this node, this whole thing's moot
    }

    nsIAtom *tag = testContent->Tag();

    // Add better focusable test here later if necessary ... 
    if (tag == nsHTMLAtoms::a &&
        testContent->IsContentOfType(nsIContent::eHTML)) {
      *aIsSelectionWithFocus = PR_TRUE;
    }
    else {
      *aIsSelectionWithFocus = testContent->HasAttr(kNameSpaceID_XLink, nsHTMLAtoms::href);
      if (*aIsSelectionWithFocus) {
        nsAutoString xlinkType;
        testContent->GetAttr(kNameSpaceID_XLink, nsHTMLAtoms::type, xlinkType);
        if (!xlinkType.EqualsLiteral("simple")) {
          *aIsSelectionWithFocus = PR_FALSE;  // Xlink must be type="simple"
        }
      }
    }

    if (*aIsSelectionWithFocus) {
      FocusElementButNotDocument(testContent);
      return NS_OK;
    }

    // Get the parent
    testContent = testContent->GetParent();

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
      if (testContent->Tag() == nsHTMLAtoms::a &&
          testContent->IsContentOfType(nsIContent::eHTML)) {
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



NS_IMETHODIMP
nsEventStateManager::MoveCaretToFocus()
{
  // If in HTML content and the pref accessibility.browsewithcaret is TRUE,
  // then always move the caret to beginning of a new focus

  PRInt32 itemType = nsIDocShellTreeItem::typeChrome;

  if (mPresContext) {
    nsCOMPtr<nsISupports> pcContainer = mPresContext->GetContainer();
    nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(pcContainer));
    if (treeItem) 
      treeItem->GetItemType(&itemType);
    nsCOMPtr<nsIEditorDocShell> editorDocShell(do_QueryInterface(treeItem));
    if (editorDocShell) {
      PRBool isEditable;
      editorDocShell->GetEditable(&isEditable);
      if (isEditable) {
        return NS_OK;  // Move focus to caret only if browsing, not editing
      }
    }
  }

  if (itemType != nsIDocShellTreeItem::typeChrome) {
    nsCOMPtr<nsIContent> selectionContent, endSelectionContent;
    nsIFrame *selectionFrame;
    PRUint32 selectionOffset;
    GetDocSelectionLocation(getter_AddRefs(selectionContent),
                            getter_AddRefs(endSelectionContent),
                            &selectionFrame, &selectionOffset);
    while (selectionContent) {
      nsIContent* parentContent = selectionContent->GetParent();
      if (mCurrentFocus == selectionContent && parentContent)
        return NS_OK; // selection is already within focus node that isn't the root content
      selectionContent = parentContent; // Keep checking up chain of parents, focus may be in a link above us
    }

    nsIPresShell *shell = mPresContext->GetPresShell();
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
          if (currentFocusNode) {
            nsCOMPtr<nsIDOMRange> newRange;
            nsresult rv = rangeDoc->CreateRange(getter_AddRefs(newRange));
            if (NS_SUCCEEDED(rv)) {
              // Set the range to the start of the currently focused node
              // Make sure it's collapsed
              newRange->SelectNodeContents(currentFocusNode);
              nsCOMPtr<nsIDOMNode> firstChild;
              currentFocusNode->GetFirstChild(getter_AddRefs(firstChild));
              if (!firstChild ) {
                // If current focus node is a leaf, set range to before the
                // node by using the parent as a container.
                // This prevents it from appearing as selected.
                newRange->SetStartBefore(currentFocusNode);
                newRange->SetEndBefore(currentFocusNode);
              }
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

nsresult
nsEventStateManager::SetCaretEnabled(nsIPresShell *aPresShell, PRBool aEnabled)
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

nsresult
nsEventStateManager::SetContentCaretVisible(nsIPresShell* aPresShell,
                                            nsIContent *aFocusedContent,
                                            PRBool aVisible)
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


PRBool
nsEventStateManager::GetBrowseWithCaret()
{
  return mBrowseWithCaret;
}

void
nsEventStateManager::ResetBrowseWithCaret()
{
  // This is called when browse with caret changes on the fly
  // or when a document gets focused 

  if (!mPresContext)
    return;

  nsCOMPtr<nsISupports> pcContainer = mPresContext->GetContainer();
  PRInt32 itemType;
  nsCOMPtr<nsIDocShellTreeItem> shellItem(do_QueryInterface(pcContainer));
  if (!shellItem)
    return;

  shellItem->GetItemType(&itemType);

  if (itemType == nsIDocShellTreeItem::typeChrome) 
    return;  // Never browse with caret in chrome

  PRPackedBool browseWithCaret =
    nsContentUtils::GetBoolPref("accessibility.browsewithcaret");

  if (mBrowseWithCaret == browseWithCaret)
    return; // already set this way, don't change caret at all

  mBrowseWithCaret = browseWithCaret;

  nsIPresShell *presShell = mPresContext->GetPresShell();

  // Make caret visible or not, depending on what's appropriate
  if (presShell) {
    SetContentCaretVisible(presShell, mCurrentFocus,
                           browseWithCaret &&
                           (!gLastFocusedDocument ||
                            gLastFocusedDocument == mDocument));
  }
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
      nsIContent *rootContent = doc->GetRootContent();
      if (rootContent) {
        PRUint32 childCount = rootContent->GetChildCount();
        for (PRUint32 i = 0; i < childCount; ++i) {
          nsIContent *childContent = rootContent->GetChildAt(i);

          nsINodeInfo *ni = childContent->GetNodeInfo();

          if (childContent->IsContentOfType(nsIContent::eHTML) &&
              ni->Equals(nsHTMLAtoms::frameset)) {
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

  nsIContent *docContent = parentDoc->FindContentForSubDocument(doc);

  if (!docContent)
    return PR_FALSE;

  return docContent->Tag() == nsHTMLAtoms::iframe;
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
      nsIEventStateManager *docESM = pc->EventStateManager();

      // we are about to shift focus to aDocShell
      // keep track of the document, so we don't try to go back into it.
      mTabbingFromDocShells.AppendObject(aDocShell);
        
      // clear out any existing focus state
      docESM->SetContentState(nsnull, NS_EVENT_STATE_FOCUS);
      // now focus the first (or last) focusable content
      docESM->ShiftFocus(aForward, nsnull);

      // remove the document from the list
      mTabbingFromDocShells.RemoveObject(aDocShell); 
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

  nsCOMPtr<nsISupports> pcContainer = mPresContext->GetContainer();
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
  nsCOMPtr<nsISupports> container = aDocument->GetContainer();
  nsCOMPtr<nsPIDOMWindow> windowPrivate = do_GetInterface(container);

  return windowPrivate ? windowPrivate->GetRootFocusController() : nsnull;
}
