/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
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
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   Ginn Chen <ginn.chen@sun.com>
 *   Simon Bünzli <zeniko@gmail.com>
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
#include "nsIMEStateManager.h"
#include "nsIContent.h"
#include "nsINodeInfo.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsIWidget.h"
#include "nsIKBStateControl.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsDOMEvent.h"
#include "nsGkAtoms.h"
#include "nsIEditorDocShell.h"
#include "nsIFormControl.h"
#include "nsIComboboxControlFrame.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMNSHTMLInputElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLButtonElement.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLMapElement.h"
#include "nsIDOMHTMLBodyElement.h"
#include "nsIDOMXULControlElement.h"
#include "nsIDOMXULTextboxElement.h"
#include "nsImageMapUtils.h"
#include "nsIHTMLDocument.h"
#include "nsINameSpaceManager.h"
#include "nsIBaseWindow.h"
#include "nsIScrollableView.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsISelection.h"
#include "nsFrameSelection.h"
#include "nsIDeviceContext.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMWindowInternal.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMEventTarget.h"
#include "nsIEnumerator.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIWebNavigation.h"
#include "nsIContentViewer.h"
#include "nsIPrefBranch2.h"
#include "nsIObjectFrame.h"

#include "nsIServiceManager.h"
#include "nsIScriptSecurityManager.h"

#include "nsIFocusController.h"

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

#include "nsIFrameFrame.h"
#include "nsIFrameTraversal.h"
#include "nsLayoutCID.h"
#include "nsLayoutUtils.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsUnicharUtils.h"
#include "nsContentUtils.h"

#include "imgIContainer.h"
#include "nsIProperties.h"
#include "nsISupportsPrimitives.h"
#include "nsEventDispatcher.h"

#ifdef XP_MACOSX
#include <Events.h>
#endif

#if defined(DEBUG_rods) || defined(DEBUG_bryner)
//#define DEBUG_DOCSHELL_FOCUS
#endif

static NS_DEFINE_CID(kFrameTraversalCID, NS_FRAMETRAVERSAL_CID);


//we will use key binding by default now. this wil lbreak viewer for now
#define NON_KEYBINDING 0

static NS_DEFINE_CID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);

nsIContent * gLastFocusedContent = 0; // Strong reference
nsIDocument * gLastFocusedDocument = 0; // Strong reference
nsPresContext* gLastFocusedPresContext = 0; // Weak reference

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
static PRInt32 sChromeAccessModifier = 0, sContentAccessModifier = 0;
PRInt32 nsEventStateManager::sUserInputEventDepth = 0;

enum {
 MOUSE_SCROLL_N_LINES,
 MOUSE_SCROLL_PAGE,
 MOUSE_SCROLL_HISTORY,
 MOUSE_SCROLL_TEXTSIZE,
 MOUSE_SCROLL_PIXELS
};

// mask values for ui.key.chromeAccess and ui.key.contentAccess
#define NS_MODIFIER_SHIFT    1
#define NS_MODIFIER_CONTROL  2
#define NS_MODIFIER_ALT      4
#define NS_MODIFIER_META     8

static nsIDocument *
GetDocumentFromWindow(nsIDOMWindow *aWindow)
{
  nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(aWindow);
  nsCOMPtr<nsIDocument> doc;

  if (win) {
    doc = do_QueryInterface(win->GetExtantDocument());
  }

  return doc;
}

static PRInt32
GetAccessModifierMaskFromPref(PRInt32 aItemType)
{
  PRInt32 accessKey = nsContentUtils::GetIntPref("ui.key.generalAccessKey", -1);
  switch (accessKey) {
    case -1:                             break; // use the individual prefs
    case nsIDOMKeyEvent::DOM_VK_SHIFT:   return NS_MODIFIER_SHIFT;
    case nsIDOMKeyEvent::DOM_VK_CONTROL: return NS_MODIFIER_CONTROL;
    case nsIDOMKeyEvent::DOM_VK_ALT:     return NS_MODIFIER_ALT;
    case nsIDOMKeyEvent::DOM_VK_META:    return NS_MODIFIER_META;
    default:                             return 0;
  }

  switch (aItemType) {
  case nsIDocShellTreeItem::typeChrome:
    return nsContentUtils::GetIntPref("ui.key.chromeAccess", 0);
  case nsIDocShellTreeItem::typeContent:
    return nsContentUtils::GetIntPref("ui.key.contentAccess", 0);
  default:
    return 0;
  }
}

static void
GetBasePrefKeyForMouseWheel(nsMouseScrollEvent* aEvent, nsACString& aPref)
{
  NS_NAMED_LITERAL_CSTRING(prefbase,    "mousewheel");
  NS_NAMED_LITERAL_CSTRING(horizscroll, ".horizscroll");
  NS_NAMED_LITERAL_CSTRING(withshift,   ".withshiftkey");
  NS_NAMED_LITERAL_CSTRING(withalt,     ".withaltkey");
  NS_NAMED_LITERAL_CSTRING(withcontrol, ".withcontrolkey");
  NS_NAMED_LITERAL_CSTRING(withmetakey, ".withmetakey");
  NS_NAMED_LITERAL_CSTRING(withno,      ".withnokey");

  aPref = prefbase;
  if (aEvent->scrollFlags & nsMouseScrollEvent::kIsHorizontal) {
    aPref.Append(horizscroll);
  }
  if (aEvent->isShift) {
    aPref.Append(withshift);
  } else if (aEvent->isControl) {
    aPref.Append(withcontrol);
  } else if (aEvent->isAlt) {
    aPref.Append(withalt);
  } else if (aEvent->isMeta) {
    aPref.Append(withmetakey);
  } else {
    aPref.Append(withno);
  }
}

class nsMouseWheelTransaction {
public:
  static nsIFrame* GetTargetFrame() { return sTargetFrame; }
  static void BeginTransaction(nsIFrame* aTargetFrame,
                               nsGUIEvent* aEvent);
  static void UpdateTransaction();
  static void EndTransaction();
  static void OnEvent(nsEvent* aEvent);
protected:
  static nsPoint GetScreenPoint(nsGUIEvent* aEvent);
  static PRUint32 GetTimeoutTime();
  static PRUint32 GetIgnoreMoveDelayTime();

  static nsWeakFrame sTargetFrame;
  static PRUint32    sTime;        // in milliseconds
  static PRUint32    sMouseMoved;  // in milliseconds
};

nsWeakFrame nsMouseWheelTransaction::sTargetFrame(nsnull);
PRUint32    nsMouseWheelTransaction::sTime        = 0;
PRUint32    nsMouseWheelTransaction::sMouseMoved  = 0;

void
nsMouseWheelTransaction::BeginTransaction(nsIFrame* aTargetFrame,
                                          nsGUIEvent* aEvent)
{
  NS_ASSERTION(!sTargetFrame, "previous transaction is not finished!");
  sTargetFrame = aTargetFrame;
  UpdateTransaction();
}

void
nsMouseWheelTransaction::UpdateTransaction()
{
  // We should use current time instead of nsEvent.time.
  // 1. Some events doesn't have the correct creation time.
  // 2. If the computer runs slowly by other processes eating the CPU resource,
  //    the event creation time doesn't keep real time.
  sTime = PR_IntervalToMilliseconds(PR_IntervalNow());
  sMouseMoved = 0;
}

void
nsMouseWheelTransaction::EndTransaction()
{
  sTargetFrame = nsnull;
}

static PRBool
OutOfTime(PRUint32 aBaseTime, PRUint32 aThreshold)
{
  PRUint32 now = PR_IntervalToMilliseconds(PR_IntervalNow());
  return (now - aBaseTime > aThreshold);
}

void
nsMouseWheelTransaction::OnEvent(nsEvent* aEvent)
{
  if (!sTargetFrame)
    return;

  if (OutOfTime(sTime, GetTimeoutTime())) {
    // Time out the current transaction.
    EndTransaction();
    return;
  }

  switch (aEvent->message) {
    case NS_MOUSE_SCROLL:
      if (sMouseMoved != 0 &&
          OutOfTime(sMouseMoved, GetIgnoreMoveDelayTime())) {
        // Terminate the current mousewheel transaction if the mouse moved more
        // than ignoremovedelay milliseconds ago
        EndTransaction();
      }
      return;
    case NS_MOUSE_MOVE:
    case NS_DRAGDROP_OVER:
      if (((nsMouseEvent*)aEvent)->reason == nsMouseEvent::eReal) {
        // If the cursor is moving to be outside the frame,
        // terminate the scrollwheel transaction.
        nsPoint pt = GetScreenPoint((nsGUIEvent*)aEvent);
        nsIntRect r = sTargetFrame->GetScreenRectExternal();
        if (!r.Contains(pt)) {
          EndTransaction();
          return;
        }

        // If the cursor is moving inside the frame, and it is less than
        // ignoremovedelay milliseconds since the last scroll operation, ignore
        // the mouse move; otherwise, record the current mouse move time to be
        // checked later
        if (OutOfTime(sTime, GetIgnoreMoveDelayTime())) {
          if (sMouseMoved == 0)
            sMouseMoved = PR_IntervalToMilliseconds(PR_IntervalNow());
        }
      }
      return;
    case NS_KEY_PRESS:
    case NS_KEY_UP:
    case NS_KEY_DOWN:
    case NS_MOUSE_BUTTON_UP:
    case NS_MOUSE_BUTTON_DOWN:
    case NS_MOUSE_DOUBLECLICK:
    case NS_MOUSE_CLICK:
    case NS_CONTEXTMENU:
    case NS_DRAGDROP_DROP:
      EndTransaction();
      return;
  }
}

nsPoint
nsMouseWheelTransaction::GetScreenPoint(nsGUIEvent* aEvent)
{
  NS_ASSERTION(aEvent, "aEvent is null");
  NS_ASSERTION(aEvent->widget, "aEvent-widget is null");
  nsRect tmpRect;
  aEvent->widget->WidgetToScreen(nsRect(aEvent->refPoint, nsSize(1, 1)),
                                 tmpRect);
  return tmpRect.TopLeft();
}

PRUint32
nsMouseWheelTransaction::GetTimeoutTime()
{
  return (PRUint32)
    nsContentUtils::GetIntPref("mousewheel.transaction.timeout", 1500);
}

PRUint32
nsMouseWheelTransaction::GetIgnoreMoveDelayTime()
{
  return (PRUint32)
    nsContentUtils::GetIntPref("mousewheel.transaction.ignoremovedelay", 100);
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
    mGestureDownPoint(0,0),
    mCurrentFocusFrame(nsnull),
    mCurrentTabIndex(0),
    mLastFocusedWith(eEventFocusedByUnknown),
    mPresContext(nsnull),
    mLClickCount(0),
    mMClickCount(0),
    mRClickCount(0),
    mNormalLMouseEventInProcess(PR_FALSE),
    m_haveShutdown(PR_FALSE),
    mBrowseWithCaret(PR_FALSE),
    mTabbedThroughDocument(PR_FALSE),
    mAccessKeys(nsnull)
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

  nsCOMPtr<nsIPrefBranch2> prefBranch =
    do_QueryInterface(nsContentUtils::GetPrefBranch());

  if (prefBranch) {
    if (sESMInstanceCount == 1) {
      sLeftClickOnly =
        nsContentUtils::GetBoolPref("nglayout.events.dispatchLeftClickOnly",
                                    sLeftClickOnly);

      sChromeAccessModifier =
        GetAccessModifierMaskFromPref(nsIDocShellTreeItem::typeChrome);
      sContentAccessModifier =
        GetAccessModifierMaskFromPref(nsIDocShellTreeItem::typeContent);

      nsIContent::sTabFocusModelAppliesToXUL =
        nsContentUtils::GetBoolPref("accessibility.tabfocus_applies_to_xul",
                                    nsIContent::sTabFocusModelAppliesToXUL);
    }
    prefBranch->AddObserver("accessibility.accesskeycausesactivation", this, PR_TRUE);
    prefBranch->AddObserver("accessibility.browsewithcaret", this, PR_TRUE);
    prefBranch->AddObserver("accessibility.tabfocus_applies_to_xul", this, PR_TRUE);
    prefBranch->AddObserver("nglayout.events.dispatchLeftClickOnly", this, PR_TRUE);
    prefBranch->AddObserver("ui.key.generalAccessKey", this, PR_TRUE);
    prefBranch->AddObserver("ui.key.chromeAccess", this, PR_TRUE);
    prefBranch->AddObserver("ui.key.contentAccess", this, PR_TRUE);
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

    prefBranch->AddObserver("dom.popup_allowed_events", this, PR_TRUE);
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
  KillClickHoldTimer();
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
  nsCOMPtr<nsIPrefBranch2> prefBranch =
    do_QueryInterface(nsContentUtils::GetPrefBranch());

  if (prefBranch) {
    prefBranch->RemoveObserver("accessibility.accesskeycausesactivation", this);
    prefBranch->RemoveObserver("accessibility.browsewithcaret", this);
    prefBranch->RemoveObserver("accessibility.tabfocus_applies_to_xul", this);
    prefBranch->RemoveObserver("nglayout.events.dispatchLeftClickOnly", this);
    prefBranch->RemoveObserver("ui.key.generalAccessKey", this);
    prefBranch->RemoveObserver("ui.key.chromeAccess", this);
    prefBranch->RemoveObserver("ui.key.contentAccess", this);
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

    prefBranch->RemoveObserver("dom.popup_allowed_events", this);
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
    } else if (data.EqualsLiteral("accessibility.tabfocus_applies_to_xul")) {
      nsIContent::sTabFocusModelAppliesToXUL =
        nsContentUtils::GetBoolPref("accessibility.tabfocus_applies_to_xul",
                                    nsIContent::sTabFocusModelAppliesToXUL);
    } else if (data.EqualsLiteral("nglayout.events.dispatchLeftClickOnly")) {
      sLeftClickOnly =
        nsContentUtils::GetBoolPref("nglayout.events.dispatchLeftClickOnly",
                                    sLeftClickOnly);
    } else if (data.EqualsLiteral("ui.key.generalAccessKey")) {
      sChromeAccessModifier =
        GetAccessModifierMaskFromPref(nsIDocShellTreeItem::typeChrome);
      sContentAccessModifier =
        GetAccessModifierMaskFromPref(nsIDocShellTreeItem::typeContent);
    } else if (data.EqualsLiteral("ui.key.chromeAccess")) {
      sChromeAccessModifier =
        GetAccessModifierMaskFromPref(nsIDocShellTreeItem::typeChrome);
    } else if (data.EqualsLiteral("ui.key.contentAccess")) {
      sContentAccessModifier =
        GetAccessModifierMaskFromPref(nsIDocShellTreeItem::typeContent);
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
    } else if (data.EqualsLiteral("dom.popup_allowed_events")) {
      nsDOMEvent::PopupAllowedEventsChanged();
    }
  }

  return NS_OK;
}


NS_IMPL_ISUPPORTS3(nsEventStateManager, nsIEventStateManager, nsIObserver, nsISupportsWeakReference)

NS_IMETHODIMP
nsEventStateManager::PreHandleEvent(nsPresContext* aPresContext,
                                    nsEvent *aEvent,
                                    nsIFrame* aTargetFrame,
                                    nsEventStatus* aStatus,
                                    nsIView* aView)
{
  NS_ENSURE_ARG_POINTER(aStatus);
  NS_ENSURE_ARG(aPresContext);
  if (!aEvent) {
    NS_ERROR("aEvent is null.  This should never happen.");
    return NS_ERROR_NULL_POINTER;
  }

  mCurrentTarget = aTargetFrame;
  mCurrentTargetContent = nsnull;

  // Focus events don't necessarily need a frame.
  if (NS_EVENT_NEEDS_FRAME(aEvent)) {
    NS_ASSERTION(mCurrentTarget, "mCurrentTarget is null.  this should not happen.  see bug #13007");
    if (!mCurrentTarget) return NS_ERROR_NULL_POINTER;
  }

  *aStatus = nsEventStatus_eIgnore;

  nsMouseWheelTransaction::OnEvent(aEvent);

  switch (aEvent->message) {
  case NS_MOUSE_BUTTON_DOWN:
    switch (NS_STATIC_CAST(nsMouseEvent*, aEvent)->button) {
    case nsMouseEvent::eLeftButton:
#ifndef XP_OS2
      BeginTrackingDragGesture ( aPresContext, (nsMouseEvent*)aEvent, aTargetFrame );
#endif
      mLClickCount = ((nsMouseEvent*)aEvent)->clickCount;
      SetClickCount(aPresContext, (nsMouseEvent*)aEvent, aStatus);
      mNormalLMouseEventInProcess = PR_TRUE;
      break;
    case nsMouseEvent::eMiddleButton:
      mMClickCount = ((nsMouseEvent*)aEvent)->clickCount;
      SetClickCount(aPresContext, (nsMouseEvent*)aEvent, aStatus);
      break;
    case nsMouseEvent::eRightButton:
#ifdef XP_OS2
      BeginTrackingDragGesture ( aPresContext, (nsMouseEvent*)aEvent, aTargetFrame );
#endif
      mRClickCount = ((nsMouseEvent*)aEvent)->clickCount;
      SetClickCount(aPresContext, (nsMouseEvent*)aEvent, aStatus);
      break;
    }
    break;
  case NS_MOUSE_BUTTON_UP:
    switch (NS_STATIC_CAST(nsMouseEvent*, aEvent)->button) {
      case nsMouseEvent::eLeftButton:
#ifdef CLICK_HOLD_CONTEXT_MENUS
      KillClickHoldTimer();
#endif
#ifndef XP_OS2
      StopTrackingDragGesture();
#endif
      mNormalLMouseEventInProcess = PR_FALSE;
    case nsMouseEvent::eRightButton:
#ifdef XP_OS2
      StopTrackingDragGesture();
#endif
    case nsMouseEvent::eMiddleButton:
      SetClickCount(aPresContext, (nsMouseEvent*)aEvent, aStatus);
      break;
    }
    break;
  case NS_MOUSE_EXIT:
    // If the event coordinate is within the bounds of the view,
    // and this is not the top-level window, then it's not really
    // an exit --- we may have traversed widget boundaries but
    // we're still in our toplevel window.
    // On the other hand, if we exit a toplevel window, then
    // it's really an exit even if the mouse is still in the
    // window bounds --- the mouse probably moved into some
    // "on top" window.
    {
      nsMouseEvent* mouseEvent = NS_STATIC_CAST(nsMouseEvent*, aEvent);
      nsIWidget* parentWidget = mouseEvent->widget->GetParent();
      nsPoint eventPoint;
      eventPoint = nsLayoutUtils::TranslateWidgetToView(aPresContext,
                                                        mouseEvent->widget,
                                                        mouseEvent->refPoint,
                                                        aView);
      if (parentWidget &&
          (aView->GetBounds() - aView->GetPosition()).Contains(eventPoint)) {
        // treat it as a move so we don't generate spurious "exit"
        // events Any necessary exit events will be generated by
        // GenerateMouseEnterExit
        aEvent->message = NS_MOUSE_MOVE;
        // then fall through...
      } else {
        GenerateMouseEnterExit((nsGUIEvent*)aEvent);
        //This is a window level mouse exit event and should stop here
        aEvent->message = 0;
        break;
      }
    }
  case NS_MOUSE_MOVE:
    // on the Mac, GenerateDragGesture() may not return until the drag
    // has completed and so |aTargetFrame| may have been deleted (moving
    // a bookmark, for example).  If this is the case, however, we know
    // that ClearFrameRefs() has been called and it cleared out
    // |mCurrentTarget|. As a result, we should pass |mCurrentTarget|
    // into UpdateCursor().
    GenerateDragGesture(aPresContext, (nsMouseEvent*)aEvent);
    UpdateCursor(aPresContext, aEvent, mCurrentTarget, aStatus);
    GenerateMouseEnterExit((nsGUIEvent*)aEvent);
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
          nsCOMPtr<nsPIDOMWindow> ourWindow =
            gLastFocusedDocument->GetWindow();

          // If the focus controller is already suppressed, it means that we
          // are in the middle of an activate sequence. In this case, we do
          // _not_ want to fire a blur on the previously focused content, since
          // we will be focusing it again later when we receive the NS_ACTIVATE
          // event.  See bug 120209.

          // Hold a strong ref to the focus controller, since we need
          // it after event dispatch.
          nsCOMPtr<nsIFocusController> focusController;
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
            nsEvent blurevent(PR_TRUE, NS_BLUR_CONTENT);

            nsEventDispatcher::Dispatch(gLastFocusedDocument,
                                        gLastFocusedPresContext,
                                        &blurevent, nsnull, &blurstatus);

            if (!mCurrentFocus && gLastFocusedContent) {
              // We also need to blur the previously focused content node here,
              // if we don't have a focused content node in this document.
              // (SendFocusBlur isn't called in this case).

              nsCOMPtr<nsIContent> blurContent = gLastFocusedContent;
              blurevent.target = nsnull;
              nsEventDispatcher::Dispatch(gLastFocusedContent,
                                          gLastFocusedPresContext,
                                          &blurevent, nsnull, &blurstatus);

              // XXX bryner this isn't quite right -- it can result in
              // firing two blur events on the content.

              nsCOMPtr<nsIDocument> doc;
              if (gLastFocusedContent) // could have changed in Dispatch
                doc = gLastFocusedContent->GetDocument();
              if (doc) {
                nsIPresShell *shell = doc->GetShellAt(0);
                if (shell) {
                  nsCOMPtr<nsPresContext> oldPresContext =
                    shell->GetPresContext();

                  nsCOMPtr<nsIEventStateManager> esm;
                  esm = oldPresContext->EventStateManager();
                  esm->SetFocusedContent(gLastFocusedContent);
                  nsEventDispatcher::Dispatch(gLastFocusedContent,
                                              oldPresContext,
                                              &blurevent, nsnull, &blurstatus);
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

        nsCOMPtr<nsPIDOMWindow> window(mDocument->GetWindow());

        if (window) {
          // We don't want there to be a focused content node while we're
          // dispatching the focus event.

          nsCOMPtr<nsIContent> currentFocus = mCurrentFocus;
          // "leak" this reference, but we take it back later
          SetFocusedContent(nsnull);

          nsIMEStateManager::OnChangeFocus(mPresContext, currentFocus);

          nsEventStatus status = nsEventStatus_eIgnore;
          nsEvent focusevent(PR_TRUE, NS_FOCUS_CONTENT);

          if (gLastFocusedDocument != mDocument) {
            //XXXsmaug bubbling for now, because in the old event dispatching
            //         code focus event did bubble from document to window.
            nsEventDispatcher::Dispatch(mDocument, aPresContext,
                                        &focusevent, nsnull, &status);
            if (currentFocus && currentFocus != gLastFocusedContent) {
              // Clear the target so that Dispatch can set it back correctly.
              focusevent.target = nsnull;
              nsEventDispatcher::Dispatch(currentFocus, aPresContext,
                                          &focusevent, nsnull, &status);
            }
          }

          // Clear the target so that Dispatch can set it back correctly.
          focusevent.target = nsnull;
          nsEventDispatcher::Dispatch(window, aPresContext, &focusevent,
                                      nsnull, &status);

          SetFocusedContent(currentFocus); // we kept this reference above
          NS_IF_RELEASE(gLastFocusedContent);
          gLastFocusedContent = mCurrentFocus;
          NS_IF_ADDREF(gLastFocusedContent);
        }

        // Try to keep the focus controllers and the globals in synch
        if (gLastFocusedDocument && gLastFocusedDocument != mDocument) {

          nsIFocusController *lastController = nsnull;
          nsPIDOMWindow* lastWindow = gLastFocusedDocument->GetWindow();
          if (lastWindow)
            lastController = lastWindow->GetRootFocusController();

          nsIFocusController *nextController = nsnull;
          nsPIDOMWindow* nextWindow = mDocument->GetWindow();
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

        if (gLastFocusedContent && !gLastFocusedContent->IsInDoc()) {
          NS_RELEASE(gLastFocusedContent);
        }

        // Now fire blurs.  We fire a blur on the focused document, element,
        // and window.

        nsEventStatus status = nsEventStatus_eIgnore;
        nsEvent event(PR_TRUE, NS_BLUR_CONTENT);

        if (gLastFocusedDocument && gLastFocusedPresContext) {
          if (gLastFocusedContent) {
            // Retrieve this content node's pres context. it can be out of sync
            // in the Ender widget case.
            nsCOMPtr<nsIDocument> doc = gLastFocusedContent->GetDocument();
            if (doc) {
              nsIPresShell *shell = doc->GetShellAt(0);
              if (shell) {
                nsCOMPtr<nsPresContext> oldPresContext =
                  shell->GetPresContext();

                nsCOMPtr<nsIEventStateManager> esm =
                  oldPresContext->EventStateManager();
                esm->SetFocusedContent(gLastFocusedContent);
                nsEventDispatcher::Dispatch(gLastFocusedContent, oldPresContext,
                                            &event, nsnull, &status);
                esm->SetFocusedContent(nsnull);
                NS_IF_RELEASE(gLastFocusedContent);
              }
            }
          }

          // fire blur on document and window
          if (gLastFocusedDocument) {
            // get the window here, in case the event causes
            // gLastFocusedDocument to change.

            nsCOMPtr<nsPIDOMWindow> window(gLastFocusedDocument->GetWindow());

            event.target = nsnull;
            nsEventDispatcher::Dispatch(gLastFocusedDocument,
                                        gLastFocusedPresContext,
                                        &event, nsnull, &status);

            if (window) {
              event.target = nsnull;
              nsEventDispatcher::Dispatch(window, gLastFocusedPresContext,
                                          &event, nsnull, &status);
            }
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

      nsCOMPtr<nsPIDOMWindow> win = mDocument->GetWindow();

      if (!win) {
        NS_ERROR("win is null.  this happens [often on xlib builds].  see bug #79213");
        return NS_ERROR_NULL_POINTER;
      }

      // Hold a strong ref to the focus controller, since we need
      // it after event dispatch.
      nsCOMPtr<nsIFocusController> focusController =
        win->GetRootFocusController();
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

      NS_ASSERTION(focusedWindow,"check why focusedWindow is null!!!");

      // Focus the DOM window.
      if (focusedWindow) {
        focusedWindow->Focus();

        nsCOMPtr<nsIDocument> document = GetDocumentFromWindow(focusedWindow);

        if (document) {
          // Use a strong ref to make sure that the shell is alive still
          // when calling FrameSelection().
          nsCOMPtr<nsIPresShell> shell = document->GetShellAt(0);
          NS_ASSERTION(shell, "Focus events should not be getting thru when this is null!");
          if (shell) {
            nsPresContext* context = shell->GetPresContext();
            nsIMEStateManager::OnActivate(context);
            if (focusedElement) {
              nsCOMPtr<nsIContent> focusContent = do_QueryInterface(focusedElement);
              focusContent->SetFocus(context);
            } else {
              nsIMEStateManager::OnChangeFocus(context, nsnull);
            }

            // disable selection mousedown state on activation
            shell->FrameSelection()->SetMouseDownState(PR_FALSE);
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

      nsIMEStateManager::OnDeactivate(aPresContext);

      nsCOMPtr<nsPIDOMWindow> ourWindow(mDocument->GetWindow());

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
        nsEvent event(PR_TRUE, NS_BLUR_CONTENT);

        if (gLastFocusedContent) {
          nsIPresShell *shell = gLastFocusedDocument->GetShellAt(0);
          if (shell) {
            nsCOMPtr<nsPresContext> oldPresContext = shell->GetPresContext();

            nsCOMPtr<nsIDOMElement> focusedElement;
            if (focusController)
              focusController->GetFocusedElement(getter_AddRefs(focusedElement));

            nsCOMPtr<nsIEventStateManager> esm;
            esm = oldPresContext->EventStateManager();
            esm->SetFocusedContent(gLastFocusedContent);

            nsCOMPtr<nsIContent> focusedContent = do_QueryInterface(focusedElement);
            if (focusedContent) {
              // Blur the element.
              nsEventDispatcher::Dispatch(focusedContent, oldPresContext,
                                          &event, nsnull, &status);
            }

            esm->SetFocusedContent(nsnull);
            NS_IF_RELEASE(gLastFocusedContent);
          }
        }

        // fire blur on document and window
        event.target = nsnull;
        nsEventDispatcher::Dispatch(mDocument, aPresContext, &event, nsnull,
                                    &status);

        if (ourWindow) {
          event.target = nsnull;
          nsEventDispatcher::Dispatch(ourWindow, aPresContext, &event, nsnull,
                                      &status);
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

      PRInt32 modifierMask = 0;
      if (keyEvent->isShift)
        modifierMask |= NS_MODIFIER_SHIFT;
      if (keyEvent->isControl)
        modifierMask |= NS_MODIFIER_CONTROL;
      if (keyEvent->isAlt)
        modifierMask |= NS_MODIFIER_ALT;
      if (keyEvent->isMeta)
        modifierMask |= NS_MODIFIER_META;

      // Prevent keyboard scrolling while an accesskey modifier is in use.
      if (modifierMask && (modifierMask == sChromeAccessModifier ||
                           modifierMask == sContentAccessModifier))
        HandleAccessKey(aPresContext, keyEvent, aStatus, -1,
                        eAccessKeyProcessingNormal, modifierMask);
    }
  case NS_KEY_DOWN:
  case NS_KEY_UP:
    {
      if (mCurrentFocus) {
        mCurrentTargetContent = mCurrentFocus;
      }
    }
    break;
  case NS_MOUSE_SCROLL:
    {
      if (mCurrentFocus) {
        mCurrentTargetContent = mCurrentFocus;
      }

      nsMouseScrollEvent* msEvent = NS_STATIC_CAST(nsMouseScrollEvent*, aEvent);

      NS_NAMED_LITERAL_CSTRING(actionslot,      ".action");
      NS_NAMED_LITERAL_CSTRING(numlinesslot,    ".numlines");
      NS_NAMED_LITERAL_CSTRING(sysnumlinesslot, ".sysnumlines");

      nsCAutoString baseKey;
      GetBasePrefKeyForMouseWheel(msEvent, baseKey);

      nsCAutoString sysNumLinesKey(baseKey);
      sysNumLinesKey.Append(sysnumlinesslot);
      PRBool useSysNumLines = nsContentUtils::GetBoolPref(sysNumLinesKey.get());

      nsCAutoString actionKey(baseKey);
      actionKey.Append(actionslot);
      PRInt32 action = nsContentUtils::GetIntPref(actionKey.get());

      if (!useSysNumLines) {
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
        PRInt32 numLines = nsContentUtils::GetIntPref(numLinesKey.get());

        PRBool swapDirs = (numLines < 0);
        PRInt32 userSize = swapDirs ? -numLines : numLines;

        PRBool deltaUp = (msEvent->delta < 0);
        if (swapDirs) {
          deltaUp = !deltaUp;
        }

        msEvent->delta = deltaUp ? -userSize : userSize;
      }
      if ((useSysNumLines &&
           (msEvent->scrollFlags & nsMouseScrollEvent::kIsFullPage)) ||
          action == MOUSE_SCROLL_PAGE) {
          msEvent->delta = (msEvent->delta > 0)
            ? nsIDOMNSUIEvent::SCROLL_PAGE_DOWN
            : nsIDOMNSUIEvent::SCROLL_PAGE_UP;
      }
    }
    break;
  }
  return NS_OK;
}

static PRInt32
GetAccessModifierMask(nsISupports* aDocShell)
{
  nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(aDocShell));
  if (!treeItem)
    return -1; // invalid modifier

  PRInt32 itemType;
  treeItem->GetItemType(&itemType);
  switch (itemType) {

  case nsIDocShellTreeItem::typeChrome:
    return sChromeAccessModifier;

  case nsIDocShellTreeItem::typeContent:
    return sContentAccessModifier;

  default:
    return -1; // invalid modifier
  }
}

// Note: for the in parameter aChildOffset,
// -1 stands for not bubbling from the child docShell
// 0 -- childCount - 1 stands for the child docShell's offset
// which bubbles up the access key handling
void
nsEventStateManager::HandleAccessKey(nsPresContext* aPresContext,
                                     nsKeyEvent *aEvent,
                                     nsEventStatus* aStatus,
                                     PRInt32 aChildOffset,
                                     ProcessingAccessKeyState aAccessKeyState,
                                     PRInt32 aModifierMask)
{
  nsCOMPtr<nsISupports> pcContainer = aPresContext->GetContainer();

  // Alt or other accesskey modifier is down, we may need to do an accesskey
  if (mAccessKeys && aModifierMask == GetAccessModifierMask(pcContainer)) {
    // Someone registered an accesskey.  Find and activate it.
    PRUint32 accKey = (IS_IN_BMP(aEvent->charCode)) ? 
      ToLowerCase((PRUnichar)aEvent->charCode) : aEvent->charCode;

    nsVoidKey key(NS_INT32_TO_PTR(accKey));
    if (mAccessKeys->Exists(&key)) {
      nsCOMPtr<nsIContent> content =
        dont_AddRef(NS_STATIC_CAST(nsIContent*, mAccessKeys->Get(&key)));
      content->PerformAccesskey(sKeyCausesActivation,
                                NS_IS_TRUSTED_EVENT(aEvent));
      *aStatus = nsEventStatus_eConsumeNoDefault;
    }
  }

  // after the local accesskey handling
  if (nsEventStatus_eConsumeNoDefault != *aStatus) {
    // checking all sub docshells

    nsCOMPtr<nsIDocShellTreeNode> docShell(do_QueryInterface(pcContainer));
    if (!docShell) {
      NS_WARNING("no docShellTreeNode for presContext");
      return;
    }

    PRInt32 childCount;
    docShell->GetChildCount(&childCount);
    for (PRInt32 counter = 0; counter < childCount; counter++) {
      // Not processing the child which bubbles up the handling
      if (aAccessKeyState == eAccessKeyProcessingUp && counter == aChildOffset)
        continue;

      nsCOMPtr<nsIDocShellTreeItem> subShellItem;
      nsCOMPtr<nsIPresShell> subPS;
      nsCOMPtr<nsPresContext> subPC;

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

        nsPresContext *subPC = subPS->GetPresContext();

        nsEventStateManager* esm =
          NS_STATIC_CAST(nsEventStateManager *, subPC->EventStateManager());

        if (esm)
          esm->HandleAccessKey(subPC, aEvent, aStatus, -1,
                               eAccessKeyProcessingDown, aModifierMask);

        if (nsEventStatus_eConsumeNoDefault == *aStatus)
          break;
      }
    }
  }// if end . checking all sub docshell ends here.

  // bubble up the process to the parent docShell if necessary
  if (eAccessKeyProcessingDown != aAccessKeyState && nsEventStatus_eConsumeNoDefault != *aStatus) {
    nsCOMPtr<nsIDocShellTreeItem> docShell(do_QueryInterface(pcContainer));
    if (!docShell) {
      NS_WARNING("no docShellTreeNode for presContext");
      return;
    }

    nsCOMPtr<nsIDocShellTreeItem> parentShellItem;
    docShell->GetParent(getter_AddRefs(parentShellItem));
    nsCOMPtr<nsIDocShell> parentDS = do_QueryInterface(parentShellItem);
    if (parentDS) {
      PRInt32 myOffset;
      docShell->GetChildOffset(&myOffset);

      nsCOMPtr<nsIPresShell> parentPS;

      parentDS->GetPresShell(getter_AddRefs(parentPS));
      NS_ASSERTION(parentPS, "Our PresShell exists but the parent's does not?");

      nsPresContext *parentPC = parentPS->GetPresContext();
      NS_ASSERTION(parentPC, "PresShell without PresContext");

      nsEventStateManager* esm =
        NS_STATIC_CAST(nsEventStateManager *, parentPC->EventStateManager());

      if (esm)
        esm->HandleAccessKey(parentPC, aEvent, aStatus, myOffset,
                             eAccessKeyProcessingUp, aModifierMask);
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
nsEventStateManager::CreateClickHoldTimer(nsPresContext* inPresContext,
                                          nsIFrame* inDownFrame,
                                          nsGUIEvent* inMouseDownEvent)
{
  if (!NS_IS_TRUSTED_EVENT(inMouseDownEvent))
    return;

  // just to be anal (er, safe)
  if (mClickHoldTimer) {
    mClickHoldTimer->Cancel();
    mClickHoldTimer = nsnull;
  }

  // if content clicked on has a popup, don't even start the timer
  // since we'll end up conflicting and both will show.
  if (mGestureDownContent) {
    // check for the |popup| attribute
    if (nsContentUtils::HasNonEmptyAttr(mGestureDownContent, kNameSpaceID_None,
                                        nsGkAtoms::popup))
      return;
    
    // check for a <menubutton> like bookmarks
    if (mGestureDownContent->Tag() == nsGkAtoms::menubutton)
      return;
  }

  mClickHoldTimer = do_CreateInstance("@mozilla.org/timer;1");
  if ( mClickHoldTimer )
    mClickHoldTimer->InitWithFuncCallback(sClickHoldCallback, this,
                                          kClickHoldDelay,
                                          nsITimer::TYPE_ONE_SHOT);
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
}


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
  if ( !mGestureDownContent )
    return;

#ifdef XP_MACOSX
  // hacky OS call to ensure that we don't show a context menu when the user
  // let go of the mouse already, after a long, cpu-hogging operation prevented
  // us from handling any OS events. See bug 117589.
  if (!::StillDown())
    return;
#endif

  nsEventStatus status = nsEventStatus_eIgnore;

  // Dispatch to the DOM. We have to fake out the ESM and tell it that the
  // current target frame is actually where the mouseDown occurred, otherwise it
  // will use the frame the mouse is currently over which may or may not be
  // the same. (Note: saari and I have decided that we don't have to reset |mCurrentTarget|
  // when we're through because no one else is doing anything more with this
  // event and it will get reset on the very next event to the correct frame).
  mCurrentTarget = nsnull;
  nsIPresShell *shell = mPresContext->GetPresShell();
  if ( shell ) {
    mCurrentTarget = shell->GetPrimaryFrameFor(mGestureDownFrameOwner);

    if ( mCurrentTarget ) {
      NS_ASSERTION(mPresContext == mCurrentTarget->GetPresContext(),
                   "a prescontext returned a primary frame that didn't belong to it?");

      // before dispatching, check that we're not on something that
      // doesn't get a context menu
      nsIAtom *tag = mGestureDownContent->Tag();
      PRBool allowedToDispatch = PR_TRUE;

      if (mGestureDownContent->IsNodeOfType(nsINode::eXUL)) {
        if (tag == nsGkAtoms::scrollbar ||
            tag == nsGkAtoms::scrollbarbutton ||
            tag == nsGkAtoms::button)
          allowedToDispatch = PR_FALSE;
        else if (tag == nsGkAtoms::toolbarbutton) {
          // a <toolbarbutton> that has the container attribute set
          // will already have its own dropdown.
          if (nsContentUtils::HasNonEmptyAttr(mGestureDownContent,
                  kNameSpaceID_None, nsGkAtoms::container)) {
            allowedToDispatch = PR_FALSE;
          } else {
            // If the toolbar button has an open menu, don't attempt to open
            // a second menu
            if (mGestureDownContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::open,
                                                 nsGkAtoms::_true, eCaseMatters)) {
              allowedToDispatch = PR_FALSE;
            }
          }
        }
      }
      else if (mGestureDownContent->IsNodeOfType(nsINode::eHTML)) {
        nsCOMPtr<nsIFormControl> formCtrl(do_QueryInterface(mGestureDownContent));

        if (formCtrl) {
          // of all form controls, only ones dealing with text are
          // allowed to have context menus
          PRInt32 type = formCtrl->GetType();

          allowedToDispatch = (type == NS_FORM_INPUT_TEXT ||
                               type == NS_FORM_INPUT_PASSWORD ||
                               type == NS_FORM_INPUT_FILE ||
                               type == NS_FORM_TEXTAREA);
        }
        else if (tag == nsGkAtoms::applet ||
                 tag == nsGkAtoms::embed  ||
                 tag == nsGkAtoms::object) {
          allowedToDispatch = PR_FALSE;
        }
      }

      if (allowedToDispatch) {
        // make sure the widget sticks around
        nsCOMPtr<nsIWidget> targetWidget(mCurrentTarget->GetWindow());
        // init the event while mCurrentTarget is still good
        nsMouseEvent event(PR_TRUE, NS_CONTEXTMENU,
                           targetWidget,
                           nsMouseEvent::eReal);
        event.clickCount = 1;
        FillInEventFromGestureDown(&event);
        
        // stop selection tracking, we're in control now
        if (mCurrentTarget)
        {
          nsFrameSelection* frameSel = mCurrentTarget->GetFrameSelection();
        
          if (frameSel && frameSel->GetMouseDownState()) {
            // note that this can cause selection changed events to fire if we're in
            // a text field, which will null out mCurrentTarget
            frameSel->SetMouseDownState(PR_FALSE);
          }
        }

        // dispatch to DOM
        nsEventDispatcher::Dispatch(mGestureDownContent, mPresContext, &event,
                                    nsnull, &status);

        // We don't need to dispatch to frame handling because no frames
        // watch NS_CONTEXTMENU except for nsMenuFrame and that's only for
        // dismissal. That's just as well since we don't really know
        // which frame to send it to.
      }
    }
  }

  // now check if the event has been handled. If so, stop tracking a drag
  if ( status == nsEventStatus_eConsumeNoDefault ) {
    StopTrackingDragGesture();
  }

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
nsEventStateManager::BeginTrackingDragGesture(nsPresContext* aPresContext,
                                              nsMouseEvent* inDownEvent,
                                              nsIFrame* inDownFrame)
{
  // Note that |inDownEvent| could be either a mouse down event or a
  // synthesized mouse move event.
  nsRect screenPt;
  inDownEvent->widget->WidgetToScreen(nsRect(inDownEvent->refPoint, nsSize(1, 1)),
                                      screenPt);
  mGestureDownPoint = screenPt.TopLeft();

  inDownFrame->GetContentForEvent(aPresContext, inDownEvent,
                                  getter_AddRefs(mGestureDownContent));

  mGestureDownFrameOwner = inDownFrame->GetContent();
  mGestureDownShift = inDownEvent->isShift;
  mGestureDownControl = inDownEvent->isControl;
  mGestureDownAlt = inDownEvent->isAlt;
  mGestureDownMeta = inDownEvent->isMeta;

#ifdef CLICK_HOLD_CONTEXT_MENUS
  // fire off a timer to track click-hold
  if (nsContentUtils::GetBoolPref("ui.click_hold_context_menus", PR_TRUE))
    CreateClickHoldTimer ( aPresContext, inDownFrame, inDownEvent );
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
  mGestureDownContent = nsnull;
  mGestureDownFrameOwner = nsnull;
}

void
nsEventStateManager::FillInEventFromGestureDown(nsMouseEvent* aEvent)
{
  NS_ASSERTION(aEvent->widget == mCurrentTarget->GetWindow(),
               "Incorrect widget in event");

  // Set the coordinates in the new event to the coordinates of
  // the old event, adjusted for the fact that the widget might be
  // different
  nsRect tmpRect(0, 0, 1, 1);
  aEvent->widget->WidgetToScreen(tmpRect, tmpRect);
  aEvent->refPoint = mGestureDownPoint - tmpRect.TopLeft();
  aEvent->isShift = mGestureDownShift;
  aEvent->isControl = mGestureDownControl;
  aEvent->isAlt = mGestureDownAlt;
  aEvent->isMeta = mGestureDownMeta;
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
// a drag too early, or very similar which might not trigger a drag.
//
// Do we need to do anything about this? Let's wait and see.
//
void
nsEventStateManager::GenerateDragGesture(nsPresContext* aPresContext,
                                         nsMouseEvent *aEvent)
{
  NS_ASSERTION(aPresContext, "This shouldn't happen.");
  if ( IsTrackingDragGesture() ) {
    mCurrentTarget = aPresContext->GetPresShell()->GetPrimaryFrameFor(mGestureDownFrameOwner);

    if (!mCurrentTarget) {
      StopTrackingDragGesture();
      return;
    }

    // Check if selection is tracking drag gestures, if so
    // don't interfere!
    if (mCurrentTarget)
    {
      nsFrameSelection* frameSel = mCurrentTarget->GetFrameSelection();
      if (frameSel && frameSel->GetMouseDownState()) {
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

    // fire drag gesture if mouse has moved enough
    nsRect tmpRect;
    aEvent->widget->WidgetToScreen(nsRect(aEvent->refPoint, nsSize(1, 1)),
                                   tmpRect);
    nsPoint pt = tmpRect.TopLeft();
    if (PR_ABS(pt.x - mGestureDownPoint.x) > pixelThresholdX ||
        PR_ABS(pt.y - mGestureDownPoint.y) > pixelThresholdY) {
#ifdef CLICK_HOLD_CONTEXT_MENUS
      // stop the click-hold before we fire off the drag gesture, in case
      // it takes a long time
      KillClickHoldTimer();
#endif

      nsCOMPtr<nsIContent> targetContent = mGestureDownContent;
      // Stop tracking the drag gesture now. This should stop us from
      // reentering GenerateDragGesture inside DOM event processing.
      StopTrackingDragGesture();

      // get the widget from the target frame
      nsEventStatus status = nsEventStatus_eIgnore;
      nsMouseEvent event(NS_IS_TRUSTED_EVENT(aEvent), NS_DRAGDROP_GESTURE,
                         mCurrentTarget->GetWindow(), nsMouseEvent::eReal);
      FillInEventFromGestureDown(&event);

      // Dispatch to the DOM. By setting mCurrentTarget we are faking
      // out the ESM and telling it that the current target frame is
      // actually where the mouseDown occurred, otherwise it will use
      // the frame the mouse is currently over which may or may not be
      // the same. (Note: saari and I have decided that we don't have
      // to reset |mCurrentTarget| when we're through because no one
      // else is doing anything more with this event and it will get
      // reset on the very next event to the correct frame).

      // Hold onto old target content through the event and reset after.
      nsCOMPtr<nsIContent> targetBeforeEvent = mCurrentTargetContent;

      // Set the current target to the content for the mouse down
      mCurrentTargetContent = targetContent;

      // Dispatch to DOM
      nsEventDispatcher::Dispatch(targetContent, aPresContext, &event, nsnull,
                                  &status);

      // Note that frame event handling doesn't care about NS_DRAGDROP_GESTURE,
      // which is just as well since we don't really know which frame to
      // send it to

      // Reset mCurretTargetContent to what it was
      mCurrentTargetContent = targetBeforeEvent;
    }

    // Now flush all pending notifications, for better responsiveness
    // while dragging.
    FlushPendingEvents(aPresContext);
  }
} // GenerateDragGesture

nsresult
nsEventStateManager::ChangeTextSize(PRInt32 change)
{
  if(!gLastFocusedDocument) return NS_ERROR_FAILURE;

  nsPIDOMWindow* ourWindow = gLastFocusedDocument->GetWindow();
  if(!ourWindow) return NS_ERROR_FAILURE;

  nsIDOMWindowInternal *rootWindow = ourWindow->GetPrivateRoot();
  if(!rootWindow) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMWindow> contentWindow;
  rootWindow->GetContent(getter_AddRefs(contentWindow));
  if(!contentWindow) return NS_ERROR_FAILURE;

  nsIDocument *doc = GetDocumentFromWindow(contentWindow);
  if(!doc) return NS_ERROR_FAILURE;

  nsIPresShell *presShell = doc->GetShellAt(0);
  if(!presShell) return NS_ERROR_FAILURE;
  nsPresContext *presContext = presShell->GetPresContext();
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
      !content->IsNodeOfType(nsINode::eHTML_FORM_CONTROL) &&
      !content->IsNodeOfType(nsINode::eXUL))
    {
      // positive adjustment to increase text size, non-positive to decrease
      ChangeTextSize((adjustment > 0) ? 1 : -1);
    }
}

static nsIFrame*
GetParentFrameToScroll(nsPresContext* aPresContext, nsIFrame* aFrame)
{
  if (!aPresContext || !aFrame)
    return nsnull;

  if (aFrame->GetStyleDisplay()->mPosition == NS_STYLE_POSITION_FIXED)
    return aPresContext->GetPresShell()->GetRootScrollFrame();

  return aFrame->GetParent();
}

nsresult
nsEventStateManager::DoScrollText(nsPresContext* aPresContext,
                                  nsIFrame* aTargetFrame,
                                  nsInputEvent* aEvent,
                                  PRInt32 aNumLines,
                                  PRBool aScrollHorizontal,
                                  ScrollQuantity aScrollQuantity)
{
  nsIScrollableView* scrollView = nsnull;
  nsIFrame* scrollFrame = aTargetFrame;

  // If the user recently scrolled with the mousewheel, then they probably want
  // to scroll the same view as before instead of the view under the cursor.
  // nsMouseWheelTransaction tracks the frame currently being scrolled with the
  // mousewheel. We consider the transaction ended when the mouse moves more than
  // "mousewheel.transaction.ignoremovedelay" milliseconds after the last scroll
  // operation, or any time the mouse moves out of the frame, or when more than
  // "mousewheel.transaction.timeout" milliseconds have passed after the last
  // operation, even if the mouse hasn't moved.
  nsIFrame* lastScrollFrame = nsMouseWheelTransaction::GetTargetFrame();
  if (lastScrollFrame) {
    nsCOMPtr<nsIScrollableViewProvider> svp =
      do_QueryInterface(lastScrollFrame);
    if (svp) {
      scrollView = svp->GetScrollableView();
      nsMouseWheelTransaction::UpdateTransaction();
    } else {
      nsMouseWheelTransaction::EndTransaction();
      lastScrollFrame = nsnull;
    }
  }
  PRBool passToParent = lastScrollFrame ? PR_FALSE : PR_TRUE;

  for (; scrollFrame && passToParent;
       scrollFrame = GetParentFrameToScroll(aPresContext, scrollFrame)) {
    // Check whether the frame wants to provide us with a scrollable view.
    scrollView = nsnull;
    nsCOMPtr<nsIScrollableViewProvider> svp = do_QueryInterface(scrollFrame);
    if (svp) {
      scrollView = svp->GetScrollableView();
    }
    if (!scrollView) {
      continue;
    }

    nsPresContext::ScrollbarStyles ss =
      nsLayoutUtils::ScrollbarStylesOfView(scrollView);
    if (NS_STYLE_OVERFLOW_HIDDEN ==
        (aScrollHorizontal ? ss.mHorizontal : ss.mVertical)) {
      continue;
    }

    // Check if the scrollable view can be scrolled any further.
    nscoord lineHeight;
    scrollView->GetLineHeight(&lineHeight);

    if (lineHeight != 0) {
      PRBool canScroll;
      nsresult rv = scrollView->CanScroll(aScrollHorizontal,
                                          (aNumLines > 0), canScroll);
      if (NS_SUCCEEDED(rv) && canScroll) {
        passToParent = PR_FALSE;
        nsMouseWheelTransaction::BeginTransaction(scrollFrame, aEvent);
      }

      // Comboboxes need special care.
      nsIComboboxControlFrame* comboBox = nsnull;
      CallQueryInterface(scrollFrame, &comboBox);
      if (comboBox) {
        if (comboBox->IsDroppedDown()) {
          // Don't propagate to parent when drop down menu is active.
          if (passToParent) {
            passToParent = PR_FALSE;
            scrollView = nsnull;
            nsMouseWheelTransaction::EndTransaction();
          }
        } else {
          // Always propagate when not dropped down (even if focused).
          passToParent = PR_TRUE;
        }
      }
    }
  }

  if (!passToParent && scrollView) {
    PRInt32 scrollX = 0;
    PRInt32 scrollY = aNumLines;

    if (aScrollQuantity == eScrollByPage)
      scrollY = (scrollY > 0) ? 1 : -1;
      
    if (aScrollHorizontal) {
      scrollX = scrollY;
      scrollY = 0;
    }
    
    if (aScrollQuantity == eScrollByPage)
      scrollView->ScrollByPages(scrollX, scrollY);
    else if (aScrollQuantity == eScrollByPixel)
      scrollView->ScrollByPixels(scrollX, scrollY);
    else
      scrollView->ScrollByLines(scrollX, scrollY);

    ForceViewUpdate(scrollView->View());
  }
  if (passToParent) {
    nsresult rv;
    nsIFrame* newFrame = nsnull;
    nsCOMPtr<nsPresContext> newPresContext;
    rv = GetParentScrollingView(aEvent, aPresContext, newFrame,
                                *getter_AddRefs(newPresContext));
    if (NS_SUCCEEDED(rv) && newFrame)
      return DoScrollText(newPresContext, newFrame, aEvent, aNumLines,
                          aScrollHorizontal, aScrollQuantity);
  }

  return NS_OK;
}

nsresult
nsEventStateManager::GetParentScrollingView(nsInputEvent *aEvent,
                                            nsPresContext* aPresContext,
                                            nsIFrame* &targetOuterFrame,
                                            nsPresContext* &presCtxOuter)
{
  targetOuterFrame = nsnull;

  if (!aEvent) return NS_ERROR_FAILURE;
  if (!aPresContext) return NS_ERROR_FAILURE;

  nsIDocument *doc = aPresContext->PresShell()->GetDocument();
  NS_ASSERTION(doc, "No document in prescontext!");

  nsIDocument *parentDoc = doc->GetParentDocument();

  if (!parentDoc) {
    return NS_OK;
  }

  nsIPresShell *pPresShell = nsnull;
  for (PRUint32 i = 0; i < parentDoc->GetNumberOfShells(); i++) {
    nsIPresShell *tmpPresShell = parentDoc->GetShellAt(i);
    NS_ENSURE_TRUE(tmpPresShell, NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(tmpPresShell->GetPresContext(), NS_ERROR_FAILURE);
    if (tmpPresShell->GetPresContext()->Type() == aPresContext->Type()) {
      pPresShell = tmpPresShell;
      break;
    }
  }
  if (!pPresShell)
    return NS_ERROR_FAILURE;

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

  nsIFrame* frameFrame = pPresShell->GetPrimaryFrameFor(frameContent);
  if (!frameFrame) return NS_ERROR_FAILURE;

  NS_IF_ADDREF(presCtxOuter = pPresShell->GetPresContext());
  targetOuterFrame = frameFrame;

  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::PostHandleEvent(nsPresContext* aPresContext,
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
  //Keep the prescontext alive, we might need it after event dispatch
  nsRefPtr<nsPresContext> presContext = aPresContext;

  NS_ASSERTION(mCurrentTarget, "mCurrentTarget is null");
  if (!mCurrentTarget) return NS_ERROR_NULL_POINTER;

  switch (aEvent->message) {
  case NS_MOUSE_BUTTON_DOWN:
    {
      if (NS_STATIC_CAST(nsMouseEvent*, aEvent)->button == nsMouseEvent::eLeftButton &&
          !mNormalLMouseEventInProcess) {
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

          PRInt32 tabIndexUnused;
          if (currFrame->IsFocusable(&tabIndexUnused, PR_TRUE)) {
            newFocus = currFrame->GetContent();
            nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(newFocus));
            if (domElement)
              break;
          }
          currFrame = currFrame->GetParent();
        }

        if (newFocus && currFrame)
          ChangeFocusWith(newFocus, eEventFocusedByMouse);
        else if (!suppressBlur) {
          SetContentState(nsnull, NS_EVENT_STATE_FOCUS);
        }

        // The rest is left button-specific.
        if (NS_STATIC_CAST(nsMouseEvent*, aEvent)->button !=
            nsMouseEvent::eLeftButton)
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
  case NS_MOUSE_BUTTON_UP:
    {
      SetContentState(nsnull, NS_EVENT_STATE_ACTIVE);
      if (!mCurrentTarget) {
        nsIFrame* targ;
        GetEventTarget(&targ);
        if (!targ) return NS_ERROR_FAILURE;
      }
      ret = CheckForAndDispatchClick(presContext, (nsMouseEvent*)aEvent, aStatus);
      nsIPresShell *shell = presContext->GetPresShell();
      if (shell) {
        shell->FrameSelection()->SetMouseDownState(PR_FALSE);
      }
    }
    break;
  case NS_MOUSE_SCROLL:
    if (nsEventStatus_eConsumeNoDefault != *aStatus) {

      // Build the preference keys, based on the event properties.
      nsMouseScrollEvent *msEvent = (nsMouseScrollEvent*) aEvent;

      NS_NAMED_LITERAL_CSTRING(actionslot,      ".action");
      NS_NAMED_LITERAL_CSTRING(sysnumlinesslot, ".sysnumlines");

      nsCAutoString baseKey;
      GetBasePrefKeyForMouseWheel(msEvent, baseKey);

      // Extract the preferences
      nsCAutoString actionKey(baseKey);
      actionKey.Append(actionslot);

      nsCAutoString sysNumLinesKey(baseKey);
      sysNumLinesKey.Append(sysnumlinesslot);

      PRInt32 action = nsContentUtils::GetIntPref(actionKey.get());
      PRBool useSysNumLines =
        nsContentUtils::GetBoolPref(sysNumLinesKey.get());

      if (useSysNumLines) {
        if (msEvent->scrollFlags & nsMouseScrollEvent::kIsFullPage)
          action = MOUSE_SCROLL_PAGE;
        else if (msEvent->scrollFlags & nsMouseScrollEvent::kIsPixels)
          action = MOUSE_SCROLL_PIXELS;
      }

      switch (action) {
      case MOUSE_SCROLL_N_LINES:
        {
          DoScrollText(presContext, aTargetFrame, msEvent, msEvent->delta,
                       (msEvent->scrollFlags & nsMouseScrollEvent::kIsHorizontal),
                       eScrollByLine);
        }
        break;

      case MOUSE_SCROLL_PAGE:
        {
          DoScrollText(presContext, aTargetFrame, msEvent, msEvent->delta,
                       (msEvent->scrollFlags & nsMouseScrollEvent::kIsHorizontal),
                       eScrollByPage);
        }
        break;

      case MOUSE_SCROLL_PIXELS:
        {
          DoScrollText(presContext, aTargetFrame, msEvent, msEvent->delta,
                       (msEvent->scrollFlags & nsMouseScrollEvent::kIsHorizontal),
                       eScrollByPixel);
        }
        break;

      case MOUSE_SCROLL_HISTORY:
        {
          DoScrollHistory(msEvent->delta);
        }
        break;

      case MOUSE_SCROLL_TEXTSIZE:
        {
          DoScrollTextsize(aTargetFrame, msEvent->delta);
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
    GenerateDragDropEnterExit(presContext, (nsGUIEvent*)aEvent);
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
            if (!((nsInputEvent*)aEvent)->isControl) {
              //Shift focus forward or back depending on shift key
              ShiftFocus(!((nsInputEvent*)aEvent)->isShift);
            } else {
              ShiftFocusByDoc(!((nsInputEvent*)aEvent)->isShift);
            }
            *aStatus = nsEventStatus_eConsumeNoDefault;
            break;

          case NS_VK_F6:
            //Shift focus forward or back depending on shift key
            ShiftFocusByDoc(!((nsInputEvent*)aEvent)->isShift);
            *aStatus = nsEventStatus_eConsumeNoDefault;
            break;

//the problem is that viewer does not have xul so we cannot completely eliminate these
#if NON_KEYBINDING
          case NS_VK_PAGE_DOWN:
          case NS_VK_PAGE_UP:
            if (!mCurrentFocus) {
              nsIScrollableView* sv = nsLayoutUtils::GetNearestScrollingView(aView, nsLayoutUtils::eVertical);
              if (sv) {
                nsKeyEvent * keyEvent = (nsKeyEvent *)aEvent;
                sv->ScrollByPages(0, (keyEvent->keyCode != NS_VK_PAGE_UP) ? 1 : -1);
              }
            }
            break;
          case NS_VK_HOME:
          case NS_VK_END:
            if (!mCurrentFocus) {
              nsIScrollableView* sv = nsLayoutUtils::GetNearestScrollingView(aView, nsLayoutUtils::eVertical);
              if (sv) {
                nsKeyEvent * keyEvent = (nsKeyEvent *)aEvent;
                sv->ScrollByWhole((keyEvent->keyCode != NS_VK_HOME) ? PR_FALSE : PR_TRUE);
              }
            }
            break;
          case NS_VK_DOWN:
          case NS_VK_UP:
            if (!mCurrentFocus) {
              nsIScrollableView* sv = nsLayoutUtils::GetNearestScrollingView(aView, nsLayoutUtils::eVertical);
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
              nsIScrollableView* sv = nsLayoutUtils::GetNearestScrollingView(aView, nsLayoutUtils::eHorizontal);
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
                nsIScrollableView* sv = nsLayoutUtils::GetNearestScrollingView(aView, nsLayoutUtils::eVertical);
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
      mCurrentTarget->GetContentForEvent(presContext, aEvent,
                                         getter_AddRefs(targetContent));
      SetContentState(targetContent, NS_EVENT_STATE_HOVER);
    }
    break;
  }

  //Reset target frame to null to avoid mistargeting after reentrant event
  mCurrentTarget = nsnull;

  return ret;
}

NS_IMETHODIMP
nsEventStateManager::NotifyDestroyPresContext(nsPresContext* aPresContext)
{
  nsIMEStateManager::OnDestroyPresContext(aPresContext);
  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::SetPresContext(nsPresContext* aPresContext)
{
  if (aPresContext == nsnull) {
    // XXX should we move this block to |NotifyDestroyPresContext|?
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
  if (aFrame && aFrame == mCurrentTarget) {
    mCurrentTargetContent = aFrame->GetContent();
  }

  return NS_OK;
}

void
nsEventStateManager::UpdateCursor(nsPresContext* aPresContext,
                                  nsEvent* aEvent, nsIFrame* aTargetFrame,
                                  nsEventStatus* aStatus)
{
  PRInt32 cursor = NS_STYLE_CURSOR_DEFAULT;
  imgIContainer* container = nsnull;
  PRBool haveHotspot = PR_FALSE;
  float hotspotX = 0.0f, hotspotY = 0.0f;

  //If cursor is locked just use the locked one
  if (mLockCursor) {
    cursor = mLockCursor;
  }
  //If not locked, look for correct cursor
  else if (aTargetFrame) {
      nsIFrame::Cursor framecursor;
      nsPoint pt = nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent,
                                                                aTargetFrame);
      if (NS_FAILED(aTargetFrame->GetCursor(pt, framecursor)))
        return;  // don't update the cursor if we failed to get it from the frame see bug 118877
      cursor = framecursor.mCursor;
      container = framecursor.mContainer;
      haveHotspot = framecursor.mHaveHotspot;
      hotspotX = framecursor.mHotspotX;
      hotspotY = framecursor.mHotspotY;
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
    container = nsnull;
  }

  if (aTargetFrame) {
    SetCursor(cursor, container, haveHotspot, hotspotX, hotspotY,
              aTargetFrame->GetWindow(), PR_FALSE);
  }

  if (mLockCursor || NS_STYLE_CURSOR_AUTO != cursor) {
    *aStatus = nsEventStatus_eConsumeDoDefault;
  }
}

NS_IMETHODIMP
nsEventStateManager::SetCursor(PRInt32 aCursor, imgIContainer* aContainer,
                               PRBool aHaveHotspot,
                               float aHotspotX, float aHotspotY,
                               nsIWidget* aWidget, PRBool aLockCursor)
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
    c = eCursor_n_resize;
    break;
  case NS_STYLE_CURSOR_S_RESIZE:
    c = eCursor_s_resize;
    break;
  case NS_STYLE_CURSOR_W_RESIZE:
    c = eCursor_w_resize;
    break;
  case NS_STYLE_CURSOR_E_RESIZE:
    c = eCursor_e_resize;
    break;
  case NS_STYLE_CURSOR_NW_RESIZE:
    c = eCursor_nw_resize;
    break;
  case NS_STYLE_CURSOR_SE_RESIZE:
    c = eCursor_se_resize;
    break;
  case NS_STYLE_CURSOR_NE_RESIZE:
    c = eCursor_ne_resize;
    break;
  case NS_STYLE_CURSOR_SW_RESIZE:
    c = eCursor_sw_resize;
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
  case NS_STYLE_CURSOR_MOZ_ZOOM_IN:
    c = eCursor_zoom_in;
    break;
  case NS_STYLE_CURSOR_MOZ_ZOOM_OUT:
    c = eCursor_zoom_out;
    break;
  case NS_STYLE_CURSOR_NOT_ALLOWED:
    c = eCursor_not_allowed;
    break;
  case NS_STYLE_CURSOR_COL_RESIZE:
    c = eCursor_col_resize;
    break;
  case NS_STYLE_CURSOR_ROW_RESIZE:
    c = eCursor_row_resize;
    break;
  case NS_STYLE_CURSOR_NO_DROP:
    c = eCursor_no_drop;
    break;
  case NS_STYLE_CURSOR_VERTICAL_TEXT:
    c = eCursor_vertical_text;
    break;
  case NS_STYLE_CURSOR_ALL_SCROLL:
    c = eCursor_all_scroll;
    break;
  case NS_STYLE_CURSOR_NESW_RESIZE:
    c = eCursor_nesw_resize;
    break;
  case NS_STYLE_CURSOR_NWSE_RESIZE:
    c = eCursor_nwse_resize;
    break;
  case NS_STYLE_CURSOR_NS_RESIZE:
    c = eCursor_ns_resize;
    break;
  case NS_STYLE_CURSOR_EW_RESIZE:
    c = eCursor_ew_resize;
    break;
  }

  // First, try the imgIContainer, if non-null
  nsresult rv = NS_ERROR_FAILURE;
  if (aContainer) {
    PRUint32 hotspotX, hotspotY;

    // css3-ui says to use the CSS-specified hotspot if present,
    // otherwise use the intrinsic hotspot, otherwise use the top left
    // corner.
    if (aHaveHotspot) {
      PRInt32 imgWidth, imgHeight;
      aContainer->GetWidth(&imgWidth);
      aContainer->GetHeight(&imgHeight);

      // XXX PR_MAX(NS_lround(x), 0)?
      hotspotX = aHotspotX > 0.0f
                   ? PRUint32(aHotspotX + 0.5f) : PRUint32(0);
      if (hotspotX >= PRUint32(imgWidth))
        hotspotX = imgWidth - 1;
      hotspotY = aHotspotY > 0.0f
                   ? PRUint32(aHotspotY + 0.5f) : PRUint32(0);
      if (hotspotY >= PRUint32(imgHeight))
        hotspotY = imgHeight - 1;
    } else {
      hotspotX = 0;
      hotspotY = 0;
      nsCOMPtr<nsIProperties> props(do_QueryInterface(aContainer));
      if (props) {
        nsCOMPtr<nsISupportsPRUint32> hotspotXWrap, hotspotYWrap;

        props->Get("hotspotX", NS_GET_IID(nsISupportsPRUint32), getter_AddRefs(hotspotXWrap));
        props->Get("hotspotY", NS_GET_IID(nsISupportsPRUint32), getter_AddRefs(hotspotYWrap));

        if (hotspotXWrap)
          hotspotXWrap->GetData(&hotspotX);
        if (hotspotYWrap)
          hotspotYWrap->GetData(&hotspotY);
      }
    }

    rv = aWidget->SetCursor(aContainer, hotspotX, hotspotY);
  }

  if (NS_FAILED(rv))
    aWidget->SetCursor(c);

  return NS_OK;
}

class nsESMEventCB : public nsDispatchingCallback
{
public:
  nsESMEventCB(nsIContent* aTarget) : mTarget(aTarget) {}

  virtual void HandleEvent(nsEventChainPostVisitor& aVisitor)
  {
    if (aVisitor.mPresContext) {
      nsIPresShell* shell = aVisitor.mPresContext->GetPresShell();
      if (shell) {
        nsIFrame* frame = shell->GetPrimaryFrameFor(mTarget);
        if (frame) {
          frame->HandleEvent(aVisitor.mPresContext,
                             (nsGUIEvent*) aVisitor.mEvent,
                             &aVisitor.mEventStatus);
        }
      }
    }
  }

  nsCOMPtr<nsIContent> mTarget;
};

nsIFrame*
nsEventStateManager::DispatchMouseEvent(nsGUIEvent* aEvent, PRUint32 aMessage,
                                        nsIContent* aTargetContent,
                                        nsIContent* aRelatedContent)
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event(NS_IS_TRUSTED_EVENT(aEvent), aMessage, aEvent->widget,
                     nsMouseEvent::eReal);
  event.refPoint = aEvent->refPoint;
  event.isShift = ((nsMouseEvent*)aEvent)->isShift;
  event.isControl = ((nsMouseEvent*)aEvent)->isControl;
  event.isAlt = ((nsMouseEvent*)aEvent)->isAlt;
  event.isMeta = ((nsMouseEvent*)aEvent)->isMeta;
  event.nativeMsg = ((nsMouseEvent*)aEvent)->nativeMsg;
  event.relatedTarget = aRelatedContent;

  mCurrentTargetContent = aTargetContent;

  nsIFrame* targetFrame = nsnull;
  if (aTargetContent) {
    nsESMEventCB callback(aTargetContent);
    nsEventDispatcher::Dispatch(aTargetContent, mPresContext, &event, nsnull,
                                &status, &callback);

    nsIPresShell *shell = mPresContext ? mPresContext->GetPresShell() : nsnull;
    if (shell) {
      // Although the primary frame was checked in event callback, 
      // it may not be the same object after event dispatching and handling.
      // So we need to refetch it.
      targetFrame = shell->GetPrimaryFrameFor(aTargetContent);
    }
  }

  mCurrentTargetContent = nsnull;

  return targetFrame;
}

void
nsEventStateManager::NotifyMouseOut(nsGUIEvent* aEvent, nsIContent* aMovingInto)
{
  if (!mLastMouseOverElement)
    return;
  // Before firing mouseout, check for recursion
  if (mLastMouseOverElement == mFirstMouseOutEventElement)
    return;

  if (mLastMouseOverFrame) {
    // if the frame is associated with a subdocument,
    // tell the subdocument that we're moving out of it
    nsIFrameFrame* subdocFrame;
    CallQueryInterface(mLastMouseOverFrame.GetFrame(), &subdocFrame);
    if (subdocFrame) {
      nsCOMPtr<nsIDocShell> docshell;
      subdocFrame->GetDocShell(getter_AddRefs(docshell));
      if (docshell) {
        nsCOMPtr<nsPresContext> presContext;
        docshell->GetPresContext(getter_AddRefs(presContext));
        
        if (presContext) {
          nsEventStateManager* kidESM =
            NS_STATIC_CAST(nsEventStateManager*, presContext->EventStateManager());
          // Not moving into any element in this subdocument
          kidESM->NotifyMouseOut(aEvent, nsnull);
        }
      }
    }
  }
  // That could have caused DOM events which could wreak havoc. Reverify
  // things and be careful.
  if (!mLastMouseOverElement)
    return;

  // Store the first mouseOut event we fire and don't refire mouseOut
  // to that element while the first mouseOut is still ongoing.
  mFirstMouseOutEventElement = mLastMouseOverElement;

  // Don't touch hover state if aMovingInto is non-null.  Caller will update
  // hover state itself, and we have optimizations for hover switching between
  // two nearby elements both deep in the DOM tree that would be defeated by
  // switching the hover state to null here.
  if (!aMovingInto) {
    // Unset :hover
    SetContentState(nsnull, NS_EVENT_STATE_HOVER);
  }
  
  // Fire mouseout
  DispatchMouseEvent(aEvent, NS_MOUSE_EXIT_SYNTH,
                     mLastMouseOverElement, aMovingInto);
  
  mLastMouseOverFrame = nsnull;
  mLastMouseOverElement = nsnull;
  
  // Turn recursion protection back off
  mFirstMouseOutEventElement = nsnull;
}

void
nsEventStateManager::NotifyMouseOver(nsGUIEvent* aEvent, nsIContent* aContent)
{
  NS_ASSERTION(aContent, "Mouse must be over something");

  if (mLastMouseOverElement == aContent)
    return;

  // Before firing mouseover, check for recursion
  if (aContent == mFirstMouseOverEventElement)
    return;

  // Check to see if we're a subdocument and if so update the parent
  // document's ESM state to indicate that the mouse is over the
  // content associated with our subdocument.
  EnsureDocument(mPresContext);
  nsIDocument *parentDoc = mDocument->GetParentDocument();
  if (parentDoc) {
    nsIContent *docContent = parentDoc->FindContentForSubDocument(mDocument);
    if (docContent) {
      nsIPresShell *parentShell = parentDoc->GetShellAt(0);
      if (parentShell) {
        nsEventStateManager* parentESM =
          NS_STATIC_CAST(nsEventStateManager*,
                         parentShell->GetPresContext()->EventStateManager());
        parentESM->NotifyMouseOver(aEvent, docContent);
      }
    }
  }
  // Firing the DOM event in the parent document could cause all kinds
  // of havoc.  Reverify and take care.
  if (mLastMouseOverElement == aContent)
    return;

  // Remember mLastMouseOverElement as the related content for the
  // DispatchMouseEvent() call below, since NotifyMouseOut() resets it, bug 298477.
  nsCOMPtr<nsIContent> lastMouseOverElement = mLastMouseOverElement;

  NotifyMouseOut(aEvent, aContent);

  // Store the first mouseOver event we fire and don't refire mouseOver
  // to that element while the first mouseOver is still ongoing.
  mFirstMouseOverEventElement = aContent;
  
  SetContentState(aContent, NS_EVENT_STATE_HOVER);
  
  // Fire mouseover
  mLastMouseOverFrame = DispatchMouseEvent(aEvent, NS_MOUSE_ENTER_SYNTH,
                                           aContent, lastMouseOverElement);
  mLastMouseOverElement = aContent;
  
  // Turn recursion protection back off
  mFirstMouseOverEventElement = nsnull;
}

void
nsEventStateManager::GenerateMouseEnterExit(nsGUIEvent* aEvent)
{
  EnsureDocument(mPresContext);
  if (!mDocument)
    return;

  // Hold onto old target content through the event and reset after.
  nsCOMPtr<nsIContent> targetBeforeEvent = mCurrentTargetContent;

  switch(aEvent->message) {
  case NS_MOUSE_MOVE:
    {
      // Get the target content target (mousemove target == mouseover target)
      nsCOMPtr<nsIContent> targetElement;
      GetEventTargetContent(aEvent, getter_AddRefs(targetElement));
      if (!targetElement) {
        // We're always over the document root, even if we're only
        // over dead space in a page (whose frame is not associated with
        // any content) or in print preview dead space
        targetElement = mDocument->GetRootContent();
      }
      NS_ASSERTION(targetElement, "Mouse move must have some target content");
      if (targetElement) {
        NotifyMouseOver(aEvent, targetElement);
      }
    }
    break;
  case NS_MOUSE_EXIT:
    {
      // This is actually the window mouse exit event. We're not moving
      // into any new element.
      NotifyMouseOut(aEvent, nsnull);
    }
    break;
  }

  // reset mCurretTargetContent to what it was
  mCurrentTargetContent = targetBeforeEvent;
}

void
nsEventStateManager::GenerateDragDropEnterExit(nsPresContext* aPresContext,
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
          nsMouseEvent event(NS_IS_TRUSTED_EVENT(aEvent),
                             NS_DRAGDROP_EXIT_SYNTH, aEvent->widget,
                             nsMouseEvent::eReal);
          event.refPoint = aEvent->refPoint;
          event.isShift = ((nsMouseEvent*)aEvent)->isShift;
          event.isControl = ((nsMouseEvent*)aEvent)->isControl;
          event.isAlt = ((nsMouseEvent*)aEvent)->isAlt;
          event.isMeta = ((nsMouseEvent*)aEvent)->isMeta;
          event.relatedTarget = targetContent;

          //The frame has change but the content may not have.  Check before dispatching to content
          mLastDragOverFrame->GetContentForEvent(aPresContext, aEvent, getter_AddRefs(lastContent));

          mCurrentTargetContent = lastContent;

          if ( lastContent != targetContent ) {
            //XXX This event should still go somewhere!!
            if (lastContent)
              nsEventDispatcher::Dispatch(lastContent, aPresContext, &event,
                                          nsnull, &status);

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
        nsMouseEvent event(NS_IS_TRUSTED_EVENT(aEvent), NS_DRAGDROP_ENTER,
                           aEvent->widget, nsMouseEvent::eReal);
        event.refPoint = aEvent->refPoint;
        event.isShift = ((nsMouseEvent*)aEvent)->isShift;
        event.isControl = ((nsMouseEvent*)aEvent)->isControl;
        event.isAlt = ((nsMouseEvent*)aEvent)->isAlt;
        event.isMeta = ((nsMouseEvent*)aEvent)->isMeta;
        event.relatedTarget = lastContent;

        mCurrentTargetContent = targetContent;

        //The frame has change but the content may not have.  Check before dispatching to content
        if (lastContent != targetContent) {
          //XXX This event should still go somewhere!!
          if (targetContent)
            nsEventDispatcher::Dispatch(targetContent, aPresContext, &event,
                                        nsnull, &status);

          // set drag hover on this frame
          if (status != nsEventStatus_eConsumeNoDefault)
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
        nsMouseEvent event(NS_IS_TRUSTED_EVENT(aEvent), NS_DRAGDROP_EXIT_SYNTH,
                           aEvent->widget, nsMouseEvent::eReal);
        event.refPoint = aEvent->refPoint;
        event.isShift = ((nsMouseEvent*)aEvent)->isShift;
        event.isControl = ((nsMouseEvent*)aEvent)->isControl;
        event.isAlt = ((nsMouseEvent*)aEvent)->isAlt;
        event.isMeta = ((nsMouseEvent*)aEvent)->isMeta;

        // dispatch to content via DOM
        nsCOMPtr<nsIContent> lastContent;
        mLastDragOverFrame->GetContentForEvent(aPresContext, aEvent, getter_AddRefs(lastContent));

        mCurrentTargetContent = lastContent;

        if (lastContent) {
          nsEventDispatcher::Dispatch(lastContent, aPresContext, &event, nsnull,
                                      &status);
          if (status != nsEventStatus_eConsumeNoDefault)
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

  // Now flush all pending notifications, for better responsiveness.
  FlushPendingEvents(aPresContext);
}

nsresult
nsEventStateManager::SetClickCount(nsPresContext* aPresContext,
                                   nsMouseEvent *aEvent,
                                   nsEventStatus* aStatus)
{
  nsCOMPtr<nsIContent> mouseContent;
  mCurrentTarget->GetContentForEvent(aPresContext, aEvent, getter_AddRefs(mouseContent));

  switch (aEvent->button) {
  case nsMouseEvent::eLeftButton:
    if (aEvent->message == NS_MOUSE_BUTTON_DOWN) {
      mLastLeftMouseDownContent = mouseContent;
    } else if (aEvent->message == NS_MOUSE_BUTTON_UP) {
      if (mLastLeftMouseDownContent == mouseContent) {
        aEvent->clickCount = mLClickCount;
        mLClickCount = 0;
      } else {
        aEvent->clickCount = 0;
      }
      mLastLeftMouseDownContent = nsnull;
    }
    break;

  case nsMouseEvent::eMiddleButton:
    if (aEvent->message == NS_MOUSE_BUTTON_DOWN) {
      mLastMiddleMouseDownContent = mouseContent;
    } else if (aEvent->message == NS_MOUSE_BUTTON_UP) {
      if (mLastMiddleMouseDownContent == mouseContent) {
        aEvent->clickCount = mMClickCount;
        mMClickCount = 0;
      } else {
        aEvent->clickCount = 0;
      }
      // XXX Why we don't clear mLastMiddleMouseDownContent here?
    }
    break;

  case nsMouseEvent::eRightButton:
    if (aEvent->message == NS_MOUSE_BUTTON_DOWN) {
      mLastRightMouseDownContent = mouseContent;
    } else if (aEvent->message == NS_MOUSE_BUTTON_UP) {
      if (mLastRightMouseDownContent == mouseContent) {
        aEvent->clickCount = mRClickCount;
        mRClickCount = 0;
      } else {
        aEvent->clickCount = 0;
      }
      // XXX Why we don't clear mLastRightMouseDownContent here?
    }
    break;
  }

  return NS_OK;
}

nsresult
nsEventStateManager::CheckForAndDispatchClick(nsPresContext* aPresContext,
                                              nsMouseEvent *aEvent,
                                              nsEventStatus* aStatus)
{
  nsresult ret = NS_OK;
  PRInt32 flags = NS_EVENT_FLAG_NONE;

  //If mouse is still over same element, clickcount will be > 1.
  //If it has moved it will be zero, so no click.
  if (0 != aEvent->clickCount) {
    //Check that the window isn't disabled before firing a click
    //(see bug 366544).
    if (aEvent->widget) {
      PRBool enabled;
      aEvent->widget->IsEnabled(&enabled);
      if (!enabled) {
        return ret;
      }
    }
    //fire click
    if (aEvent->button == nsMouseEvent::eMiddleButton ||
        aEvent->button == nsMouseEvent::eRightButton) {
      flags |=
        sLeftClickOnly ? NS_EVENT_FLAG_NO_CONTENT_DISPATCH : NS_EVENT_FLAG_NONE;
    }

    nsMouseEvent event(NS_IS_TRUSTED_EVENT(aEvent), NS_MOUSE_CLICK, aEvent->widget,
                       nsMouseEvent::eReal);
    event.refPoint = aEvent->refPoint;
    event.clickCount = aEvent->clickCount;
    event.isShift = aEvent->isShift;
    event.isControl = aEvent->isControl;
    event.isAlt = aEvent->isAlt;
    event.isMeta = aEvent->isMeta;
    event.time = aEvent->time;
    event.flags |= flags;
    event.button = aEvent->button;

    nsCOMPtr<nsIPresShell> presShell = mPresContext->GetPresShell();
    if (presShell) {
      nsCOMPtr<nsIContent> mouseContent;
      GetEventTargetContent(aEvent, getter_AddRefs(mouseContent));

      ret = presShell->HandleEventWithTarget(&event, mCurrentTarget,
                                             mouseContent, aStatus);
      if (NS_SUCCEEDED(ret) && aEvent->clickCount == 2) {
        //fire double click
        nsMouseEvent event2(NS_IS_TRUSTED_EVENT(aEvent), NS_MOUSE_DOUBLECLICK,
                            aEvent->widget, nsMouseEvent::eReal);
        event2.refPoint = aEvent->refPoint;
        event2.clickCount = aEvent->clickCount;
        event2.isShift = aEvent->isShift;
        event2.isControl = aEvent->isControl;
        event2.isAlt = aEvent->isAlt;
        event2.isMeta = aEvent->isMeta;
        event2.flags |= flags;
        event2.button = aEvent->button;

        ret = presShell->HandleEventWithTarget(&event2, mCurrentTarget,
                                               mouseContent, aStatus);
      }
    }
  }
  return ret;
}

NS_IMETHODIMP
nsEventStateManager::ChangeFocusWith(nsIContent* aFocusContent,
                                     EFocusedWithType aFocusedWith)
{
  mLastFocusedWith = aFocusedWith;
  if (!aFocusContent) {
    SetContentState(nsnull, NS_EVENT_STATE_FOCUS);
    return NS_OK;
  }

  // Get focus controller.
  EnsureDocument(mPresContext);
  nsCOMPtr<nsIFocusController> focusController = nsnull;
  nsCOMPtr<nsPIDOMWindow> window(mDocument->GetWindow());
  if (window)
    focusController = window->GetRootFocusController();

  // If this is called from mouse event, we lock to scroll.
  // Because the part of element is always in view. See bug 105894.
  PRBool suppressFocusScroll =
    focusController && (aFocusedWith == eEventFocusedByMouse);
  if (suppressFocusScroll) {
    PRBool currentState = PR_FALSE;
    focusController->GetSuppressFocusScroll(&currentState);
    NS_ASSERTION(!currentState, "locked scroll already!");
    focusController->SetSuppressFocusScroll(PR_TRUE);
  }

  aFocusContent->SetFocus(mPresContext);
  if (aFocusedWith != eEventFocusedByMouse) {
    MoveCaretToFocus();
    // Select text fields when focused via keyboard (tab or accesskey)
    if (sTextfieldSelectModel == eTextfieldSelect_auto &&
        mCurrentFocus &&
        mCurrentFocus->IsNodeOfType(nsINode::eHTML_FORM_CONTROL)) {
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

  // Unlock scroll
  if (suppressFocusScroll)
    focusController->SetSuppressFocusScroll(PR_FALSE);

  return NS_OK;
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
  nsCOMPtr<nsPresContext> presContext;
  parentAsDocShell->GetPresContext(getter_AddRefs(presContext));
  nsIDocument *doc = presShell->GetDocument();

  nsCOMPtr<nsIDOMWindowInternal> domwin = doc->GetWindow();

  nsCOMPtr<nsIWidget> widget;
  nsIViewManager* vm = presShell->GetViewManager();
  if (vm) {
    vm->GetWidget(getter_AddRefs(widget));
  }

  printf("DS %p  Type %s  Cnt %d  Doc %p  DW %p  EM %p\n",
    parentAsDocShell.get(),
    type==nsIDocShellTreeItem::typeChrome?"Chrome":"Content",
    childWebshellCount, doc, domwin.get(),
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
  nsCOMPtr<nsILookAndFeel> lookNFeel(do_GetService(kLookAndFeelCID));
  lookNFeel->GetMetric(nsILookAndFeel::eMetric_TabFocusModel,
                       nsIContent::sTabFocusModel);

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
  nsCOMPtr<nsIPresShell> presShell = mPresContext->PresShell();

  // We might use the selection position, rather than mCurrentFocus, as our position to shift focus from
  PRInt32 itemType;
  nsCOMPtr<nsIDocShellTreeItem> shellItem(do_QueryInterface(docShell));
  shellItem->GetItemType(&itemType);

  // Tab from the selection if it exists, but not if we're in chrome or an explicit starting
  // point was given.
  if (!aStart && itemType != nsIDocShellTreeItem::typeChrome) {
    // We're going to tab from the selection position
    if (!mCurrentFocus || (mLastFocusedWith != eEventFocusedByMouse && mCurrentFocus->Tag() != nsGkAtoms::area)) {
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
        // Refresh |selectionFrame| since MoveFocusToCaret() could have
        // destroyed it. (bug 308086)
        GetDocSelectionLocation(getter_AddRefs(selectionContent),
                                getter_AddRefs(endSelectionContent),
                                &selectionFrame, &selectionOffset);
      }
    }
  }

  nsIContent *startContent = nsnull;

  if (aStart) {
    curFocusFrame = presShell->GetPrimaryFrameFor(aStart);

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
    if (aStart->HasAttr(kNameSpaceID_None, nsGkAtoms::tabindex)) {
      aStart->IsFocusable(&mCurrentTabIndex);
    } else {
      ignoreTabIndex = PR_TRUE; // ignore current tabindex, bug 81481
    }
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
                           aForward, ignoreTabIndex || mCurrentTabIndex < 0,
                           getter_AddRefs(nextFocus), &nextFocusFrame);

  // Clear out mCurrentTabIndex. It has a garbage value because of GetNextTabbableContent()'s side effects
  // It will be set correctly when focus is changed via ChangeFocusWith()
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
      // Make sure to scroll before possibly dispatching focus/blur events.
      presShell->ScrollContentIntoView(nextFocus,
                                       NS_PRESSHELL_SCROLL_ANYWHERE,
                                       NS_PRESSHELL_SCROLL_ANYWHERE);

      SetContentState(nsnull, NS_EVENT_STATE_FOCUS);

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

      nsCOMPtr<nsIContent> oldFocus(mCurrentFocus);
      ChangeFocusWith(nextFocus, eEventFocusedByKey);
      if (!mCurrentFocus && oldFocus) {
        // ChangeFocusWith failed to move focus to nextFocus because a blur handler
        // made it unfocusable. (bug #118685)
        // Try again unless it's from the same point, bug 232368.
        if (oldFocus != aStart && oldFocus->GetDocument()) {
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
        nsIFrame* focusedFrame = nsnull;
        GetFocusedFrame(&focusedFrame);
        mCurrentTarget = focusedFrame;
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

          nsCOMPtr<nsIDocument> parent_doc = parentShell->GetDocument();
          nsCOMPtr<nsIContent> docContent = parent_doc->FindContentForSubDocument(mDocument);

          nsCOMPtr<nsPresContext> parentPC = parentShell->GetPresContext();
          nsIEventStateManager *parentESM = parentPC->EventStateManager();

          SetContentState(nsnull, NS_EVENT_STATE_FOCUS);

          nsCOMPtr<nsISupports> parentContainer = parentPC->GetContainer();
          if (parentContainer && docContent && docContent->GetCurrentDoc() == parent_doc) {
#ifdef DEBUG_DOCSHELL_FOCUS
            printf("popping out focus to parent docshell\n");
#endif
            parentESM->MoveCaretToFocus();
            parentESM->ShiftFocus(aForward, docContent);
#ifdef DEBUG_DOCSHELL_FOCUS
          } else {
            printf("can't pop out focus to parent docshell\n"); // bug 308025
#endif
          }
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

  nsresult rv;
  nsCOMPtr<nsIFrameTraversal> trav(do_CreateInstance(kFrameTraversalCID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIBidirectionalEnumerator> frameTraversal;

  // --- Get frame to start with ---
  if (!aStartFrame) {
    // No frame means we need to start with the root content again.
    NS_ENSURE_TRUE(mPresContext, NS_ERROR_FAILURE);
    nsIPresShell *presShell = mPresContext->GetPresShell();
    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);
    aStartFrame = presShell->GetPrimaryFrameFor(aRootContent);
    NS_ENSURE_TRUE(aStartFrame, NS_ERROR_FAILURE);
    rv = trav->NewFrameTraversal(getter_AddRefs(frameTraversal),
                                mPresContext, aStartFrame,
                                ePreOrder,
                                PR_FALSE, // aVisual
                                PR_FALSE, // aLockInScrollView
                                PR_TRUE   // aFollowOOFs
                                );
    NS_ENSURE_SUCCESS(rv, rv);
    if (!forward) {
      rv = frameTraversal->Last();
    }
  }
  else {
    rv = trav->NewFrameTraversal(getter_AddRefs(frameTraversal),
                                 mPresContext, aStartFrame,
                                 ePreOrder,
                                 PR_FALSE, // aVisual
                                 PR_FALSE, // aLockInScrollView
                                 PR_TRUE   // aFollowOOFs
                                 );
    NS_ENSURE_SUCCESS(rv, rv);
    if (!aStartContent || aStartContent->Tag() != nsGkAtoms::area ||
        !aStartContent->IsNodeOfType(nsINode::eHTML)) {
      // Need to do special check in case we're in an imagemap which has multiple
      // content per frame, so don't skip over the starting frame.
      rv = forward ? frameTraversal->Next() : frameTraversal->Prev();
    }
  }

  // -- Walk frames to find something tabbable matching mCurrentTabIndex --
  while (NS_SUCCEEDED(rv)) {
    nsISupports* currentItem;
    frameTraversal->CurrentItem(&currentItem);
    *aResultFrame = (nsIFrame*)currentItem;
    if (!*aResultFrame) {
      break;
    }

    // TabIndex not set defaults to 0 for form elements, anchors and other
    // elements that are normally focusable. Tabindex defaults to -1
    // for elements that are not normally focusable.
    // The returned computed tabindex from IsFocusable() is as follows:
    //          < 0 not tabbable at all
    //          == 0 in normal tab order (last after positive tabindex'd items)
    //          > 0 can be tabbed to in the order specified by this value
    PRInt32 tabIndex;
    nsIContent* currentContent = (*aResultFrame)->GetContent();
    (*aResultFrame)->IsFocusable(&tabIndex);
    if (tabIndex >= 0) {
      if (currentContent->Tag() == nsGkAtoms::img &&
          currentContent->HasAttr(kNameSpaceID_None, nsGkAtoms::usemap)) {
        // Must be an image w/ a map -- it's tabbable but no tabindex is specified
        // Special case for image maps: they don't get walked by nsIFrameTraversal
        nsIContent *areaContent = GetNextTabbableMapArea(forward, currentContent);
        if (areaContent) {
          NS_ADDREF(*aResultNode = areaContent);
          return NS_OK;
        }
      }
      else if ((aIgnoreTabIndex || mCurrentTabIndex == tabIndex) &&
          currentContent != aStartContent) {
        NS_ADDREF(*aResultNode = currentContent);
        return NS_OK;
      }
    }
    rv = forward ? frameTraversal->Next() : frameTraversal->Prev();
  }

  // -- Reached end or beginning of document --

  // If already at lowest priority tab (0), end search completely.
  // A bit counterintuitive but true, tabindex order goes 1, 2, ... 32767, 0
  if (mCurrentTabIndex == (forward? 0: 1)) {
    return NS_OK;
  }

  // else continue looking for next highest priority tabindex
  mCurrentTabIndex = GetNextTabIndex(aRootContent, forward);
  return GetNextTabbableContent(aRootContent, aStartContent, nsnull, forward,
                                aIgnoreTabIndex, aResultNode, aResultFrame);
}

nsIContent*
nsEventStateManager::GetNextTabbableMapArea(PRBool aForward,
                                            nsIContent *aImageContent)
{
  nsAutoString useMap;
  aImageContent->GetAttr(kNameSpaceID_None, nsGkAtoms::usemap, useMap);

  nsCOMPtr<nsIDocument> doc = aImageContent->GetDocument();
  if (doc) {
    nsCOMPtr<nsIDOMHTMLMapElement> imageMap = nsImageMapUtils::FindImageMap(doc, useMap);
    nsCOMPtr<nsIContent> mapContent = do_QueryInterface(imageMap);
    PRUint32 count = mapContent->GetChildCount();
    // First see if mCurrentFocus is in this map
    PRInt32 index = mapContent->IndexOf(mCurrentFocus);
    PRInt32 tabIndex;
    if (index < 0 || (mCurrentFocus->IsFocusable(&tabIndex) &&
                      tabIndex != mCurrentTabIndex)) {
      // If mCurrentFocus is in this map we must start iterating past it.
      // We skip the case where mCurrentFocus has tabindex == mCurrentTabIndex
      // since the next tab ordered element might be before it
      // (or after for backwards) in the child list.
      index = aForward ? -1 : (PRInt32)count;
    }

    // GetChildAt will return nsnull if our index < 0 or index >= count
    nsCOMPtr<nsIContent> areaContent;
    while ((areaContent = mapContent->GetChildAt(aForward? ++index : --index)) != nsnull) {
      if (areaContent->IsFocusable(&tabIndex) && tabIndex == mCurrentTabIndex) {
        return areaContent;
      }
    }
  }

  return nsnull;
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
      child->GetAttr(kNameSpaceID_None, nsGkAtoms::tabindex, tabIndexStr);
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
      child->GetAttr(kNameSpaceID_None, nsGkAtoms::tabindex, tabIndexStr);
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
        mCurrentTarget = shell->GetPrimaryFrameFor(mCurrentTargetContent);
      }
    }
  }

  if (!mCurrentTarget) {
    nsIPresShell *presShell = mPresContext->GetPresShell();
    if (presShell) {
      nsIFrame* frame = nsnull;
      presShell->GetEventTargetFrame(&frame);
      mCurrentTarget = frame;
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
nsEventStateManager::GetContentState(nsIContent *aContent, PRInt32& aState)
{
  aState = aContent->IntrinsicState();

  // Hierchical active:  Check the ancestor chain of mActiveContent to see
  // if we are on it.
  for (nsIContent* activeContent = mActiveContent; activeContent;
       activeContent = activeContent->GetParent()) {
    if (aContent == activeContent) {
      aState |= NS_EVENT_STATE_ACTIVE;
      break;
    }
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

static nsIContent* FindCommonAncestor(nsIContent *aNode1, nsIContent *aNode2)
{
  // Find closest common ancestor
  if (aNode1 && aNode2) {
    // Find the nearest common ancestor by counting the distance to the
    // root and then walking up again, in pairs.
    PRInt32 offset = 0;
    nsIContent *anc1 = aNode1;
    for (;;) {
      ++offset;
      nsIContent* parent = anc1->GetParent();
      if (!parent)
        break;
      anc1 = parent;
    }
    nsIContent *anc2 = aNode2;
    for (;;) {
      --offset;
      nsIContent* parent = anc2->GetParent();
      if (!parent)
        break;
      anc2 = parent;
    }
    if (anc1 == anc2) {
      anc1 = aNode1;
      anc2 = aNode2;
      while (offset > 0) {
        anc1 = anc1->GetParent();
        --offset;
      }
      while (offset < 0) {
        anc2 = anc2->GetParent();
        ++offset;
      }
      while (anc1 != anc2) {
        anc1 = anc1->GetParent();
        anc2 = anc2->GetParent();
      }
      return anc1;
    }
  }
  return nsnull;
}

PRBool
nsEventStateManager::SetContentState(nsIContent *aContent, PRInt32 aState)
{
  const PRInt32 maxNotify = 5;
  // We must initialize this array with memset for the sake of the boneheaded
  // OS X compiler.  See bug 134934.
  nsIContent  *notifyContent[maxNotify];
  memset(notifyContent, 0, sizeof(notifyContent));

  // check to see that this state is allowed by style. Check dragover too?
  // XXX This doesn't consider that |aState| is a bitfield.
  // XXX Is this even what we want?
  if (mCurrentTarget && (aState == NS_EVENT_STATE_ACTIVE || aState == NS_EVENT_STATE_HOVER))
  {
    const nsStyleUserInterface* ui = mCurrentTarget->GetStyleUserInterface();
    if (ui->mUserInput == NS_STYLE_USER_INPUT_NONE)
      return PR_FALSE;
  }

  PRBool didContentChangeAllStates = PR_TRUE;

  if ((aState & NS_EVENT_STATE_DRAGOVER) && (aContent != mDragOverContent)) {
    notifyContent[3] = mDragOverContent; // notify dragover first, since more common case
    NS_IF_ADDREF(notifyContent[3]);
    mDragOverContent = aContent;
  }

  if ((aState & NS_EVENT_STATE_URLTARGET) && (aContent != mURLTargetContent)) {
    notifyContent[4] = mURLTargetContent;
    NS_IF_ADDREF(notifyContent[4]);
    mURLTargetContent = aContent;
  }

  nsCOMPtr<nsIContent> commonActiveAncestor, oldActive, newActive;
  if ((aState & NS_EVENT_STATE_ACTIVE) && (aContent != mActiveContent)) {
    oldActive = mActiveContent;
    newActive = aContent;
    commonActiveAncestor = FindCommonAncestor(mActiveContent, aContent);
    mActiveContent = aContent;
  }

  nsCOMPtr<nsIContent> commonHoverAncestor, oldHover, newHover;
  if ((aState & NS_EVENT_STATE_HOVER) && (aContent != mHoverContent)) {
    oldHover = mHoverContent;

    if (!mPresContext || mPresContext->IsDynamic()) {
      newHover = aContent;
    } else {
      nsIFrame *frame = aContent ?
        mPresContext->PresShell()->GetPrimaryFrameFor(aContent) : nsnull;
      if (frame && nsLayoutUtils::IsViewportScrollbarFrame(frame)) {
        // The scrollbars of viewport should not ignore the hover state.
        // Because they are *not* the content of the web page.
        newHover = aContent;
      } else {
        // All contents of the web page should ignore the hover state.
        newHover = nsnull;
      }
    }

    commonHoverAncestor = FindCommonAncestor(mHoverContent, aContent);
    mHoverContent = aContent;
  }

  if ((aState & NS_EVENT_STATE_FOCUS)) {
    EnsureDocument(mPresContext);
    nsIMEStateManager::OnChangeFocus(mPresContext, aContent);
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
      notifyContent[2] = gLastFocusedContent;
      NS_IF_ADDREF(gLastFocusedContent);
      // only raise window if the the focus controller is active
      SendFocusBlur(mPresContext, aContent, fcActive);
      if (mCurrentFocus != aContent) {
        didContentChangeAllStates = PR_FALSE;
      }

#ifdef DEBUG_aleventhal
      nsPIDOMWindow *currentWindow = mDocument->GetWindow();
      if (currentWindow) {
        nsIFocusController *fc = currentWindow->GetRootFocusController();
        if (fc) {
          nsCOMPtr<nsIDOMElement> focusedElement;
          fc->GetFocusedElement(getter_AddRefs(focusedElement));
          if (!SameCOMIdentity(mCurrentFocus, focusedElement)) {
            printf("\n\nFocus out of whack!!!\n\n");
          }
        }
      }
#endif
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

  PRInt32 simpleStates = aState & ~(NS_EVENT_STATE_ACTIVE|NS_EVENT_STATE_HOVER);

  if (aContent && simpleStates != 0) {
    // notify about new content too
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

  if (notifyContent[0] || newHover || oldHover || newActive || oldActive) {
    // have at least one to notify about
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
    else {
      EnsureDocument(mPresContext);
      doc1 = mDocument;
    }
    if (doc1) {
      doc1->BeginUpdate(UPDATE_CONTENT_STATE);

      // Notify all content from newActive to the commonActiveAncestor
      while (newActive && newActive != commonActiveAncestor) {
        doc1->ContentStatesChanged(newActive, nsnull, NS_EVENT_STATE_ACTIVE);
        newActive = newActive->GetParent();
      }
      // Notify all content from oldActive to the commonActiveAncestor
      while (oldActive && oldActive != commonActiveAncestor) {
        doc1->ContentStatesChanged(oldActive, nsnull, NS_EVENT_STATE_ACTIVE);
        oldActive = oldActive->GetParent();
      }

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
                                   simpleStates);
        if (notifyContent[2]) {
          // more that two notifications are needed (should be rare)
          // XXX a further optimization here would be to group the
          // notification pairs together by parent/child, only needed if
          // more than two content changed (ie: if [0] and [2] are
          // parent/child, then notify (0,2) (1,3))
          doc1->ContentStatesChanged(notifyContent[2], notifyContent[3],
                                     simpleStates);
          if (notifyContent[4]) {
            // more that four notifications are needed (should be rare)
            doc1->ContentStatesChanged(notifyContent[4], nsnull,
                                       simpleStates);
          }
        }
      }
      doc1->EndUpdate(UPDATE_CONTENT_STATE);

      if (doc2) {
        doc2->BeginUpdate(UPDATE_CONTENT_STATE);
        doc2->ContentStatesChanged(notifyContent[1], notifyContent[2],
                                   simpleStates);
        if (notifyContent[3]) {
          doc1->ContentStatesChanged(notifyContent[3], notifyContent[4],
                                     simpleStates);
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

  return didContentChangeAllStates;
}

static PRBool
IsFocusable(nsIPresShell* aPresShell, nsIContent* aContent)
{
  // Flush pending updates to the frame tree first (bug 305840).
  aPresShell->FlushPendingNotifications(Flush_Frames);

  nsIFrame* focusFrame = aPresShell->GetPrimaryFrameFor(aContent);
  if (!focusFrame) {
    return PR_FALSE;
  }

  // XUL frames in general have a somewhat confusing notion of
  // focusability so we only require a visible frame and that
  // the content is focusable (not disabled).
  // We don't use nsIFrame::IsFocusable() because it defaults
  // tabindex to -1 for -moz-user-focus:ignore (bug 305840).
  if (aContent->IsNodeOfType(nsINode::eXUL)) {
    // XXX Eventually we should have a better check, but for
    // now checking for style visibility and focusability caused
    // too many regressions.
    return focusFrame->AreAncestorViewsVisible();
  }

  if (aContent->Tag() != nsGkAtoms::area) {
    return focusFrame->IsFocusable();
  }
  // HTML areas do not have their own frame, and the img frame we get from
  // GetPrimaryFrameFor() is not relevant to whether it is focusable or not,
  // so we have to do all the relevant checks manually for them.
  return focusFrame->AreAncestorViewsVisible() &&
         focusFrame->GetStyleVisibility()->IsVisible() &&
         aContent->IsFocusable();
}

nsresult
nsEventStateManager::SendFocusBlur(nsPresContext* aPresContext,
                                   nsIContent *aContent,
                                   PRBool aEnsureWindowHasFocus)
{
  // Keep a ref to presShell since dispatching the DOM event may cause
  // the document to be destroyed.
  nsCOMPtr<nsIPresShell> presShell = aPresContext->GetPresShell();
  if (!presShell)
    return NS_OK;

  nsCOMPtr<nsIContent> previousFocus = mCurrentFocus;

  // Make sure previousFocus is in a document.  If it's not, then
  // we should never abort firing events based on what happens when we
  // send it a blur.

  if (previousFocus && !previousFocus->GetDocument())
    previousFocus = nsnull;

  // Track the old focus controller if any focus suppressions is used on it.
  nsFocusSuppressor oldFocusSuppressor;
  
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

          nsCOMPtr<nsPresContext> oldPresContext = shell->GetPresContext();

          //fire blur
          nsEventStatus status = nsEventStatus_eIgnore;
          nsEvent event(PR_TRUE, NS_BLUR_CONTENT);

          EnsureDocument(presShell);

          // Make sure we're not switching command dispatchers, if so,
          // surpress the blurred one
          if(gLastFocusedDocument && mDocument) {
            nsPIDOMWindow *newWindow = mDocument->GetWindow();
            if (newWindow) {
              nsIFocusController *newFocusController =
                newWindow->GetRootFocusController();
              nsPIDOMWindow *oldWindow = gLastFocusedDocument->GetWindow();
              if (oldWindow) {
                nsIFocusController *suppressed =
                  oldWindow->GetRootFocusController();

                if (suppressed != newFocusController) {
                  oldFocusSuppressor.Suppress(suppressed, "SendFocusBlur Window Switch #1");
                }
              }
            }
          }

          nsCOMPtr<nsIEventStateManager> esm;
          esm = oldPresContext->EventStateManager();
          esm->SetFocusedContent(gLastFocusedContent);
          nsCOMPtr<nsIContent> temp = gLastFocusedContent;
          NS_RELEASE(gLastFocusedContent); // nulls out gLastFocusedContent

          nsCxPusher pusher(temp);
          nsEventDispatcher::Dispatch(temp, oldPresContext, &event, nsnull,
                                      &status);
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
        EnsureFocusSynchronization();
        return NS_OK;
      }
    }

    // Go ahead and fire a blur on the window.
    nsCOMPtr<nsPIDOMWindow> window;

    if(gLastFocusedDocument)
      window = gLastFocusedDocument->GetWindow();

    EnsureDocument(presShell);

    if (gLastFocusedDocument && (gLastFocusedDocument != mDocument) &&
        window) {
      nsEventStatus status = nsEventStatus_eIgnore;
      nsEvent event(PR_TRUE, NS_BLUR_CONTENT);

      // Make sure we're not switching command dispatchers, if so,
      // suppress the blurred one if it isn't already suppressed
      if (mDocument && !oldFocusSuppressor.Suppressing()) {
        nsCOMPtr<nsPIDOMWindow> newWindow(mDocument->GetWindow());

        if (newWindow) {
          nsCOMPtr<nsPIDOMWindow> oldWindow(gLastFocusedDocument->GetWindow());
          nsIFocusController *newFocusController =
            newWindow->GetRootFocusController();
          if (oldWindow) {
            nsIFocusController *suppressed =
              oldWindow->GetRootFocusController();
            if (suppressed != newFocusController) {
              oldFocusSuppressor.Suppress(suppressed, "SendFocusBlur Window Switch #2");
            }
          }
        }
      }

      gLastFocusedPresContext->EventStateManager()->SetFocusedContent(nsnull);
      nsCOMPtr<nsIDocument> temp = gLastFocusedDocument;
      NS_RELEASE(gLastFocusedDocument);
      gLastFocusedDocument = nsnull;

      nsCxPusher pusher(temp);
      nsEventDispatcher::Dispatch(temp, gLastFocusedPresContext, &event, nsnull,
                                  &status);
      pusher.Pop();

      if (previousFocus && mCurrentFocus != previousFocus) {
        // The document's blur handler focused something else.
        // Abort firing any additional blur or focus events, and make sure
        // nsFocusController:mFocusedElement is not nulled out, but agrees
        // with our current concept of focus.
        EnsureFocusSynchronization();
        return NS_OK;
      }

      pusher.Push(window);
      nsEventDispatcher::Dispatch(window, gLastFocusedPresContext, &event,
                                  nsnull, &status);

      if (previousFocus && mCurrentFocus != previousFocus) {
        // The window's blur handler focused something else.
        // Abort firing any additional blur or focus events.
        EnsureFocusSynchronization();
        return NS_OK;
      }
    }
  }

  // Check if the nsEventDispatcher::Dispatch calls above destroyed our frame
  // (bug #118685) or made it not focusable in any way.
  if (aContent && !::IsFocusable(presShell, aContent)) {
    aContent = nsnull;
  }

  NS_IF_RELEASE(gLastFocusedContent);
  gLastFocusedContent = aContent;
  NS_IF_ADDREF(gLastFocusedContent);
  SetFocusedContent(aContent);
  EnsureFocusSynchronization();

  // Moved widget focusing code here, from end of SendFocusBlur
  // This fixes the order of accessibility focus events, so that
  // the window focus event goes first, and then the focus event for the control
  if (aEnsureWindowHasFocus) {
    nsCOMPtr<nsIWidget> widget;
    // Plug-ins with native widget need a special handling
    nsIFrame* currentFocusFrame = nsnull;
    if (mCurrentFocus)
      currentFocusFrame = presShell->GetPrimaryFrameFor(mCurrentFocus);
    if (!currentFocusFrame)
      currentFocusFrame = mCurrentTarget;
    nsIObjectFrame* objFrame = nsnull;
    if (currentFocusFrame)
      CallQueryInterface(currentFocusFrame, &objFrame);
    if (objFrame) {
      nsIView* view = currentFocusFrame->GetViewExternal();
      NS_ASSERTION(view, "Object frames must have views");
      widget = view->GetWidget();
    }
    if (!widget) {
      // This raises the window that has both content and scroll bars in it
      // instead of the child window just below it that contains only the content
      // That way we focus the same window that gets focused by a mouse click
      nsIViewManager* vm = presShell->GetViewManager();
      if (vm) {
        vm->GetWidget(getter_AddRefs(widget));
      }
    }
    if (widget)
      widget->SetFocus(PR_TRUE);
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
    nsEvent event(PR_TRUE, NS_FOCUS_CONTENT);

    if (nsnull != mPresContext) {
      nsCxPusher pusher(aContent);
      nsEventDispatcher::Dispatch(aContent, mPresContext, &event, nsnull,
                                  &status);
      nsAutoString name;
      aContent->Tag()->ToString(name);
    }

    nsAutoString tabIndex;
    aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::tabindex, tabIndex);
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
    nsEvent event(PR_TRUE, NS_FOCUS_CONTENT);

    if (nsnull != mPresContext && mDocument) {
      nsCxPusher pusher(mDocument);
      nsEventDispatcher::Dispatch(mDocument, mPresContext, &event, nsnull,
                                  &status);
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

void nsEventStateManager::EnsureFocusSynchronization()
{
  // Sometimes the focus can get out of whack due to a blur handler
  // resetting focus. In addition, we fire onchange from the blur handler
  // for some controls, which is another place where focus can be changed.
  // XXX Ideally we will eventually store focus in one place instead of
  // the focus controller, esm, tabbrowser and some frames, so that it
  // cannot get out of sync.
  // See Bug 304751, calling FireOnChange() inside
  //                 nsComboboxControlFrame::SetFocus() is bad
  nsPIDOMWindow *currentWindow = mDocument->GetWindow();
  if (currentWindow) {
    nsIFocusController *fc = currentWindow->GetRootFocusController();
    if (fc) {
      nsCOMPtr<nsIDOMElement> focusedElement = do_QueryInterface(mCurrentFocus);
      fc->SetFocusedElement(focusedElement);
    }
  }
}

NS_IMETHODIMP
nsEventStateManager::SetFocusedContent(nsIContent* aContent)
{

  if (aContent &&
      (!mPresContext || mPresContext->Type() == nsPresContext::eContext_PrintPreview)) {
    return NS_OK;
  }

  mCurrentFocus = aContent;
  if (mCurrentFocus)
    mLastFocus = mCurrentFocus;
  mCurrentFocusFrame = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::GetLastFocusedContent(nsIContent** aContent)
{
  *aContent = mLastFocus;
  NS_IF_ADDREF(*aContent);
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
        mCurrentFocusFrame = shell->GetPrimaryFrameFor(mCurrentFocus);
      }
    }
  }

  *aFrame = mCurrentFocusFrame;
  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::ContentRemoved(nsIContent* aContent)
{
  if (mCurrentFocus &&
      nsContentUtils::ContentIsDescendantOf(mCurrentFocus, aContent)) {
    // Note that we don't use SetContentState() here because
    // we don't want to fire a blur.  Blurs should only be fired
    // in response to clicks or tabbing.
    nsIMEStateManager::OnRemoveContent(mPresContext, mCurrentFocus);
    SetFocusedContent(nsnull);
  }

  if (mLastFocus &&
      nsContentUtils::ContentIsDescendantOf(mLastFocus, aContent)) {
    mLastFocus = nsnull;
  }

  if (mHoverContent &&
      nsContentUtils::ContentIsDescendantOf(mHoverContent, aContent)) {
    // Since hover is hierarchical, set the current hover to the
    // content's parent node.
    mHoverContent = aContent->GetParent();
  }

  if (mActiveContent &&
      nsContentUtils::ContentIsDescendantOf(mActiveContent, aContent)) {
    // Active is hierarchical, so set the current active to the
    // content's parent node.
    mActiveContent = aContent->GetParent();
  }

  if (mDragOverContent &&
      nsContentUtils::ContentIsDescendantOf(mDragOverContent, aContent)) {
    mDragOverContent = nsnull;
  }

  if (mLastMouseOverElement &&
      nsContentUtils::ContentIsDescendantOf(mLastMouseOverElement, aContent)) {
    // See bug 292146 for why we want to null this out
    mLastMouseOverElement = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::EventStatusOK(nsGUIEvent* aEvent, PRBool *aOK)
{
  *aOK = PR_TRUE;
  if (aEvent->message == NS_MOUSE_BUTTON_DOWN &&
      NS_STATIC_CAST(nsMouseEvent*, aEvent)->button == nsMouseEvent::eLeftButton) {
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
    PRUint32 accKey = (IS_IN_BMP(aKey)) ? ToLowerCase((PRUnichar)aKey) : aKey;

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
    PRUint32 accKey = (IS_IN_BMP(aKey)) ? ToLowerCase((PRUnichar)aKey) : aKey;

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

void
nsEventStateManager::EnsureDocument(nsPresContext* aPresContext)
{
  if (!mDocument)
    mDocument = aPresContext->Document();
}

void
nsEventStateManager::EnsureDocument(nsIPresShell* aPresShell)
{
  if (!mDocument && aPresShell)
    mDocument = aPresShell->GetDocument();
}

void
nsEventStateManager::FlushPendingEvents(nsPresContext* aPresContext)
{
  NS_PRECONDITION(nsnull != aPresContext, "nsnull ptr");
  nsIPresShell *shell = aPresContext->GetPresShell();
  if (shell) {
    shell->FlushPendingNotifications(Flush_Display);
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

  NS_ASSERTION(mPresContext, "mPresContent is null!!");
  EnsureDocument(mPresContext);
  if (!mDocument)
    return rv;
  nsIPresShell *shell;
  shell = mPresContext->GetPresShell();

  nsFrameSelection *frameSelection = nsnull;
  if (shell)
    frameSelection = shell->FrameSelection();

  nsCOMPtr<nsISelection> domSelection;
  if (frameSelection) {
    domSelection = frameSelection->
      GetSelection(nsISelectionController::SELECTION_NORMAL);
  }

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
      if (startContent && startContent->IsNodeOfType(nsINode::eELEMENT)) {
        NS_ASSERTION(*aStartOffset >= 0, "Start offset cannot be negative");  
        childContent = startContent->GetChildAt(*aStartOffset);
        if (childContent) {
          startContent = childContent;
        }
      }

      endContent = do_QueryInterface(endNode);
      if (endContent && endContent->IsNodeOfType(nsINode::eELEMENT)) {
        PRInt32 endOffset = 0;
        domRange->GetEndOffset(&endOffset);
        NS_ASSERTION(endOffset >= 0, "End offset cannot be negative");
        childContent = endContent->GetChildAt(endOffset);
        if (childContent) {
          endContent = childContent;
        }
      }
    }
  }
  else {
    rv = NS_ERROR_INVALID_ARG;
  }

  nsIFrame *startFrame = nsnull;
  if (startContent) {
    startFrame = shell->GetPrimaryFrameFor(startContent);
    if (isCollapsed) {
      // First check to see if we're in a <label>
      // We don't want to return the selection in a label, because
      // we we should be tabbing relative to what the label 
      // points to (the current focus), not relative to the label itself.
      nsIContent *parentContent = startContent;
      while ((parentContent = parentContent->GetParent()) != nsnull) {
        if (parentContent->Tag() == nsGkAtoms::label) {
          return NS_OK; // Don't return selection location, we're on a label
        }
      }
      // Next check to see if our caret is at the very end of a node
      // If so, the caret is actually sitting in front of the next
      // logical frame's primary node - so for this case we need to
      // change caretContent to that node.

      nsCOMPtr<nsIDOMNode> domNode(do_QueryInterface(startContent));
      PRUint16 nodeType;
      domNode->GetNodeType(&nodeType);

      if (nodeType == nsIDOMNode::TEXT_NODE) {
        nsAutoString nodeValue;
        domNode->GetNodeValue(nodeValue);

        PRBool isFormControl =
          startContent->IsNodeOfType(nsINode::eHTML_FORM_CONTROL);

        if (nodeValue.Length() == *aStartOffset && !isFormControl &&
            startContent != mDocument->GetRootContent()) {
          // Yes, indeed we were at the end of the last node
          nsCOMPtr<nsIBidirectionalEnumerator> frameTraversal;

          nsCOMPtr<nsIFrameTraversal> trav(do_CreateInstance(kFrameTraversalCID,
                                                             &rv));
          NS_ENSURE_SUCCESS(rv, rv);

          rv = trav->NewFrameTraversal(getter_AddRefs(frameTraversal),
                                       mPresContext, startFrame,
                                       eLeaf,
                                       PR_FALSE, // aVisual
                                       PR_FALSE, // aLockInScrollView
                                       PR_FALSE  // aFollowOOFs
                                       );
          NS_ENSURE_SUCCESS(rv, rv);

          nsIFrame *newCaretFrame = nsnull;
          nsCOMPtr<nsIContent> newCaretContent = startContent;
          PRBool endOfSelectionInStartNode(startContent == endContent);
          do {
            // Continue getting the next frame until the primary content for the frame
            // we are on changes - we don't want to be stuck in the same place
            frameTraversal->Next();
            nsISupports* currentItem;
            frameTraversal->CurrentItem(&currentItem);
            if (nsnull == (newCaretFrame = NS_STATIC_CAST(nsIFrame*, currentItem))) {
              break;
            }
            newCaretContent = newCaretFrame->GetContent();            
          } while (!newCaretContent || newCaretContent == startContent);

          if (newCaretFrame && newCaretContent) {
            // If the caret is exactly at the same position of the new frame,
            // then we can use the newCaretFrame and newCaretContent for our position
            nsCOMPtr<nsICaret> caret;
            shell->GetCaret(getter_AddRefs(caret));
            nsRect caretRect;
            nsIView *caretView;
            caret->GetCaretCoordinates(nsICaret::eClosestViewCoordinates, 
                                       domSelection, &caretRect,
                                       &isCollapsed, &caretView);
            nsPoint framePt;
            nsIView *frameClosestView = newCaretFrame->GetClosestView(&framePt);
            if (caretView == frameClosestView && caretRect.y == framePt.y &&
                caretRect.x == framePt.x) {
              // The caret is at the start of the new element.
              startFrame = newCaretFrame;
              startContent = newCaretContent;
              if (endOfSelectionInStartNode) {
                endContent = newCaretContent; // Ensure end of selection is not before start
              }
            }
          }
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
    if (tag == nsGkAtoms::a &&
        testContent->IsNodeOfType(nsINode::eHTML)) {
      *aIsSelectionWithFocus = PR_TRUE;
    }
    else {
      // Xlink must be type="simple"
      *aIsSelectionWithFocus =
        testContent->HasAttr(kNameSpaceID_XLink, nsGkAtoms::href) &&
        testContent->AttrValueIs(kNameSpaceID_XLink, nsGkAtoms::type,
                                 nsGkAtoms::simple, eCaseMatters);
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
      if (testContent->Tag() == nsGkAtoms::a &&
          testContent->IsNodeOfType(nsINode::eHTML)) {
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

    nsIPresShell *shell = mPresContext->GetPresShell();
    if (shell) {
      // rangeDoc is a document interface we can create a range with
      nsCOMPtr<nsIDOMDocumentRange> rangeDoc(do_QueryInterface(mDocument));

      if (rangeDoc) {
        nsISelection* domSelection = shell->FrameSelection()->
          GetSelection(nsISelectionController::SELECTION_NORMAL);
        if (domSelection) {
          nsCOMPtr<nsIDOMNode> currentFocusNode(do_QueryInterface(mCurrentFocus));
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
              if (!firstChild ||
                  mCurrentFocus->IsNodeOfType(nsINode::eHTML_FORM_CONTROL)) {
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

  nsFrameSelection* frameSelection = nsnull;
  if (aFocusedContent) {
    nsIFrame *focusFrame = aPresShell->GetPrimaryFrameFor(aFocusedContent);
    
    if (focusFrame)
      frameSelection = focusFrame->GetFrameSelection();
  }

  nsFrameSelection *docFrameSelection = aPresShell->FrameSelection();

  if (docFrameSelection && caret &&
     (frameSelection == docFrameSelection || !aFocusedContent)) {
    nsISelection* domSelection = docFrameSelection->
      GetSelection(nsISelectionController::SELECTION_NORMAL);
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

  nsCOMPtr<nsIEditorDocShell> editorDocShell(do_QueryInterface(shellItem));
  if (editorDocShell) {
    PRBool isEditable;
    editorDocShell->GetEditable(&isEditable);
    if (isEditable) {
      return;  // Reset caret visibility only if browsing, not editing
    }
  }

  PRPackedBool browseWithCaret =
    nsContentUtils::GetBoolPref("accessibility.browsewithcaret");

  mBrowseWithCaret = browseWithCaret;

  nsIPresShell *presShell = mPresContext->GetPresShell();

  // Make caret visible or not, depending on what's appropriate
  // Set caret visibility for focused document only
  // Others will be set when they get focused again
  if (presShell && gLastFocusedDocument && gLastFocusedDocument == mDocument) {
    SetContentCaretVisible(presShell, mCurrentFocus, browseWithCaret);
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
    nsIDocument *doc = presShell->GetDocument();
    nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(doc);
    if (htmlDoc) {
      nsIContent *rootContent = doc->GetRootContent();
      if (rootContent) {
        PRUint32 childCount = rootContent->GetChildCount();
        for (PRUint32 i = 0; i < childCount; ++i) {
          nsIContent *childContent = rootContent->GetChildAt(i);

          nsINodeInfo *ni = childContent->NodeInfo();

          if (childContent->IsNodeOfType(nsINode::eHTML) &&
              ni->Equals(nsGkAtoms::frameset)) {
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

  nsCOMPtr<nsPIDOMWindow> domWindow(do_GetInterface(aDocShell));
  if (!domWindow) {
    NS_ERROR("We're a child of a docshell without a window?");
    return PR_FALSE;
  }

  nsCOMPtr<nsIContent> docContent =
    do_QueryInterface(domWindow->GetFrameElementInternal());

  if (!docContent) {
    return PR_FALSE;
  }

  return docContent->Tag() == nsGkAtoms::iframe;
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

  nsCOMPtr<nsPresContext> presContext;
  aDocShell->GetPresContext(getter_AddRefs(presContext));
  PRBool focusDocument;
  if (presContext &&
      presContext->Type() == nsPresContext::eContext_PrintPreview) {
    // Don't focus any content in print preview mode, bug 244128.
    focusDocument = PR_TRUE;
  } else {
    if (!aForward || (itemType == nsIDocShellTreeItem::typeChrome))
      focusDocument = PR_FALSE;
    else {
      // Check for a frameset document
      focusDocument = !(IsFrameSetDoc(aDocShell));
    }
  }

  if (focusDocument) {
    // make sure we're in view
    aDocShell->SetCanvasHasFocus(PR_TRUE);
  }
  else {
    aDocShell->SetHasFocus(PR_FALSE);

    if (presContext) {
      nsIEventStateManager *docESM = presContext->EventStateManager();

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

