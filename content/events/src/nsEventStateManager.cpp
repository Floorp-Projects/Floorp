/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TabParent.h"

#include "nsCOMPtr.h"
#include "nsEventStateManager.h"
#include "nsEventListenerManager.h"
#include "nsIMEStateManager.h"
#include "nsContentEventHandler.h"
#include "nsIContent.h"
#include "nsINodeInfo.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsIWidget.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsDOMEvent.h"
#include "nsGkAtoms.h"
#include "nsIEditorDocShell.h"
#include "nsIFormControl.h"
#include "nsIComboboxControlFrame.h"
#include "nsIScrollableFrame.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMXULControlElement.h"
#include "nsINameSpaceManager.h"
#include "nsIBaseWindow.h"
#include "nsISelection.h"
#include "nsFrameSelection.h"
#include "nsPIDOMWindow.h"
#include "nsPIWindowRoot.h"
#include "nsIEnumerator.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIWebNavigation.h"
#include "nsIContentViewer.h"
#ifdef MOZ_XUL
#include "nsXULPopupManager.h"
#endif
#include "nsFrameManager.h"

#include "nsIServiceManager.h"
#include "nsIScriptSecurityManager.h"

#include "nsFocusManager.h"

#include "nsIDOMXULElement.h"
#include "nsIDOMDocument.h"
#include "nsIDOMKeyEvent.h"
#include "nsIObserverService.h"
#include "nsIDocShell.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIDOMMouseScrollEvent.h"
#include "nsIDOMDragEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMUIEvent.h"
#include "nsDOMDragEvent.h"
#include "nsIDOMNSEditableElement.h"
#include "nsIDOMMozBrowserFrame.h"
#include "nsIMozBrowserFrame.h"

#include "nsCaret.h"

#include "nsSubDocumentFrame.h"
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

#include "nsServiceManagerUtils.h"
#include "nsITimer.h"
#include "nsFontMetrics.h"
#include "nsIDOMXULDocument.h"
#include "nsIDragService.h"
#include "nsIDragSession.h"
#include "nsDOMDataTransfer.h"
#include "nsContentAreaDragDrop.h"
#ifdef MOZ_XUL
#include "nsTreeBodyFrame.h"
#endif
#include "nsIController.h"
#include "nsICommandParams.h"
#include "mozilla/Services.h"
#include "mozAutoDocUpdate.h"
#include "nsHTMLLabelElement.h"

#include "mozilla/Preferences.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/Attributes.h"
#include "sampler.h"

#include "nsIDOMClientRect.h"

#ifdef XP_MACOSX
#import <ApplicationServices/ApplicationServices.h>
#endif

using namespace mozilla;
using namespace mozilla::dom;

//#define DEBUG_DOCSHELL_FOCUS

#define NS_USER_INTERACTION_INTERVAL 5000 // ms

static NS_DEFINE_CID(kFrameTraversalCID, NS_FRAMETRAVERSAL_CID);

static bool sLeftClickOnly = true;
static bool sKeyCausesActivation = true;
static PRUint32 sESMInstanceCount = 0;
static PRInt32 sChromeAccessModifier = 0, sContentAccessModifier = 0;
PRInt32 nsEventStateManager::sUserInputEventDepth = 0;
bool nsEventStateManager::sNormalLMouseEventInProcess = false;
nsEventStateManager* nsEventStateManager::sActiveESM = nsnull;
nsIDocument* nsEventStateManager::sMouseOverDocument = nsnull;
nsWeakFrame nsEventStateManager::sLastDragOverFrame = nsnull;
nsIntPoint nsEventStateManager::sLastRefPoint = nsIntPoint(0,0);
nsIntPoint nsEventStateManager::sLastScreenPoint = nsIntPoint(0,0);
nsIntPoint nsEventStateManager::sLastClientPoint = nsIntPoint(0,0);
bool nsEventStateManager::sIsPointerLocked = false;
// Reference to the pointer locked element.
nsWeakPtr nsEventStateManager::sPointerLockedElement;
// Reference to the document which requested pointer lock.
nsWeakPtr nsEventStateManager::sPointerLockedDoc;
nsCOMPtr<nsIContent> nsEventStateManager::sDragOverContent = nsnull;

static PRUint32 gMouseOrKeyboardEventCounter = 0;
static nsITimer* gUserInteractionTimer = nsnull;
static nsITimerCallback* gUserInteractionTimerCallback = nsnull;

// Pixel scroll accumulation for synthetic line scrolls
static nscoord gPixelScrollDeltaX = 0;
static nscoord gPixelScrollDeltaY = 0;
static PRUint32 gPixelScrollDeltaTimeout = 0;

static nscoord
GetScrollableLineHeight(nsIFrame* aTargetFrame);

TimeStamp nsEventStateManager::sHandlingInputStart;

static inline bool
IsMouseEventReal(nsEvent* aEvent)
{
  NS_ABORT_IF_FALSE(NS_IS_MOUSE_EVENT_STRUCT(aEvent), "Not a mouse event");
  // Return true if not synthesized.
  return static_cast<nsMouseEvent*>(aEvent)->reason == nsMouseEvent::eReal;
}

#ifdef DEBUG_DOCSHELL_FOCUS
static void
PrintDocTree(nsIDocShellTreeItem* aParentItem, int aLevel)
{
  for (PRInt32 i=0;i<aLevel;i++) printf("  ");

  PRInt32 childWebshellCount;
  aParentItem->GetChildCount(&childWebshellCount);
  nsCOMPtr<nsIDocShell> parentAsDocShell(do_QueryInterface(aParentItem));
  PRInt32 type;
  aParentItem->GetItemType(&type);
  nsCOMPtr<nsIPresShell> presShell;
  parentAsDocShell->GetPresShell(getter_AddRefs(presShell));
  nsRefPtr<nsPresContext> presContext;
  parentAsDocShell->GetPresContext(getter_AddRefs(presContext));
  nsCOMPtr<nsIContentViewer> cv;
  parentAsDocShell->GetContentViewer(getter_AddRefs(cv));
  nsCOMPtr<nsIDOMDocument> domDoc;
  if (cv)
    cv->GetDOMDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  nsCOMPtr<nsIDOMWindow> domwin = doc ? doc->GetWindow() : nsnull;
  nsIURI* uri = doc ? doc->GetDocumentURI() : nsnull;

  printf("DS %p  Type %s  Cnt %d  Doc %p  DW %p  EM %p%c",
    static_cast<void*>(parentAsDocShell.get()),
    type==nsIDocShellTreeItem::typeChrome?"Chrome":"Content",
    childWebshellCount, static_cast<void*>(doc.get()),
    static_cast<void*>(domwin.get()),
    static_cast<void*>(presContext ? presContext->EventStateManager() : nsnull),
    uri ? ' ' : '\n');
  if (uri) {
    nsCAutoString spec;
    uri->GetSpec(spec);
    printf("\"%s\"\n", spec.get());
  }

  if (childWebshellCount > 0) {
    for (PRInt32 i = 0; i < childWebshellCount; i++) {
      nsCOMPtr<nsIDocShellTreeItem> child;
      aParentItem->GetChildAt(i, getter_AddRefs(child));
      PrintDocTree(child, aLevel + 1);
    }
  }
}

static void
PrintDocTreeAll(nsIDocShellTreeItem* aItem)
{
  nsCOMPtr<nsIDocShellTreeItem> item = aItem;
  for(;;) {
    nsCOMPtr<nsIDocShellTreeItem> parent;
    item->GetParent(getter_AddRefs(parent));
    if (!parent)
      break;
    item = parent;
  }

  PrintDocTree(item, 0);
}
#endif

class nsUITimerCallback MOZ_FINAL : public nsITimerCallback
{
public:
  nsUITimerCallback() : mPreviousCount(0) {}
  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK
private:
  PRUint32 mPreviousCount;
};

NS_IMPL_ISUPPORTS1(nsUITimerCallback, nsITimerCallback)

// If aTimer is nsnull, this method always sends "user-interaction-inactive"
// notification.
NS_IMETHODIMP
nsUITimerCallback::Notify(nsITimer* aTimer)
{
  nsCOMPtr<nsIObserverService> obs =
    mozilla::services::GetObserverService();
  if (!obs)
    return NS_ERROR_FAILURE;
  if ((gMouseOrKeyboardEventCounter == mPreviousCount) || !aTimer) {
    gMouseOrKeyboardEventCounter = 0;
    obs->NotifyObservers(nsnull, "user-interaction-inactive", nsnull);
    if (gUserInteractionTimer) {
      gUserInteractionTimer->Cancel();
      NS_RELEASE(gUserInteractionTimer);
    }
  } else {
    obs->NotifyObservers(nsnull, "user-interaction-active", nsnull);
    nsEventStateManager::UpdateUserActivityTimer();
  }
  mPreviousCount = gMouseOrKeyboardEventCounter;
  return NS_OK;
}

enum {
 MOUSE_SCROLL_N_LINES,
 MOUSE_SCROLL_PAGE,
 MOUSE_SCROLL_HISTORY,
 MOUSE_SCROLL_ZOOM,
 MOUSE_SCROLL_PIXELS
};

// mask values for ui.key.chromeAccess and ui.key.contentAccess
#define NS_MODIFIER_SHIFT    1
#define NS_MODIFIER_CONTROL  2
#define NS_MODIFIER_ALT      4
#define NS_MODIFIER_META     8
#define NS_MODIFIER_OS       16

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
  PRInt32 accessKey = Preferences::GetInt("ui.key.generalAccessKey", -1);
  switch (accessKey) {
    case -1:                             break; // use the individual prefs
    case nsIDOMKeyEvent::DOM_VK_SHIFT:   return NS_MODIFIER_SHIFT;
    case nsIDOMKeyEvent::DOM_VK_CONTROL: return NS_MODIFIER_CONTROL;
    case nsIDOMKeyEvent::DOM_VK_ALT:     return NS_MODIFIER_ALT;
    case nsIDOMKeyEvent::DOM_VK_META:    return NS_MODIFIER_META;
    case nsIDOMKeyEvent::DOM_VK_WIN:     return NS_MODIFIER_OS;
    default:                             return 0;
  }

  switch (aItemType) {
  case nsIDocShellTreeItem::typeChrome:
    return Preferences::GetInt("ui.key.chromeAccess", 0);
  case nsIDocShellTreeItem::typeContent:
    return Preferences::GetInt("ui.key.contentAccess", 0);
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
  if (aEvent->IsShift()) {
    aPref.Append(withshift);
  } else if (aEvent->IsControl()) {
    aPref.Append(withcontrol);
  } else if (aEvent->IsAlt()) {
    aPref.Append(withalt);
  } else if (aEvent->IsMeta()) {
    aPref.Append(withmetakey);
  } else {
    aPref.Append(withno);
  }
}

class nsMouseWheelTransaction {
public:
  static nsIFrame* GetTargetFrame() { return sTargetFrame; }
  static void BeginTransaction(nsIFrame* aTargetFrame,
                               PRInt32 aNumLines,
                               bool aScrollHorizontal);
  // Be careful, UpdateTransaction may fire a DOM event, therefore, the target
  // frame might be destroyed in the event handler.
  static bool UpdateTransaction(PRInt32 aNumLines,
                                  bool aScrollHorizontal);
  static void EndTransaction();
  static void OnEvent(nsEvent* aEvent);
  static void Shutdown();
  static PRUint32 GetTimeoutTime();
  static PRInt32 AccelerateWheelDelta(PRInt32 aScrollLines,
                   bool aIsHorizontal, bool aAllowScrollSpeedOverride,
                   nsIScrollableFrame::ScrollUnit *aScrollQuantity,
                   bool aLimitToMaxOnePageScroll = true);
  static bool IsAccelerationEnabled();

  enum {
    kScrollSeriesTimeout = 80
  };
protected:
  static nsIntPoint GetScreenPoint(nsGUIEvent* aEvent);
  static void OnFailToScrollTarget();
  static void OnTimeout(nsITimer *aTimer, void *aClosure);
  static void SetTimeout();
  static PRUint32 GetIgnoreMoveDelayTime();
  static PRInt32 GetAccelerationStart();
  static PRInt32 GetAccelerationFactor();
  static PRInt32 OverrideSystemScrollSpeed(PRInt32 aScrollLines,
                                           bool aIsHorizontal);
  static PRInt32 ComputeAcceleratedWheelDelta(PRInt32 aDelta, PRInt32 aFactor);
  static PRInt32 LimitToOnePageScroll(PRInt32 aScrollLines,
                   bool aIsHorizontal,
                   nsIScrollableFrame::ScrollUnit *aScrollQuantity);

  static nsWeakFrame sTargetFrame;
  static PRUint32    sTime;        // in milliseconds
  static PRUint32    sMouseMoved;  // in milliseconds
  static nsITimer*   sTimer;
  static PRInt32     sScrollSeriesCounter;
};

nsWeakFrame nsMouseWheelTransaction::sTargetFrame(nsnull);
PRUint32    nsMouseWheelTransaction::sTime        = 0;
PRUint32    nsMouseWheelTransaction::sMouseMoved  = 0;
nsITimer*   nsMouseWheelTransaction::sTimer       = nsnull;
PRInt32     nsMouseWheelTransaction::sScrollSeriesCounter = 0;

static bool
OutOfTime(PRUint32 aBaseTime, PRUint32 aThreshold)
{
  PRUint32 now = PR_IntervalToMilliseconds(PR_IntervalNow());
  return (now - aBaseTime > aThreshold);
}

static bool
CanScrollInRange(nscoord aMin, nscoord aValue, nscoord aMax, PRInt32 aDirection)
{
  return aDirection > 0 ? aValue < aMax : aMin < aValue;
}

static bool
CanScrollOn(nsIScrollableFrame* aScrollFrame, PRInt32 aNumLines,
            bool aScrollHorizontal)
{
  NS_PRECONDITION(aScrollFrame, "aScrollFrame is null");
  NS_PRECONDITION(aNumLines, "aNumLines must be non-zero");
  nsPoint scrollPt = aScrollFrame->GetScrollPosition();
  nsRect scrollRange = aScrollFrame->GetScrollRange();

  return aScrollHorizontal
    ? CanScrollInRange(scrollRange.x, scrollPt.x, scrollRange.XMost(), aNumLines)
    : CanScrollInRange(scrollRange.y, scrollPt.y, scrollRange.YMost(), aNumLines);
}

void
nsMouseWheelTransaction::BeginTransaction(nsIFrame* aTargetFrame,
                                          PRInt32 aNumLines,
                                          bool aScrollHorizontal)
{
  NS_ASSERTION(!sTargetFrame, "previous transaction is not finished!");
  sTargetFrame = aTargetFrame;
  sScrollSeriesCounter = 0;
  if (!UpdateTransaction(aNumLines, aScrollHorizontal)) {
    NS_ERROR("BeginTransaction is called even cannot scroll the frame");
    EndTransaction();
  }
}

bool
nsMouseWheelTransaction::UpdateTransaction(PRInt32 aNumLines,
                                           bool aScrollHorizontal)
{
  nsIScrollableFrame* sf = GetTargetFrame()->GetScrollTargetFrame();
  NS_ENSURE_TRUE(sf, false);

  if (!CanScrollOn(sf, aNumLines, aScrollHorizontal)) {
    OnFailToScrollTarget();
    // We should not modify the transaction state when the view will not be
    // scrolled actually.
    return false;
  }

  SetTimeout();

  if (sScrollSeriesCounter != 0 && OutOfTime(sTime, kScrollSeriesTimeout))
    sScrollSeriesCounter = 0;
  sScrollSeriesCounter++;

  // We should use current time instead of nsEvent.time.
  // 1. Some events doesn't have the correct creation time.
  // 2. If the computer runs slowly by other processes eating the CPU resource,
  //    the event creation time doesn't keep real time.
  sTime = PR_IntervalToMilliseconds(PR_IntervalNow());
  sMouseMoved = 0;
  return true;
}

void
nsMouseWheelTransaction::EndTransaction()
{
  if (sTimer)
    sTimer->Cancel();
  sTargetFrame = nsnull;
  sScrollSeriesCounter = 0;
}

void
nsMouseWheelTransaction::OnEvent(nsEvent* aEvent)
{
  if (!sTargetFrame)
    return;

  if (OutOfTime(sTime, GetTimeoutTime())) {
    // Even if the scroll event which is handled after timeout, but onTimeout
    // was not fired by timer, then the scroll event will scroll old frame,
    // therefore, we should call OnTimeout here and ensure to finish the old
    // transaction.
    OnTimeout(nsnull, nsnull);
    return;
  }

  PRInt32 message = aEvent->message;
  // If the event is query scroll target info event, that causes modifying
  // wheel transaction because DoScrollText() needs to use them.  Therefore,
  // we should handle the event as its mouse scroll event here.
  if (message == NS_QUERY_SCROLL_TARGET_INFO) {
    nsQueryContentEvent* queryEvent = static_cast<nsQueryContentEvent*>(aEvent);
    message = queryEvent->mInput.mMouseScrollEvent->message;
  }

  switch (message) {
    case NS_MOUSE_SCROLL:
    case NS_MOUSE_PIXEL_SCROLL:
      if (sMouseMoved != 0 &&
          OutOfTime(sMouseMoved, GetIgnoreMoveDelayTime())) {
        // Terminate the current mousewheel transaction if the mouse moved more
        // than ignoremovedelay milliseconds ago
        EndTransaction();
      }
      return;
    case NS_MOUSE_MOVE:
    case NS_DRAGDROP_OVER:
      if (IsMouseEventReal(aEvent)) {
        // If the cursor is moving to be outside the frame,
        // terminate the scrollwheel transaction.
        nsIntPoint pt = GetScreenPoint((nsGUIEvent*)aEvent);
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

void
nsMouseWheelTransaction::Shutdown()
{
  NS_IF_RELEASE(sTimer);
}

void
nsMouseWheelTransaction::OnFailToScrollTarget()
{
  NS_PRECONDITION(sTargetFrame, "We don't have mouse scrolling transaction");

  if (Preferences::GetBool("test.mousescroll", false)) {
    // This event is used for automated tests, see bug 442774.
    nsContentUtils::DispatchTrustedEvent(
                      sTargetFrame->GetContent()->OwnerDoc(),
                      sTargetFrame->GetContent(),
                      NS_LITERAL_STRING("MozMouseScrollFailed"),
                      true, true);
  }
  // The target frame might be destroyed in the event handler, at that time,
  // we need to finish the current transaction
  if (!sTargetFrame)
    EndTransaction();
}

void
nsMouseWheelTransaction::OnTimeout(nsITimer* aTimer, void* aClosure)
{
  if (!sTargetFrame) {
    // The transaction target was destroyed already
    EndTransaction();
    return;
  }
  // Store the sTargetFrame, the variable becomes null in EndTransaction.
  nsIFrame* frame = sTargetFrame;
  // We need to finish current transaction before DOM event firing. Because
  // the next DOM event might create strange situation for us.
  EndTransaction();

  if (Preferences::GetBool("test.mousescroll", false)) {
    // This event is used for automated tests, see bug 442774.
    nsContentUtils::DispatchTrustedEvent(
                      frame->GetContent()->OwnerDoc(),
                      frame->GetContent(),
                      NS_LITERAL_STRING("MozMouseScrollTransactionTimeout"),
                      true, true);
  }
}

void
nsMouseWheelTransaction::SetTimeout()
{
  if (!sTimer) {
    nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID);
    if (!timer)
      return;
    timer.swap(sTimer);
  }
  sTimer->Cancel();
#ifdef DEBUG
  nsresult rv =
#endif
  sTimer->InitWithFuncCallback(OnTimeout, nsnull, GetTimeoutTime(),
                               nsITimer::TYPE_ONE_SHOT);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "nsITimer::InitWithFuncCallback failed");
}

nsIntPoint
nsMouseWheelTransaction::GetScreenPoint(nsGUIEvent* aEvent)
{
  NS_ASSERTION(aEvent, "aEvent is null");
  NS_ASSERTION(aEvent->widget, "aEvent-widget is null");
  return aEvent->refPoint + aEvent->widget->WidgetToScreenOffset();
}

PRUint32
nsMouseWheelTransaction::GetTimeoutTime()
{
  return Preferences::GetUint("mousewheel.transaction.timeout", 1500);
}

PRUint32
nsMouseWheelTransaction::GetIgnoreMoveDelayTime()
{
  return Preferences::GetUint("mousewheel.transaction.ignoremovedelay", 100);
}

bool
nsMouseWheelTransaction::IsAccelerationEnabled()
{
  return GetAccelerationStart() >= 0 && GetAccelerationFactor() > 0;
}

PRInt32
nsMouseWheelTransaction::AccelerateWheelDelta(PRInt32 aScrollLines,
                           bool aIsHorizontal,
                           bool aAllowScrollSpeedOverride,
                           nsIScrollableFrame::ScrollUnit *aScrollQuantity,
                           bool aLimitToMaxOnePageScroll)
{
  if (aAllowScrollSpeedOverride) {
    aScrollLines = OverrideSystemScrollSpeed(aScrollLines, aIsHorizontal);
  }

  // Accelerate by the sScrollSeriesCounter
  PRInt32 start = GetAccelerationStart();
  if (start >= 0 && sScrollSeriesCounter >= start) {
    PRInt32 factor = GetAccelerationFactor();
    if (factor > 0) {
      aScrollLines = ComputeAcceleratedWheelDelta(aScrollLines, factor);
    }
  }

  // If the computed delta is larger than the page, we should limit
  // the delta value to the one page size.
  return !aLimitToMaxOnePageScroll ? aScrollLines :
    LimitToOnePageScroll(aScrollLines, aIsHorizontal, aScrollQuantity);
}

PRInt32
nsMouseWheelTransaction::ComputeAcceleratedWheelDelta(PRInt32 aDelta,
                                                      PRInt32 aFactor)
{
  if (aDelta == 0)
    return 0;

  return PRInt32(NS_round(aDelta * sScrollSeriesCounter *
                          (double)aFactor / 10));
}

PRInt32
nsMouseWheelTransaction::GetAccelerationStart()
{
  return Preferences::GetInt("mousewheel.acceleration.start", -1);
}

PRInt32
nsMouseWheelTransaction::GetAccelerationFactor()
{
  return Preferences::GetInt("mousewheel.acceleration.factor", -1);
}

PRInt32
nsMouseWheelTransaction::OverrideSystemScrollSpeed(PRInt32 aScrollLines,
                                                   bool aIsHorizontal)
{
  NS_PRECONDITION(sTargetFrame, "We don't have mouse scrolling transaction");

  if (aScrollLines == 0) {
    return 0;
  }

  // We shouldn't override the scrolling speed on non root scroll frame.
  if (sTargetFrame !=
        sTargetFrame->PresContext()->PresShell()->GetRootScrollFrame()) {
    return aScrollLines;
  }

  // Compute the overridden speed to nsIWidget.  The widget can check the
  // conditions (e.g., checking the prefs, and also whether the user customized
  // the system settings of the mouse wheel scrolling or not), and can limit
  // the speed for preventing the unexpected high speed scrolling.
  nsCOMPtr<nsIWidget> widget(sTargetFrame->GetNearestWidget());
  NS_ENSURE_TRUE(widget, aScrollLines);
  PRInt32 overriddenDelta;
  nsresult rv = widget->OverrideSystemMouseScrollSpeed(aScrollLines,
                                                       aIsHorizontal,
                                                       overriddenDelta);
  NS_ENSURE_SUCCESS(rv, aScrollLines);
  return overriddenDelta;
}

PRInt32
nsMouseWheelTransaction::LimitToOnePageScroll(PRInt32 aScrollLines,
                           bool aIsHorizontal,
                           nsIScrollableFrame::ScrollUnit *aScrollQuantity)
{
  NS_ENSURE_TRUE(aScrollQuantity, aScrollLines);
  NS_PRECONDITION(*aScrollQuantity == nsIScrollableFrame::LINES,
                  "aScrollQuantity isn't by line");

  NS_ENSURE_TRUE(sTargetFrame, aScrollLines);
  nsIScrollableFrame* sf = sTargetFrame->GetScrollTargetFrame();
  NS_ENSURE_TRUE(sf, aScrollLines);

  // Limit scrolling to be at most one page, but if possible, try to
  // just adjust the number of scrolled lines.
  nsSize lineAmount = sf->GetLineScrollAmount();
  nscoord lineScroll = aIsHorizontal ? lineAmount.width : lineAmount.height;

  if (lineScroll == 0)
    return aScrollLines;

  nsSize pageAmount = sf->GetPageScrollAmount();
  nscoord pageScroll = aIsHorizontal ? pageAmount.width : pageAmount.height;

  if (NS_ABS(aScrollLines) * lineScroll < pageScroll)
    return aScrollLines;

  nscoord maxLines = (pageScroll / lineScroll);
  if (maxLines >= 1)
    return ((aScrollLines < 0) ? -1 : 1) * maxLines;

  *aScrollQuantity = nsIScrollableFrame::PAGES;
  return (aScrollLines < 0) ? -1 : 1;
}

/******************************************************************/
/* nsEventStateManager                                            */
/******************************************************************/

nsEventStateManager::nsEventStateManager()
  : mLockCursor(0),
    mPreLockPoint(0,0),
    mCurrentTarget(nsnull),
    mLastMouseOverFrame(nsnull),
    // init d&d gesture state machine variables
    mGestureDownPoint(0,0),
    mPresContext(nsnull),
    mLClickCount(0),
    mMClickCount(0),
    mRClickCount(0),
    m_haveShutdown(false),
    mLastLineScrollConsumedX(false),
    mLastLineScrollConsumedY(false),
    mClickHoldContextMenu(false)
{
  if (sESMInstanceCount == 0) {
    gUserInteractionTimerCallback = new nsUITimerCallback();
    if (gUserInteractionTimerCallback)
      NS_ADDREF(gUserInteractionTimerCallback);
    UpdateUserActivityTimer();
  }
  ++sESMInstanceCount;
}

nsresult
nsEventStateManager::UpdateUserActivityTimer(void)
{
  if (!gUserInteractionTimerCallback)
    return NS_OK;

  if (!gUserInteractionTimer)
    CallCreateInstance("@mozilla.org/timer;1", &gUserInteractionTimer);

  if (gUserInteractionTimer) {
    gUserInteractionTimer->InitWithCallback(gUserInteractionTimerCallback,
                                            NS_USER_INTERACTION_INTERVAL,
                                            nsITimer::TYPE_ONE_SHOT);
  }
  return NS_OK;
}

static const char* kObservedPrefs[] = {
  "accessibility.accesskeycausesactivation",
  "nglayout.events.dispatchLeftClickOnly",
  "ui.key.generalAccessKey",
  "ui.key.chromeAccess",
  "ui.key.contentAccess",
  "ui.click_hold_context_menus",
#if 0
  "mousewheel.withaltkey.action",
  "mousewheel.withaltkey.numlines",
  "mousewheel.withaltkey.sysnumlines",
  "mousewheel.withcontrolkey.action",
  "mousewheel.withcontrolkey.numlines",
  "mousewheel.withcontrolkey.sysnumlines",
  "mousewheel.withnokey.action",
  "mousewheel.withnokey.numlines",
  "mousewheel.withnokey.sysnumlines",
  "mousewheel.withshiftkey.action",
  "mousewheel.withshiftkey.numlines",
  "mousewheel.withshiftkey.sysnumlines",
#endif
  "dom.popup_allowed_events",
  nsnull
};

nsresult
nsEventStateManager::Init()
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (!observerService)
    return NS_ERROR_FAILURE;

  observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, true);

  if (sESMInstanceCount == 1) {
    sKeyCausesActivation =
      Preferences::GetBool("accessibility.accesskeycausesactivation",
                           sKeyCausesActivation);
    sLeftClickOnly =
      Preferences::GetBool("nglayout.events.dispatchLeftClickOnly",
                           sLeftClickOnly);
    sChromeAccessModifier =
      GetAccessModifierMaskFromPref(nsIDocShellTreeItem::typeChrome);
    sContentAccessModifier =
      GetAccessModifierMaskFromPref(nsIDocShellTreeItem::typeContent);
  }
  Preferences::AddWeakObservers(this, kObservedPrefs);

  mClickHoldContextMenu =
    Preferences::GetBool("ui.click_hold_context_menus", false);

  return NS_OK;
}

nsEventStateManager::~nsEventStateManager()
{
  if (sActiveESM == this) {
    sActiveESM = nsnull;
  }
  if (mClickHoldContextMenu)
    KillClickHoldTimer();

  if (mDocument == sMouseOverDocument)
    sMouseOverDocument = nsnull;

  --sESMInstanceCount;
  if(sESMInstanceCount == 0) {
    nsMouseWheelTransaction::Shutdown();
    if (gUserInteractionTimerCallback) {
      gUserInteractionTimerCallback->Notify(nsnull);
      NS_RELEASE(gUserInteractionTimerCallback);
    }
    if (gUserInteractionTimer) {
      gUserInteractionTimer->Cancel();
      NS_RELEASE(gUserInteractionTimer);
    }
  }

  if (sDragOverContent && sDragOverContent->OwnerDoc() == mDocument) {
    sDragOverContent = nsnull;
  }

  if (!m_haveShutdown) {
    Shutdown();

    // Don't remove from Observer service in Shutdown because Shutdown also
    // gets called from xpcom shutdown observer.  And we don't want to remove
    // from the service in that case.

    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    if (observerService) {
      observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    }
  }

}

nsresult
nsEventStateManager::Shutdown()
{
  Preferences::RemoveObservers(this, kObservedPrefs);
  m_haveShutdown = true;
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
        Preferences::GetBool("accessibility.accesskeycausesactivation",
                             sKeyCausesActivation);
    } else if (data.EqualsLiteral("nglayout.events.dispatchLeftClickOnly")) {
      sLeftClickOnly =
        Preferences::GetBool("nglayout.events.dispatchLeftClickOnly",
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
    } else if (data.EqualsLiteral("ui.click_hold_context_menus")) {
      mClickHoldContextMenu =
        Preferences::GetBool("ui.click_hold_context_menus", false);
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

NS_IMPL_CYCLE_COLLECTION_CLASS(nsEventStateManager)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsEventStateManager)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
   NS_INTERFACE_MAP_ENTRY(nsIObserver)
   NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsEventStateManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsEventStateManager)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsEventStateManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mCurrentTargetContent);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mLastMouseOverElement);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mGestureDownContent);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mGestureDownFrameOwner);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mLastLeftMouseDownContent);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mLastLeftMouseDownContentParent);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mLastMiddleMouseDownContent);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mLastMiddleMouseDownContentParent);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mLastRightMouseDownContent);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mLastRightMouseDownContentParent);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mActiveContent);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mHoverContent);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mURLTargetContent);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mFirstMouseOverEventElement);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mFirstMouseOutEventElement);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDocument);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mAccessKeys);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsEventStateManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mCurrentTargetContent);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mLastMouseOverElement);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mGestureDownContent);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mGestureDownFrameOwner);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mLastLeftMouseDownContent);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mLastLeftMouseDownContentParent);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mLastMiddleMouseDownContent);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mLastMiddleMouseDownContentParent);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mLastRightMouseDownContent);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mLastRightMouseDownContentParent);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mActiveContent);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mHoverContent);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mURLTargetContent);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mFirstMouseOverEventElement);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mFirstMouseOutEventElement);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDocument);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMARRAY(mAccessKeys);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

nsresult
nsEventStateManager::PreHandleEvent(nsPresContext* aPresContext,
                                    nsEvent *aEvent,
                                    nsIFrame* aTargetFrame,
                                    nsEventStatus* aStatus)
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
#ifdef DEBUG
  if (NS_IS_DRAG_EVENT(aEvent) && sIsPointerLocked) {
    NS_ASSERTION(sIsPointerLocked,
      "sIsPointerLocked is true. Drag events should be suppressed when the pointer is locked.");
  }
#endif
  // Store last known screenPoint and clientPoint so pointer lock
  // can use these values as constants.
  if (NS_IS_TRUSTED_EVENT(aEvent) &&
      ((NS_IS_MOUSE_EVENT_STRUCT(aEvent) &&
       IsMouseEventReal(aEvent)) ||
       aEvent->eventStructType == NS_MOUSE_SCROLL_EVENT)) {
    if (!sIsPointerLocked) {
      sLastScreenPoint = nsDOMUIEvent::CalculateScreenPoint(aPresContext, aEvent);
      sLastClientPoint = nsDOMUIEvent::CalculateClientPoint(aPresContext, aEvent, nsnull);
    }
  }

  // Do not take account NS_MOUSE_ENTER/EXIT so that loading a page
  // when user is not active doesn't change the state to active.
  if (NS_IS_TRUSTED_EVENT(aEvent) &&
      ((aEvent->eventStructType == NS_MOUSE_EVENT  &&
        IsMouseEventReal(aEvent) &&
        aEvent->message != NS_MOUSE_ENTER &&
        aEvent->message != NS_MOUSE_EXIT) ||
       aEvent->eventStructType == NS_MOUSE_SCROLL_EVENT ||
       aEvent->eventStructType == NS_KEY_EVENT)) {
    if (gMouseOrKeyboardEventCounter == 0) {
      nsCOMPtr<nsIObserverService> obs =
        mozilla::services::GetObserverService();
      if (obs) {
        obs->NotifyObservers(nsnull, "user-interaction-active", nsnull);
        UpdateUserActivityTimer();
      }
    }
    ++gMouseOrKeyboardEventCounter;
  }

  *aStatus = nsEventStatus_eIgnore;

  nsMouseWheelTransaction::OnEvent(aEvent);

  switch (aEvent->message) {
  case NS_MOUSE_BUTTON_DOWN:
    switch (static_cast<nsMouseEvent*>(aEvent)->button) {
    case nsMouseEvent::eLeftButton:
#ifndef XP_OS2
      BeginTrackingDragGesture(aPresContext, (nsMouseEvent*)aEvent, aTargetFrame);
#endif
      mLClickCount = ((nsMouseEvent*)aEvent)->clickCount;
      SetClickCount(aPresContext, (nsMouseEvent*)aEvent, aStatus);
      sNormalLMouseEventInProcess = true;
      break;
    case nsMouseEvent::eMiddleButton:
      mMClickCount = ((nsMouseEvent*)aEvent)->clickCount;
      SetClickCount(aPresContext, (nsMouseEvent*)aEvent, aStatus);
      break;
    case nsMouseEvent::eRightButton:
#ifdef XP_OS2
      BeginTrackingDragGesture(aPresContext, (nsMouseEvent*)aEvent, aTargetFrame);
#endif
      mRClickCount = ((nsMouseEvent*)aEvent)->clickCount;
      SetClickCount(aPresContext, (nsMouseEvent*)aEvent, aStatus);
      break;
    }
    break;
  case NS_MOUSE_BUTTON_UP:
    switch (static_cast<nsMouseEvent*>(aEvent)->button) {
      case nsMouseEvent::eLeftButton:
        if (mClickHoldContextMenu) {
          KillClickHoldTimer();
        }
#ifndef XP_OS2
        StopTrackingDragGesture();
#endif
        sNormalLMouseEventInProcess = false;
        // then fall through...
      case nsMouseEvent::eRightButton:
#ifdef XP_OS2
        StopTrackingDragGesture();
#endif
        // then fall through...
      case nsMouseEvent::eMiddleButton:
        SetClickCount(aPresContext, (nsMouseEvent*)aEvent, aStatus);
        break;
    }
    break;
  case NS_MOUSE_EXIT:
    // If the event is not a top-level window exit, then it's not
    // really an exit --- we may have traversed widget boundaries but
    // we're still in our toplevel window.
    {
      nsMouseEvent* mouseEvent = static_cast<nsMouseEvent*>(aEvent);
      if (mouseEvent->exit != nsMouseEvent::eTopLevel) {
        // Treat it as a synthetic move so we don't generate spurious
        // "exit" or "move" events.  Any necessary "out" or "over" events
        // will be generated by GenerateMouseEnterExit
        mouseEvent->message = NS_MOUSE_MOVE;
        mouseEvent->reason = nsMouseEvent::eSynthesized;
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
    // Flush pending layout changes, so that later mouse move events
    // will go to the right nodes.
    FlushPendingEvents(aPresContext);
    break;
  case NS_DRAGDROP_GESTURE:
    if (mClickHoldContextMenu) {
      // an external drag gesture event came in, not generated internally
      // by Gecko. Make sure we get rid of the click-hold timer.
      KillClickHoldTimer();
    }
    break;
  case NS_DRAGDROP_OVER:
    // NS_DRAGDROP_DROP is fired before NS_DRAGDROP_DRAGDROP so send
    // the enter/exit events before NS_DRAGDROP_DROP.
    GenerateDragDropEnterExit(aPresContext, (nsGUIEvent*)aEvent);
    break;

  case NS_KEY_PRESS:
    {

      nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;

      PRInt32 modifierMask = 0;
      if (keyEvent->IsShift())
        modifierMask |= NS_MODIFIER_SHIFT;
      if (keyEvent->IsControl())
        modifierMask |= NS_MODIFIER_CONTROL;
      if (keyEvent->IsAlt())
        modifierMask |= NS_MODIFIER_ALT;
      if (keyEvent->IsMeta())
        modifierMask |= NS_MODIFIER_META;
      if (keyEvent->IsOS())
        modifierMask |= NS_MODIFIER_OS;

      // Prevent keyboard scrolling while an accesskey modifier is in use.
      if (modifierMask && (modifierMask == sChromeAccessModifier ||
                           modifierMask == sContentAccessModifier))
        HandleAccessKey(aPresContext, keyEvent, aStatus, nsnull,
                        eAccessKeyProcessingNormal, modifierMask);
    }
    // then fall through...
  case NS_KEY_DOWN:
  case NS_KEY_UP:
    {
      nsIContent* content = GetFocusedContent();
      if (content)
        mCurrentTargetContent = content;
    }
    break;
  case NS_MOUSE_SCROLL:
    {
      nsIContent* content = GetFocusedContent();
      if (content)
        mCurrentTargetContent = content;

      nsMouseScrollEvent* msEvent = static_cast<nsMouseScrollEvent*>(aEvent);

      msEvent->delta = ComputeWheelDeltaFor(msEvent);
    }
    break;
  case NS_MOUSE_PIXEL_SCROLL:
    {
      nsIContent* content = GetFocusedContent();
      if (content)
        mCurrentTargetContent = content;

      nsMouseScrollEvent *msEvent = static_cast<nsMouseScrollEvent*>(aEvent);

      // Clear old deltas after a period of non action
      if (OutOfTime(gPixelScrollDeltaTimeout, nsMouseWheelTransaction::GetTimeoutTime())) {
        gPixelScrollDeltaX = gPixelScrollDeltaY = 0;
      }
      gPixelScrollDeltaTimeout = PR_IntervalToMilliseconds(PR_IntervalNow());

      // If needed send a line scroll event for pixel scrolls with kNoLines
      if (msEvent->scrollFlags & nsMouseScrollEvent::kNoLines) {
        nscoord pixelHeight = aPresContext->AppUnitsToIntCSSPixels(
          GetScrollableLineHeight(aTargetFrame));

        if (msEvent->scrollFlags & nsMouseScrollEvent::kIsVertical) {
          gPixelScrollDeltaX += msEvent->delta;
          if (!gPixelScrollDeltaX || !pixelHeight)
            break;

          if (NS_ABS(gPixelScrollDeltaX) >= pixelHeight) {
            PRInt32 numLines = (PRInt32)ceil((float)gPixelScrollDeltaX/(float)pixelHeight);

            gPixelScrollDeltaX -= numLines*pixelHeight;

            nsWeakFrame weakFrame(aTargetFrame);
            SendLineScrollEvent(aTargetFrame, msEvent, aPresContext,
              aStatus, numLines);
            NS_ENSURE_STATE(weakFrame.IsAlive());
          }
        } else if (msEvent->scrollFlags & nsMouseScrollEvent::kIsHorizontal) {
          gPixelScrollDeltaY += msEvent->delta;
          if (!gPixelScrollDeltaY || !pixelHeight)
            break;

          if (NS_ABS(gPixelScrollDeltaY) >= pixelHeight) {
            PRInt32 numLines = (PRInt32)ceil((float)gPixelScrollDeltaY/(float)pixelHeight);

            gPixelScrollDeltaY -= numLines*pixelHeight;

            nsWeakFrame weakFrame(aTargetFrame);
            SendLineScrollEvent(aTargetFrame, msEvent, aPresContext,
              aStatus, numLines);
            NS_ENSURE_STATE(weakFrame.IsAlive());
          }
        }
      }

      // When the last line scroll has been canceled, eat the pixel scroll event
      if ((msEvent->scrollFlags & nsMouseScrollEvent::kIsHorizontal) ?
           mLastLineScrollConsumedX : mLastLineScrollConsumedY) {
        *aStatus = nsEventStatus_eConsumeNoDefault;
      }
    }
    break;
  case NS_QUERY_SELECTED_TEXT:
    DoQuerySelectedText(static_cast<nsQueryContentEvent*>(aEvent));
    break;
  case NS_QUERY_TEXT_CONTENT:
    {
      if (RemoteQueryContentEvent(aEvent))
        break;
      nsContentEventHandler handler(mPresContext);
      handler.OnQueryTextContent((nsQueryContentEvent*)aEvent);
    }
    break;
  case NS_QUERY_CARET_RECT:
    {
      // XXX remote event
      nsContentEventHandler handler(mPresContext);
      handler.OnQueryCaretRect((nsQueryContentEvent*)aEvent);
    }
    break;
  case NS_QUERY_TEXT_RECT:
    {
      // XXX remote event
      nsContentEventHandler handler(mPresContext);
      handler.OnQueryTextRect((nsQueryContentEvent*)aEvent);
    }
    break;
  case NS_QUERY_EDITOR_RECT:
    {
      // XXX remote event
      nsContentEventHandler handler(mPresContext);
      handler.OnQueryEditorRect((nsQueryContentEvent*)aEvent);
    }
    break;
  case NS_QUERY_CONTENT_STATE:
    {
      // XXX remote event
      nsContentEventHandler handler(mPresContext);
      handler.OnQueryContentState(static_cast<nsQueryContentEvent*>(aEvent));
    }
    break;
  case NS_QUERY_SELECTION_AS_TRANSFERABLE:
    {
      // XXX remote event
      nsContentEventHandler handler(mPresContext);
      handler.OnQuerySelectionAsTransferable(static_cast<nsQueryContentEvent*>(aEvent));
    }
    break;
  case NS_QUERY_CHARACTER_AT_POINT:
    {
      // XXX remote event
      nsContentEventHandler handler(mPresContext);
      handler.OnQueryCharacterAtPoint(static_cast<nsQueryContentEvent*>(aEvent));
    }
    break;
  case NS_QUERY_DOM_WIDGET_HITTEST:
    {
      // XXX remote event
      nsContentEventHandler handler(mPresContext);
      handler.OnQueryDOMWidgetHittest(static_cast<nsQueryContentEvent*>(aEvent));
    }
    break;
  case NS_QUERY_SCROLL_TARGET_INFO:
    {
      DoQueryScrollTargetInfo(static_cast<nsQueryContentEvent*>(aEvent),
                              aTargetFrame);
      break;
    }
  case NS_SELECTION_SET:
    {
      nsSelectionEvent *selectionEvent =
          static_cast<nsSelectionEvent*>(aEvent);
      if (IsTargetCrossProcess(selectionEvent)) {
        // Will not be handled locally, remote the event
        if (GetCrossProcessTarget()->SendSelectionEvent(*selectionEvent))
          selectionEvent->mSucceeded = true;
        break;
      }
      nsContentEventHandler handler(mPresContext);
      handler.OnSelectionEvent((nsSelectionEvent*)aEvent);
    }
    break;
  case NS_CONTENT_COMMAND_CUT:
  case NS_CONTENT_COMMAND_COPY:
  case NS_CONTENT_COMMAND_PASTE:
  case NS_CONTENT_COMMAND_DELETE:
  case NS_CONTENT_COMMAND_UNDO:
  case NS_CONTENT_COMMAND_REDO:
  case NS_CONTENT_COMMAND_PASTE_TRANSFERABLE:
    {
      DoContentCommandEvent(static_cast<nsContentCommandEvent*>(aEvent));
    }
    break;
  case NS_CONTENT_COMMAND_SCROLL:
    {
      DoContentCommandScrollEvent(static_cast<nsContentCommandEvent*>(aEvent));
    }
    break;
  case NS_TEXT_TEXT:
    {
      nsTextEvent *textEvent = static_cast<nsTextEvent*>(aEvent);
      if (IsTargetCrossProcess(textEvent)) {
        // Will not be handled locally, remote the event
        if (GetCrossProcessTarget()->SendTextEvent(*textEvent)) {
          // Cancel local dispatching
          aEvent->flags |= NS_EVENT_FLAG_STOP_DISPATCH;
        }
      }
    }
    break;
  case NS_COMPOSITION_START:
    if (NS_IS_TRUSTED_EVENT(aEvent)) {
      // If the event is trusted event, set the selected text to data of
      // composition event.
      nsCompositionEvent *compositionEvent =
        static_cast<nsCompositionEvent*>(aEvent);
      nsQueryContentEvent selectedText(true, NS_QUERY_SELECTED_TEXT,
                                       compositionEvent->widget);
      DoQuerySelectedText(&selectedText);
      NS_ASSERTION(selectedText.mSucceeded, "Failed to get selected text");
      compositionEvent->data = selectedText.mReply.mString;
    }
    // through to compositionend handling
  case NS_COMPOSITION_UPDATE:
  case NS_COMPOSITION_END:
    {
      nsCompositionEvent *compositionEvent =
          static_cast<nsCompositionEvent*>(aEvent);
      if (IsTargetCrossProcess(compositionEvent)) {
        // Will not be handled locally, remote the event
        if (GetCrossProcessTarget()->SendCompositionEvent(*compositionEvent)) {
          // Cancel local dispatching
          aEvent->flags |= NS_EVENT_FLAG_STOP_DISPATCH;
        }
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

static bool
IsAccessKeyTarget(nsIContent* aContent, nsIFrame* aFrame, nsAString& aKey)
{
  if (!aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::accesskey, aKey,
                             eIgnoreCase))
    return false;

  nsCOMPtr<nsIDOMXULDocument> xulDoc =
    do_QueryInterface(aContent->OwnerDoc());
  if (!xulDoc && !aContent->IsXUL())
    return true;

    // For XUL we do visibility checks.
  if (!aFrame)
    return false;

  if (aFrame->IsFocusable())
    return true;

  if (!aFrame->IsVisibleConsideringAncestors())
    return false;

  // XUL controls can be activated.
  nsCOMPtr<nsIDOMXULControlElement> control(do_QueryInterface(aContent));
  if (control)
    return true;

  if (aContent->IsHTML()) {
    nsIAtom* tag = aContent->Tag();

    // HTML area, label and legend elements are never focusable, so
    // we need to check for them explicitly before giving up.
    if (tag == nsGkAtoms::area ||
        tag == nsGkAtoms::label ||
        tag == nsGkAtoms::legend)
      return true;

  } else if (aContent->IsXUL()) {
    // XUL label elements are never focusable, so we need to check for them
    // explicitly before giving up.
    if (aContent->Tag() == nsGkAtoms::label)
      return true;
  }

  return false;
}

bool
nsEventStateManager::ExecuteAccessKey(nsTArray<PRUint32>& aAccessCharCodes,
                                      bool aIsTrustedEvent)
{
  PRInt32 count, start = -1;
  nsIContent* focusedContent = GetFocusedContent();
  if (focusedContent) {
    start = mAccessKeys.IndexOf(focusedContent);
    if (start == -1 && focusedContent->GetBindingParent())
      start = mAccessKeys.IndexOf(focusedContent->GetBindingParent());
  }
  nsIContent *content;
  nsIFrame *frame;
  PRInt32 length = mAccessKeys.Count();
  for (PRUint32 i = 0; i < aAccessCharCodes.Length(); ++i) {
    PRUint32 ch = aAccessCharCodes[i];
    nsAutoString accessKey;
    AppendUCS4ToUTF16(ch, accessKey);
    for (count = 1; count <= length; ++count) {
      content = mAccessKeys[(start + count) % length];
      frame = content->GetPrimaryFrame();
      if (IsAccessKeyTarget(content, frame, accessKey)) {
        bool shouldActivate = sKeyCausesActivation;
        while (shouldActivate && ++count <= length) {
          nsIContent *oc = mAccessKeys[(start + count) % length];
          nsIFrame *of = oc->GetPrimaryFrame();
          if (IsAccessKeyTarget(oc, of, accessKey))
            shouldActivate = false;
        }
        if (shouldActivate)
          content->PerformAccesskey(shouldActivate, aIsTrustedEvent);
        else {
          nsIFocusManager* fm = nsFocusManager::GetFocusManager();
          if (fm) {
            nsCOMPtr<nsIDOMElement> element = do_QueryInterface(content);
            fm->SetFocus(element, nsIFocusManager::FLAG_BYKEY);
          }
        }
        return true;
      }
    }
  }
  return false;
}

bool
nsEventStateManager::GetAccessKeyLabelPrefix(nsAString& aPrefix)
{
  aPrefix.Truncate();
  nsAutoString separator, modifierText;
  nsContentUtils::GetModifierSeparatorText(separator);

  nsCOMPtr<nsISupports> container = mPresContext->GetContainer();
  PRInt32 modifier = GetAccessModifierMask(container);

  if (modifier & NS_MODIFIER_CONTROL) {
    nsContentUtils::GetControlText(modifierText);
    aPrefix.Append(modifierText + separator);
  }
  if (modifier & NS_MODIFIER_META) {
    nsContentUtils::GetMetaText(modifierText);
    aPrefix.Append(modifierText + separator);
  }
  if (modifier & NS_MODIFIER_OS) {
    nsContentUtils::GetOSText(modifierText);
    aPrefix.Append(modifierText + separator);
  }
  if (modifier & NS_MODIFIER_ALT) {
    nsContentUtils::GetAltText(modifierText);
    aPrefix.Append(modifierText + separator);
  }
  if (modifier & NS_MODIFIER_SHIFT) {
    nsContentUtils::GetShiftText(modifierText);
    aPrefix.Append(modifierText + separator);
  }
  return !aPrefix.IsEmpty();
}

void
nsEventStateManager::HandleAccessKey(nsPresContext* aPresContext,
                                     nsKeyEvent *aEvent,
                                     nsEventStatus* aStatus,
                                     nsIDocShellTreeItem* aBubbledFrom,
                                     ProcessingAccessKeyState aAccessKeyState,
                                     PRInt32 aModifierMask)
{
  nsCOMPtr<nsISupports> pcContainer = aPresContext->GetContainer();

  // Alt or other accesskey modifier is down, we may need to do an accesskey
  if (mAccessKeys.Count() > 0 &&
      aModifierMask == GetAccessModifierMask(pcContainer)) {
    // Someone registered an accesskey.  Find and activate it.
    bool isTrusted = NS_IS_TRUSTED_EVENT(aEvent);
    nsAutoTArray<PRUint32, 10> accessCharCodes;
    nsContentUtils::GetAccessKeyCandidates(aEvent, accessCharCodes);
    if (ExecuteAccessKey(accessCharCodes, isTrusted)) {
      *aStatus = nsEventStatus_eConsumeNoDefault;
      return;
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
      nsCOMPtr<nsIDocShellTreeItem> subShellItem;
      docShell->GetChildAt(counter, getter_AddRefs(subShellItem));
      if (aAccessKeyState == eAccessKeyProcessingUp &&
          subShellItem == aBubbledFrom)
        continue;

      nsCOMPtr<nsIDocShell> subDS = do_QueryInterface(subShellItem);
      if (subDS && IsShellVisible(subDS)) {
        nsCOMPtr<nsIPresShell> subPS;
        subDS->GetPresShell(getter_AddRefs(subPS));

        // Docshells need not have a presshell (eg. display:none
        // iframes, docshells in transition between documents, etc).
        if (!subPS) {
          // Oh, well.  Just move on to the next child
          continue;
        }

        nsPresContext *subPC = subPS->GetPresContext();

        nsEventStateManager* esm =
          static_cast<nsEventStateManager *>(subPC->EventStateManager());

        if (esm)
          esm->HandleAccessKey(subPC, aEvent, aStatus, nsnull,
                               eAccessKeyProcessingDown, aModifierMask);

        if (nsEventStatus_eConsumeNoDefault == *aStatus)
          break;
      }
    }
  }// if end . checking all sub docshell ends here.

  // bubble up the process to the parent docshell if necessary
  if (eAccessKeyProcessingDown != aAccessKeyState && nsEventStatus_eConsumeNoDefault != *aStatus) {
    nsCOMPtr<nsIDocShellTreeItem> docShell(do_QueryInterface(pcContainer));
    if (!docShell) {
      NS_WARNING("no docShellTreeItem for presContext");
      return;
    }

    nsCOMPtr<nsIDocShellTreeItem> parentShellItem;
    docShell->GetParent(getter_AddRefs(parentShellItem));
    nsCOMPtr<nsIDocShell> parentDS = do_QueryInterface(parentShellItem);
    if (parentDS) {
      nsCOMPtr<nsIPresShell> parentPS;

      parentDS->GetPresShell(getter_AddRefs(parentPS));
      NS_ASSERTION(parentPS, "Our PresShell exists but the parent's does not?");

      nsPresContext *parentPC = parentPS->GetPresContext();
      NS_ASSERTION(parentPC, "PresShell without PresContext");

      nsEventStateManager* esm =
        static_cast<nsEventStateManager *>(parentPC->EventStateManager());

      if (esm)
        esm->HandleAccessKey(parentPC, aEvent, aStatus, docShell,
                             eAccessKeyProcessingUp, aModifierMask);
    }
  }// if end. bubble up process
}// end of HandleAccessKey

bool
nsEventStateManager::DispatchCrossProcessEvent(nsEvent* aEvent,
                                               nsFrameLoader* aFrameLoader,
                                               nsEventStatus *aStatus) {
  PBrowserParent* remoteBrowser = aFrameLoader->GetRemoteBrowser();
  TabParent* remote = static_cast<TabParent*>(remoteBrowser);
  if (!remote) {
    return false;
  }

  switch (aEvent->eventStructType) {
  case NS_MOUSE_EVENT: {
    nsMouseEvent* mouseEvent = static_cast<nsMouseEvent*>(aEvent);
    return remote->SendRealMouseEvent(*mouseEvent);
  }
  case NS_KEY_EVENT: {
    nsKeyEvent* keyEvent = static_cast<nsKeyEvent*>(aEvent);
    return remote->SendRealKeyEvent(*keyEvent);
  }
  case NS_MOUSE_SCROLL_EVENT: {
    nsMouseScrollEvent* scrollEvent = static_cast<nsMouseScrollEvent*>(aEvent);
    return remote->SendMouseScrollEvent(*scrollEvent);
  }
  case NS_TOUCH_EVENT: {
    // Let the child process synthesize a mouse event if needed, and
    // ensure we don't synthesize one in this process.
    *aStatus = nsEventStatus_eConsumeNoDefault;
    nsTouchEvent* touchEvent = static_cast<nsTouchEvent*>(aEvent);
    return remote->SendRealTouchEvent(*touchEvent);
  }
  default: {
    MOZ_NOT_REACHED("Attempt to send non-whitelisted event?");
    return false;
  }
  }
}

bool
nsEventStateManager::IsRemoteTarget(nsIContent* target) {
  if (!target) {
    return false;
  }

  // <browser/iframe remote=true> from XUL
  if ((target->Tag() == nsGkAtoms::browser ||
       target->Tag() == nsGkAtoms::iframe) &&
      target->IsXUL() &&
      target->AttrValueIs(kNameSpaceID_None, nsGkAtoms::Remote,
                          nsGkAtoms::_true, eIgnoreCase)) {
    return true;
  }

  // <frame/iframe mozbrowser>
  nsCOMPtr<nsIMozBrowserFrame> browserFrame = do_QueryInterface(target);
  if (browserFrame) {
    bool isBrowser = false;
    browserFrame->GetReallyIsBrowser(&isBrowser);
    if (isBrowser) {
      return !!TabParent::GetFrom(target);
    }
  }

  return false;
}

bool
CrossProcessSafeEvent(const nsEvent& aEvent)
{
  switch (aEvent.eventStructType) {
  case NS_KEY_EVENT:
  case NS_MOUSE_SCROLL_EVENT:
    return true;
  case NS_MOUSE_EVENT:
    switch (aEvent.message) {
    case NS_MOUSE_BUTTON_DOWN:
    case NS_MOUSE_BUTTON_UP:
    case NS_MOUSE_MOVE:
      return true;
    default:
      return false;
    }
  case NS_TOUCH_EVENT:
    switch (aEvent.message) {
    case NS_TOUCH_START:
    case NS_TOUCH_MOVE:
    case NS_TOUCH_END:
    case NS_TOUCH_CANCEL:
      return true;
    default:
      return false;
    }
  default:
    return false;
  }
}

bool
nsEventStateManager::HandleCrossProcessEvent(nsEvent *aEvent,
                                             nsIFrame* aTargetFrame,
                                             nsEventStatus *aStatus) {
  if (*aStatus == nsEventStatus_eConsumeNoDefault ||
      !CrossProcessSafeEvent(*aEvent)) {
    return false;
  }

  // Collect the remote event targets we're going to forward this
  // event to.
  //
  // NB: the elements of |targets| must be unique, for correctness.
  nsAutoTArray<nsCOMPtr<nsIContent>, 1> targets;
  if (aEvent->eventStructType != NS_TOUCH_EVENT ||
      aEvent->message == NS_TOUCH_START) {
    // If this event only has one target, and it's remote, add it to
    // the array.
    nsIContent* target = mCurrentTargetContent;
    if (!target && aTargetFrame) {
      target = aTargetFrame->GetContent();
    }
    if (IsRemoteTarget(target)) {
      targets.AppendElement(target);
    }
  } else {
    // This is a touch event with possibly multiple touch points.
    // Each touch point may have its own target.  So iterate through
    // all of them and collect the unique set of targets for event
    // forwarding.
    //
    // This loop is similar to the one used in
    // PresShell::DispatchTouchEvent().
    nsTouchEvent* touchEvent = static_cast<nsTouchEvent*>(aEvent);
    const nsTArray<nsCOMPtr<nsIDOMTouch> >& touches = touchEvent->touches;
    for (PRUint32 i = 0; i < touches.Length(); ++i) {
      nsIDOMTouch* touch = touches[i];
      // NB: the |mChanged| check is an optimization, subprocesses can
      // compute this for themselves.  If the touch hasn't changed, we
      // may be able to avoid forwarding the event entirely (which is
      // not free).
      if (!touch || !touch->mChanged) {
        continue;
      }
      nsCOMPtr<nsIDOMEventTarget> targetPtr;
      touch->GetTarget(getter_AddRefs(targetPtr));
      if (!targetPtr) {
        continue;
      }
      nsCOMPtr<nsIContent> target = do_QueryInterface(targetPtr);
      if (IsRemoteTarget(target) && !targets.Contains(target)) {
        targets.AppendElement(target);
      }
    }
  }

  if (targets.Length() == 0) {
    return false;
  }

  // Look up the frame loader for all the remote targets we found, and
  // then dispatch the event to the remote content they represent.
  bool dispatched = false;
  for (PRUint32 i = 0; i < targets.Length(); ++i) {
    nsIContent* target = targets[i];
    nsCOMPtr<nsIFrameLoaderOwner> loaderOwner = do_QueryInterface(target);
    if (!loaderOwner) {
      continue;
    }

    nsRefPtr<nsFrameLoader> frameLoader = loaderOwner->GetFrameLoader();
    if (!frameLoader) {
      continue;
    }

    PRUint32 eventMode;
    frameLoader->GetEventMode(&eventMode);
    if (eventMode == nsIFrameLoader::EVENT_MODE_DONT_FORWARD_TO_CHILD) {
      continue;
    }

    // The "toplevel widget" in content processes is always at position
    // 0,0.  Map the event coordinates to match that.
    if (aEvent->eventStructType != NS_TOUCH_EVENT) {
      nsPoint pt = nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent,
                                                                aTargetFrame);
      aEvent->refPoint =
        pt.ToNearestPixels(mPresContext->AppUnitsPerDevPixel());
    } else {
      nsIFrame* targetFrame = frameLoader->GetPrimaryFrameOfOwningContent();
      aEvent->refPoint = nsIntPoint();
      // Find out how far we're offset from the nearest widget.
      nsPoint offset =
        nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, targetFrame);
      nsIntPoint intOffset =
        offset.ToNearestPixels(mPresContext->AppUnitsPerDevPixel());
      nsTouchEvent* touchEvent = static_cast<nsTouchEvent*>(aEvent);
      // Then offset all the touch points by that distance, to put them
      // in the space where top-left is 0,0.
      const nsTArray<nsCOMPtr<nsIDOMTouch> >& touches = touchEvent->touches;
      for (PRUint32 i = 0; i < touches.Length(); ++i) {
        nsIDOMTouch* touch = touches[i];
        if (touch) {
          touch->mRefPoint += intOffset;
        }
      }
    }

    dispatched |= DispatchCrossProcessEvent(aEvent, frameLoader, aStatus);
  }
  return dispatched;
}

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
  if (mClickHoldTimer) {
    PRInt32 clickHoldDelay =
      Preferences::GetInt("ui.click_hold_context_menus.delay", 500);
    mClickHoldTimer->InitWithFuncCallback(sClickHoldCallback, this,
                                          clickHoldDelay,
                                          nsITimer::TYPE_ONE_SHOT);
  }
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
  nsEventStateManager* self = static_cast<nsEventStateManager*>(aESM);
  if (self)
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
  if (!mGestureDownContent)
    return;

#ifdef XP_MACOSX
  // Hack to ensure that we don't show a context menu when the user
  // let go of the mouse after a long cpu-hogging operation prevented
  // us from handling any OS events. See bug 117589.
  if (!CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, kCGMouseButtonLeft))
    return;
#endif

  nsEventStatus status = nsEventStatus_eIgnore;

  // Dispatch to the DOM. We have to fake out the ESM and tell it that the
  // current target frame is actually where the mouseDown occurred, otherwise it
  // will use the frame the mouse is currently over which may or may not be
  // the same. (Note: saari and I have decided that we don't have to reset |mCurrentTarget|
  // when we're through because no one else is doing anything more with this
  // event and it will get reset on the very next event to the correct frame).
  mCurrentTarget = mPresContext->GetPrimaryFrameFor(mGestureDownContent);
  if (mCurrentTarget) {
    NS_ASSERTION(mPresContext == mCurrentTarget->PresContext(),
                 "a prescontext returned a primary frame that didn't belong to it?");

    // before dispatching, check that we're not on something that
    // doesn't get a context menu
    nsIAtom *tag = mGestureDownContent->Tag();
    bool allowedToDispatch = true;

    if (mGestureDownContent->IsXUL()) {
      if (tag == nsGkAtoms::scrollbar ||
          tag == nsGkAtoms::scrollbarbutton ||
          tag == nsGkAtoms::button)
        allowedToDispatch = false;
      else if (tag == nsGkAtoms::toolbarbutton) {
        // a <toolbarbutton> that has the container attribute set
        // will already have its own dropdown.
        if (nsContentUtils::HasNonEmptyAttr(mGestureDownContent,
                kNameSpaceID_None, nsGkAtoms::container)) {
          allowedToDispatch = false;
        } else {
          // If the toolbar button has an open menu, don't attempt to open
            // a second menu
          if (mGestureDownContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::open,
                                               nsGkAtoms::_true, eCaseMatters)) {
            allowedToDispatch = false;
          }
        }
      }
    }
    else if (mGestureDownContent->IsHTML()) {
      nsCOMPtr<nsIFormControl> formCtrl(do_QueryInterface(mGestureDownContent));

      if (formCtrl) {
        // of all form controls, only ones dealing with text are
        // allowed to have context menus
        PRInt32 type = formCtrl->GetType();

        allowedToDispatch = (type == NS_FORM_INPUT_TEXT ||
                             type == NS_FORM_INPUT_EMAIL ||
                             type == NS_FORM_INPUT_SEARCH ||
                             type == NS_FORM_INPUT_TEL ||
                             type == NS_FORM_INPUT_URL ||
                             type == NS_FORM_INPUT_PASSWORD ||
                             type == NS_FORM_INPUT_FILE ||
                             type == NS_FORM_INPUT_NUMBER ||
                             type == NS_FORM_TEXTAREA);
      }
      else if (tag == nsGkAtoms::applet ||
               tag == nsGkAtoms::embed  ||
               tag == nsGkAtoms::object) {
        allowedToDispatch = false;
      }
    }

    if (allowedToDispatch) {
      // make sure the widget sticks around
      nsCOMPtr<nsIWidget> targetWidget(mCurrentTarget->GetNearestWidget());
      // init the event while mCurrentTarget is still good
      nsMouseEvent event(true, NS_CONTEXTMENU,
                         targetWidget,
                         nsMouseEvent::eReal);
      event.clickCount = 1;
      FillInEventFromGestureDown(&event);
        
      // stop selection tracking, we're in control now
      if (mCurrentTarget)
      {
        nsRefPtr<nsFrameSelection> frameSel =
          mCurrentTarget->GetFrameSelection();
        
        if (frameSel && frameSel->GetMouseDownState()) {
          // note that this can cause selection changed events to fire if we're in
          // a text field, which will null out mCurrentTarget
          frameSel->SetMouseDownState(false);
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

  // now check if the event has been handled. If so, stop tracking a drag
  if (status == nsEventStatus_eConsumeNoDefault) {
    StopTrackingDragGesture();
  }

  KillClickHoldTimer();

} // FireContextClick


//
// BeginTrackingDragGesture
//
// Record that the mouse has gone down and that we should move to TRACKING state
// of d&d gesture tracker.
//
// We also use this to track click-hold context menus. When the mouse goes down,
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
  if (!inDownEvent->widget)
    return;

  // Note that |inDownEvent| could be either a mouse down event or a
  // synthesized mouse move event.
  mGestureDownPoint = inDownEvent->refPoint +
    inDownEvent->widget->WidgetToScreenOffset();

  inDownFrame->GetContentForEvent(inDownEvent,
                                  getter_AddRefs(mGestureDownContent));

  mGestureDownFrameOwner = inDownFrame->GetContent();
  mGestureModifiers = inDownEvent->modifiers;
  mGestureDownButtons = inDownEvent->buttons;

  if (mClickHoldContextMenu) {
    // fire off a timer to track click-hold
    CreateClickHoldTimer(aPresContext, inDownFrame, inDownEvent);
  }
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
  NS_ASSERTION(aEvent->widget == mCurrentTarget->GetNearestWidget(),
               "Incorrect widget in event");

  // Set the coordinates in the new event to the coordinates of
  // the old event, adjusted for the fact that the widget might be
  // different
  nsIntPoint tmpPoint = aEvent->widget->WidgetToScreenOffset();
  aEvent->refPoint = mGestureDownPoint - tmpPoint;
  aEvent->modifiers = mGestureModifiers;
  aEvent->buttons = mGestureDownButtons;
}

//
// GenerateDragGesture
//
// If we're in the TRACKING state of the d&d gesture tracker, check the current position
// of the mouse in relation to the old one. If we've moved a sufficient amount from
// the mouse down, then fire off a drag gesture event.
void
nsEventStateManager::GenerateDragGesture(nsPresContext* aPresContext,
                                         nsMouseEvent *aEvent)
{
  NS_ASSERTION(aPresContext, "This shouldn't happen.");
  if (IsTrackingDragGesture()) {
    mCurrentTarget = mGestureDownFrameOwner->GetPrimaryFrame();

    if (!mCurrentTarget) {
      StopTrackingDragGesture();
      return;
    }

    // Check if selection is tracking drag gestures, if so
    // don't interfere!
    if (mCurrentTarget)
    {
      nsRefPtr<nsFrameSelection> frameSel = mCurrentTarget->GetFrameSelection();
      if (frameSel && frameSel->GetMouseDownState()) {
        StopTrackingDragGesture();
        return;
      }
    }

    // If non-native code is capturing the mouse don't start a drag.
    if (nsIPresShell::IsMouseCapturePreventingDrag()) {
      StopTrackingDragGesture();
      return;
    }

    static PRInt32 pixelThresholdX = 0;
    static PRInt32 pixelThresholdY = 0;

    if (!pixelThresholdX) {
      pixelThresholdX =
        LookAndFeel::GetInt(LookAndFeel::eIntID_DragThresholdX, 0);
      pixelThresholdY =
        LookAndFeel::GetInt(LookAndFeel::eIntID_DragThresholdY, 0);
      if (!pixelThresholdX)
        pixelThresholdX = 5;
      if (!pixelThresholdY)
        pixelThresholdY = 5;
    }

    // fire drag gesture if mouse has moved enough
    nsIntPoint pt = aEvent->refPoint + aEvent->widget->WidgetToScreenOffset();
    if (NS_ABS(pt.x - mGestureDownPoint.x) > pixelThresholdX ||
        NS_ABS(pt.y - mGestureDownPoint.y) > pixelThresholdY) {
      if (mClickHoldContextMenu) {
        // stop the click-hold before we fire off the drag gesture, in case
        // it takes a long time
        KillClickHoldTimer();
      }

      nsRefPtr<nsDOMDataTransfer> dataTransfer = new nsDOMDataTransfer();
      if (!dataTransfer)
        return;

      nsCOMPtr<nsISelection> selection;
      nsCOMPtr<nsIContent> eventContent, targetContent;
      mCurrentTarget->GetContentForEvent(aEvent, getter_AddRefs(eventContent));
      if (eventContent)
        DetermineDragTarget(aPresContext, eventContent, dataTransfer,
                            getter_AddRefs(selection), getter_AddRefs(targetContent));

      // Stop tracking the drag gesture now. This should stop us from
      // reentering GenerateDragGesture inside DOM event processing.
      StopTrackingDragGesture();

      if (!targetContent)
        return;

      sLastDragOverFrame = nsnull;
      nsCOMPtr<nsIWidget> widget = mCurrentTarget->GetNearestWidget();

      // get the widget from the target frame
      nsDragEvent startEvent(NS_IS_TRUSTED_EVENT(aEvent), NS_DRAGDROP_START, widget);
      FillInEventFromGestureDown(&startEvent);

      nsDragEvent gestureEvent(NS_IS_TRUSTED_EVENT(aEvent), NS_DRAGDROP_GESTURE, widget);
      FillInEventFromGestureDown(&gestureEvent);

      startEvent.dataTransfer = gestureEvent.dataTransfer = dataTransfer;
      startEvent.inputSource = gestureEvent.inputSource = aEvent->inputSource;

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

      // Dispatch both the dragstart and draggesture events to the DOM. For
      // elements in an editor, only fire the draggesture event so that the
      // editor code can handle it but content doesn't see a dragstart.
      nsEventStatus status = nsEventStatus_eIgnore;
      nsEventDispatcher::Dispatch(targetContent, aPresContext, &startEvent, nsnull,
                                  &status);

      nsDragEvent* event = &startEvent;
      if (status != nsEventStatus_eConsumeNoDefault) {
        status = nsEventStatus_eIgnore;
        nsEventDispatcher::Dispatch(targetContent, aPresContext, &gestureEvent, nsnull,
                                    &status);
        event = &gestureEvent;
      }

      // now that the dataTransfer has been updated in the dragstart and
      // draggesture events, make it read only so that the data doesn't
      // change during the drag.
      dataTransfer->SetReadOnly();

      if (status != nsEventStatus_eConsumeNoDefault) {
        bool dragStarted = DoDefaultDragStart(aPresContext, event, dataTransfer,
                                              targetContent, selection);
        if (dragStarted) {
          sActiveESM = nsnull;
          aEvent->flags |= NS_EVENT_FLAG_STOP_DISPATCH;
        }
      }

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

void
nsEventStateManager::DetermineDragTarget(nsPresContext* aPresContext,
                                         nsIContent* aSelectionTarget,
                                         nsDOMDataTransfer* aDataTransfer,
                                         nsISelection** aSelection,
                                         nsIContent** aTargetNode)
{
  *aTargetNode = nsnull;

  nsCOMPtr<nsISupports> container = aPresContext->GetContainer();
  nsCOMPtr<nsIDOMWindow> window = do_GetInterface(container);
  if (!window)
    return;

  // GetDragData determines if a selection, link or image in the content
  // should be dragged, and places the data associated with the drag in the
  // data transfer.
  // mGestureDownContent is the node where the mousedown event for the drag
  // occurred, and aSelectionTarget is the node to use when a selection is used
  bool canDrag;
  nsCOMPtr<nsIContent> dragDataNode;
  bool wasAlt = (mGestureModifiers & widget::MODIFIER_ALT) != 0;
  nsresult rv = nsContentAreaDragDrop::GetDragData(window, mGestureDownContent,
                                                   aSelectionTarget, wasAlt,
                                                   aDataTransfer, &canDrag, aSelection,
                                                   getter_AddRefs(dragDataNode));
  if (NS_FAILED(rv) || !canDrag)
    return;

  // if GetDragData returned a node, use that as the node being dragged.
  // Otherwise, if a selection is being dragged, use the node within the
  // selection that was dragged. Otherwise, just use the mousedown target.
  nsIContent* dragContent = mGestureDownContent;
  if (dragDataNode)
    dragContent = dragDataNode;
  else if (*aSelection)
    dragContent = aSelectionTarget;

  nsIContent* originalDragContent = dragContent;

  // If a selection isn't being dragged, look for an ancestor with the
  // draggable property set. If one is found, use that as the target of the
  // drag instead of the node that was clicked on. If a draggable node wasn't
  // found, just use the clicked node.
  if (!*aSelection) {
    while (dragContent) {
      nsCOMPtr<nsIDOMHTMLElement> htmlElement = do_QueryInterface(dragContent);
      if (htmlElement) {
        bool draggable = false;
        htmlElement->GetDraggable(&draggable);
        if (draggable)
          break;
      }
      else {
        nsCOMPtr<nsIDOMXULElement> xulElement = do_QueryInterface(dragContent);
        if (xulElement) {
          // All XUL elements are draggable, so if a XUL element is
          // encountered, stop looking for draggable nodes and just use the
          // original clicked node instead.
          // XXXndeakin
          // In the future, we will want to improve this so that XUL has a
          // better way to specify whether something is draggable than just
          // on/off.
          dragContent = mGestureDownContent;
          break;
        }
        // otherwise, it's not an HTML or XUL element, so just keep looking
      }
      dragContent = dragContent->GetParent();
    }
  }

  // if no node in the hierarchy was found to drag, but the GetDragData method
  // returned a node, use that returned node. Otherwise, nothing is draggable.
  if (!dragContent && dragDataNode)
    dragContent = dragDataNode;

  if (dragContent) {
    // if an ancestor node was used instead, clear the drag data
    // XXXndeakin rework this a bit. Find a way to just not call GetDragData if we don't need to.
    if (dragContent != originalDragContent)
      aDataTransfer->ClearAll();
    *aTargetNode = dragContent;
    NS_ADDREF(*aTargetNode);
  }
}

bool
nsEventStateManager::DoDefaultDragStart(nsPresContext* aPresContext,
                                        nsDragEvent* aDragEvent,
                                        nsDOMDataTransfer* aDataTransfer,
                                        nsIContent* aDragTarget,
                                        nsISelection* aSelection)
{
  nsCOMPtr<nsIDragService> dragService =
    do_GetService("@mozilla.org/widget/dragservice;1");
  if (!dragService)
    return false;

  // Default handling for the draggesture/dragstart event.
  //
  // First, check if a drag session already exists. This means that the drag
  // service was called directly within a draggesture handler. In this case,
  // don't do anything more, as it is assumed that the handler is managing
  // drag and drop manually. Make sure to return true to indicate that a drag
  // began.
  nsCOMPtr<nsIDragSession> dragSession;
  dragService->GetCurrentSession(getter_AddRefs(dragSession));
  if (dragSession)
    return true;

  // No drag session is currently active, so check if a handler added
  // any items to be dragged. If not, there isn't anything to drag.
  PRUint32 count = 0;
  if (aDataTransfer)
    aDataTransfer->GetMozItemCount(&count);
  if (!count)
    return false;

  // Get the target being dragged, which may not be the same as the
  // target of the mouse event. If one wasn't set in the
  // aDataTransfer during the event handler, just use the original
  // target instead.
  nsCOMPtr<nsIDOMNode> dragTarget;
  nsCOMPtr<nsIDOMElement> dragTargetElement;
  aDataTransfer->GetDragTarget(getter_AddRefs(dragTargetElement));
  dragTarget = do_QueryInterface(dragTargetElement);
  if (!dragTarget) {
    dragTarget = do_QueryInterface(aDragTarget);
    if (!dragTarget)
      return false;
  }

  // check which drag effect should initially be used. If the effect was not
  // set, just use all actions, otherwise Windows won't allow a drop.
  PRUint32 action;
  aDataTransfer->GetEffectAllowedInt(&action);
  if (action == nsIDragService::DRAGDROP_ACTION_UNINITIALIZED)
    action = nsIDragService::DRAGDROP_ACTION_COPY |
             nsIDragService::DRAGDROP_ACTION_MOVE |
             nsIDragService::DRAGDROP_ACTION_LINK;

  // get any custom drag image that was set
  PRInt32 imageX, imageY;
  nsIDOMElement* dragImage = aDataTransfer->GetDragImage(&imageX, &imageY);

  nsCOMPtr<nsISupportsArray> transArray;
  aDataTransfer->GetTransferables(getter_AddRefs(transArray), dragTarget);
  if (!transArray)
    return false;

  // XXXndeakin don't really want to create a new drag DOM event
  // here, but we need something to pass to the InvokeDragSession
  // methods.
  nsCOMPtr<nsIDOMEvent> domEvent;
  NS_NewDOMDragEvent(getter_AddRefs(domEvent), aPresContext, aDragEvent);

  nsCOMPtr<nsIDOMDragEvent> domDragEvent = do_QueryInterface(domEvent);
  // if creating a drag event failed, starting a drag session will
  // just fail.

  // Use InvokeDragSessionWithSelection if a selection is being dragged,
  // such that the image can be generated from the selected text. However,
  // use InvokeDragSessionWithImage if a custom image was set or something
  // other than a selection is being dragged.
  if (!dragImage && aSelection) {
    dragService->InvokeDragSessionWithSelection(aSelection, transArray,
                                                action, domDragEvent,
                                                aDataTransfer);
  }
  else {
    // if dragging within a XUL tree and no custom drag image was
    // set, the region argument to InvokeDragSessionWithImage needs
    // to be set to the area encompassing the selected rows of the
    // tree to ensure that the drag feedback gets clipped to those
    // rows. For other content, region should be null.
    nsCOMPtr<nsIScriptableRegion> region;
#ifdef MOZ_XUL
    if (dragTarget && !dragImage) {
      nsCOMPtr<nsIContent> content = do_QueryInterface(dragTarget);
      if (content->NodeInfo()->Equals(nsGkAtoms::treechildren,
                                      kNameSpaceID_XUL)) {
        nsTreeBodyFrame* treeBody = do_QueryFrame(content->GetPrimaryFrame());
        if (treeBody) {
          treeBody->GetSelectionRegion(getter_AddRefs(region));
        }
      }
    }
#endif

    dragService->InvokeDragSessionWithImage(dragTarget, transArray,
                                            region, action, dragImage,
                                            imageX, imageY, domDragEvent,
                                            aDataTransfer);
  }

  return true;
}

nsresult
nsEventStateManager::GetMarkupDocumentViewer(nsIMarkupDocumentViewer** aMv)
{
  *aMv = nsnull;

  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if(!fm) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMWindow> focusedWindow;
  fm->GetFocusedWindow(getter_AddRefs(focusedWindow));

  nsCOMPtr<nsPIDOMWindow> ourWindow = do_QueryInterface(focusedWindow);
  if(!ourWindow) return NS_ERROR_FAILURE;

  nsIDOMWindow *rootWindow = ourWindow->GetPrivateRoot();
  if(!rootWindow) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMWindow> contentWindow;
  rootWindow->GetContent(getter_AddRefs(contentWindow));
  if(!contentWindow) return NS_ERROR_FAILURE;

  nsIDocument *doc = GetDocumentFromWindow(contentWindow);
  if(!doc) return NS_ERROR_FAILURE;

  nsIPresShell *presShell = doc->GetShell();
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

  *aMv = mv;
  NS_IF_ADDREF(*aMv);

  return NS_OK;
}

nsresult
nsEventStateManager::ChangeTextSize(PRInt32 change)
{
  nsCOMPtr<nsIMarkupDocumentViewer> mv;
  nsresult rv = GetMarkupDocumentViewer(getter_AddRefs(mv));
  NS_ENSURE_SUCCESS(rv, rv);

  float textzoom;
  float zoomMin = ((float)Preferences::GetInt("zoom.minPercent", 50)) / 100;
  float zoomMax = ((float)Preferences::GetInt("zoom.maxPercent", 300)) / 100;
  mv->GetTextZoom(&textzoom);
  textzoom += ((float)change) / 10;
  if (textzoom < zoomMin)
    textzoom = zoomMin;
  else if (textzoom > zoomMax)
    textzoom = zoomMax;
  mv->SetTextZoom(textzoom);

  return NS_OK;
}

nsresult
nsEventStateManager::ChangeFullZoom(PRInt32 change)
{
  nsCOMPtr<nsIMarkupDocumentViewer> mv;
  nsresult rv = GetMarkupDocumentViewer(getter_AddRefs(mv));
  NS_ENSURE_SUCCESS(rv, rv);

  float fullzoom;
  float zoomMin = ((float)Preferences::GetInt("zoom.minPercent", 50)) / 100;
  float zoomMax = ((float)Preferences::GetInt("zoom.maxPercent", 300)) / 100;
  mv->GetFullZoom(&fullzoom);
  fullzoom += ((float)change) / 10;
  if (fullzoom < zoomMin)
    fullzoom = zoomMin;
  else if (fullzoom > zoomMax)
    fullzoom = zoomMax;
  mv->SetFullZoom(fullzoom);

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
nsEventStateManager::DoScrollZoom(nsIFrame *aTargetFrame,
                                  PRInt32 adjustment)
{
  // Exclude form controls and XUL content.
  nsIContent *content = aTargetFrame->GetContent();
  if (content &&
      !content->IsNodeOfType(nsINode::eHTML_FORM_CONTROL) &&
      !content->IsXUL())
    {
      // positive adjustment to decrease zoom, negative to increase
      PRInt32 change = (adjustment > 0) ? -1 : 1;

      if (Preferences::GetBool("browser.zoom.full") || content->GetCurrentDoc()->IsSyntheticDocument()) {
        ChangeFullZoom(change);
      } else {
        ChangeTextSize(change);
      }
    }
}

static nsIFrame*
GetParentFrameToScroll(nsIFrame* aFrame)
{
  if (!aFrame)
    return nsnull;

  if (aFrame->GetStyleDisplay()->mPosition == NS_STYLE_POSITION_FIXED &&
      nsLayoutUtils::IsReallyFixedPos(aFrame))
    return aFrame->PresContext()->GetPresShell()->GetRootScrollFrame();

  return aFrame->GetParent();
}

static nscoord
GetScrollableLineHeight(nsIFrame* aTargetFrame)
{
  for (nsIFrame* f = aTargetFrame; f; f = GetParentFrameToScroll(f)) {
    nsIScrollableFrame* sf = f->GetScrollTargetFrame();
    if (sf)
      return sf->GetLineScrollAmount().height;
  }

  // Fall back to the font height of the target frame.
  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(aTargetFrame, getter_AddRefs(fm),
    nsLayoutUtils::FontSizeInflationFor(aTargetFrame));
  NS_ASSERTION(fm, "FontMetrics is null!");
  if (fm)
    return fm->MaxHeight();
  return 0;
}

void
nsEventStateManager::SendLineScrollEvent(nsIFrame* aTargetFrame,
                                         nsMouseScrollEvent* aEvent,
                                         nsPresContext* aPresContext,
                                         nsEventStatus* aStatus,
                                         PRInt32 aNumLines)
{
  nsCOMPtr<nsIContent> targetContent = aTargetFrame->GetContent();
  if (!targetContent)
    targetContent = GetFocusedContent();
  if (!targetContent)
    return;

  while (targetContent->IsNodeOfType(nsINode::eTEXT)) {
    targetContent = targetContent->GetParent();
  }

  bool isTrusted = (aEvent->flags & NS_EVENT_FLAG_TRUSTED) != 0;
  nsMouseScrollEvent event(isTrusted, NS_MOUSE_SCROLL, nsnull);
  event.refPoint = aEvent->refPoint;
  event.widget = aEvent->widget;
  event.time = aEvent->time;
  event.modifiers = aEvent->modifiers;
  event.buttons = aEvent->buttons;
  event.scrollFlags = aEvent->scrollFlags;
  event.delta = aNumLines;
  event.inputSource = static_cast<nsMouseEvent_base*>(aEvent)->inputSource;

  nsEventDispatcher::Dispatch(targetContent, aPresContext, &event, nsnull, aStatus);
}

void
nsEventStateManager::SendPixelScrollEvent(nsIFrame* aTargetFrame,
                                          nsMouseScrollEvent* aEvent,
                                          nsPresContext* aPresContext,
                                          nsEventStatus* aStatus)
{
  nsCOMPtr<nsIContent> targetContent = aTargetFrame->GetContent();
  if (!targetContent) {
    targetContent = GetFocusedContent();
    if (!targetContent)
      return;
  }

  while (targetContent->IsNodeOfType(nsINode::eTEXT)) {
    targetContent = targetContent->GetParent();
  }

  nscoord lineHeight = GetScrollableLineHeight(aTargetFrame);

  bool isTrusted = (aEvent->flags & NS_EVENT_FLAG_TRUSTED) != 0;
  nsMouseScrollEvent event(isTrusted, NS_MOUSE_PIXEL_SCROLL, nsnull);
  event.refPoint = aEvent->refPoint;
  event.widget = aEvent->widget;
  event.time = aEvent->time;
  event.modifiers = aEvent->modifiers;
  event.buttons = aEvent->buttons;
  event.scrollFlags = aEvent->scrollFlags;
  event.inputSource = static_cast<nsMouseEvent_base*>(aEvent)->inputSource;
  event.delta = aPresContext->AppUnitsToIntCSSPixels(aEvent->delta * lineHeight);

  nsEventDispatcher::Dispatch(targetContent, aPresContext, &event, nsnull, aStatus);
}

PRInt32
nsEventStateManager::ComputeWheelDeltaFor(nsMouseScrollEvent* aMouseEvent)
{
  PRInt32 delta = aMouseEvent->delta;
  bool useSysNumLines = UseSystemScrollSettingFor(aMouseEvent);
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

    PRInt32 numLines = GetScrollLinesFor(aMouseEvent);

    bool swapDirs = (numLines < 0);
    PRInt32 userSize = swapDirs ? -numLines : numLines;

    bool deltaUp = (delta < 0);
    if (swapDirs) {
      deltaUp = !deltaUp;
    }
    delta = deltaUp ? -userSize : userSize;
  }

  if (ComputeWheelActionFor(aMouseEvent, useSysNumLines) == MOUSE_SCROLL_PAGE) {
    delta = (delta > 0) ? PRInt32(nsIDOMUIEvent::SCROLL_PAGE_DOWN) :
                          PRInt32(nsIDOMUIEvent::SCROLL_PAGE_UP);
  }

  return delta;
}

PRInt32
nsEventStateManager::ComputeWheelActionFor(nsMouseScrollEvent* aMouseEvent,
                                           bool aUseSystemSettings)
{
  PRInt32 action = GetWheelActionFor(aMouseEvent);
  if (aUseSystemSettings &&
      (aMouseEvent->scrollFlags & nsMouseScrollEvent::kIsFullPage)) {
    action = MOUSE_SCROLL_PAGE;
  }

  if (aMouseEvent->message == NS_MOUSE_PIXEL_SCROLL) {
    if (action == MOUSE_SCROLL_N_LINES || action == MOUSE_SCROLL_PAGE ||
        (aMouseEvent->scrollFlags & nsMouseScrollEvent::kIsMomentum)) {
      action = MOUSE_SCROLL_PIXELS;
    } else {
      // Do not scroll pixels when zooming
      action = -1;
    }
  } else if (((aMouseEvent->scrollFlags & nsMouseScrollEvent::kHasPixels) &&
              (aUseSystemSettings ||
               action == MOUSE_SCROLL_N_LINES || action == MOUSE_SCROLL_PAGE)) ||
             ((aMouseEvent->scrollFlags & nsMouseScrollEvent::kIsMomentum) &&
              (action == MOUSE_SCROLL_HISTORY || action == MOUSE_SCROLL_ZOOM))) {
    // Don't scroll lines or page when a pixel scroll event will follow.
    // Also, don't do history scrolling or zooming for momentum scrolls,
    // no matter what's going on with pixel scrolling.
    action = -1;
  }

  return action;
}

PRInt32
nsEventStateManager::GetWheelActionFor(nsMouseScrollEvent* aMouseEvent)
{
  nsCAutoString prefName;
  GetBasePrefKeyForMouseWheel(aMouseEvent, prefName);
  prefName.Append(".action");
  return Preferences::GetInt(prefName.get());
}

PRInt32
nsEventStateManager::GetScrollLinesFor(nsMouseScrollEvent* aMouseEvent)
{
  NS_ASSERTION(!UseSystemScrollSettingFor(aMouseEvent),
    "GetScrollLinesFor() called when should use system settings");
  nsCAutoString prefName;
  GetBasePrefKeyForMouseWheel(aMouseEvent, prefName);
  prefName.Append(".numlines");
  return Preferences::GetInt(prefName.get());
}

bool
nsEventStateManager::UseSystemScrollSettingFor(nsMouseScrollEvent* aMouseEvent)
{
  nsCAutoString prefName;
  GetBasePrefKeyForMouseWheel(aMouseEvent, prefName);
  prefName.Append(".sysnumlines");
  return Preferences::GetBool(prefName.get());
}

nsresult
nsEventStateManager::DoScrollText(nsIFrame* aTargetFrame,
                                  nsMouseScrollEvent* aMouseEvent,
                                  nsIScrollableFrame::ScrollUnit aScrollQuantity,
                                  bool aAllowScrollSpeedOverride,
                                  nsQueryContentEvent* aQueryEvent,
                                  nsIAtom *aOrigin)
{
  nsIScrollableFrame* frameToScroll = nsnull;
  nsIFrame* scrollFrame = aTargetFrame;
  PRInt32 numLines = aMouseEvent->delta;
  bool isHorizontal = aMouseEvent->scrollFlags & nsMouseScrollEvent::kIsHorizontal;
  aMouseEvent->scrollOverflow = 0;

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
    frameToScroll = lastScrollFrame->GetScrollTargetFrame();
    if (frameToScroll) {
      nsMouseWheelTransaction::UpdateTransaction(numLines, isHorizontal);
      // When the scroll event will not scroll any views, UpdateTransaction
      // fired MozMouseScrollFailed event which is for automated testing.
      // In the event handler, the target frame might be destroyed.  Then,
      // we should not keep handling this scroll event.
      if (!nsMouseWheelTransaction::GetTargetFrame())
        return NS_OK;
    } else {
      nsMouseWheelTransaction::EndTransaction();
      lastScrollFrame = nsnull;
    }
  }
  bool passToParent = lastScrollFrame ? false : true;

  for (; scrollFrame && passToParent;
       scrollFrame = GetParentFrameToScroll(scrollFrame)) {
    // Check whether the frame wants to provide us with a scrollable view.
    frameToScroll = scrollFrame->GetScrollTargetFrame();
    if (!frameToScroll) {
      continue;
    }

    nsPresContext::ScrollbarStyles ss = frameToScroll->GetScrollbarStyles();
    if (NS_STYLE_OVERFLOW_HIDDEN ==
        (isHorizontal ? ss.mHorizontal : ss.mVertical)) {
      continue;
    }

    // Check if the scrollable view can be scrolled any further.
    nscoord lineHeight = frameToScroll->GetLineScrollAmount().height;
    if (lineHeight != 0) {
      if (CanScrollOn(frameToScroll, numLines, isHorizontal)) {
        passToParent = false;
        nsMouseWheelTransaction::BeginTransaction(scrollFrame,
                                                  numLines, isHorizontal);
      }

      // Comboboxes need special care.
      nsIComboboxControlFrame* comboBox = do_QueryFrame(scrollFrame);
      if (comboBox) {
        if (comboBox->IsDroppedDown()) {
          // Don't propagate to parent when drop down menu is active.
          if (passToParent) {
            passToParent = false;
            frameToScroll = nsnull;
            nsMouseWheelTransaction::EndTransaction();
          }
        } else {
          // Always propagate when not dropped down (even if focused).
          if (!passToParent) {
            passToParent = true;
            nsMouseWheelTransaction::EndTransaction();
          }
        }
      }
    }
  }

  if (!passToParent && frameToScroll) {
    if (aScrollQuantity == nsIScrollableFrame::LINES) {
      // When this is called for querying the scroll target information,
      // we shouldn't limit the scrolling amount to less one page.
      // Otherwise, we shouldn't scroll more one page at once.
      numLines =
        nsMouseWheelTransaction::AccelerateWheelDelta(numLines, isHorizontal,
                                                      aAllowScrollSpeedOverride,
                                                      &aScrollQuantity,
                                                      !aQueryEvent);
    }
#ifdef DEBUG
    else {
      NS_ASSERTION(!aAllowScrollSpeedOverride,
        "aAllowScrollSpeedOverride is true but the quantity isn't by-line scrolling.");
    }
#endif

    if (aScrollQuantity == nsIScrollableFrame::PAGES) {
      numLines = (numLines > 0) ? 1 : -1;
    }

    if (aQueryEvent) {
      // If acceleration is enabled, pixel scroll shouldn't be used for
      // high resolution scrolling.
      if (nsMouseWheelTransaction::IsAccelerationEnabled()) {
        return NS_OK;
      }

      nscoord appUnitsPerDevPixel =
        aTargetFrame->PresContext()->AppUnitsPerDevPixel();
      aQueryEvent->mReply.mLineHeight =
        frameToScroll->GetLineScrollAmount().height / appUnitsPerDevPixel;
      aQueryEvent->mReply.mPageHeight =
        frameToScroll->GetPageScrollAmount().height / appUnitsPerDevPixel;
      aQueryEvent->mReply.mPageWidth =
        frameToScroll->GetPageScrollAmount().width / appUnitsPerDevPixel;

      // Returns computed numLines to widget which is needed to compute the
      // pixel scrolling amout for high resolution scrolling.
      aQueryEvent->mReply.mComputedScrollAmount = numLines;

      switch (aScrollQuantity) {
        case nsIScrollableFrame::LINES:
          aQueryEvent->mReply.mComputedScrollAction =
            nsQueryContentEvent::SCROLL_ACTION_LINE;
          break;
        case nsIScrollableFrame::PAGES:
          aQueryEvent->mReply.mComputedScrollAction =
            nsQueryContentEvent::SCROLL_ACTION_PAGE;
          break;
        default:
          aQueryEvent->mReply.mComputedScrollAction =
            nsQueryContentEvent::SCROLL_ACTION_NONE;
          break;
      }

      aQueryEvent->mSucceeded = true;
      return NS_OK;
    }

    PRInt32 scrollX = 0;
    PRInt32 scrollY = numLines;

    if (isHorizontal) {
      scrollX = scrollY;
      scrollY = 0;
    }

    nsIScrollableFrame::ScrollMode mode;
    if (aMouseEvent->scrollFlags & nsMouseScrollEvent::kNoDefer) {
      mode = nsIScrollableFrame::INSTANT;
    } else if (aScrollQuantity != nsIScrollableFrame::DEVICE_PIXELS ||
               (aMouseEvent->scrollFlags &
                  nsMouseScrollEvent::kAllowSmoothScroll) != 0) {
      mode = nsIScrollableFrame::SMOOTH;
    } else {
      mode = nsIScrollableFrame::NORMAL;
    }

    // XXX Why don't we limit the pixel scroll amount to less one page??

    nsIntPoint overflow;
    frameToScroll->ScrollBy(nsIntPoint(scrollX, scrollY), aScrollQuantity,
                            mode, &overflow, aOrigin);
    aMouseEvent->scrollOverflow = isHorizontal ? overflow.x : overflow.y;
    return NS_OK;
  }
  
  if (passToParent) {
    nsIFrame* newFrame = nsLayoutUtils::GetCrossDocParentFrame(
        aTargetFrame->PresContext()->FrameManager()->GetRootFrame());
    if (newFrame)
      return DoScrollText(newFrame, aMouseEvent, aScrollQuantity,
                          aAllowScrollSpeedOverride, aQueryEvent, aOrigin);
  }

  aMouseEvent->scrollOverflow = numLines;

  return NS_OK;
}

void
nsEventStateManager::DecideGestureEvent(nsGestureNotifyEvent* aEvent,
                                        nsIFrame* targetFrame)
{

  NS_ASSERTION(aEvent->message == NS_GESTURENOTIFY_EVENT_START,
               "DecideGestureEvent called with a non-gesture event");

  /* Check the ancestor tree to decide if any frame is willing* to receive
   * a MozPixelScroll event. If that's the case, the current touch gesture
   * will be used as a pan gesture; otherwise it will be a regular
   * mousedown/mousemove/click event.
   *
   * *willing: determine if it makes sense to pan the element using scroll events:
   *  - For web content: if there are any visible scrollbars on the touch point
   *  - For XUL: if it's an scrollable element that can currently scroll in some
    *    direction.
   *
   * Note: we'll have to one-off various cases to ensure a good usable behavior
   */
  nsGestureNotifyEvent::ePanDirection panDirection = nsGestureNotifyEvent::ePanNone;
  bool displayPanFeedback = false;
  for (nsIFrame* current = targetFrame; current;
       current = nsLayoutUtils::GetCrossDocParentFrame(current)) {

    nsIAtom* currentFrameType = current->GetType();

    // Scrollbars should always be draggable
    if (currentFrameType == nsGkAtoms::scrollbarFrame) {
      panDirection = nsGestureNotifyEvent::ePanNone;
      break;
    }

#ifdef MOZ_XUL
    // Special check for trees
    nsTreeBodyFrame* treeFrame = do_QueryFrame(current);
    if (treeFrame) {
      if (treeFrame->GetHorizontalOverflow()) {
        panDirection = nsGestureNotifyEvent::ePanHorizontal;
      }
      if (treeFrame->GetVerticalOverflow()) {
        panDirection = nsGestureNotifyEvent::ePanVertical;
      }
      break;
    }
#endif

    nsIScrollableFrame* scrollableFrame = do_QueryFrame(current);
    if (scrollableFrame) {
      if (current->IsFrameOfType(nsIFrame::eXULBox)) {
        displayPanFeedback = true;

        nsRect scrollRange = scrollableFrame->GetScrollRange();
        bool canScrollHorizontally = scrollRange.width > 0;

        if (targetFrame->GetType() == nsGkAtoms::menuFrame) {
          // menu frames report horizontal scroll when they have submenus
          // and we don't want that
          canScrollHorizontally = false;
          displayPanFeedback = false;
        }

        // Vertical panning has priority over horizontal panning, so
        // when vertical movement is possible we can just finish the loop.
        if (scrollRange.height > 0) {
          panDirection = nsGestureNotifyEvent::ePanVertical;
          break;
        }

        if (canScrollHorizontally) {
          panDirection = nsGestureNotifyEvent::ePanHorizontal;
          displayPanFeedback = false;
        }
      } else { //Not a XUL box
        PRUint32 scrollbarVisibility = scrollableFrame->GetScrollbarVisibility();

        //Check if we have visible scrollbars
        if (scrollbarVisibility & nsIScrollableFrame::VERTICAL) {
          panDirection = nsGestureNotifyEvent::ePanVertical;
          displayPanFeedback = true;
          break;
        }

        if (scrollbarVisibility & nsIScrollableFrame::HORIZONTAL) {
          panDirection = nsGestureNotifyEvent::ePanHorizontal;
          displayPanFeedback = true;
        }
      }
    } //scrollableFrame
  } //ancestor chain

  aEvent->displayPanFeedback = displayPanFeedback;
  aEvent->panDirection = panDirection;
}

#ifdef XP_MACOSX
static bool
NodeAllowsClickThrough(nsINode* aNode)
{
  while (aNode) {
    if (aNode->IsElement() && aNode->AsElement()->IsXUL()) {
      mozilla::dom::Element* element = aNode->AsElement();
      static nsIContent::AttrValuesArray strings[] =
        {&nsGkAtoms::always, &nsGkAtoms::never, nsnull};
      switch (element->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::clickthrough,
                                       strings, eCaseMatters)) {
        case 0:
          return true;
        case 1:
          return false;
      }
    }
    aNode = nsContentUtils::GetCrossDocParentNode(aNode);
  }
  return true;
}
#endif

nsresult
nsEventStateManager::PostHandleEvent(nsPresContext* aPresContext,
                                     nsEvent *aEvent,
                                     nsIFrame* aTargetFrame,
                                     nsEventStatus* aStatus)
{
  NS_ENSURE_ARG(aPresContext);
  NS_ENSURE_ARG_POINTER(aStatus);

  HandleCrossProcessEvent(aEvent, aTargetFrame, aStatus);

  mCurrentTarget = aTargetFrame;
  mCurrentTargetContent = nsnull;

  // Most of the events we handle below require a frame.
  // Add special cases here.
  if (!mCurrentTarget && aEvent->message != NS_MOUSE_BUTTON_UP &&
      aEvent->message != NS_MOUSE_BUTTON_DOWN) {
    return NS_OK;
  }

  //Keep the prescontext alive, we might need it after event dispatch
  nsRefPtr<nsPresContext> presContext = aPresContext;
  nsresult ret = NS_OK;

  switch (aEvent->message) {
  case NS_MOUSE_BUTTON_DOWN:
    {
      if (static_cast<nsMouseEvent*>(aEvent)->button == nsMouseEvent::eLeftButton &&
          !sNormalLMouseEventInProcess) {
        // We got a mouseup event while a mousedown event was being processed.
        // Make sure that the capturing content is cleared.
        nsIPresShell::SetCapturingContent(nsnull, 0);
        break;
      }

      nsCOMPtr<nsIContent> activeContent;
      if (nsEventStatus_eConsumeNoDefault != *aStatus) {
        nsCOMPtr<nsIContent> newFocus;      
        bool suppressBlur = false;
        if (mCurrentTarget) {
          mCurrentTarget->GetContentForEvent(aEvent, getter_AddRefs(newFocus));
          const nsStyleUserInterface* ui = mCurrentTarget->GetStyleUserInterface();
          suppressBlur = (ui->mUserFocus == NS_STYLE_USER_FOCUS_IGNORE);
          activeContent = mCurrentTarget->GetContent();
        }

        nsIFrame* currFrame = mCurrentTarget;

        // When a root content which isn't editable but has an editable HTML
        // <body> element is clicked, we should redirect the focus to the
        // the <body> element.  E.g., when an user click bottom of the editor
        // where is outside of the <body> element, the <body> should be focused
        // and the user can edit immediately after that.
        //
        // NOTE: The newFocus isn't editable that also means it's not in
        // designMode.  In designMode, all contents are not focusable.
        if (newFocus && !newFocus->IsEditable()) {
          nsIDocument *doc = newFocus->GetCurrentDoc();
          if (doc && newFocus == doc->GetRootElement()) {
            nsIContent *bodyContent =
              nsLayoutUtils::GetEditableRootContentByContentEditable(doc);
            if (bodyContent) {
              nsIFrame* bodyFrame = bodyContent->GetPrimaryFrame();
              if (bodyFrame) {
                currFrame = bodyFrame;
                newFocus = bodyContent;
              }
            }
          }
        }

        // When the mouse is pressed, the default action is to focus the
        // target. Look for the nearest enclosing focusable frame.
        while (currFrame) {
          // If the mousedown happened inside a popup, don't
          // try to set focus on one of its containing elements
          const nsStyleDisplay* display = currFrame->GetStyleDisplay();
          if (display->mDisplay == NS_STYLE_DISPLAY_POPUP) {
            newFocus = nsnull;
            break;
          }

          PRInt32 tabIndexUnused;
          if (currFrame->IsFocusable(&tabIndexUnused, true)) {
            newFocus = currFrame->GetContent();
            nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(newFocus));
            if (domElement)
              break;
          }
          currFrame = currFrame->GetParent();
        }

        nsIFocusManager* fm = nsFocusManager::GetFocusManager();
        if (fm) {
          // if something was found to focus, focus it. Otherwise, if the
          // element that was clicked doesn't have -moz-user-focus: ignore,
          // clear the existing focus. For -moz-user-focus: ignore, the focus
          // is just left as is.
          // Another effect of mouse clicking, handled in nsSelection, is that
          // it should update the caret position to where the mouse was
          // clicked. Because the focus is cleared when clicking on a
          // non-focusable node, the next press of the tab key will cause
          // focus to be shifted from the caret position instead of the root.
          if (newFocus && currFrame) {
            // use the mouse flag and the noscroll flag so that the content
            // doesn't unexpectedly scroll when clicking an element that is
            // only hald visible
            nsCOMPtr<nsIDOMElement> newFocusElement = do_QueryInterface(newFocus);
            fm->SetFocus(newFocusElement, nsIFocusManager::FLAG_BYMOUSE |
                                          nsIFocusManager::FLAG_NOSCROLL);
          }
          else if (!suppressBlur) {
            // clear the focus within the frame and then set it as the
            // focused frame
            EnsureDocument(mPresContext);
            if (mDocument) {
#ifdef XP_MACOSX
              if (!activeContent || !activeContent->IsXUL())
#endif
                fm->ClearFocus(mDocument->GetWindow());
              fm->SetFocusedWindow(mDocument->GetWindow());
            }
          }
        }

        // The rest is left button-specific.
        if (static_cast<nsMouseEvent*>(aEvent)->button !=
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
        }
      }
      else {
        // if we're here, the event handler returned false, so stop
        // any of our own processing of a drag. Workaround for bug 43258.
        StopTrackingDragGesture();

        // When the event was cancelled, there is currently a chrome document
        // focused and a mousedown just occurred on a content document, ensure
        // that the window that was clicked is focused.
        EnsureDocument(mPresContext);
        nsIFocusManager* fm = nsFocusManager::GetFocusManager();
        if (mDocument && fm) {
          nsCOMPtr<nsIDOMWindow> currentWindow;
          fm->GetFocusedWindow(getter_AddRefs(currentWindow));
          if (currentWindow && currentWindow != mDocument->GetWindow() &&
              !nsContentUtils::IsChromeDoc(mDocument)) {
            nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(currentWindow);
            nsCOMPtr<nsIDocument> currentDoc = do_QueryInterface(win->GetExtantDocument());
            if (nsContentUtils::IsChromeDoc(currentDoc)) {
              fm->SetFocusedWindow(mDocument->GetWindow());
            }
          }
        }
      }
      SetActiveManager(this, activeContent);
    }
    break;
  case NS_MOUSE_BUTTON_UP:
    {
      ClearGlobalActiveContent(this);
      if (IsMouseEventReal(aEvent)) {
        if (!mCurrentTarget) {
          GetEventTarget();
        }
        // Make sure to dispatch the click even if there is no frame for
        // the current target element. This is required for Web compatibility.
        ret = CheckForAndDispatchClick(presContext, (nsMouseEvent*)aEvent,
                                       aStatus);
      }

      nsIPresShell *shell = presContext->GetPresShell();
      if (shell) {
        nsRefPtr<nsFrameSelection> frameSelection = shell->FrameSelection();
        frameSelection->SetMouseDownState(false);
      }
    }
    break;
  case NS_MOUSE_SCROLL:
  case NS_MOUSE_PIXEL_SCROLL:
    {
      nsMouseScrollEvent *msEvent = static_cast<nsMouseScrollEvent*>(aEvent);

      if (aEvent->message == NS_MOUSE_SCROLL) {
        // Mark the subsequent pixel scrolls as valid / invalid, based on the
        // observation if the previous line scroll has been canceled
        if (msEvent->scrollFlags & nsMouseScrollEvent::kIsHorizontal) {
          mLastLineScrollConsumedX = (nsEventStatus_eConsumeNoDefault == *aStatus);
        } else if (msEvent->scrollFlags & nsMouseScrollEvent::kIsVertical) {
          mLastLineScrollConsumedY = (nsEventStatus_eConsumeNoDefault == *aStatus);
        }
        if (!(msEvent->scrollFlags & nsMouseScrollEvent::kHasPixels)) {
          // No generated pixel scroll event will follow.
          // Create and send a pixel scroll DOM event now.
          nsWeakFrame weakFrame(aTargetFrame);
          SendPixelScrollEvent(aTargetFrame, msEvent, presContext, aStatus);
          NS_ENSURE_STATE(weakFrame.IsAlive());
        }
      }

      if (*aStatus != nsEventStatus_eConsumeNoDefault) {
        bool useSysNumLines = UseSystemScrollSettingFor(msEvent);
        PRInt32 action = ComputeWheelActionFor(msEvent, useSysNumLines);

        switch (action) {
        case MOUSE_SCROLL_N_LINES:
          DoScrollText(aTargetFrame, msEvent, nsIScrollableFrame::LINES,
                       useSysNumLines, nsnull, nsGkAtoms::mouseWheel);
          break;

        case MOUSE_SCROLL_PAGE:
          DoScrollText(aTargetFrame, msEvent, nsIScrollableFrame::PAGES,
                       false);
          break;

        case MOUSE_SCROLL_PIXELS:
          {
            bool fromLines = msEvent->scrollFlags & nsMouseScrollEvent::kFromLines;
            DoScrollText(aTargetFrame, msEvent, nsIScrollableFrame::DEVICE_PIXELS,
                         false, nsnull, (fromLines ? nsGkAtoms::mouseWheel : nsnull));
          }
          break;

        case MOUSE_SCROLL_HISTORY:
          DoScrollHistory(msEvent->delta);
          break;

        case MOUSE_SCROLL_ZOOM:
          DoScrollZoom(aTargetFrame, msEvent->delta);
          break;

        default:  // Including -1 (do nothing)
          break;
        }
        *aStatus = nsEventStatus_eConsumeNoDefault;
      }
    }
    break;

  case NS_GESTURENOTIFY_EVENT_START:
    {
      if (nsEventStatus_eConsumeNoDefault != *aStatus)
        DecideGestureEvent(static_cast<nsGestureNotifyEvent*>(aEvent), mCurrentTarget);
    }
    break;

  case NS_DRAGDROP_ENTER:
  case NS_DRAGDROP_OVER:
    {
      NS_ASSERTION(aEvent->eventStructType == NS_DRAG_EVENT, "Expected a drag event");

      nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
      if (!dragSession)
        break;

      // Reset the flag.
      dragSession->SetOnlyChromeDrop(false);
      if (mPresContext) {
        EnsureDocument(mPresContext);
      }
      bool isChromeDoc = nsContentUtils::IsChromeDoc(mDocument);

      // the initial dataTransfer is the one from the dragstart event that
      // was set on the dragSession when the drag began.
      nsCOMPtr<nsIDOMDataTransfer> dataTransfer;
      nsCOMPtr<nsIDOMDataTransfer> initialDataTransfer;
      dragSession->GetDataTransfer(getter_AddRefs(initialDataTransfer));

      nsDragEvent *dragEvent = (nsDragEvent*)aEvent;

      // collect any changes to moz cursor settings stored in the event's
      // data transfer.
      UpdateDragDataTransfer(dragEvent);

      // cancelling a dragenter or dragover event means that a drop should be
      // allowed, so update the dropEffect and the canDrop state to indicate
      // that a drag is allowed. If the event isn't cancelled, a drop won't be
      // allowed. Essentially, to allow a drop somewhere, specify the effects
      // using the effectAllowed and dropEffect properties in a dragenter or
      // dragover event and cancel the event. To not allow a drop somewhere,
      // don't cancel the event or set the effectAllowed or dropEffect to
      // "none". This way, if the event is just ignored, no drop will be
      // allowed.
      PRUint32 dropEffect = nsIDragService::DRAGDROP_ACTION_NONE;
      if (nsEventStatus_eConsumeNoDefault == *aStatus) {
        // if the event has a dataTransfer set, use it.
        if (dragEvent->dataTransfer) {
          // get the dataTransfer and the dropEffect that was set on it
          dataTransfer = do_QueryInterface(dragEvent->dataTransfer);
          dataTransfer->GetDropEffectInt(&dropEffect);
        }
        else {
          // if dragEvent->dataTransfer is null, it means that no attempt was
          // made to access the dataTransfer during the event, yet the event
          // was cancelled. Instead, use the initial data transfer available
          // from the drag session. The drop effect would not have been
          // initialized (which is done in nsDOMDragEvent::GetDataTransfer),
          // so set it from the drag action. We'll still want to filter it
          // based on the effectAllowed below.
          dataTransfer = initialDataTransfer;

          PRUint32 action;
          dragSession->GetDragAction(&action);

          // filter the drop effect based on the action. Use UNINITIALIZED as
          // any effect is allowed.
          dropEffect = nsContentUtils::FilterDropEffect(action,
                         nsIDragService::DRAGDROP_ACTION_UNINITIALIZED);
        }

        // At this point, if the dataTransfer is null, it means that the
        // drag was originally started by directly calling the drag service.
        // Just assume that all effects are allowed.
        PRUint32 effectAllowed = nsIDragService::DRAGDROP_ACTION_UNINITIALIZED;
        if (dataTransfer)
          dataTransfer->GetEffectAllowedInt(&effectAllowed);

        // set the drag action based on the drop effect and effect allowed.
        // The drop effect field on the drag transfer object specifies the
        // desired current drop effect. However, it cannot be used if the
        // effectAllowed state doesn't include that type of action. If the
        // dropEffect is "none", then the action will be 'none' so a drop will
        // not be allowed.
        PRUint32 action = nsIDragService::DRAGDROP_ACTION_NONE;
        if (effectAllowed == nsIDragService::DRAGDROP_ACTION_UNINITIALIZED ||
            dropEffect & effectAllowed)
          action = dropEffect;

        if (action == nsIDragService::DRAGDROP_ACTION_NONE)
          dropEffect = nsIDragService::DRAGDROP_ACTION_NONE;

        // inform the drag session that a drop is allowed on this node.
        dragSession->SetDragAction(action);
        dragSession->SetCanDrop(action != nsIDragService::DRAGDROP_ACTION_NONE);

        // For now, do this only for dragover.
        //XXXsmaug dragenter needs some more work.
        if (aEvent->message == NS_DRAGDROP_OVER && !isChromeDoc) {
          // Someone has called preventDefault(), check whether is was content.
          dragSession->SetOnlyChromeDrop(
            !(aEvent->flags & NS_EVENT_FLAG_NO_DEFAULT_CALLED_IN_CONTENT));
        }
      } else if (aEvent->message == NS_DRAGDROP_OVER && !isChromeDoc) {
        // No one called preventDefault(), so handle drop only in chrome.
        dragSession->SetOnlyChromeDrop(true);
      }

      // now set the drop effect in the initial dataTransfer. This ensures
      // that we can get the desired drop effect in the drop event.
      if (initialDataTransfer)
        initialDataTransfer->SetDropEffectInt(dropEffect);
    }
    break;

  case NS_DRAGDROP_DROP:
    {
      // now fire the dragdrop event, for compatibility with XUL
      if (mCurrentTarget && nsEventStatus_eConsumeNoDefault != *aStatus) {
        nsCOMPtr<nsIContent> targetContent;
        mCurrentTarget->GetContentForEvent(aEvent,
                                           getter_AddRefs(targetContent));

        nsCOMPtr<nsIWidget> widget = mCurrentTarget->GetNearestWidget();
        nsDragEvent event(NS_IS_TRUSTED_EVENT(aEvent), NS_DRAGDROP_DRAGDROP, widget);

        nsMouseEvent* mouseEvent = static_cast<nsMouseEvent*>(aEvent);
        event.refPoint = mouseEvent->refPoint;
        if (mouseEvent->widget) {
          event.refPoint += mouseEvent->widget->WidgetToScreenOffset();
        }
        event.refPoint -= widget->WidgetToScreenOffset();
        event.modifiers = mouseEvent->modifiers;
        event.buttons = mouseEvent->buttons;
        event.inputSource = mouseEvent->inputSource;

        nsEventStatus status = nsEventStatus_eIgnore;
        nsCOMPtr<nsIPresShell> presShell = mPresContext->GetPresShell();
        if (presShell) {
          presShell->HandleEventWithTarget(&event, mCurrentTarget,
                                           targetContent, &status);
        }
      }
      sLastDragOverFrame = nsnull;
      ClearGlobalActiveContent(this);
      break;
    }
  case NS_DRAGDROP_EXIT:
     // make sure to fire the enter and exit_synth events after the
     // NS_DRAGDROP_EXIT event, otherwise we'll clean up too early
    GenerateDragDropEnterExit(presContext, (nsGUIEvent*)aEvent);
    break;

  case NS_KEY_UP:
    break;

  case NS_KEY_PRESS:
    if (nsEventStatus_eConsumeNoDefault != *aStatus) {
      nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
      //This is to prevent keyboard scrolling while alt modifier in use.
      if (!keyEvent->IsAlt()) {
        switch(keyEvent->keyCode) {
          case NS_VK_TAB:
          case NS_VK_F6:
            EnsureDocument(mPresContext);
            nsIFocusManager* fm = nsFocusManager::GetFocusManager();
            if (fm && mDocument) {
              // Shift focus forward or back depending on shift key
              bool isDocMove = ((nsInputEvent*)aEvent)->IsControl() ||
                                 (keyEvent->keyCode == NS_VK_F6);
              PRUint32 dir =
                static_cast<nsInputEvent*>(aEvent)->IsShift() ?
                  (isDocMove ? static_cast<PRUint32>(nsIFocusManager::MOVEFOCUS_BACKWARDDOC) :
                               static_cast<PRUint32>(nsIFocusManager::MOVEFOCUS_BACKWARD)) :
                  (isDocMove ? static_cast<PRUint32>(nsIFocusManager::MOVEFOCUS_FORWARDDOC) :
                               static_cast<PRUint32>(nsIFocusManager::MOVEFOCUS_FORWARD));
              nsCOMPtr<nsIDOMElement> result;
              fm->MoveFocus(mDocument->GetWindow(), nsnull, dir,
                            nsIFocusManager::FLAG_BYKEY,
                            getter_AddRefs(result));
            }
            *aStatus = nsEventStatus_eConsumeNoDefault;
            break;
        }
      }
    }
    break;

  case NS_MOUSE_ENTER:
    if (mCurrentTarget) {
      nsCOMPtr<nsIContent> targetContent;
      mCurrentTarget->GetContentForEvent(aEvent, getter_AddRefs(targetContent));
      SetContentState(targetContent, NS_EVENT_STATE_HOVER);
    }
    break;

#ifdef XP_MACOSX
  case NS_MOUSE_ACTIVATE:
    if (mCurrentTarget) {
      nsCOMPtr<nsIContent> targetContent;
      mCurrentTarget->GetContentForEvent(aEvent, getter_AddRefs(targetContent));
      if (!NodeAllowsClickThrough(targetContent)) {
        *aStatus = nsEventStatus_eConsumeNoDefault;
      }
    }
    break;
#endif
  }

  //Reset target frame to null to avoid mistargeting after reentrant event
  mCurrentTarget = nsnull;
  mCurrentTargetContent = nsnull;

  return ret;
}

bool
nsEventStateManager::RemoteQueryContentEvent(nsEvent *aEvent)
{
  nsQueryContentEvent *queryEvent =
      static_cast<nsQueryContentEvent*>(aEvent);
  if (!IsTargetCrossProcess(queryEvent)) {
    return false;
  }
  // Will not be handled locally, remote the event
  GetCrossProcessTarget()->HandleQueryContentEvent(*queryEvent);
  return true;
}

TabParent*
nsEventStateManager::GetCrossProcessTarget()
{
  return TabParent::GetIMETabParent();
}

bool
nsEventStateManager::IsTargetCrossProcess(nsGUIEvent *aEvent)
{
  // Check to see if there is a focused, editable content in chrome,
  // in that case, do not forward IME events to content
  nsIContent *focusedContent = GetFocusedContent();
  if (focusedContent && focusedContent->IsEditable())
    return false;
  return TabParent::GetIMETabParent() != nsnull;
}

void
nsEventStateManager::NotifyDestroyPresContext(nsPresContext* aPresContext)
{
  nsIMEStateManager::OnDestroyPresContext(aPresContext);
  if (mHoverContent) {
    // Bug 70855: Presentation is going away, possibly for a reframe.
    // Reset the hover state so that if we're recreating the presentation,
    // we won't have the old hover state still set in the new presentation,
    // as if the new presentation is resized, a new element may be hovered. 
    SetContentState(nsnull, NS_EVENT_STATE_HOVER);
  }
}

void
nsEventStateManager::SetPresContext(nsPresContext* aPresContext)
{
  mPresContext = aPresContext;
}

void
nsEventStateManager::ClearFrameRefs(nsIFrame* aFrame)
{
  if (aFrame && aFrame == mCurrentTarget) {
    mCurrentTargetContent = aFrame->GetContent();
  }
}

void
nsEventStateManager::UpdateCursor(nsPresContext* aPresContext,
                                  nsEvent* aEvent, nsIFrame* aTargetFrame,
                                  nsEventStatus* aStatus)
{
  if (aTargetFrame && IsRemoteTarget(aTargetFrame->GetContent())) {
    return;
  }

  PRInt32 cursor = NS_STYLE_CURSOR_DEFAULT;
  imgIContainer* container = nsnull;
  bool haveHotspot = false;
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

  if (Preferences::GetBool("ui.use_activity_cursor", false)) {
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
  }

  if (aTargetFrame) {
    SetCursor(cursor, container, haveHotspot, hotspotX, hotspotY,
              aTargetFrame->GetNearestWidget(), false);
  }

  if (mLockCursor || NS_STYLE_CURSOR_AUTO != cursor) {
    *aStatus = nsEventStatus_eConsumeDoDefault;
  }
}

nsresult
nsEventStateManager::SetCursor(PRInt32 aCursor, imgIContainer* aContainer,
                               bool aHaveHotspot,
                               float aHotspotX, float aHotspotY,
                               nsIWidget* aWidget, bool aLockCursor)
{
  EnsureDocument(mPresContext);
  NS_ENSURE_TRUE(mDocument, NS_ERROR_FAILURE);
  sMouseOverDocument = mDocument.get();

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
  case NS_STYLE_CURSOR_NONE:
    c = eCursor_none;
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

      // XXX NS_MAX(NS_lround(x), 0)?
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

class NS_STACK_CLASS nsESMEventCB : public nsDispatchingCallback
{
public:
  nsESMEventCB(nsIContent* aTarget) : mTarget(aTarget) {}

  virtual void HandleEvent(nsEventChainPostVisitor& aVisitor)
  {
    if (aVisitor.mPresContext) {
      nsIFrame* frame = aVisitor.mPresContext->GetPrimaryFrameFor(mTarget);
      if (frame) {
        frame->HandleEvent(aVisitor.mPresContext,
                           (nsGUIEvent*) aVisitor.mEvent,
                           &aVisitor.mEventStatus);
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
  // http://dvcs.w3.org/hg/webevents/raw-file/default/mouse-lock.html#methods
  // "[When the mouse is locked on an element...e]vents that require the concept
  // of a mouse cursor must not be dispatched (for example: mouseover, mouseout).
  if (sIsPointerLocked &&
      (aMessage == NS_MOUSELEAVE ||
       aMessage == NS_MOUSEENTER ||
       aMessage == NS_MOUSE_ENTER_SYNTH ||
       aMessage == NS_MOUSE_EXIT_SYNTH)) {
    mCurrentTargetContent = nsnull;
    nsCOMPtr<Element> pointerLockedElement =
      do_QueryReferent(nsEventStateManager::sPointerLockedElement);
    if (!pointerLockedElement) {
      NS_WARNING("Should have pointer locked element, but didn't.");
      return nsnull;
    }
    nsCOMPtr<nsIContent> content = do_QueryInterface(pointerLockedElement);
    return mPresContext->GetPrimaryFrameFor(content);
  }

  SAMPLE_LABEL("Input", "DispatchMouseEvent");
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event(NS_IS_TRUSTED_EVENT(aEvent), aMessage, aEvent->widget,
                     nsMouseEvent::eReal);
  event.refPoint = aEvent->refPoint;
  event.modifiers = ((nsMouseEvent*)aEvent)->modifiers;
  event.buttons = ((nsMouseEvent*)aEvent)->buttons;
  event.pluginEvent = ((nsMouseEvent*)aEvent)->pluginEvent;
  event.relatedTarget = aRelatedContent;
  event.inputSource = static_cast<nsMouseEvent*>(aEvent)->inputSource;

  nsWeakFrame previousTarget = mCurrentTarget;

  mCurrentTargetContent = aTargetContent;

  nsIFrame* targetFrame = nsnull;
  if (aTargetContent) {
    nsESMEventCB callback(aTargetContent);
    nsEventDispatcher::Dispatch(aTargetContent, mPresContext, &event, nsnull,
                                &status, &callback);

    // Although the primary frame was checked in event callback, 
    // it may not be the same object after event dispatching and handling.
    // So we need to refetch it.
    if (mPresContext) {
      targetFrame = mPresContext->GetPrimaryFrameFor(aTargetContent);
    }
  }

  mCurrentTargetContent = nsnull;
  mCurrentTarget = previousTarget;

  return targetFrame;
}

class MouseEnterLeaveDispatcher
{
public:
  MouseEnterLeaveDispatcher(nsEventStateManager* aESM,
                            nsIContent* aTarget, nsIContent* aRelatedTarget,
                            nsGUIEvent* aEvent, PRUint32 aType)
  : mESM(aESM), mEvent(aEvent), mType(aType)
  {
    nsPIDOMWindow* win =
      aTarget ? aTarget->OwnerDoc()->GetInnerWindow() : nsnull;
    if (win && win->HasMouseEnterLeaveEventListeners()) {
      mRelatedTarget = aRelatedTarget ?
        aRelatedTarget->FindFirstNonNativeAnonymous() : nsnull;
      nsINode* commonParent = nsnull;
      if (aTarget && aRelatedTarget) {
        commonParent =
          nsContentUtils::GetCommonAncestor(aTarget, aRelatedTarget);
      }
      nsIContent* current = aTarget;
      // Note, it is ok if commonParent is null!
      while (current && current != commonParent) {
        if (!current->IsInNativeAnonymousSubtree()) {
          mTargets.AppendObject(current);
        }
        // mouseenter/leave is fired only on elements.
        current = current->GetParent();
      }
    }
  }

  ~MouseEnterLeaveDispatcher()
  {
    if (mType == NS_MOUSEENTER) {
      for (PRInt32 i = mTargets.Count() - 1; i >= 0; --i) {
        mESM->DispatchMouseEvent(mEvent, mType, mTargets[i], mRelatedTarget);
      }
    } else {
      for (PRInt32 i = 0; i < mTargets.Count(); ++i) {
        mESM->DispatchMouseEvent(mEvent, mType, mTargets[i], mRelatedTarget);
      }
    }
  }

  nsEventStateManager*   mESM;
  nsCOMArray<nsIContent> mTargets;
  nsCOMPtr<nsIContent>   mRelatedTarget;
  nsGUIEvent*            mEvent;
  PRUint32               mType;
};

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
    nsSubDocumentFrame* subdocFrame = do_QueryFrame(mLastMouseOverFrame.GetFrame());
    if (subdocFrame) {
      nsCOMPtr<nsIDocShell> docshell;
      subdocFrame->GetDocShell(getter_AddRefs(docshell));
      if (docshell) {
        nsRefPtr<nsPresContext> presContext;
        docshell->GetPresContext(getter_AddRefs(presContext));

        if (presContext) {
          nsEventStateManager* kidESM = presContext->EventStateManager();
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

  MouseEnterLeaveDispatcher leaveDispatcher(this, mLastMouseOverElement, aMovingInto,
                                            aEvent, NS_MOUSELEAVE);

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
      nsIPresShell *parentShell = parentDoc->GetShell();
      if (parentShell) {
        nsEventStateManager* parentESM = parentShell->GetPresContext()->EventStateManager();
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

  MouseEnterLeaveDispatcher enterDispatcher(this, aContent, lastMouseOverElement,
                                            aEvent, NS_MOUSEENTER);
  
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

// Returns the center point of the window's inner content area.
// This is in widget coordinates, i.e. relative to the widget's top
// left corner, not in screen coordinates.
static nsIntPoint
GetWindowInnerRectCenter(nsPIDOMWindow* aWindow,
                         nsIWidget* aWidget,
                         nsPresContext* aContext)
{
  NS_ENSURE_TRUE(aWindow && aWidget && aContext, nsIntPoint(0,0));

  float cssInnerX = 0.0;
  aWindow->GetMozInnerScreenX(&cssInnerX);
  PRInt32 innerX = PRInt32(NS_round(aContext->CSSPixelsToDevPixels(cssInnerX)));

  float cssInnerY = 0.0;
  aWindow->GetMozInnerScreenY(&cssInnerY);
  PRInt32 innerY = PRInt32(NS_round(aContext->CSSPixelsToDevPixels(cssInnerY)));
 
  PRInt32 innerWidth = 0;
  aWindow->GetInnerWidth(&innerWidth);

  PRInt32 innerHeight = 0;
  aWindow->GetInnerHeight(&innerHeight);
 
  nsIntRect screen;
  aWidget->GetScreenBounds(screen);

  return nsIntPoint(innerX - screen.x + innerWidth / 2,
                    innerY - screen.y + innerHeight / 2);
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
      if (sIsPointerLocked && aEvent->widget) {
        // Perform mouse lock by recentering the mouse directly, and storing
        // the refpoints so movement deltas can be calculated.
        nsIntPoint center = GetWindowInnerRectCenter(mDocument->GetWindow(),
                                                     aEvent->widget,
                                                     mPresContext);
        aEvent->lastRefPoint = center;
        if (aEvent->refPoint != center) {
          // This mouse move doesn't finish at the center of the widget,
          // dispatch a synthetic mouse move to return the mouse back to
          // the center.
          aEvent->widget->SynthesizeNativeMouseMove(center);
        }
      } else {
        aEvent->lastRefPoint = sLastRefPoint;
      }

      // Update the last known refPoint with the current refPoint.
      sLastRefPoint = aEvent->refPoint;

      // Get the target content target (mousemove target == mouseover target)
      nsCOMPtr<nsIContent> targetElement = GetEventTargetContent(aEvent);
      if (!targetElement) {
        // We're always over the document root, even if we're only
        // over dead space in a page (whose frame is not associated with
        // any content) or in print preview dead space
        targetElement = mDocument->GetRootElement();
      }
      if (targetElement) {
        NotifyMouseOver(aEvent, targetElement);
      }
    }
    break;
  case NS_MOUSE_EXIT:
    {
      // This is actually the window mouse exit event. We're not moving
      // into any new element.

      if (mLastMouseOverFrame &&
          nsContentUtils::GetTopLevelWidget(aEvent->widget) !=
          nsContentUtils::GetTopLevelWidget(mLastMouseOverFrame->GetNearestWidget())) {
        // the MouseOut event widget doesn't have same top widget with
        // mLastMouseOverFrame, it's a spurious event for mLastMouseOverFrame
        break;
      }

      NotifyMouseOut(aEvent, nsnull);
    }
    break;
  }

  // reset mCurretTargetContent to what it was
  mCurrentTargetContent = targetBeforeEvent;
}

void
nsEventStateManager::SetPointerLock(nsIWidget* aWidget,
                                    nsIContent* aElement)
{
  // NOTE: aElement will be nsnull when unlocking.
  sIsPointerLocked = !!aElement;

  if (!aWidget) {
    return;
  }

  // Reset mouse wheel transaction
  nsMouseWheelTransaction::EndTransaction();

  // Deal with DnD events
  nsCOMPtr<nsIDragService> dragService =
    do_GetService("@mozilla.org/widget/dragservice;1");

  if (sIsPointerLocked) {
    // Store the last known ref point so we can reposition the pointer after unlock.
    mPreLockPoint = sLastRefPoint;

    // Fire a synthetic mouse move to ensure event state is updated. We first
    // set the mouse to the center of the window, so that the mouse event
    // doesn't report any movement.
    sLastRefPoint = GetWindowInnerRectCenter(aElement->OwnerDoc()->GetWindow(),
                                             aWidget,
                                             mPresContext);
    aWidget->SynthesizeNativeMouseMove(sLastRefPoint);

    // Retarget all events to this element via capture.
    nsIPresShell::SetCapturingContent(aElement, CAPTURE_POINTERLOCK);

    // Suppress DnD
    if (dragService) {
      dragService->Suppress();
    }
  } else {
    // Unlocking, so return pointer to the original position by firing a
    // synthetic mouse event. We first reset sLastRefPoint to its
    // pre-pointerlock position, so that the synthetic mouse event reports
    // no movement.
    sLastRefPoint = mPreLockPoint;
    aWidget->SynthesizeNativeMouseMove(mPreLockPoint);

    // Don't retarget events to this element any more.
    nsIPresShell::SetCapturingContent(nsnull, CAPTURE_POINTERLOCK);

    // Unsuppress DnD
    if (dragService) {
      dragService->Unsuppress();
    }
  }
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
      // when dragging from one frame to another, events are fired in the
      // order: dragexit, dragenter, dragleave
      if (sLastDragOverFrame != mCurrentTarget) {
        //We'll need the content, too, to check if it changed separately from the frames.
        nsCOMPtr<nsIContent> lastContent;
        nsCOMPtr<nsIContent> targetContent;
        mCurrentTarget->GetContentForEvent(aEvent, getter_AddRefs(targetContent));

        if (sLastDragOverFrame) {
          //The frame has changed but the content may not have. Check before dispatching to content
          sLastDragOverFrame->GetContentForEvent(aEvent, getter_AddRefs(lastContent));

          FireDragEnterOrExit(sLastDragOverFrame->PresContext(),
                              aEvent, NS_DRAGDROP_EXIT_SYNTH,
                              targetContent, lastContent, sLastDragOverFrame);
        }

        FireDragEnterOrExit(aPresContext, aEvent, NS_DRAGDROP_ENTER,
                            lastContent, targetContent, mCurrentTarget);

        if (sLastDragOverFrame) {
          FireDragEnterOrExit(sLastDragOverFrame->PresContext(),
                              aEvent, NS_DRAGDROP_LEAVE_SYNTH,
                              targetContent, lastContent, sLastDragOverFrame);
        }

        sLastDragOverFrame = mCurrentTarget;
      }
    }
    break;

  case NS_DRAGDROP_EXIT:
    {
      //This is actually the window mouse exit event.
      if (sLastDragOverFrame) {
        nsCOMPtr<nsIContent> lastContent;
        sLastDragOverFrame->GetContentForEvent(aEvent, getter_AddRefs(lastContent));

        nsRefPtr<nsPresContext> lastDragOverFramePresContext = sLastDragOverFrame->PresContext();
        FireDragEnterOrExit(lastDragOverFramePresContext,
                            aEvent, NS_DRAGDROP_EXIT_SYNTH,
                            nsnull, lastContent, sLastDragOverFrame);
        FireDragEnterOrExit(lastDragOverFramePresContext,
                            aEvent, NS_DRAGDROP_LEAVE_SYNTH,
                            nsnull, lastContent, sLastDragOverFrame);

        sLastDragOverFrame = nsnull;
      }
    }
    break;
  }

  //reset mCurretTargetContent to what it was
  mCurrentTargetContent = targetBeforeEvent;

  // Now flush all pending notifications, for better responsiveness.
  FlushPendingEvents(aPresContext);
}

void
nsEventStateManager::FireDragEnterOrExit(nsPresContext* aPresContext,
                                         nsGUIEvent* aEvent,
                                         PRUint32 aMsg,
                                         nsIContent* aRelatedTarget,
                                         nsIContent* aTargetContent,
                                         nsWeakFrame& aTargetFrame)
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsDragEvent event(NS_IS_TRUSTED_EVENT(aEvent), aMsg, aEvent->widget);
  event.refPoint = aEvent->refPoint;
  event.modifiers = ((nsMouseEvent*)aEvent)->modifiers;
  event.buttons = ((nsMouseEvent*)aEvent)->buttons;
  event.relatedTarget = aRelatedTarget;
  event.inputSource = static_cast<nsMouseEvent*>(aEvent)->inputSource;

  mCurrentTargetContent = aTargetContent;

  if (aTargetContent != aRelatedTarget) {
    //XXX This event should still go somewhere!!
    if (aTargetContent)
      nsEventDispatcher::Dispatch(aTargetContent, aPresContext, &event,
                                  nsnull, &status);

    // adjust the drag hover if the dragenter event was cancelled or this is a drag exit
    if (status == nsEventStatus_eConsumeNoDefault || aMsg == NS_DRAGDROP_EXIT)
      SetContentState((aMsg == NS_DRAGDROP_ENTER) ? aTargetContent : nsnull,
                      NS_EVENT_STATE_DRAGOVER);

    // collect any changes to moz cursor settings stored in the event's
    // data transfer.
    if (aMsg == NS_DRAGDROP_LEAVE_SYNTH || aMsg == NS_DRAGDROP_EXIT_SYNTH ||
        aMsg == NS_DRAGDROP_ENTER)
      UpdateDragDataTransfer(&event);
  }

  // Finally dispatch the event to the frame
  if (aTargetFrame)
    aTargetFrame->HandleEvent(aPresContext, &event, &status);
}

void
nsEventStateManager::UpdateDragDataTransfer(nsDragEvent* dragEvent)
{
  NS_ASSERTION(dragEvent, "drag event is null in UpdateDragDataTransfer!");
  if (!dragEvent->dataTransfer)
    return;

  nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();

  if (dragSession) {
    // the initial dataTransfer is the one from the dragstart event that
    // was set on the dragSession when the drag began.
    nsCOMPtr<nsIDOMDataTransfer> initialDataTransfer;
    dragSession->GetDataTransfer(getter_AddRefs(initialDataTransfer));
    if (initialDataTransfer) {
      // retrieve the current moz cursor setting and save it.
      nsAutoString mozCursor;
      dragEvent->dataTransfer->GetMozCursor(mozCursor);
      initialDataTransfer->SetMozCursor(mozCursor);
    }
  }
}

nsresult
nsEventStateManager::SetClickCount(nsPresContext* aPresContext,
                                   nsMouseEvent *aEvent,
                                   nsEventStatus* aStatus)
{
  nsCOMPtr<nsIContent> mouseContent;
  nsIContent* mouseContentParent = nsnull;
  mCurrentTarget->GetContentForEvent(aEvent, getter_AddRefs(mouseContent));
  if (mouseContent) {
    if (mouseContent->IsNodeOfType(nsINode::eTEXT)) {
      mouseContent = mouseContent->GetParent();
    }
    if (mouseContent && mouseContent->IsRootOfNativeAnonymousSubtree()) {
      mouseContentParent = mouseContent->GetParent();
    }
  }

  switch (aEvent->button) {
  case nsMouseEvent::eLeftButton:
    if (aEvent->message == NS_MOUSE_BUTTON_DOWN) {
      mLastLeftMouseDownContent = mouseContent;
      mLastLeftMouseDownContentParent = mouseContentParent;
    } else if (aEvent->message == NS_MOUSE_BUTTON_UP) {
      if (mLastLeftMouseDownContent == mouseContent ||
          mLastLeftMouseDownContentParent == mouseContent ||
          mLastLeftMouseDownContent == mouseContentParent) {
        aEvent->clickCount = mLClickCount;
        mLClickCount = 0;
      } else {
        aEvent->clickCount = 0;
      }
      mLastLeftMouseDownContent = nsnull;
      mLastLeftMouseDownContentParent = nsnull;
    }
    break;

  case nsMouseEvent::eMiddleButton:
    if (aEvent->message == NS_MOUSE_BUTTON_DOWN) {
      mLastMiddleMouseDownContent = mouseContent;
      mLastMiddleMouseDownContentParent = mouseContentParent;
    } else if (aEvent->message == NS_MOUSE_BUTTON_UP) {
      if (mLastMiddleMouseDownContent == mouseContent ||
          mLastMiddleMouseDownContentParent == mouseContent ||
          mLastMiddleMouseDownContent == mouseContentParent) {
        aEvent->clickCount = mMClickCount;
        mMClickCount = 0;
      } else {
        aEvent->clickCount = 0;
      }
      mLastMiddleMouseDownContent = nsnull;
      mLastMiddleMouseDownContentParent = nsnull;
    }
    break;

  case nsMouseEvent::eRightButton:
    if (aEvent->message == NS_MOUSE_BUTTON_DOWN) {
      mLastRightMouseDownContent = mouseContent;
      mLastRightMouseDownContentParent = mouseContentParent;
    } else if (aEvent->message == NS_MOUSE_BUTTON_UP) {
      if (mLastRightMouseDownContent == mouseContent ||
          mLastRightMouseDownContentParent == mouseContent ||
          mLastRightMouseDownContent == mouseContentParent) {
        aEvent->clickCount = mRClickCount;
        mRClickCount = 0;
      } else {
        aEvent->clickCount = 0;
      }
      mLastRightMouseDownContent = nsnull;
      mLastRightMouseDownContentParent = nsnull;
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
      bool enabled;
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
    event.modifiers = aEvent->modifiers;
    event.buttons = aEvent->buttons;
    event.time = aEvent->time;
    event.flags |= flags;
    event.button = aEvent->button;
    event.inputSource = aEvent->inputSource;

    nsCOMPtr<nsIPresShell> presShell = mPresContext->GetPresShell();
    if (presShell) {
      nsCOMPtr<nsIContent> mouseContent = GetEventTargetContent(aEvent);

      ret = presShell->HandleEventWithTarget(&event, mCurrentTarget,
                                             mouseContent, aStatus);
      if (NS_SUCCEEDED(ret) && aEvent->clickCount == 2) {
        //fire double click
        nsMouseEvent event2(NS_IS_TRUSTED_EVENT(aEvent), NS_MOUSE_DOUBLECLICK,
                            aEvent->widget, nsMouseEvent::eReal);
        event2.refPoint = aEvent->refPoint;
        event2.clickCount = aEvent->clickCount;
        event2.modifiers = aEvent->modifiers;
        event2.buttons = aEvent->buttons;
        event2.flags |= flags;
        event2.button = aEvent->button;
        event2.inputSource = aEvent->inputSource;

        ret = presShell->HandleEventWithTarget(&event2, mCurrentTarget,
                                               mouseContent, aStatus);
      }
    }
  }
  return ret;
}

nsIFrame*
nsEventStateManager::GetEventTarget()
{
  nsIPresShell *shell;
  if (mCurrentTarget ||
      !mPresContext ||
      !(shell = mPresContext->GetPresShell())) {
    return mCurrentTarget;
  }

  if (mCurrentTargetContent) {
    mCurrentTarget = mPresContext->GetPrimaryFrameFor(mCurrentTargetContent);
    if (mCurrentTarget) {
      return mCurrentTarget;
    }
  }

  nsIFrame* frame = shell->GetEventTargetFrame();
  return (mCurrentTarget = frame);
}

already_AddRefed<nsIContent>
nsEventStateManager::GetEventTargetContent(nsEvent* aEvent)
{
  if (aEvent &&
      (aEvent->message == NS_FOCUS_CONTENT ||
       aEvent->message == NS_BLUR_CONTENT)) {
    nsCOMPtr<nsIContent> content = GetFocusedContent();
    return content.forget();
  }

  if (mCurrentTargetContent) {
    nsCOMPtr<nsIContent> content = mCurrentTargetContent;
    return content.forget();
  }

  nsIContent *content = nsnull;

  nsIPresShell *presShell = mPresContext->GetPresShell();
  if (presShell) {
    content = presShell->GetEventTargetContent(aEvent).get();
  }

  // Some events here may set mCurrentTarget but not set the corresponding
  // event target in the PresShell.
  if (!content && mCurrentTarget) {
    mCurrentTarget->GetContentForEvent(aEvent, &content);
  }

  return content;
}

static Element*
GetLabelTarget(nsIContent* aPossibleLabel)
{
  nsHTMLLabelElement* label = nsHTMLLabelElement::FromContent(aPossibleLabel);
  if (!label)
    return nsnull;

  return label->GetLabeledElement();
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

static Element*
GetParentElement(Element* aElement)
{
  nsIContent* p = aElement->GetParent();
  return (p && p->IsElement()) ? p->AsElement() : nsnull;
}

/* static */
void
nsEventStateManager::SetFullScreenState(Element* aElement, bool aIsFullScreen)
{
  DoStateChange(aElement, NS_EVENT_STATE_FULL_SCREEN, aIsFullScreen);
  Element* ancestor = aElement;
  while ((ancestor = GetParentElement(ancestor))) {
    DoStateChange(ancestor, NS_EVENT_STATE_FULL_SCREEN_ANCESTOR, aIsFullScreen);
  }
}

/* static */
inline void
nsEventStateManager::DoStateChange(Element* aElement, nsEventStates aState,
                                   bool aAddState)
{
  if (aAddState) {
    aElement->AddStates(aState);
  } else {
    aElement->RemoveStates(aState);
  }
}

/* static */
inline void
nsEventStateManager::DoStateChange(nsIContent* aContent, nsEventStates aState,
                                   bool aStateAdded)
{
  if (aContent->IsElement()) {
    DoStateChange(aContent->AsElement(), aState, aStateAdded);
  }
}

/* static */
void
nsEventStateManager::UpdateAncestorState(nsIContent* aStartNode,
                                         nsIContent* aStopBefore,
                                         nsEventStates aState,
                                         bool aAddState)
{
  for (; aStartNode && aStartNode != aStopBefore;
       aStartNode = aStartNode->GetParent()) {
    // We might be starting with a non-element (e.g. a text node) and
    // if someone is doing something weird might be ending with a
    // non-element too (e.g. a document fragment)
    if (!aStartNode->IsElement()) {
      continue;
    }
    Element* element = aStartNode->AsElement();
    DoStateChange(element, aState, aAddState);
    Element* labelTarget = GetLabelTarget(element);
    if (labelTarget) {
      DoStateChange(labelTarget, aState, aAddState);
    }
  }

  if (aAddState) {
    // We might be in a situation where a node was in hover both
    // because it was hovered and because the label for it was
    // hovered, and while we stopped hovering the node the label is
    // still hovered.  Or we might have had two nested labels for the
    // same node, and while one is no longer hovered the other still
    // is.  In that situation, the label that's still hovered will be
    // aStopBefore or some ancestor of it, and the call we just made
    // to UpdateAncestorState with aAddState = false would have
    // removed the hover state from the node.  But the node should
    // still be in hover state.  To handle this situation we need to
    // keep walking up the tree and any time we find a label mark its
    // corresponding node as still in our state.
    for ( ; aStartNode; aStartNode = aStartNode->GetParent()) {
      if (!aStartNode->IsElement()) {
        continue;
      }

      Element* labelTarget = GetLabelTarget(aStartNode->AsElement());
      if (labelTarget && !labelTarget->State().HasState(aState)) {
        DoStateChange(labelTarget, aState, true);
      }
    }
  }
}

bool
nsEventStateManager::SetContentState(nsIContent *aContent, nsEventStates aState)
{
  // We manage 4 states here: ACTIVE, HOVER, DRAGOVER, URLTARGET
  // The input must be exactly one of them.
  NS_PRECONDITION(aState == NS_EVENT_STATE_ACTIVE ||
                  aState == NS_EVENT_STATE_HOVER ||
                  aState == NS_EVENT_STATE_DRAGOVER ||
                  aState == NS_EVENT_STATE_URLTARGET,
                  "Unexpected state");

  nsCOMPtr<nsIContent> notifyContent1;
  nsCOMPtr<nsIContent> notifyContent2;
  bool updateAncestors;

  if (aState == NS_EVENT_STATE_HOVER || aState == NS_EVENT_STATE_ACTIVE) {
    // Hover and active are hierarchical
    updateAncestors = true;

    // check to see that this state is allowed by style. Check dragover too?
    // XXX Is this even what we want?
    if (mCurrentTarget)
    {
      const nsStyleUserInterface* ui = mCurrentTarget->GetStyleUserInterface();
      if (ui->mUserInput == NS_STYLE_USER_INPUT_NONE)
        return false;
    }

    if (aState == NS_EVENT_STATE_ACTIVE) {
      if (aContent != mActiveContent) {
        notifyContent1 = aContent;
        notifyContent2 = mActiveContent;
        mActiveContent = aContent;
      }
    } else {
      NS_ASSERTION(aState == NS_EVENT_STATE_HOVER, "How did that happen?");
      nsIContent* newHover;
      
      if (mPresContext->IsDynamic()) {
        newHover = aContent;
      } else {
        NS_ASSERTION(!aContent ||
                     aContent->GetCurrentDoc() == mPresContext->PresShell()->GetDocument(),
                     "Unexpected document");
        nsIFrame *frame = aContent ? aContent->GetPrimaryFrame() : nsnull;
        if (frame && nsLayoutUtils::IsViewportScrollbarFrame(frame)) {
          // The scrollbars of viewport should not ignore the hover state.
          // Because they are *not* the content of the web page.
          newHover = aContent;
        } else {
          // All contents of the web page should ignore the hover state.
          newHover = nsnull;
        }
      }

      if (newHover != mHoverContent) {
        notifyContent1 = newHover;
        notifyContent2 = mHoverContent;
        mHoverContent = newHover;
      }
    }
  } else {
    updateAncestors = false;
    if (aState == NS_EVENT_STATE_DRAGOVER) {
      if (aContent != sDragOverContent) {
        notifyContent1 = aContent;
        notifyContent2 = sDragOverContent;
        sDragOverContent = aContent;
      }
    } else if (aState == NS_EVENT_STATE_URLTARGET) {
      if (aContent != mURLTargetContent) {
        notifyContent1 = aContent;
        notifyContent2 = mURLTargetContent;
        mURLTargetContent = aContent;
      }
    }
  }

  // We need to keep track of which of notifyContent1 and notifyContent2 is
  // getting the state set and which is getting it unset.  If both are
  // non-null, then notifyContent1 is having the state set and notifyContent2
  // is having it unset.  But if one of them is null, we need to keep track of
  // the right thing for notifyContent1 explicitly.
  bool content1StateSet = true;
  if (!notifyContent1) {
    // This is ok because FindCommonAncestor wouldn't find anything
    // anyway if notifyContent1 is null.
    notifyContent1 = notifyContent2;
    notifyContent2 = nsnull;
    content1StateSet = false;
  }

  if (notifyContent1 && mPresContext) {
    EnsureDocument(mPresContext);
    if (mDocument) {
      nsAutoScriptBlocker scriptBlocker;

      if (updateAncestors) {
        nsCOMPtr<nsIContent> commonAncestor =
          FindCommonAncestor(notifyContent1, notifyContent2);
        if (notifyContent2) {
          // It's very important to first notify the state removal and
          // then the state addition, because due to labels it's
          // possible that we're removing state from some element but
          // then adding it again (say because mHoverContent changed
          // from a control to its label).
          UpdateAncestorState(notifyContent2, commonAncestor, aState, false);
        }
        UpdateAncestorState(notifyContent1, commonAncestor, aState,
                            content1StateSet);
      } else {
        if (notifyContent2) {
          DoStateChange(notifyContent2, aState, false);
        }
        DoStateChange(notifyContent1, aState, content1StateSet);
      }
    }
  }

  return true;
}

void
nsEventStateManager::ContentRemoved(nsIDocument* aDocument, nsIContent* aContent)
{
  /*
   * Anchor and area elements when focused or hovered might make the UI to show
   * the current link. We want to make sure that the UI gets informed when they
   * are actually removed from the DOM.
   */
  if (aContent->IsHTML() &&
      (aContent->Tag() == nsGkAtoms::a || aContent->Tag() == nsGkAtoms::area) &&
      (aContent->AsElement()->State().HasAtLeastOneOfStates(NS_EVENT_STATE_FOCUS |
                                                            NS_EVENT_STATE_HOVER))) {
    nsGenericHTMLElement* element = static_cast<nsGenericHTMLElement*>(aContent);
    element->LeaveLink(element->GetPresContext());
  }

  // inform the focus manager that the content is being removed. If this
  // content is focused, the focus will be removed without firing events.
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm)
    fm->ContentRemoved(aDocument, aContent);

  if (mHoverContent &&
      nsContentUtils::ContentIsDescendantOf(mHoverContent, aContent)) {
    // Since hover is hierarchical, set the current hover to the
    // content's parent node.
    SetContentState(aContent->GetParent(), NS_EVENT_STATE_HOVER);
  }

  if (mActiveContent &&
      nsContentUtils::ContentIsDescendantOf(mActiveContent, aContent)) {
    // Active is hierarchical, so set the current active to the
    // content's parent node.
    SetContentState(aContent->GetParent(), NS_EVENT_STATE_ACTIVE);
  }

  if (sDragOverContent &&
      sDragOverContent->OwnerDoc() == aContent->OwnerDoc() &&
      nsContentUtils::ContentIsDescendantOf(sDragOverContent, aContent)) {
    sDragOverContent = nsnull;
  }

  if (mLastMouseOverElement &&
      nsContentUtils::ContentIsDescendantOf(mLastMouseOverElement, aContent)) {
    // See bug 292146 for why we want to null this out
    mLastMouseOverElement = nsnull;
  }
}

bool
nsEventStateManager::EventStatusOK(nsGUIEvent* aEvent)
{
  return !(aEvent->message == NS_MOUSE_BUTTON_DOWN &&
      static_cast<nsMouseEvent*>(aEvent)->button == nsMouseEvent::eLeftButton  && 
      !sNormalLMouseEventInProcess);
}

//-------------------------------------------
// Access Key Registration
//-------------------------------------------
void
nsEventStateManager::RegisterAccessKey(nsIContent* aContent, PRUint32 aKey)
{
  if (aContent && mAccessKeys.IndexOf(aContent) == -1)
    mAccessKeys.AppendObject(aContent);
}

void
nsEventStateManager::UnregisterAccessKey(nsIContent* aContent, PRUint32 aKey)
{
  if (aContent)
    mAccessKeys.RemoveObject(aContent);
}

PRUint32
nsEventStateManager::GetRegisteredAccessKey(nsIContent* aContent)
{
  NS_ENSURE_ARG(aContent);

  if (mAccessKeys.IndexOf(aContent) == -1)
    return 0;

  nsAutoString accessKey;
  aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, accessKey);
  return accessKey.First();
}

void
nsEventStateManager::EnsureDocument(nsPresContext* aPresContext)
{
  if (!mDocument)
    mDocument = aPresContext->Document();
}

void
nsEventStateManager::FlushPendingEvents(nsPresContext* aPresContext)
{
  NS_PRECONDITION(nsnull != aPresContext, "nsnull ptr");
  nsIPresShell *shell = aPresContext->GetPresShell();
  if (shell) {
    shell->FlushPendingNotifications(Flush_InterruptibleLayout);
  }
}

nsIContent*
nsEventStateManager::GetFocusedContent()
{
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm || !mDocument)
    return nsnull;

  nsCOMPtr<nsPIDOMWindow> focusedWindow;
  return nsFocusManager::GetFocusedDescendant(mDocument->GetWindow(), false,
                                              getter_AddRefs(focusedWindow));
}

//-------------------------------------------------------
// Return true if the docshell is visible

bool
nsEventStateManager::IsShellVisible(nsIDocShell* aShell)
{
  NS_ASSERTION(aShell, "docshell is null");

  nsCOMPtr<nsIBaseWindow> basewin = do_QueryInterface(aShell);
  if (!basewin)
    return true;

  bool isVisible = true;
  basewin->GetVisibility(&isVisible);

  // We should be doing some additional checks here so that
  // we don't tab into hidden tabs of tabbrowser.  -bryner

  return isVisible;
}

nsresult
nsEventStateManager::DoContentCommandEvent(nsContentCommandEvent* aEvent)
{
  EnsureDocument(mPresContext);
  NS_ENSURE_TRUE(mDocument, NS_ERROR_FAILURE);
  nsCOMPtr<nsPIDOMWindow> window(mDocument->GetWindow());
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsCOMPtr<nsPIWindowRoot> root = window->GetTopWindowRoot();
  NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);
  const char* cmd;
  switch (aEvent->message) {
    case NS_CONTENT_COMMAND_CUT:
      cmd = "cmd_cut";
      break;
    case NS_CONTENT_COMMAND_COPY:
      cmd = "cmd_copy";
      break;
    case NS_CONTENT_COMMAND_PASTE:
      cmd = "cmd_paste";
      break;
    case NS_CONTENT_COMMAND_DELETE:
      cmd = "cmd_delete";
      break;
    case NS_CONTENT_COMMAND_UNDO:
      cmd = "cmd_undo";
      break;
    case NS_CONTENT_COMMAND_REDO:
      cmd = "cmd_redo";
      break;
    case NS_CONTENT_COMMAND_PASTE_TRANSFERABLE:
      cmd = "cmd_pasteTransferable";
      break;
    default:
      return NS_ERROR_NOT_IMPLEMENTED;
  }
  nsCOMPtr<nsIController> controller;
  nsresult rv = root->GetControllerForCommand(cmd, getter_AddRefs(controller));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!controller) {
    // When GetControllerForCommand succeeded but there is no controller, the
    // command isn't supported.
    aEvent->mIsEnabled = false;
  } else {
    bool canDoIt;
    rv = controller->IsCommandEnabled(cmd, &canDoIt);
    NS_ENSURE_SUCCESS(rv, rv);
    aEvent->mIsEnabled = canDoIt;
    if (canDoIt && !aEvent->mOnlyEnabledCheck) {
      switch (aEvent->message) {
        case NS_CONTENT_COMMAND_PASTE_TRANSFERABLE: {
          nsCOMPtr<nsICommandController> commandController = do_QueryInterface(controller);
          NS_ENSURE_STATE(commandController);

          nsCOMPtr<nsICommandParams> params = do_CreateInstance("@mozilla.org/embedcomp/command-params;1", &rv);
          NS_ENSURE_SUCCESS(rv, rv);

          rv = params->SetISupportsValue("transferable", aEvent->mTransferable);
          NS_ENSURE_SUCCESS(rv, rv);

          rv = commandController->DoCommandWithParams(cmd, params);
          break;
        }
        
        default:
          rv = controller->DoCommand(cmd);
          break;
      }
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  aEvent->mSucceeded = true;
  return NS_OK;
}

nsresult
nsEventStateManager::DoContentCommandScrollEvent(nsContentCommandEvent* aEvent)
{
  NS_ENSURE_TRUE(mPresContext, NS_ERROR_NOT_AVAILABLE);
  nsIPresShell* ps = mPresContext->GetPresShell();
  NS_ENSURE_TRUE(ps, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(aEvent->mScroll.mAmount != 0, NS_ERROR_INVALID_ARG);

  nsIScrollableFrame::ScrollUnit scrollUnit;
  switch (aEvent->mScroll.mUnit) {
    case nsContentCommandEvent::eCmdScrollUnit_Line:
      scrollUnit = nsIScrollableFrame::LINES;
      break;
    case nsContentCommandEvent::eCmdScrollUnit_Page:
      scrollUnit = nsIScrollableFrame::PAGES;
      break;
    case nsContentCommandEvent::eCmdScrollUnit_Whole:
      scrollUnit = nsIScrollableFrame::WHOLE;
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }

  aEvent->mSucceeded = true;

  nsIScrollableFrame* sf =
    ps->GetFrameToScrollAsScrollable(nsIPresShell::eEither);
  aEvent->mIsEnabled = sf ? CanScrollOn(sf, aEvent->mScroll.mAmount,
                                        aEvent->mScroll.mIsHorizontal) :
                            false;

  if (!aEvent->mIsEnabled || aEvent->mOnlyEnabledCheck) {
    return NS_OK;
  }

  nsIntPoint pt(0, 0);
  if (aEvent->mScroll.mIsHorizontal) {
    pt.x = aEvent->mScroll.mAmount;
  } else {
    pt.y = aEvent->mScroll.mAmount;
  }

  // The caller may want synchronous scrolling.
  sf->ScrollBy(pt, scrollUnit, nsIScrollableFrame::INSTANT);
  return NS_OK;
}

void
nsEventStateManager::DoQueryScrollTargetInfo(nsQueryContentEvent* aEvent,
                                             nsIFrame* aTargetFrame)
{
  // Don't modify the test event which in mInput.
  nsMouseScrollEvent msEvent(
    NS_IS_TRUSTED_EVENT(aEvent->mInput.mMouseScrollEvent),
    aEvent->mInput.mMouseScrollEvent->message,
    aEvent->mInput.mMouseScrollEvent->widget);

  msEvent.modifiers = aEvent->mInput.mMouseScrollEvent->modifiers;
  msEvent.buttons = aEvent->mInput.mMouseScrollEvent->buttons;

  msEvent.scrollFlags = aEvent->mInput.mMouseScrollEvent->scrollFlags;
  msEvent.delta = ComputeWheelDeltaFor(aEvent->mInput.mMouseScrollEvent);
  msEvent.scrollOverflow = aEvent->mInput.mMouseScrollEvent->scrollOverflow;

  bool useSystemSettings = UseSystemScrollSettingFor(&msEvent);

  nsIScrollableFrame::ScrollUnit unit;
  bool allowOverrideSystemSettings;
  switch (ComputeWheelActionFor(&msEvent, useSystemSettings)) {
    case MOUSE_SCROLL_N_LINES:
      unit = nsIScrollableFrame::LINES;
      allowOverrideSystemSettings = useSystemSettings;
      break;
    case MOUSE_SCROLL_PAGE:
      unit = nsIScrollableFrame::PAGES;
      allowOverrideSystemSettings = false;
      break;
    default:
      // Don't use high resolution scrolling when the action doesn't scroll
      // contents.
      return;
  }

  DoScrollText(aTargetFrame, &msEvent, unit,
               allowOverrideSystemSettings, aEvent);
}

void
nsEventStateManager::DoQuerySelectedText(nsQueryContentEvent* aEvent)
{
  if (RemoteQueryContentEvent(aEvent)) {
    return;
  }
  nsContentEventHandler handler(mPresContext);
  handler.OnQuerySelectedText(aEvent);
}

void
nsEventStateManager::SetActiveManager(nsEventStateManager* aNewESM,
                                      nsIContent* aContent)
{
  if (sActiveESM && aNewESM != sActiveESM) {
    sActiveESM->SetContentState(nsnull, NS_EVENT_STATE_ACTIVE);
  }
  sActiveESM = aNewESM;
  if (sActiveESM && aContent) {
    sActiveESM->SetContentState(aContent, NS_EVENT_STATE_ACTIVE);
  }
}

void
nsEventStateManager::ClearGlobalActiveContent(nsEventStateManager* aClearer)
{
  if (aClearer) {
    aClearer->SetContentState(nsnull, NS_EVENT_STATE_ACTIVE);
    if (sDragOverContent) {
      aClearer->SetContentState(nsnull, NS_EVENT_STATE_DRAGOVER);
    }
  }
  if (sActiveESM && aClearer != sActiveESM) {
    sActiveESM->SetContentState(nsnull, NS_EVENT_STATE_ACTIVE);
  }
  sActiveESM = nsnull;
}
