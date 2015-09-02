/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TextComposition.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/DragEvent.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/dom/UIEvent.h"

#include "ContentEventHandler.h"
#include "IMEContentObserver.h"
#include "WheelHandlingHelper.h"

#include "nsCOMPtr.h"
#include "nsFocusManager.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsIWidget.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsGkAtoms.h"
#include "nsIFormControl.h"
#include "nsIComboboxControlFrame.h"
#include "nsIScrollableFrame.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMXULControlElement.h"
#include "nsNameSpaceManager.h"
#include "nsIBaseWindow.h"
#include "nsISelection.h"
#include "nsITextControlElement.h"
#include "nsFrameSelection.h"
#include "nsPIDOMWindow.h"
#include "nsPIWindowRoot.h"
#include "nsIWebNavigation.h"
#include "nsIContentViewer.h"
#include "nsFrameManager.h"

#include "nsIDOMXULElement.h"
#include "nsIDOMKeyEvent.h"
#include "nsIObserverService.h"
#include "nsIDocShell.h"
#include "nsIDOMWheelEvent.h"
#include "nsIDOMUIEvent.h"
#include "nsIMozBrowserFrame.h"

#include "nsSubDocumentFrame.h"
#include "nsLayoutUtils.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsUnicharUtils.h"
#include "nsContentUtils.h"

#include "imgIContainer.h"
#include "nsIProperties.h"
#include "nsISupportsPrimitives.h"

#include "nsServiceManagerUtils.h"
#include "nsITimer.h"
#include "nsFontMetrics.h"
#include "nsIDOMXULDocument.h"
#include "nsIDragService.h"
#include "nsIDragSession.h"
#include "mozilla/dom/DataTransfer.h"
#include "nsContentAreaDragDrop.h"
#ifdef MOZ_XUL
#include "nsTreeBodyFrame.h"
#endif
#include "nsIController.h"
#include "nsICommandParams.h"
#include "mozilla/Services.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/HTMLLabelElement.h"

#include "mozilla/Preferences.h"
#include "mozilla/LookAndFeel.h"
#include "GeckoProfiler.h"
#include "Units.h"
#include "mozilla/layers/APZCTreeManager.h"

#ifdef XP_MACOSX
#import <ApplicationServices/ApplicationServices.h>
#endif

namespace mozilla {

using namespace dom;

//#define DEBUG_DOCSHELL_FOCUS

#define NS_USER_INTERACTION_INTERVAL 5000 // ms

static const LayoutDeviceIntPoint kInvalidRefPoint = LayoutDeviceIntPoint(-1,-1);

static uint32_t gMouseOrKeyboardEventCounter = 0;
static nsITimer* gUserInteractionTimer = nullptr;
static nsITimerCallback* gUserInteractionTimerCallback = nullptr;

static inline int32_t
RoundDown(double aDouble)
{
  return (aDouble > 0) ? static_cast<int32_t>(floor(aDouble)) :
                         static_cast<int32_t>(ceil(aDouble));
}

#ifdef DEBUG_DOCSHELL_FOCUS
static void
PrintDocTree(nsIDocShellTreeItem* aParentItem, int aLevel)
{
  for (int32_t i=0;i<aLevel;i++) printf("  ");

  int32_t childWebshellCount;
  aParentItem->GetChildCount(&childWebshellCount);
  nsCOMPtr<nsIDocShell> parentAsDocShell(do_QueryInterface(aParentItem));
  int32_t type = aParentItem->ItemType();
  nsCOMPtr<nsIPresShell> presShell = parentAsDocShell->GetPresShell();
  nsRefPtr<nsPresContext> presContext;
  parentAsDocShell->GetPresContext(getter_AddRefs(presContext));
  nsCOMPtr<nsIContentViewer> cv;
  parentAsDocShell->GetContentViewer(getter_AddRefs(cv));
  nsCOMPtr<nsIDOMDocument> domDoc;
  if (cv)
    cv->GetDOMDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  nsCOMPtr<nsIDOMWindow> domwin = doc ? doc->GetWindow() : nullptr;
  nsIURI* uri = doc ? doc->GetDocumentURI() : nullptr;

  printf("DS %p  Type %s  Cnt %d  Doc %p  DW %p  EM %p%c",
    static_cast<void*>(parentAsDocShell.get()),
    type==nsIDocShellTreeItem::typeChrome?"Chrome":"Content",
    childWebshellCount, static_cast<void*>(doc.get()),
    static_cast<void*>(domwin.get()),
    static_cast<void*>(presContext ? presContext->EventStateManager() : nullptr),
    uri ? ' ' : '\n');
  if (uri) {
    nsAutoCString spec;
    uri->GetSpec(spec);
    printf("\"%s\"\n", spec.get());
  }

  if (childWebshellCount > 0) {
    for (int32_t i = 0; i < childWebshellCount; i++) {
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
  return win ? win->GetExtantDoc() : nullptr;
}

/******************************************************************/
/* mozilla::UITimerCallback                                       */
/******************************************************************/

class UITimerCallback final : public nsITimerCallback
{
public:
  UITimerCallback() : mPreviousCount(0) {}
  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK
private:
  ~UITimerCallback() {}
  uint32_t mPreviousCount;
};

NS_IMPL_ISUPPORTS(UITimerCallback, nsITimerCallback)

// If aTimer is nullptr, this method always sends "user-interaction-inactive"
// notification.
NS_IMETHODIMP
UITimerCallback::Notify(nsITimer* aTimer)
{
  nsCOMPtr<nsIObserverService> obs =
    mozilla::services::GetObserverService();
  if (!obs)
    return NS_ERROR_FAILURE;
  if ((gMouseOrKeyboardEventCounter == mPreviousCount) || !aTimer) {
    gMouseOrKeyboardEventCounter = 0;
    obs->NotifyObservers(nullptr, "user-interaction-inactive", nullptr);
    if (gUserInteractionTimer) {
      gUserInteractionTimer->Cancel();
      NS_RELEASE(gUserInteractionTimer);
    }
  } else {
    obs->NotifyObservers(nullptr, "user-interaction-active", nullptr);
    EventStateManager::UpdateUserActivityTimer();
  }
  mPreviousCount = gMouseOrKeyboardEventCounter;
  return NS_OK;
}

/******************************************************************/
/* mozilla::OverOutElementsWrapper                                */
/******************************************************************/

OverOutElementsWrapper::OverOutElementsWrapper()
  : mLastOverFrame(nullptr)
{
}

OverOutElementsWrapper::~OverOutElementsWrapper()
{
}

NS_IMPL_CYCLE_COLLECTION(OverOutElementsWrapper,
                         mLastOverElement,
                         mFirstOverEventElement,
                         mFirstOutEventElement)
NS_IMPL_CYCLE_COLLECTING_ADDREF(OverOutElementsWrapper)
NS_IMPL_CYCLE_COLLECTING_RELEASE(OverOutElementsWrapper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(OverOutElementsWrapper)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/******************************************************************/
/* mozilla::EventStateManager                                     */
/******************************************************************/

static uint32_t sESMInstanceCount = 0;
static bool sPointerEventEnabled = false;

int32_t EventStateManager::sUserInputEventDepth = 0;
bool EventStateManager::sNormalLMouseEventInProcess = false;
EventStateManager* EventStateManager::sActiveESM = nullptr;
nsIDocument* EventStateManager::sMouseOverDocument = nullptr;
nsWeakFrame EventStateManager::sLastDragOverFrame = nullptr;
LayoutDeviceIntPoint EventStateManager::sLastRefPoint = kInvalidRefPoint;
LayoutDeviceIntPoint EventStateManager::sLastScreenPoint = LayoutDeviceIntPoint(0, 0);
LayoutDeviceIntPoint EventStateManager::sSynthCenteringPoint = kInvalidRefPoint;
CSSIntPoint EventStateManager::sLastClientPoint = CSSIntPoint(0, 0);
bool EventStateManager::sIsPointerLocked = false;
// Reference to the pointer locked element.
nsWeakPtr EventStateManager::sPointerLockedElement;
// Reference to the document which requested pointer lock.
nsWeakPtr EventStateManager::sPointerLockedDoc;
nsCOMPtr<nsIContent> EventStateManager::sDragOverContent = nullptr;
TimeStamp EventStateManager::sHandlingInputStart;

EventStateManager::WheelPrefs*
  EventStateManager::WheelPrefs::sInstance = nullptr;
EventStateManager::DeltaAccumulator*
  EventStateManager::DeltaAccumulator::sInstance = nullptr;

EventStateManager::EventStateManager()
  : mLockCursor(0)
  , mPreLockPoint(0,0)
  , mCurrentTarget(nullptr)
    // init d&d gesture state machine variables
  , mGestureDownPoint(0,0)
  , mPresContext(nullptr)
  , mLClickCount(0)
  , mMClickCount(0)
  , mRClickCount(0)
  , m_haveShutdown(false)
{
  if (sESMInstanceCount == 0) {
    gUserInteractionTimerCallback = new UITimerCallback();
    if (gUserInteractionTimerCallback)
      NS_ADDREF(gUserInteractionTimerCallback);
    UpdateUserActivityTimer();
  }
  ++sESMInstanceCount;

  static bool sAddedPointerEventEnabled = false;
  if (!sAddedPointerEventEnabled) {
    Preferences::AddBoolVarCache(&sPointerEventEnabled,
                                 "dom.w3c_pointer_events.enabled", false);
    sAddedPointerEventEnabled = true;
  }
}

nsresult
EventStateManager::UpdateUserActivityTimer()
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

nsresult
EventStateManager::Init()
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (!observerService)
    return NS_ERROR_FAILURE;

  observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, true);

  if (sESMInstanceCount == 1) {
    Prefs::Init();
  }

  return NS_OK;
}

EventStateManager::~EventStateManager()
{
  ReleaseCurrentIMEContentObserver();

  if (sActiveESM == this) {
    sActiveESM = nullptr;
  }
  if (Prefs::ClickHoldContextMenu())
    KillClickHoldTimer();

  if (mDocument == sMouseOverDocument)
    sMouseOverDocument = nullptr;

  --sESMInstanceCount;
  if(sESMInstanceCount == 0) {
    WheelTransaction::Shutdown();
    if (gUserInteractionTimerCallback) {
      gUserInteractionTimerCallback->Notify(nullptr);
      NS_RELEASE(gUserInteractionTimerCallback);
    }
    if (gUserInteractionTimer) {
      gUserInteractionTimer->Cancel();
      NS_RELEASE(gUserInteractionTimer);
    }
    Prefs::Shutdown();
    WheelPrefs::Shutdown();
    DeltaAccumulator::Shutdown();
  }

  if (sDragOverContent && sDragOverContent->OwnerDoc() == mDocument) {
    sDragOverContent = nullptr;
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
EventStateManager::Shutdown()
{
  m_haveShutdown = true;
  return NS_OK;
}

NS_IMETHODIMP
EventStateManager::Observe(nsISupports* aSubject,
                           const char* aTopic,
                           const char16_t *someData)
{
  if (!nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    Shutdown();
  }

  return NS_OK;
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(EventStateManager)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
   NS_INTERFACE_MAP_ENTRY(nsIObserver)
   NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(EventStateManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(EventStateManager)

NS_IMPL_CYCLE_COLLECTION(EventStateManager,
                         mCurrentTargetContent,
                         mGestureDownContent,
                         mGestureDownFrameOwner,
                         mLastLeftMouseDownContent,
                         mLastLeftMouseDownContentParent,
                         mLastMiddleMouseDownContent,
                         mLastMiddleMouseDownContentParent,
                         mLastRightMouseDownContent,
                         mLastRightMouseDownContentParent,
                         mActiveContent,
                         mHoverContent,
                         mURLTargetContent,
                         mMouseEnterLeaveHelper,
                         mPointersEnterLeaveHelper,
                         mDocument,
                         mIMEContentObserver,
                         mAccessKeys)

void
EventStateManager::ReleaseCurrentIMEContentObserver()
{
  if (mIMEContentObserver) {
    mIMEContentObserver->DisconnectFromEventStateManager();
  }
  mIMEContentObserver = nullptr;
}

void
EventStateManager::OnStartToObserveContent(
                     IMEContentObserver* aIMEContentObserver)
{
  if (mIMEContentObserver == aIMEContentObserver) {
    return;
  }
  ReleaseCurrentIMEContentObserver();
  mIMEContentObserver = aIMEContentObserver;
}

void
EventStateManager::OnStopObservingContent(
                     IMEContentObserver* aIMEContentObserver)
{
  aIMEContentObserver->DisconnectFromEventStateManager();
  NS_ENSURE_TRUE_VOID(mIMEContentObserver == aIMEContentObserver);
  mIMEContentObserver = nullptr;
}

nsresult
EventStateManager::PreHandleEvent(nsPresContext* aPresContext,
                                  WidgetEvent* aEvent,
                                  nsIFrame* aTargetFrame,
                                  nsIContent* aTargetContent,
                                  nsEventStatus* aStatus)
{
  NS_ENSURE_ARG_POINTER(aStatus);
  NS_ENSURE_ARG(aPresContext);
  if (!aEvent) {
    NS_ERROR("aEvent is null.  This should never happen.");
    return NS_ERROR_NULL_POINTER;
  }

  NS_WARN_IF_FALSE(!aTargetFrame ||
                   !aTargetFrame->GetContent() ||
                   aTargetFrame->GetContent() == aTargetContent ||
                   aTargetFrame->GetContent()->GetFlattenedTreeParent() == aTargetContent,
                   "aTargetFrame should be related with aTargetContent");

  mCurrentTarget = aTargetFrame;
  mCurrentTargetContent = nullptr;

  // Do not take account eMouseEnterIntoWidget/ExitFromWidget so that loading
  // a page when user is not active doesn't change the state to active.
  WidgetMouseEvent* mouseEvent = aEvent->AsMouseEvent();
  if (aEvent->mFlags.mIsTrusted &&
      ((mouseEvent && mouseEvent->IsReal() &&
        mouseEvent->mMessage != eMouseEnterIntoWidget &&
        mouseEvent->mMessage != eMouseExitFromWidget) ||
       aEvent->mClass == eWheelEventClass ||
       aEvent->mClass == eKeyboardEventClass)) {
    if (gMouseOrKeyboardEventCounter == 0) {
      nsCOMPtr<nsIObserverService> obs =
        mozilla::services::GetObserverService();
      if (obs) {
        obs->NotifyObservers(nullptr, "user-interaction-active", nullptr);
        UpdateUserActivityTimer();
      }
    }
    ++gMouseOrKeyboardEventCounter;
  }

  WheelTransaction::OnEvent(aEvent);

  // Focus events don't necessarily need a frame.
  if (!mCurrentTarget && !aTargetContent) {
    NS_ERROR("mCurrentTarget and aTargetContent are null");
    return NS_ERROR_NULL_POINTER;
  }
#ifdef DEBUG
  if (aEvent->HasDragEventMessage() && sIsPointerLocked) {
    NS_ASSERTION(sIsPointerLocked,
      "sIsPointerLocked is true. Drag events should be suppressed when "
      "the pointer is locked.");
  }
#endif
  // Store last known screenPoint and clientPoint so pointer lock
  // can use these values as constants.
  if (aEvent->mFlags.mIsTrusted &&
      ((mouseEvent && mouseEvent->IsReal()) ||
       aEvent->mClass == eWheelEventClass) &&
      !sIsPointerLocked) {
    sLastScreenPoint =
      UIEvent::CalculateScreenPoint(aPresContext, aEvent);
    sLastClientPoint =
      UIEvent::CalculateClientPoint(aPresContext, aEvent, nullptr);
  }

  *aStatus = nsEventStatus_eIgnore;

  switch (aEvent->mMessage) {
  case eContextMenu:
    if (sIsPointerLocked) {
      return NS_ERROR_DOM_INVALID_STATE_ERR;
    }
    break;
  case eMouseDown: {
    switch (mouseEvent->button) {
    case WidgetMouseEvent::eLeftButton:
      BeginTrackingDragGesture(aPresContext, mouseEvent, aTargetFrame);
      mLClickCount = mouseEvent->clickCount;
      SetClickCount(aPresContext, mouseEvent, aStatus);
      sNormalLMouseEventInProcess = true;
      break;
    case WidgetMouseEvent::eMiddleButton:
      mMClickCount = mouseEvent->clickCount;
      SetClickCount(aPresContext, mouseEvent, aStatus);
      break;
    case WidgetMouseEvent::eRightButton:
      mRClickCount = mouseEvent->clickCount;
      SetClickCount(aPresContext, mouseEvent, aStatus);
      break;
    }
    break;
  }
  case eMouseUp: {
    switch (mouseEvent->button) {
      case WidgetMouseEvent::eLeftButton:
        if (Prefs::ClickHoldContextMenu()) {
          KillClickHoldTimer();
        }
        StopTrackingDragGesture();
        sNormalLMouseEventInProcess = false;
        // then fall through...
      case WidgetMouseEvent::eRightButton:
      case WidgetMouseEvent::eMiddleButton:
        SetClickCount(aPresContext, mouseEvent, aStatus);
        break;
    }
    break;
  }
  case eMouseEnterIntoWidget:
    // In some cases on e10s eMouseEnterIntoWidget
    // event was sent twice into child process of content.
    // (From specific widget code (sending is not permanent) and
    // from ESM::DispatchMouseOrPointerEvent (sending is permanent)).
    // Flag mNoCrossProcessBoundaryForwarding helps to
    // suppress sending accidental event from widget code.
    aEvent->mFlags.mNoCrossProcessBoundaryForwarding = true;
    break;
  case eMouseExitFromWidget:
    // If this is a remote frame, we receive eMouseExitFromWidget from the
    // parent the mouse exits our content. Since the parent may update the
    // cursor while the mouse is outside our frame, and since PuppetWidget
    // caches the current cursor internally, re-entering our content (say from
    // over a window edge) wont update the cursor if the cached value and the
    // current cursor match. So when the mouse exits a remote frame, clear the
    // cached widget cursor so a proper update will occur when the mouse
    // re-enters.
    if (XRE_IsContentProcess()) {
      ClearCachedWidgetCursor(mCurrentTarget);
    }

    // Flag helps to suppress double event sending into process of content.
    // For more information see comment above, at eMouseEnterIntoWidget case.
    aEvent->mFlags.mNoCrossProcessBoundaryForwarding = true;

    // If the event is not a top-level window exit, then it's not
    // really an exit --- we may have traversed widget boundaries but
    // we're still in our toplevel window.
    if (mouseEvent->exit != WidgetMouseEvent::eTopLevel) {
      // Treat it as a synthetic move so we don't generate spurious
      // "exit" or "move" events.  Any necessary "out" or "over" events
      // will be generated by GenerateMouseEnterExit
      mouseEvent->mMessage = eMouseMove;
      mouseEvent->reason = WidgetMouseEvent::eSynthesized;
      // then fall through...
    } else {
      if (sPointerEventEnabled) {
        // We should synthetize corresponding pointer events
        GeneratePointerEnterExit(ePointerLeave, mouseEvent);
      }
      GenerateMouseEnterExit(mouseEvent);
      //This is a window level mouse exit event and should stop here
      aEvent->mMessage = eVoidEvent;
      break;
    }
  case eMouseMove:
  case ePointerDown:
  case ePointerMove: {
    // on the Mac, GenerateDragGesture() may not return until the drag
    // has completed and so |aTargetFrame| may have been deleted (moving
    // a bookmark, for example).  If this is the case, however, we know
    // that ClearFrameRefs() has been called and it cleared out
    // |mCurrentTarget|. As a result, we should pass |mCurrentTarget|
    // into UpdateCursor().
    GenerateDragGesture(aPresContext, mouseEvent);
    UpdateCursor(aPresContext, aEvent, mCurrentTarget, aStatus);
    GenerateMouseEnterExit(mouseEvent);
    // Flush pending layout changes, so that later mouse move events
    // will go to the right nodes.
    FlushPendingEvents(aPresContext);
    break;
  }
  case NS_DRAGDROP_GESTURE:
    if (Prefs::ClickHoldContextMenu()) {
      // an external drag gesture event came in, not generated internally
      // by Gecko. Make sure we get rid of the click-hold timer.
      KillClickHoldTimer();
    }
    break;
  case eDragOver:
    // eDrop is fired before NS_DRAGDROP_DRAGDROP so send
    // the enter/exit events before eDrop.
    GenerateDragDropEnterExit(aPresContext, aEvent->AsDragEvent());
    break;

  case eKeyPress:
    {
      WidgetKeyboardEvent* keyEvent = aEvent->AsKeyboardEvent();

      int32_t modifierMask = 0;
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
      if (modifierMask &&
          (modifierMask == Prefs::ChromeAccessModifierMask() ||
           modifierMask == Prefs::ContentAccessModifierMask())) {
        HandleAccessKey(aPresContext, keyEvent, aStatus, nullptr,
                        eAccessKeyProcessingNormal, modifierMask);
      }
    }
    // then fall through...
  case eBeforeKeyDown:
  case eKeyDown:
  case eAfterKeyDown:
  case eBeforeKeyUp:
  case eKeyUp:
  case eAfterKeyUp:
    {
      nsIContent* content = GetFocusedContent();
      if (content)
        mCurrentTargetContent = content;

      // NOTE: Don't refer TextComposition::IsComposing() since DOM Level 3
      //       Events defines that KeyboardEvent.isComposing is true when it's
      //       dispatched after compositionstart and compositionend.
      //       TextComposition::IsComposing() is false even before
      //       compositionend if there is no composing string.
      WidgetKeyboardEvent* keyEvent = aEvent->AsKeyboardEvent();
      nsRefPtr<TextComposition> composition =
        IMEStateManager::GetTextCompositionFor(keyEvent);
      keyEvent->mIsComposing = !!composition;
    }
    break;
  case NS_WHEEL_WHEEL:
  case NS_WHEEL_START:
  case NS_WHEEL_STOP:
    {
      NS_ASSERTION(aEvent->mFlags.mIsTrusted,
                   "Untrusted wheel event shouldn't be here");

      nsIContent* content = GetFocusedContent();
      if (content) {
        mCurrentTargetContent = content;
      }

      if (aEvent->mMessage != NS_WHEEL_WHEEL) {
        break;
      }

      WidgetWheelEvent* wheelEvent = aEvent->AsWheelEvent();
      WheelPrefs::GetInstance()->ApplyUserPrefsToDelta(wheelEvent);

      // If we won't dispatch a DOM event for this event, nothing to do anymore.
      if (!wheelEvent->IsAllowedToDispatchDOMEvent()) {
        break;
      }

      // Init lineOrPageDelta values for line scroll events for some devices
      // on some platforms which might dispatch wheel events which don't have
      // lineOrPageDelta values.  And also, if delta values are customized by
      // prefs, this recomputes them.
      DeltaAccumulator::GetInstance()->
        InitLineOrPageDelta(aTargetFrame, this, wheelEvent);
    }
    break;
  case NS_QUERY_SELECTED_TEXT:
    DoQuerySelectedText(aEvent->AsQueryContentEvent());
    break;
  case NS_QUERY_TEXT_CONTENT:
    {
      if (RemoteQueryContentEvent(aEvent)) {
        break;
      }
      ContentEventHandler handler(mPresContext);
      handler.OnQueryTextContent(aEvent->AsQueryContentEvent());
    }
    break;
  case NS_QUERY_CARET_RECT:
    {
      if (RemoteQueryContentEvent(aEvent)) {
        break;
      }
      ContentEventHandler handler(mPresContext);
      handler.OnQueryCaretRect(aEvent->AsQueryContentEvent());
    }
    break;
  case NS_QUERY_TEXT_RECT:
    {
      if (RemoteQueryContentEvent(aEvent)) {
        break;
      }
      ContentEventHandler handler(mPresContext);
      handler.OnQueryTextRect(aEvent->AsQueryContentEvent());
    }
    break;
  case NS_QUERY_EDITOR_RECT:
    {
      if (RemoteQueryContentEvent(aEvent)) {
        break;
      }
      ContentEventHandler handler(mPresContext);
      handler.OnQueryEditorRect(aEvent->AsQueryContentEvent());
    }
    break;
  case NS_QUERY_CONTENT_STATE:
    {
      // XXX remote event
      ContentEventHandler handler(mPresContext);
      handler.OnQueryContentState(aEvent->AsQueryContentEvent());
    }
    break;
  case NS_QUERY_SELECTION_AS_TRANSFERABLE:
    {
      // XXX remote event
      ContentEventHandler handler(mPresContext);
      handler.OnQuerySelectionAsTransferable(aEvent->AsQueryContentEvent());
    }
    break;
  case NS_QUERY_CHARACTER_AT_POINT:
    {
      // XXX remote event
      ContentEventHandler handler(mPresContext);
      handler.OnQueryCharacterAtPoint(aEvent->AsQueryContentEvent());
    }
    break;
  case NS_QUERY_DOM_WIDGET_HITTEST:
    {
      // XXX remote event
      ContentEventHandler handler(mPresContext);
      handler.OnQueryDOMWidgetHittest(aEvent->AsQueryContentEvent());
    }
    break;
  case NS_SELECTION_SET:
    IMEStateManager::HandleSelectionEvent(aPresContext, GetFocusedContent(),
                                          aEvent->AsSelectionEvent());
    break;
  case NS_CONTENT_COMMAND_CUT:
  case NS_CONTENT_COMMAND_COPY:
  case NS_CONTENT_COMMAND_PASTE:
  case NS_CONTENT_COMMAND_DELETE:
  case NS_CONTENT_COMMAND_UNDO:
  case NS_CONTENT_COMMAND_REDO:
  case NS_CONTENT_COMMAND_PASTE_TRANSFERABLE:
    {
      DoContentCommandEvent(aEvent->AsContentCommandEvent());
    }
    break;
  case NS_CONTENT_COMMAND_SCROLL:
    {
      DoContentCommandScrollEvent(aEvent->AsContentCommandEvent());
    }
    break;
  case NS_COMPOSITION_START:
    if (aEvent->mFlags.mIsTrusted) {
      // If the event is trusted event, set the selected text to data of
      // composition event.
      WidgetCompositionEvent* compositionEvent = aEvent->AsCompositionEvent();
      WidgetQueryContentEvent selectedText(true, NS_QUERY_SELECTED_TEXT,
                                           compositionEvent->widget);
      DoQuerySelectedText(&selectedText);
      NS_ASSERTION(selectedText.mSucceeded, "Failed to get selected text");
      compositionEvent->mData = selectedText.mReply.mString;
    }
    break;
  default:
    break;
  }
  return NS_OK;
}

// static
int32_t
EventStateManager::GetAccessModifierMaskFor(nsISupports* aDocShell)
{
  nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(aDocShell));
  if (!treeItem)
    return -1; // invalid modifier

  switch (treeItem->ItemType()) {
  case nsIDocShellTreeItem::typeChrome:
    return Prefs::ChromeAccessModifierMask();

  case nsIDocShellTreeItem::typeContent:
    return Prefs::ContentAccessModifierMask();

  default:
    return -1; // invalid modifier
  }
}

static bool
IsAccessKeyTarget(nsIContent* aContent, nsIFrame* aFrame, nsAString& aKey)
{
  // Use GetAttr because we want Unicode case=insensitive matching
  // XXXbz shouldn't this be case-sensitive, per spec?
  nsString contentKey;
  if (!aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, contentKey) ||
      !contentKey.Equals(aKey, nsCaseInsensitiveStringComparator()))
    return false;

  nsCOMPtr<nsIDOMXULDocument> xulDoc =
    do_QueryInterface(aContent->OwnerDoc());
  if (!xulDoc && !aContent->IsXULElement())
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

  // HTML area, label and legend elements are never focusable, so
  // we need to check for them explicitly before giving up.
  if (aContent->IsAnyOfHTMLElements(nsGkAtoms::area,
                                    nsGkAtoms::label,
                                    nsGkAtoms::legend)) {
    return true;
  }

  // XUL label elements are never focusable, so we need to check for them
  // explicitly before giving up.
  if (aContent->IsXULElement(nsGkAtoms::label)) {
    return true;
  }

  return false;
}

bool
EventStateManager::ExecuteAccessKey(nsTArray<uint32_t>& aAccessCharCodes,
                                    bool aIsTrustedEvent)
{
  int32_t count, start = -1;
  nsIContent* focusedContent = GetFocusedContent();
  if (focusedContent) {
    start = mAccessKeys.IndexOf(focusedContent);
    if (start == -1 && focusedContent->GetBindingParent())
      start = mAccessKeys.IndexOf(focusedContent->GetBindingParent());
  }
  nsIContent *content;
  nsIFrame *frame;
  int32_t length = mAccessKeys.Count();
  for (uint32_t i = 0; i < aAccessCharCodes.Length(); ++i) {
    uint32_t ch = aAccessCharCodes[i];
    nsAutoString accessKey;
    AppendUCS4ToUTF16(ch, accessKey);
    for (count = 1; count <= length; ++count) {
      content = mAccessKeys[(start + count) % length];
      frame = content->GetPrimaryFrame();
      if (IsAccessKeyTarget(content, frame, accessKey)) {
        bool shouldActivate = Prefs::KeyCausesActivation();
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

// static
void
EventStateManager::GetAccessKeyLabelPrefix(Element* aElement, nsAString& aPrefix)
{
  aPrefix.Truncate();
  nsAutoString separator, modifierText;
  nsContentUtils::GetModifierSeparatorText(separator);

  nsCOMPtr<nsISupports> container = aElement->OwnerDoc()->GetDocShell();
  int32_t modifierMask = GetAccessModifierMaskFor(container);

  if (modifierMask == -1) {
    return;
  }

  if (modifierMask & NS_MODIFIER_CONTROL) {
    nsContentUtils::GetControlText(modifierText);
    aPrefix.Append(modifierText + separator);
  }
  if (modifierMask & NS_MODIFIER_META) {
    nsContentUtils::GetMetaText(modifierText);
    aPrefix.Append(modifierText + separator);
  }
  if (modifierMask & NS_MODIFIER_OS) {
    nsContentUtils::GetOSText(modifierText);
    aPrefix.Append(modifierText + separator);
  }
  if (modifierMask & NS_MODIFIER_ALT) {
    nsContentUtils::GetAltText(modifierText);
    aPrefix.Append(modifierText + separator);
  }
  if (modifierMask & NS_MODIFIER_SHIFT) {
    nsContentUtils::GetShiftText(modifierText);
    aPrefix.Append(modifierText + separator);
  }
}

void
EventStateManager::HandleAccessKey(nsPresContext* aPresContext,
                                   WidgetKeyboardEvent* aEvent,
                                   nsEventStatus* aStatus,
                                   nsIDocShellTreeItem* aBubbledFrom,
                                   ProcessingAccessKeyState aAccessKeyState,
                                   int32_t aModifierMask)
{
  nsCOMPtr<nsIDocShell> docShell = aPresContext->GetDocShell();

  // Alt or other accesskey modifier is down, we may need to do an accesskey
  if (mAccessKeys.Count() > 0 &&
      aModifierMask == GetAccessModifierMaskFor(docShell)) {
    // Someone registered an accesskey.  Find and activate it.
    nsAutoTArray<uint32_t, 10> accessCharCodes;
    nsContentUtils::GetAccessKeyCandidates(aEvent, accessCharCodes);
    if (ExecuteAccessKey(accessCharCodes, aEvent->mFlags.mIsTrusted)) {
      *aStatus = nsEventStatus_eConsumeNoDefault;
      return;
    }
  }

  // after the local accesskey handling
  if (nsEventStatus_eConsumeNoDefault != *aStatus) {
    // checking all sub docshells

    if (!docShell) {
      NS_WARNING("no docShellTreeNode for presContext");
      return;
    }

    int32_t childCount;
    docShell->GetChildCount(&childCount);
    for (int32_t counter = 0; counter < childCount; counter++) {
      // Not processing the child which bubbles up the handling
      nsCOMPtr<nsIDocShellTreeItem> subShellItem;
      docShell->GetChildAt(counter, getter_AddRefs(subShellItem));
      if (aAccessKeyState == eAccessKeyProcessingUp &&
          subShellItem == aBubbledFrom)
        continue;

      nsCOMPtr<nsIDocShell> subDS = do_QueryInterface(subShellItem);
      if (subDS && IsShellVisible(subDS)) {
        nsCOMPtr<nsIPresShell> subPS = subDS->GetPresShell();

        // Docshells need not have a presshell (eg. display:none
        // iframes, docshells in transition between documents, etc).
        if (!subPS) {
          // Oh, well.  Just move on to the next child
          continue;
        }

        nsPresContext *subPC = subPS->GetPresContext();

        EventStateManager* esm =
          static_cast<EventStateManager*>(subPC->EventStateManager());

        if (esm)
          esm->HandleAccessKey(subPC, aEvent, aStatus, nullptr,
                               eAccessKeyProcessingDown, aModifierMask);

        if (nsEventStatus_eConsumeNoDefault == *aStatus)
          break;
      }
    }
  }// if end . checking all sub docshell ends here.

  // bubble up the process to the parent docshell if necessary
  if (eAccessKeyProcessingDown != aAccessKeyState && nsEventStatus_eConsumeNoDefault != *aStatus) {
    if (!docShell) {
      NS_WARNING("no docShellTreeItem for presContext");
      return;
    }

    nsCOMPtr<nsIDocShellTreeItem> parentShellItem;
    docShell->GetParent(getter_AddRefs(parentShellItem));
    nsCOMPtr<nsIDocShell> parentDS = do_QueryInterface(parentShellItem);
    if (parentDS) {
      nsCOMPtr<nsIPresShell> parentPS = parentDS->GetPresShell();
      NS_ASSERTION(parentPS, "Our PresShell exists but the parent's does not?");

      nsPresContext *parentPC = parentPS->GetPresContext();
      NS_ASSERTION(parentPC, "PresShell without PresContext");

      EventStateManager* esm =
        static_cast<EventStateManager*>(parentPC->EventStateManager());

      if (esm)
        esm->HandleAccessKey(parentPC, aEvent, aStatus, docShell,
                             eAccessKeyProcessingUp, aModifierMask);
    }
  }// if end. bubble up process
}// end of HandleAccessKey

bool
EventStateManager::DispatchCrossProcessEvent(WidgetEvent* aEvent,
                                             nsFrameLoader* aFrameLoader,
                                             nsEventStatus *aStatus) {
  TabParent* remote = TabParent::GetFrom(aFrameLoader);
  if (!remote) {
    return false;
  }

  switch (aEvent->mClass) {
  case eMouseEventClass: {
    return remote->SendRealMouseEvent(*aEvent->AsMouseEvent());
  }
  case eKeyboardEventClass: {
    return remote->SendRealKeyEvent(*aEvent->AsKeyboardEvent());
  }
  case eWheelEventClass: {
    return remote->SendMouseWheelEvent(*aEvent->AsWheelEvent());
  }
  case eTouchEventClass: {
    // Let the child process synthesize a mouse event if needed, and
    // ensure we don't synthesize one in this process.
    *aStatus = nsEventStatus_eConsumeNoDefault;
    return remote->SendRealTouchEvent(*aEvent->AsTouchEvent());
  }
  case eDragEventClass: {
    if (remote->Manager()->IsContentParent()) {
      remote->Manager()->AsContentParent()->MaybeInvokeDragSession(remote);
    }

    nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
    uint32_t dropEffect = nsIDragService::DRAGDROP_ACTION_NONE;
    uint32_t action = nsIDragService::DRAGDROP_ACTION_NONE;
    if (dragSession) {
      dragSession->DragEventDispatchedToChildProcess();
      dragSession->GetDragAction(&action);
      nsCOMPtr<nsIDOMDataTransfer> initialDataTransfer;
      dragSession->GetDataTransfer(getter_AddRefs(initialDataTransfer));
      if (initialDataTransfer) {
        initialDataTransfer->GetDropEffectInt(&dropEffect);
      }
    }

    bool retval = remote->SendRealDragEvent(*aEvent->AsDragEvent(),
                                            action, dropEffect);

    return retval;
  }
  default: {
    MOZ_CRASH("Attempt to send non-whitelisted event?");
  }
  }
}

bool
EventStateManager::IsRemoteTarget(nsIContent* target) {
  if (!target) {
    return false;
  }

  // <browser/iframe remote=true> from XUL
  if (target->IsAnyOfXULElements(nsGkAtoms::browser, nsGkAtoms::iframe) &&
      target->AttrValueIs(kNameSpaceID_None, nsGkAtoms::Remote,
                          nsGkAtoms::_true, eIgnoreCase)) {
    return true;
  }

  // <frame/iframe mozbrowser/mozapp>
  nsCOMPtr<nsIMozBrowserFrame> browserFrame = do_QueryInterface(target);
  if (browserFrame && browserFrame->GetReallyIsBrowserOrApp()) {
    return !!TabParent::GetFrom(target);
  }

  return false;
}

bool
CrossProcessSafeEvent(const WidgetEvent& aEvent)
{
  switch (aEvent.mClass) {
  case eKeyboardEventClass:
  case eWheelEventClass:
    return true;
  case eMouseEventClass:
    switch (aEvent.mMessage) {
    case eMouseDown:
    case eMouseUp:
    case eMouseMove:
    case eContextMenu:
    case eMouseEnterIntoWidget:
    case eMouseExitFromWidget:
      return true;
    default:
      return false;
    }
  case eTouchEventClass:
    switch (aEvent.mMessage) {
    case NS_TOUCH_START:
    case NS_TOUCH_MOVE:
    case NS_TOUCH_END:
    case NS_TOUCH_CANCEL:
      return true;
    default:
      return false;
    }
  case eDragEventClass:
    switch (aEvent.mMessage) {
    case eDragOver:
    case NS_DRAGDROP_EXIT:
    case eDrop:
      return true;
    default:
      break;
    }
  default:
    return false;
  }
}

bool
EventStateManager::HandleCrossProcessEvent(WidgetEvent* aEvent,
                                           nsEventStatus *aStatus) {
  if (*aStatus == nsEventStatus_eConsumeNoDefault ||
      aEvent->mFlags.mNoCrossProcessBoundaryForwarding ||
      !CrossProcessSafeEvent(*aEvent)) {
    return false;
  }

  // Collect the remote event targets we're going to forward this
  // event to.
  //
  // NB: the elements of |targets| must be unique, for correctness.
  nsAutoTArray<nsCOMPtr<nsIContent>, 1> targets;
  if (aEvent->mClass != eTouchEventClass ||
      aEvent->mMessage == NS_TOUCH_START) {
    // If this event only has one target, and it's remote, add it to
    // the array.
    nsIFrame* frame = GetEventTarget();
    nsIContent* target = frame ? frame->GetContent() : nullptr;
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
    const WidgetTouchEvent::TouchArray& touches =
      aEvent->AsTouchEvent()->touches;
    for (uint32_t i = 0; i < touches.Length(); ++i) {
      Touch* touch = touches[i];
      // NB: the |mChanged| check is an optimization, subprocesses can
      // compute this for themselves.  If the touch hasn't changed, we
      // may be able to avoid forwarding the event entirely (which is
      // not free).
      if (!touch || !touch->mChanged) {
        continue;
      }
      nsCOMPtr<EventTarget> targetPtr = touch->mTarget;
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
  for (uint32_t i = 0; i < targets.Length(); ++i) {
    nsIContent* target = targets[i];
    nsCOMPtr<nsIFrameLoaderOwner> loaderOwner = do_QueryInterface(target);
    if (!loaderOwner) {
      continue;
    }

    nsRefPtr<nsFrameLoader> frameLoader = loaderOwner->GetFrameLoader();
    if (!frameLoader) {
      continue;
    }

    uint32_t eventMode;
    frameLoader->GetEventMode(&eventMode);
    if (eventMode == nsIFrameLoader::EVENT_MODE_DONT_FORWARD_TO_CHILD) {
      continue;
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
EventStateManager::CreateClickHoldTimer(nsPresContext* inPresContext,
                                        nsIFrame* inDownFrame,
                                        WidgetGUIEvent* inMouseDownEvent)
{
  if (!inMouseDownEvent->mFlags.mIsTrusted ||
      IsRemoteTarget(mGestureDownContent) ||
      sIsPointerLocked) {
    return;
  }

  // just to be anal (er, safe)
  if (mClickHoldTimer) {
    mClickHoldTimer->Cancel();
    mClickHoldTimer = nullptr;
  }

  // if content clicked on has a popup, don't even start the timer
  // since we'll end up conflicting and both will show.
  if (mGestureDownContent) {
    // check for the |popup| attribute
    if (nsContentUtils::HasNonEmptyAttr(mGestureDownContent, kNameSpaceID_None,
                                        nsGkAtoms::popup))
      return;
    
    // check for a <menubutton> like bookmarks
    if (mGestureDownContent->IsXULElement(nsGkAtoms::menubutton))
      return;
  }

  mClickHoldTimer = do_CreateInstance("@mozilla.org/timer;1");
  if (mClickHoldTimer) {
    int32_t clickHoldDelay =
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
EventStateManager::KillClickHoldTimer()
{
  if (mClickHoldTimer) {
    mClickHoldTimer->Cancel();
    mClickHoldTimer = nullptr;
  }
}


//
// sClickHoldCallback
//
// This fires after the mouse has been down for a certain length of time.
//
void
EventStateManager::sClickHoldCallback(nsITimer* aTimer, void* aESM)
{
  nsRefPtr<EventStateManager> self = static_cast<EventStateManager*>(aESM);
  if (self) {
    self->FireContextClick();
  }

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
EventStateManager::FireContextClick()
{
  if (!mGestureDownContent || !mPresContext || sIsPointerLocked) {
    return;
  }

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
  // make sure the widget sticks around
  nsCOMPtr<nsIWidget> targetWidget;
  if (mCurrentTarget && (targetWidget = mCurrentTarget->GetNearestWidget())) {
    NS_ASSERTION(mPresContext == mCurrentTarget->PresContext(),
                 "a prescontext returned a primary frame that didn't belong to it?");

    // before dispatching, check that we're not on something that
    // doesn't get a context menu
    bool allowedToDispatch = true;

    if (mGestureDownContent->IsAnyOfXULElements(nsGkAtoms::scrollbar,
                                                nsGkAtoms::scrollbarbutton,
                                                nsGkAtoms::button)) {
      allowedToDispatch = false;
    } else if (mGestureDownContent->IsXULElement(nsGkAtoms::toolbarbutton)) {
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
    else if (mGestureDownContent->IsHTMLElement()) {
      nsCOMPtr<nsIFormControl> formCtrl(do_QueryInterface(mGestureDownContent));

      if (formCtrl) {
        allowedToDispatch = formCtrl->IsTextControl(false) ||
                            formCtrl->GetType() == NS_FORM_INPUT_FILE;
      }
      else if (mGestureDownContent->IsAnyOfHTMLElements(nsGkAtoms::applet,
                                                        nsGkAtoms::embed,
                                                        nsGkAtoms::object)) {
        allowedToDispatch = false;
      }
    }

    if (allowedToDispatch) {
      // init the event while mCurrentTarget is still good
      WidgetMouseEvent event(true, eContextMenu, targetWidget,
                             WidgetMouseEvent::eReal);
      event.clickCount = 1;
      FillInEventFromGestureDown(&event);
        
      // stop selection tracking, we're in control now
      if (mCurrentTarget)
      {
        nsRefPtr<nsFrameSelection> frameSel =
          mCurrentTarget->GetFrameSelection();
        
        if (frameSel && frameSel->GetDragState()) {
          // note that this can cause selection changed events to fire if we're in
          // a text field, which will null out mCurrentTarget
          frameSel->SetDragState(false);
        }
      }

      nsIDocument* doc = mGestureDownContent->GetCrossShadowCurrentDoc();
      AutoHandlingUserInputStatePusher userInpStatePusher(true, &event, doc);

      // dispatch to DOM
      EventDispatcher::Dispatch(mGestureDownContent, mPresContext, &event,
                                nullptr, &status);

      // We don't need to dispatch to frame handling because no frames
      // watch eContextMenu except for nsMenuFrame and that's only for
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
EventStateManager::BeginTrackingDragGesture(nsPresContext* aPresContext,
                                            WidgetMouseEvent* inDownEvent,
                                            nsIFrame* inDownFrame)
{
  if (!inDownEvent->widget)
    return;

  // Note that |inDownEvent| could be either a mouse down event or a
  // synthesized mouse move event.
  mGestureDownPoint = inDownEvent->refPoint + inDownEvent->widget->WidgetToScreenOffset();

  if (inDownFrame) {
    inDownFrame->GetContentForEvent(inDownEvent,
                                    getter_AddRefs(mGestureDownContent));

    mGestureDownFrameOwner = inDownFrame->GetContent();
    if (!mGestureDownFrameOwner) {
      mGestureDownFrameOwner = mGestureDownContent;
    }
  }
  mGestureModifiers = inDownEvent->modifiers;
  mGestureDownButtons = inDownEvent->buttons;

  if (Prefs::ClickHoldContextMenu()) {
    // fire off a timer to track click-hold
    CreateClickHoldTimer(aPresContext, inDownFrame, inDownEvent);
  }
}

void
EventStateManager::BeginTrackingRemoteDragGesture(nsIContent* aContent)
{
  mGestureDownContent = aContent;
  mGestureDownFrameOwner = aContent;
}

//
// StopTrackingDragGesture
//
// Record that the mouse has gone back up so that we should leave the TRACKING
// state of d&d gesture tracker and return to the START state.
//
void
EventStateManager::StopTrackingDragGesture()
{
  mGestureDownContent = nullptr;
  mGestureDownFrameOwner = nullptr;
}

void
EventStateManager::FillInEventFromGestureDown(WidgetMouseEvent* aEvent)
{
  NS_ASSERTION(aEvent->widget == mCurrentTarget->GetNearestWidget(),
               "Incorrect widget in event");

  // Set the coordinates in the new event to the coordinates of
  // the old event, adjusted for the fact that the widget might be
  // different
  aEvent->refPoint = mGestureDownPoint - aEvent->widget->WidgetToScreenOffset();
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
EventStateManager::GenerateDragGesture(nsPresContext* aPresContext,
                                       WidgetMouseEvent* aEvent)
{
  NS_ASSERTION(aPresContext, "This shouldn't happen.");
  if (IsTrackingDragGesture()) {
    mCurrentTarget = mGestureDownFrameOwner->GetPrimaryFrame();

    if (!mCurrentTarget || !mCurrentTarget->GetNearestWidget()) {
      StopTrackingDragGesture();
      return;
    }

    // Check if selection is tracking drag gestures, if so
    // don't interfere!
    if (mCurrentTarget)
    {
      nsRefPtr<nsFrameSelection> frameSel = mCurrentTarget->GetFrameSelection();
      if (frameSel && frameSel->GetDragState()) {
        StopTrackingDragGesture();
        return;
      }
    }

    // If non-native code is capturing the mouse don't start a drag.
    if (nsIPresShell::IsMouseCapturePreventingDrag()) {
      StopTrackingDragGesture();
      return;
    }

    static int32_t pixelThresholdX = 0;
    static int32_t pixelThresholdY = 0;

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
    LayoutDeviceIntPoint pt = aEvent->refPoint + aEvent->widget->WidgetToScreenOffset();
    LayoutDeviceIntPoint distance = pt - mGestureDownPoint;
    if (Abs(distance.x) > AssertedCast<uint32_t>(pixelThresholdX) ||
        Abs(distance.y) > AssertedCast<uint32_t>(pixelThresholdY)) {
      if (Prefs::ClickHoldContextMenu()) {
        // stop the click-hold before we fire off the drag gesture, in case
        // it takes a long time
        KillClickHoldTimer();
      }

      nsCOMPtr<nsISupports> container = aPresContext->GetContainerWeak();
      nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(container);
      if (!window)
        return;

      nsRefPtr<DataTransfer> dataTransfer =
        new DataTransfer(window, eDragStart, false, -1);

      nsCOMPtr<nsISelection> selection;
      nsCOMPtr<nsIContent> eventContent, targetContent;
      mCurrentTarget->GetContentForEvent(aEvent, getter_AddRefs(eventContent));
      if (eventContent)
        DetermineDragTargetAndDefaultData(window, eventContent, dataTransfer,
                                          getter_AddRefs(selection),
                                          getter_AddRefs(targetContent));

      // Stop tracking the drag gesture now. This should stop us from
      // reentering GenerateDragGesture inside DOM event processing.
      StopTrackingDragGesture();

      if (!targetContent)
        return;

      // Use our targetContent, now that we've determined it, as the
      // parent object of the DataTransfer.
      dataTransfer->SetParentObject(targetContent);

      sLastDragOverFrame = nullptr;
      nsCOMPtr<nsIWidget> widget = mCurrentTarget->GetNearestWidget();

      // get the widget from the target frame
      WidgetDragEvent startEvent(aEvent->mFlags.mIsTrusted,
                                 eDragStart, widget);
      FillInEventFromGestureDown(&startEvent);

      WidgetDragEvent gestureEvent(aEvent->mFlags.mIsTrusted,
                                   NS_DRAGDROP_GESTURE, widget);
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
      EventDispatcher::Dispatch(targetContent, aPresContext, &startEvent,
                                nullptr, &status);

      WidgetDragEvent* event = &startEvent;
      if (status != nsEventStatus_eConsumeNoDefault) {
        status = nsEventStatus_eIgnore;
        EventDispatcher::Dispatch(targetContent, aPresContext, &gestureEvent,
                                  nullptr, &status);
        event = &gestureEvent;
      }

      nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
      // Emit observer event to allow addons to modify the DataTransfer object.
      if (observerService) {
        observerService->NotifyObservers(dataTransfer,
                                         "on-datatransfer-available",
                                         nullptr);
      }

      // now that the dataTransfer has been updated in the dragstart and
      // draggesture events, make it read only so that the data doesn't
      // change during the drag.
      dataTransfer->SetReadOnly();

      if (status != nsEventStatus_eConsumeNoDefault) {
        bool dragStarted = DoDefaultDragStart(aPresContext, event, dataTransfer,
                                              targetContent, selection);
        if (dragStarted) {
          sActiveESM = nullptr;
          aEvent->mFlags.mPropagationStopped = true;
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
EventStateManager::DetermineDragTargetAndDefaultData(nsPIDOMWindow* aWindow,
                                                     nsIContent* aSelectionTarget,
                                                     DataTransfer* aDataTransfer,
                                                     nsISelection** aSelection,
                                                     nsIContent** aTargetNode)
{
  *aTargetNode = nullptr;

  // GetDragData determines if a selection, link or image in the content
  // should be dragged, and places the data associated with the drag in the
  // data transfer.
  // mGestureDownContent is the node where the mousedown event for the drag
  // occurred, and aSelectionTarget is the node to use when a selection is used
  bool canDrag;
  nsCOMPtr<nsIContent> dragDataNode;
  bool wasAlt = (mGestureModifiers & MODIFIER_ALT) != 0;
  nsresult rv = nsContentAreaDragDrop::GetDragData(aWindow, mGestureDownContent,
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
EventStateManager::DoDefaultDragStart(nsPresContext* aPresContext,
                                      WidgetDragEvent* aDragEvent,
                                      DataTransfer* aDataTransfer,
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
  uint32_t count = 0;
  if (aDataTransfer)
    aDataTransfer->GetMozItemCount(&count);
  if (!count)
    return false;

  // Get the target being dragged, which may not be the same as the
  // target of the mouse event. If one wasn't set in the
  // aDataTransfer during the event handler, just use the original
  // target instead.
  nsCOMPtr<nsIContent> dragTarget = aDataTransfer->GetDragTarget();
  if (!dragTarget) {
    dragTarget = aDragTarget;
    if (!dragTarget)
      return false;
  }

  // check which drag effect should initially be used. If the effect was not
  // set, just use all actions, otherwise Windows won't allow a drop.
  uint32_t action;
  aDataTransfer->GetEffectAllowedInt(&action);
  if (action == nsIDragService::DRAGDROP_ACTION_UNINITIALIZED)
    action = nsIDragService::DRAGDROP_ACTION_COPY |
             nsIDragService::DRAGDROP_ACTION_MOVE |
             nsIDragService::DRAGDROP_ACTION_LINK;

  // get any custom drag image that was set
  int32_t imageX, imageY;
  Element* dragImage = aDataTransfer->GetDragImage(&imageX, &imageY);

  nsCOMPtr<nsISupportsArray> transArray =
    aDataTransfer->GetTransferables(dragTarget->AsDOMNode());
  if (!transArray)
    return false;

  // XXXndeakin don't really want to create a new drag DOM event
  // here, but we need something to pass to the InvokeDragSession
  // methods.
  nsRefPtr<DragEvent> event =
    NS_NewDOMDragEvent(dragTarget, aPresContext, aDragEvent);

  // Use InvokeDragSessionWithSelection if a selection is being dragged,
  // such that the image can be generated from the selected text. However,
  // use InvokeDragSessionWithImage if a custom image was set or something
  // other than a selection is being dragged.
  if (!dragImage && aSelection) {
    dragService->InvokeDragSessionWithSelection(aSelection, transArray,
                                                action, event, aDataTransfer);
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
      if (dragTarget->NodeInfo()->Equals(nsGkAtoms::treechildren,
                                         kNameSpaceID_XUL)) {
        nsTreeBodyFrame* treeBody =
          do_QueryFrame(dragTarget->GetPrimaryFrame());
        if (treeBody) {
          treeBody->GetSelectionRegion(getter_AddRefs(region));
        }
      }
    }
#endif

    dragService->InvokeDragSessionWithImage(dragTarget->AsDOMNode(), transArray,
                                            region, action,
                                            dragImage ? dragImage->AsDOMNode() :
                                                        nullptr,
                                            imageX, imageY, event,
                                            aDataTransfer);
  }

  return true;
}

nsresult
EventStateManager::GetContentViewer(nsIContentViewer** aCv)
{
  *aCv = nullptr;

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

  nsCOMPtr<nsIDocShell> docshell(presContext->GetDocShell());
  if(!docshell) return NS_ERROR_FAILURE;

  docshell->GetContentViewer(aCv);
  if(!*aCv) return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult
EventStateManager::ChangeTextSize(int32_t change)
{
  nsCOMPtr<nsIContentViewer> cv;
  nsresult rv = GetContentViewer(getter_AddRefs(cv));
  NS_ENSURE_SUCCESS(rv, rv);

  float textzoom;
  float zoomMin = ((float)Preferences::GetInt("zoom.minPercent", 50)) / 100;
  float zoomMax = ((float)Preferences::GetInt("zoom.maxPercent", 300)) / 100;
  cv->GetTextZoom(&textzoom);
  textzoom += ((float)change) / 10;
  if (textzoom < zoomMin)
    textzoom = zoomMin;
  else if (textzoom > zoomMax)
    textzoom = zoomMax;
  cv->SetTextZoom(textzoom);

  return NS_OK;
}

nsresult
EventStateManager::ChangeFullZoom(int32_t change)
{
  nsCOMPtr<nsIContentViewer> cv;
  nsresult rv = GetContentViewer(getter_AddRefs(cv));
  NS_ENSURE_SUCCESS(rv, rv);

  float fullzoom;
  float zoomMin = ((float)Preferences::GetInt("zoom.minPercent", 50)) / 100;
  float zoomMax = ((float)Preferences::GetInt("zoom.maxPercent", 300)) / 100;
  cv->GetFullZoom(&fullzoom);
  fullzoom += ((float)change) / 10;
  if (fullzoom < zoomMin)
    fullzoom = zoomMin;
  else if (fullzoom > zoomMax)
    fullzoom = zoomMax;
  cv->SetFullZoom(fullzoom);

  return NS_OK;
}

void
EventStateManager::DoScrollHistory(int32_t direction)
{
  nsCOMPtr<nsISupports> pcContainer(mPresContext->GetContainerWeak());
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
EventStateManager::DoScrollZoom(nsIFrame* aTargetFrame,
                                int32_t adjustment)
{
  // Exclude form controls and content in chrome docshells.
  nsIContent *content = aTargetFrame->GetContent();
  if (content &&
      !content->IsNodeOfType(nsINode::eHTML_FORM_CONTROL) &&
      !nsContentUtils::IsInChromeDocshell(content->OwnerDoc()))
    {
      // positive adjustment to decrease zoom, negative to increase
      int32_t change = (adjustment > 0) ? -1 : 1;

      if (Preferences::GetBool("browser.zoom.full") || content->OwnerDoc()->IsSyntheticDocument()) {
        ChangeFullZoom(change);
      } else {
        ChangeTextSize(change);
      }
      EnsureDocument(mPresContext);
      nsContentUtils::DispatchChromeEvent(mDocument, static_cast<nsIDocument*>(mDocument),
                                          NS_LITERAL_STRING("ZoomChangeUsingMouseWheel"),
                                          true, true);
    }
}

static nsIFrame*
GetParentFrameToScroll(nsIFrame* aFrame)
{
  if (!aFrame)
    return nullptr;

  if (aFrame->StyleDisplay()->mPosition == NS_STYLE_POSITION_FIXED &&
      nsLayoutUtils::IsReallyFixedPos(aFrame))
    return aFrame->PresContext()->GetPresShell()->GetRootScrollFrame();

  return aFrame->GetParent();
}

/*static*/ bool
EventStateManager::CanVerticallyScrollFrameWithWheel(nsIFrame* aFrame)
{
  nsIContent* c = aFrame->GetContent();
  if (!c) {
    return true;
  }
  nsCOMPtr<nsITextControlElement> ctrl =
    do_QueryInterface(c->IsInAnonymousSubtree() ? c->GetBindingParent() : c);
  if (ctrl && ctrl->IsSingleLineTextControl()) {
    return false;
  }
  return true;
}

void
EventStateManager::DispatchLegacyMouseScrollEvents(nsIFrame* aTargetFrame,
                                                   WidgetWheelEvent* aEvent,
                                                   nsEventStatus* aStatus)
{
  MOZ_ASSERT(aEvent);
  MOZ_ASSERT(aStatus);

  if (!aTargetFrame || *aStatus == nsEventStatus_eConsumeNoDefault) {
    return;
  }

  // Ignore mouse wheel transaction for computing legacy mouse wheel
  // events' delta value.
  nsIScrollableFrame* scrollTarget =
    ComputeScrollTarget(aTargetFrame, aEvent,
                        COMPUTE_LEGACY_MOUSE_SCROLL_EVENT_TARGET);

  nsIFrame* scrollFrame = do_QueryFrame(scrollTarget);
  nsPresContext* pc =
    scrollFrame ? scrollFrame->PresContext() : aTargetFrame->PresContext();

  // DOM event's delta vales are computed from CSS pixels.
  nsSize scrollAmount = GetScrollAmount(pc, aEvent, scrollTarget);
  nsIntSize scrollAmountInCSSPixels(
    nsPresContext::AppUnitsToIntCSSPixels(scrollAmount.width),
    nsPresContext::AppUnitsToIntCSSPixels(scrollAmount.height));

  // XXX We don't deal with fractional amount in legacy event, though the
  //     default action handler (DoScrollText()) deals with it.
  //     If we implemented such strict computation, we would need additional
  //     accumulated delta values. It would made the code more complicated.
  //     And also it would computes different delta values from older version.
  //     It doesn't make sense to implement such code for legacy events and
  //     rare cases.
  int32_t scrollDeltaX, scrollDeltaY, pixelDeltaX, pixelDeltaY;
  switch (aEvent->deltaMode) {
    case nsIDOMWheelEvent::DOM_DELTA_PAGE:
      scrollDeltaX =
        !aEvent->lineOrPageDeltaX ? 0 :
          (aEvent->lineOrPageDeltaX > 0  ? nsIDOMUIEvent::SCROLL_PAGE_DOWN :
                                           nsIDOMUIEvent::SCROLL_PAGE_UP);
      scrollDeltaY =
        !aEvent->lineOrPageDeltaY ? 0 :
          (aEvent->lineOrPageDeltaY > 0  ? nsIDOMUIEvent::SCROLL_PAGE_DOWN :
                                           nsIDOMUIEvent::SCROLL_PAGE_UP);
      pixelDeltaX = RoundDown(aEvent->deltaX * scrollAmountInCSSPixels.width);
      pixelDeltaY = RoundDown(aEvent->deltaY * scrollAmountInCSSPixels.height);
      break;

    case nsIDOMWheelEvent::DOM_DELTA_LINE:
      scrollDeltaX = aEvent->lineOrPageDeltaX;
      scrollDeltaY = aEvent->lineOrPageDeltaY;
      pixelDeltaX = RoundDown(aEvent->deltaX * scrollAmountInCSSPixels.width);
      pixelDeltaY = RoundDown(aEvent->deltaY * scrollAmountInCSSPixels.height);
      break;

    case nsIDOMWheelEvent::DOM_DELTA_PIXEL:
      scrollDeltaX = aEvent->lineOrPageDeltaX;
      scrollDeltaY = aEvent->lineOrPageDeltaY;
      pixelDeltaX = RoundDown(aEvent->deltaX);
      pixelDeltaY = RoundDown(aEvent->deltaY);
      break;

    default:
      MOZ_CRASH("Invalid deltaMode value comes");
  }

  // Send the legacy events in following order:
  // 1. Vertical scroll
  // 2. Vertical pixel scroll (even if #1 isn't consumed)
  // 3. Horizontal scroll (even if #1 and/or #2 are consumed)
  // 4. Horizontal pixel scroll (even if #3 isn't consumed)

  nsWeakFrame targetFrame(aTargetFrame);

  MOZ_ASSERT(*aStatus != nsEventStatus_eConsumeNoDefault &&
             !aEvent->mFlags.mDefaultPrevented,
             "If you make legacy events dispatched for default prevented wheel "
             "event, you need to initialize stateX and stateY");
  EventState stateX, stateY;
  if (scrollDeltaY) {
    SendLineScrollEvent(aTargetFrame, aEvent, stateY,
                        scrollDeltaY, DELTA_DIRECTION_Y);
    if (!targetFrame.IsAlive()) {
      *aStatus = nsEventStatus_eConsumeNoDefault;
      return;
    }
  }

  if (pixelDeltaY) {
    SendPixelScrollEvent(aTargetFrame, aEvent, stateY,
                         pixelDeltaY, DELTA_DIRECTION_Y);
    if (!targetFrame.IsAlive()) {
      *aStatus = nsEventStatus_eConsumeNoDefault;
      return;
    }
  }

  if (scrollDeltaX) {
    SendLineScrollEvent(aTargetFrame, aEvent, stateX,
                        scrollDeltaX, DELTA_DIRECTION_X);
    if (!targetFrame.IsAlive()) {
      *aStatus = nsEventStatus_eConsumeNoDefault;
      return;
    }
  }

  if (pixelDeltaX) {
    SendPixelScrollEvent(aTargetFrame, aEvent, stateX,
                         pixelDeltaX, DELTA_DIRECTION_X);
    if (!targetFrame.IsAlive()) {
      *aStatus = nsEventStatus_eConsumeNoDefault;
      return;
    }
  }

  if (stateY.mDefaultPrevented || stateX.mDefaultPrevented) {
    *aStatus = nsEventStatus_eConsumeNoDefault;
    aEvent->mFlags.mDefaultPrevented = true;
    aEvent->mFlags.mDefaultPreventedByContent |=
      stateY.mDefaultPreventedByContent || stateX.mDefaultPreventedByContent;
  }
}

void
EventStateManager::SendLineScrollEvent(nsIFrame* aTargetFrame,
                                       WidgetWheelEvent* aEvent,
                                       EventState& aState,
                                       int32_t aDelta,
                                       DeltaDirection aDeltaDirection)
{
  nsCOMPtr<nsIContent> targetContent = aTargetFrame->GetContent();
  if (!targetContent)
    targetContent = GetFocusedContent();
  if (!targetContent)
    return;

  while (targetContent->IsNodeOfType(nsINode::eTEXT)) {
    targetContent = targetContent->GetParent();
  }

  WidgetMouseScrollEvent event(aEvent->mFlags.mIsTrusted, NS_MOUSE_SCROLL,
                               aEvent->widget);
  event.mFlags.mDefaultPrevented = aState.mDefaultPrevented;
  event.mFlags.mDefaultPreventedByContent = aState.mDefaultPreventedByContent;
  event.refPoint = aEvent->refPoint;
  event.widget = aEvent->widget;
  event.time = aEvent->time;
  event.timeStamp = aEvent->timeStamp;
  event.modifiers = aEvent->modifiers;
  event.buttons = aEvent->buttons;
  event.isHorizontal = (aDeltaDirection == DELTA_DIRECTION_X);
  event.delta = aDelta;
  event.inputSource = aEvent->inputSource;

  nsEventStatus status = nsEventStatus_eIgnore;
  EventDispatcher::Dispatch(targetContent, aTargetFrame->PresContext(),
                            &event, nullptr, &status);
  aState.mDefaultPrevented =
    event.mFlags.mDefaultPrevented || status == nsEventStatus_eConsumeNoDefault;
  aState.mDefaultPreventedByContent = event.mFlags.mDefaultPreventedByContent;
}

void
EventStateManager::SendPixelScrollEvent(nsIFrame* aTargetFrame,
                                        WidgetWheelEvent* aEvent,
                                        EventState& aState,
                                        int32_t aPixelDelta,
                                        DeltaDirection aDeltaDirection)
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

  WidgetMouseScrollEvent event(aEvent->mFlags.mIsTrusted, NS_MOUSE_PIXEL_SCROLL,
                               aEvent->widget);
  event.mFlags.mDefaultPrevented = aState.mDefaultPrevented;
  event.mFlags.mDefaultPreventedByContent = aState.mDefaultPreventedByContent;
  event.refPoint = aEvent->refPoint;
  event.widget = aEvent->widget;
  event.time = aEvent->time;
  event.timeStamp = aEvent->timeStamp;
  event.modifiers = aEvent->modifiers;
  event.buttons = aEvent->buttons;
  event.isHorizontal = (aDeltaDirection == DELTA_DIRECTION_X);
  event.delta = aPixelDelta;
  event.inputSource = aEvent->inputSource;

  nsEventStatus status = nsEventStatus_eIgnore;
  EventDispatcher::Dispatch(targetContent, aTargetFrame->PresContext(),
                            &event, nullptr, &status);
  aState.mDefaultPrevented =
    event.mFlags.mDefaultPrevented || status == nsEventStatus_eConsumeNoDefault;
  aState.mDefaultPreventedByContent = event.mFlags.mDefaultPreventedByContent;
}

nsIScrollableFrame*
EventStateManager::ComputeScrollTarget(nsIFrame* aTargetFrame,
                                       WidgetWheelEvent* aEvent,
                                       ComputeScrollTargetOptions aOptions)
{
  return ComputeScrollTarget(aTargetFrame, aEvent->deltaX, aEvent->deltaY,
                             aEvent, aOptions);
}

// Overload ComputeScrollTarget method to allow passing "test" dx and dy when looking
// for which scrollbarmediators to activate when two finger down on trackpad
// and before any actual motion
nsIScrollableFrame*
EventStateManager::ComputeScrollTarget(nsIFrame* aTargetFrame,
                                       double aDirectionX,
                                       double aDirectionY,
                                       WidgetWheelEvent* aEvent,
                                       ComputeScrollTargetOptions aOptions)
{
  if (aOptions & PREFER_MOUSE_WHEEL_TRANSACTION) {
    // If the user recently scrolled with the mousewheel, then they probably
    // want to scroll the same view as before instead of the view under the
    // cursor.  WheelTransaction tracks the frame currently being
    // scrolled with the mousewheel. We consider the transaction ended when the
    // mouse moves more than "mousewheel.transaction.ignoremovedelay"
    // milliseconds after the last scroll operation, or any time the mouse moves
    // out of the frame, or when more than "mousewheel.transaction.timeout"
    // milliseconds have passed after the last operation, even if the mouse
    // hasn't moved.
    nsIFrame* lastScrollFrame = WheelTransaction::GetTargetFrame();
    if (lastScrollFrame) {
      nsIScrollableFrame* frameToScroll =
        lastScrollFrame->GetScrollTargetFrame();
      if (frameToScroll) {
        return frameToScroll;
      }
    }
  }

  // If the event doesn't cause scroll actually, we cannot find scroll target
  // because we check if the event can cause scroll actually on each found
  // scrollable frame.
  if (!aDirectionX && !aDirectionY) {
    return nullptr;
  }

  bool checkIfScrollableX =
    aDirectionX && (aOptions & PREFER_ACTUAL_SCROLLABLE_TARGET_ALONG_X_AXIS);
  bool checkIfScrollableY =
    aDirectionY && (aOptions & PREFER_ACTUAL_SCROLLABLE_TARGET_ALONG_Y_AXIS);

  nsIScrollableFrame* frameToScroll = nullptr;
  nsIFrame* scrollFrame =
    !(aOptions & START_FROM_PARENT) ? aTargetFrame :
                                      GetParentFrameToScroll(aTargetFrame);
  for (; scrollFrame; scrollFrame = GetParentFrameToScroll(scrollFrame)) {
    // Check whether the frame wants to provide us with a scrollable view.
    frameToScroll = scrollFrame->GetScrollTargetFrame();
    if (!frameToScroll) {
      continue;
    }

    // Don't scroll vertically by mouse-wheel on a single-line text control.
    if (checkIfScrollableY) {
      if (!CanVerticallyScrollFrameWithWheel(scrollFrame)) {
        continue;
      }
    }

    if (!checkIfScrollableX && !checkIfScrollableY) {
      return frameToScroll;
    }

    ScrollbarStyles ss = frameToScroll->GetScrollbarStyles();
    bool hiddenForV = (NS_STYLE_OVERFLOW_HIDDEN == ss.mVertical);
    bool hiddenForH = (NS_STYLE_OVERFLOW_HIDDEN == ss.mHorizontal);
    if ((hiddenForV && hiddenForH) ||
        (checkIfScrollableY && !checkIfScrollableX && hiddenForV) ||
        (checkIfScrollableX && !checkIfScrollableY && hiddenForH)) {
      continue;
    }

    // For default action, we should climb up the tree if cannot scroll it
    // by the event actually.
    bool canScroll =
      WheelHandlingUtils::CanScrollOn(frameToScroll, aDirectionX, aDirectionY);
    // Comboboxes need special care.
    nsIComboboxControlFrame* comboBox = do_QueryFrame(scrollFrame);
    if (comboBox) {
      if (comboBox->IsDroppedDown()) {
        // Don't propagate to parent when drop down menu is active.
        return canScroll ? frameToScroll : nullptr;
      }
      // Always propagate when not dropped down (even if focused).
      continue;
    }

    if (canScroll) {
      return frameToScroll;
    }
  }

  nsIFrame* newFrame = nsLayoutUtils::GetCrossDocParentFrame(
      aTargetFrame->PresContext()->FrameManager()->GetRootFrame());
  aOptions =
    static_cast<ComputeScrollTargetOptions>(aOptions & ~START_FROM_PARENT);
  return newFrame ? ComputeScrollTarget(newFrame, aEvent, aOptions) : nullptr;
}

nsSize
EventStateManager::GetScrollAmount(nsPresContext* aPresContext,
                                   WidgetWheelEvent* aEvent,
                                   nsIScrollableFrame* aScrollableFrame)
{
  MOZ_ASSERT(aPresContext);
  MOZ_ASSERT(aEvent);

  bool isPage = (aEvent->deltaMode == nsIDOMWheelEvent::DOM_DELTA_PAGE);
  if (aScrollableFrame) {
    return isPage ? aScrollableFrame->GetPageScrollAmount() :
                    aScrollableFrame->GetLineScrollAmount();
  }

  // If there is no scrollable frame and page scrolling, use view port size.
  if (isPage) {
    return aPresContext->GetVisibleArea().Size();
  }

  // If there is no scrollable frame, we should use root frame's information.
  nsIFrame* rootFrame = aPresContext->PresShell()->GetRootFrame();
  if (!rootFrame) {
    return nsSize(0, 0);
  }
  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(rootFrame, getter_AddRefs(fm),
    nsLayoutUtils::FontSizeInflationFor(rootFrame));
  NS_ENSURE_TRUE(fm, nsSize(0, 0));
  return nsSize(fm->AveCharWidth(), fm->MaxHeight());
}

void
EventStateManager::DoScrollText(nsIScrollableFrame* aScrollableFrame,
                                WidgetWheelEvent* aEvent)
{
  MOZ_ASSERT(aScrollableFrame);
  MOZ_ASSERT(aEvent);

  nsIFrame* scrollFrame = do_QueryFrame(aScrollableFrame);
  MOZ_ASSERT(scrollFrame);
  nsWeakFrame scrollFrameWeak(scrollFrame);

  nsIFrame* lastScrollFrame = WheelTransaction::GetTargetFrame();
  if (!lastScrollFrame) {
    WheelTransaction::BeginTransaction(scrollFrame, aEvent);
  } else if (lastScrollFrame != scrollFrame) {
    WheelTransaction::EndTransaction();
    WheelTransaction::BeginTransaction(scrollFrame, aEvent);
  } else {
    WheelTransaction::UpdateTransaction(aEvent);
  }

  // When the scroll event will not scroll any views, UpdateTransaction
  // fired MozMouseScrollFailed event which is for automated testing.
  // In the event handler, the target frame might be destroyed.  Then,
  // we should not try scrolling anything.
  if (!scrollFrameWeak.IsAlive()) {
    WheelTransaction::EndTransaction();
    return;
  }

  // Default action's actual scroll amount should be computed from device
  // pixels.
  nsPresContext* pc = scrollFrame->PresContext();
  nsSize scrollAmount = GetScrollAmount(pc, aEvent, aScrollableFrame);
  nsIntSize scrollAmountInDevPixels(
    pc->AppUnitsToDevPixels(scrollAmount.width),
    pc->AppUnitsToDevPixels(scrollAmount.height));
  nsIntPoint actualDevPixelScrollAmount =
    DeltaAccumulator::GetInstance()->
      ComputeScrollAmountForDefaultAction(aEvent, scrollAmountInDevPixels);

  // Don't scroll around the axis whose overflow style is hidden.
  ScrollbarStyles overflowStyle = aScrollableFrame->GetScrollbarStyles();
  if (overflowStyle.mHorizontal == NS_STYLE_OVERFLOW_HIDDEN) {
    actualDevPixelScrollAmount.x = 0;
  }
  if (overflowStyle.mVertical == NS_STYLE_OVERFLOW_HIDDEN) {
    actualDevPixelScrollAmount.y = 0;
  }

  nsIScrollbarMediator::ScrollSnapMode snapMode = nsIScrollbarMediator::DISABLE_SNAP;
  nsIAtom* origin = nullptr;
  switch (aEvent->deltaMode) {
    case nsIDOMWheelEvent::DOM_DELTA_LINE:
      origin = nsGkAtoms::mouseWheel;
      snapMode = nsIScrollableFrame::ENABLE_SNAP;
      break;
    case nsIDOMWheelEvent::DOM_DELTA_PAGE:
      origin = nsGkAtoms::pages;
      snapMode = nsIScrollableFrame::ENABLE_SNAP;
      break;
    case nsIDOMWheelEvent::DOM_DELTA_PIXEL:
      origin = nsGkAtoms::pixels;
      break;
    default:
      MOZ_CRASH("Invalid deltaMode value comes");
  }

  // We shouldn't scroll more one page at once except when over one page scroll
  // is allowed for the event.
  nsSize pageSize = aScrollableFrame->GetPageScrollAmount();
  nsIntSize devPixelPageSize(pc->AppUnitsToDevPixels(pageSize.width),
                             pc->AppUnitsToDevPixels(pageSize.height));
  if (!WheelPrefs::GetInstance()->IsOverOnePageScrollAllowedX(aEvent) &&
      DeprecatedAbs(actualDevPixelScrollAmount.x) > devPixelPageSize.width) {
    actualDevPixelScrollAmount.x =
      (actualDevPixelScrollAmount.x >= 0) ? devPixelPageSize.width :
                                            -devPixelPageSize.width;
  }

  if (!WheelPrefs::GetInstance()->IsOverOnePageScrollAllowedY(aEvent) &&
      DeprecatedAbs(actualDevPixelScrollAmount.y) > devPixelPageSize.height) {
    actualDevPixelScrollAmount.y =
      (actualDevPixelScrollAmount.y >= 0) ? devPixelPageSize.height :
                                            -devPixelPageSize.height;
  }

  bool isDeltaModePixel =
    (aEvent->deltaMode == nsIDOMWheelEvent::DOM_DELTA_PIXEL);

  nsIScrollableFrame::ScrollMode mode;
  switch (aEvent->scrollType) {
    case WidgetWheelEvent::SCROLL_DEFAULT:
      if (isDeltaModePixel) {
        mode = nsIScrollableFrame::NORMAL;
      } else {
        mode = nsIScrollableFrame::SMOOTH;
      }
      break;
    case WidgetWheelEvent::SCROLL_SYNCHRONOUSLY:
      mode = nsIScrollableFrame::INSTANT;
      break;
    case WidgetWheelEvent::SCROLL_ASYNCHRONOUSELY:
      mode = nsIScrollableFrame::NORMAL;
      break;
    case WidgetWheelEvent::SCROLL_SMOOTHLY:
      mode = nsIScrollableFrame::SMOOTH;
      break;
    default:
      MOZ_CRASH("Invalid scrollType value comes");
  }

  nsIScrollableFrame::ScrollMomentum momentum =
    aEvent->isMomentum ? nsIScrollableFrame::SYNTHESIZED_MOMENTUM_EVENT
                       : nsIScrollableFrame::NOT_MOMENTUM;

  nsIntPoint overflow;
  aScrollableFrame->ScrollBy(actualDevPixelScrollAmount,
                             nsIScrollableFrame::DEVICE_PIXELS,
                             mode, &overflow, origin, momentum, snapMode);

  if (!scrollFrameWeak.IsAlive()) {
    // If the scroll causes changing the layout, we can think that the event
    // has been completely consumed by the content.  Then, users probably don't
    // want additional action.
    aEvent->overflowDeltaX = aEvent->overflowDeltaY = 0;
  } else if (isDeltaModePixel) {
    aEvent->overflowDeltaX = overflow.x;
    aEvent->overflowDeltaY = overflow.y;
  } else {
    aEvent->overflowDeltaX =
      static_cast<double>(overflow.x) / scrollAmountInDevPixels.width;
    aEvent->overflowDeltaY =
      static_cast<double>(overflow.y) / scrollAmountInDevPixels.height;
  }

  // If CSS overflow properties caused not to scroll, the overflowDelta* values
  // should be same as delta* values since they may be used as gesture event by
  // widget.  However, if there is another scrollable element in the ancestor
  // along the axis, probably users don't want the operation to cause
  // additional action such as moving history.  In such case, overflowDelta
  // values should stay zero.
  if (scrollFrameWeak.IsAlive()) {
    if (aEvent->deltaX &&
        overflowStyle.mHorizontal == NS_STYLE_OVERFLOW_HIDDEN &&
        !ComputeScrollTarget(scrollFrame, aEvent,
                             COMPUTE_SCROLLABLE_ANCESTOR_ALONG_X_AXIS)) {
      aEvent->overflowDeltaX = aEvent->deltaX;
    }
    if (aEvent->deltaY &&
        overflowStyle.mVertical == NS_STYLE_OVERFLOW_HIDDEN &&
        !ComputeScrollTarget(scrollFrame, aEvent,
                             COMPUTE_SCROLLABLE_ANCESTOR_ALONG_Y_AXIS)) {
      aEvent->overflowDeltaY = aEvent->deltaY;
    }
  }

  NS_ASSERTION(aEvent->overflowDeltaX == 0 ||
    (aEvent->overflowDeltaX > 0) == (aEvent->deltaX > 0),
    "The sign of overflowDeltaX is different from the scroll direction");
  NS_ASSERTION(aEvent->overflowDeltaY == 0 ||
    (aEvent->overflowDeltaY > 0) == (aEvent->deltaY > 0),
    "The sign of overflowDeltaY is different from the scroll direction");

  WheelPrefs::GetInstance()->CancelApplyingUserPrefsFromOverflowDelta(aEvent);
}

void
EventStateManager::DecideGestureEvent(WidgetGestureNotifyEvent* aEvent,
                                      nsIFrame* targetFrame)
{

  NS_ASSERTION(aEvent->mMessage == NS_GESTURENOTIFY_EVENT_START,
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
  WidgetGestureNotifyEvent::ePanDirection panDirection =
    WidgetGestureNotifyEvent::ePanNone;
  bool displayPanFeedback = false;
  for (nsIFrame* current = targetFrame; current;
       current = nsLayoutUtils::GetCrossDocParentFrame(current)) {

    // e10s - mark remote content as pannable. This is a work around since
    // we don't have access to remote frame scroll info here. Apz data may
    // assist is solving this.
    if (current && IsRemoteTarget(current->GetContent())) {
      panDirection = WidgetGestureNotifyEvent::ePanBoth;
      // We don't know when we reach bounds, so just disable feedback for now.
      displayPanFeedback = false;
      break;
    }

    nsIAtom* currentFrameType = current->GetType();

    // Scrollbars should always be draggable
    if (currentFrameType == nsGkAtoms::scrollbarFrame) {
      panDirection = WidgetGestureNotifyEvent::ePanNone;
      break;
    }

#ifdef MOZ_XUL
    // Special check for trees
    nsTreeBodyFrame* treeFrame = do_QueryFrame(current);
    if (treeFrame) {
      if (treeFrame->GetHorizontalOverflow()) {
        panDirection = WidgetGestureNotifyEvent::ePanHorizontal;
      }
      if (treeFrame->GetVerticalOverflow()) {
        panDirection = WidgetGestureNotifyEvent::ePanVertical;
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
          panDirection = WidgetGestureNotifyEvent::ePanVertical;
          break;
        }

        if (canScrollHorizontally) {
          panDirection = WidgetGestureNotifyEvent::ePanHorizontal;
          displayPanFeedback = false;
        }
      } else { //Not a XUL box
        uint32_t scrollbarVisibility = scrollableFrame->GetScrollbarVisibility();

        //Check if we have visible scrollbars
        if (scrollbarVisibility & nsIScrollableFrame::VERTICAL) {
          panDirection = WidgetGestureNotifyEvent::ePanVertical;
          displayPanFeedback = true;
          break;
        }

        if (scrollbarVisibility & nsIScrollableFrame::HORIZONTAL) {
          panDirection = WidgetGestureNotifyEvent::ePanHorizontal;
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
    if (aNode->IsXULElement()) {
      mozilla::dom::Element* element = aNode->AsElement();
      static nsIContent::AttrValuesArray strings[] =
        {&nsGkAtoms::always, &nsGkAtoms::never, nullptr};
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

void
EventStateManager::PostHandleKeyboardEvent(WidgetKeyboardEvent* aKeyboardEvent,
                                           nsEventStatus& aStatus,
                                           bool dispatchedToContentProcess)
{
  if (aStatus == nsEventStatus_eConsumeNoDefault) {
    return;
  }

  // XXX Currently, our automated tests don't support mKeyNameIndex.
  //     Therefore, we still need to handle this with keyCode.
  switch(aKeyboardEvent->keyCode) {
    case NS_VK_TAB:
    case NS_VK_F6:
      // This is to prevent keyboard scrolling while alt modifier in use.
      if (!aKeyboardEvent->IsAlt()) {
        aStatus = nsEventStatus_eConsumeNoDefault;

        // Handling the tab event after it was sent to content is bad,
        // because to the FocusManager the remote-browser looks like one
        // element, so we would just move the focus to the next element
        // in chrome, instead of handling it in content.
        if (dispatchedToContentProcess)
          break;

        EnsureDocument(mPresContext);
        nsIFocusManager* fm = nsFocusManager::GetFocusManager();
        if (fm && mDocument) {
          // Shift focus forward or back depending on shift key
          bool isDocMove =
            aKeyboardEvent->IsControl() || aKeyboardEvent->keyCode == NS_VK_F6;
          uint32_t dir = aKeyboardEvent->IsShift() ?
            (isDocMove ? static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_BACKWARDDOC) :
                         static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_BACKWARD)) :
            (isDocMove ? static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_FORWARDDOC) :
                         static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_FORWARD));
          nsCOMPtr<nsIDOMElement> result;
          fm->MoveFocus(mDocument->GetWindow(), nullptr, dir,
                        nsIFocusManager::FLAG_BYKEY,
                        getter_AddRefs(result));
        }
      }
      return;
    case 0:
      // We handle keys with no specific keycode value below.
      break;
    default:
      return;
  }

  switch(aKeyboardEvent->mKeyNameIndex) {
    case KEY_NAME_INDEX_ZoomIn:
    case KEY_NAME_INDEX_ZoomOut:
      ChangeFullZoom(
        aKeyboardEvent->mKeyNameIndex == KEY_NAME_INDEX_ZoomIn ? 1 : -1);
      aStatus = nsEventStatus_eConsumeNoDefault;
      break;
    default:
      break;
  }
}

nsresult
EventStateManager::PostHandleEvent(nsPresContext* aPresContext,
                                   WidgetEvent* aEvent,
                                   nsIFrame* aTargetFrame,
                                   nsEventStatus* aStatus)
{
  NS_ENSURE_ARG(aPresContext);
  NS_ENSURE_ARG_POINTER(aStatus);

  bool dispatchedToContentProcess = HandleCrossProcessEvent(aEvent,
                                                            aStatus);

  mCurrentTarget = aTargetFrame;
  mCurrentTargetContent = nullptr;

  // Most of the events we handle below require a frame.
  // Add special cases here.
  if (!mCurrentTarget && aEvent->mMessage != eMouseUp &&
      aEvent->mMessage != eMouseDown) {
    return NS_OK;
  }

  //Keep the prescontext alive, we might need it after event dispatch
  nsRefPtr<nsPresContext> presContext = aPresContext;
  nsresult ret = NS_OK;

  switch (aEvent->mMessage) {
  case eMouseDown:
    {
      WidgetMouseEvent* mouseEvent = aEvent->AsMouseEvent();
      if (mouseEvent->button == WidgetMouseEvent::eLeftButton &&
          !sNormalLMouseEventInProcess) {
        // We got a mouseup event while a mousedown event was being processed.
        // Make sure that the capturing content is cleared.
        nsIPresShell::SetCapturingContent(nullptr, 0);
        break;
      }

      // For remote content, capture the event in the parent process at the
      // <xul:browser remote> element. This will ensure that subsequent mousemove/mouseup
      // events will continue to be dispatched to this element and therefore forwarded
      // to the child.
      if (dispatchedToContentProcess && !nsIPresShell::GetCapturingContent()) {
        nsIContent* content = mCurrentTarget ? mCurrentTarget->GetContent() : nullptr;
        nsIPresShell::SetCapturingContent(content, 0);
      }

      nsCOMPtr<nsIContent> activeContent;
      if (nsEventStatus_eConsumeNoDefault != *aStatus) {
        nsCOMPtr<nsIContent> newFocus;      
        bool suppressBlur = false;
        if (mCurrentTarget) {
          mCurrentTarget->GetContentForEvent(aEvent, getter_AddRefs(newFocus));
          const nsStyleUserInterface* ui = mCurrentTarget->StyleUserInterface();
          activeContent = mCurrentTarget->GetContent();

          // In some cases, we do not want to even blur the current focused
          // element. Those cases are:
          // 1. -moz-user-focus CSS property is set to 'ignore';
          // 2. Element with NS_EVENT_STATE_DISABLED
          //    (aka :disabled pseudo-class for HTML element);
          // 3. XUL control element has the disabled property set to 'true'.
          //
          // We can't use nsIFrame::IsFocusable() because we want to blur when
          // we click on a visibility: none element.
          // We can't use nsIContent::IsFocusable() because we want to blur when
          // we click on a non-focusable element like a <div>.
          // We have to use |aEvent->target| to not make sure we do not check an
          // anonymous node of the targeted element.
          suppressBlur = (ui->mUserFocus == NS_STYLE_USER_FOCUS_IGNORE);

          if (!suppressBlur) {
            nsCOMPtr<Element> element = do_QueryInterface(aEvent->target);
            suppressBlur = element &&
                           element->State().HasState(NS_EVENT_STATE_DISABLED);
          }

          if (!suppressBlur) {
            nsCOMPtr<nsIDOMXULControlElement> xulControl =
              do_QueryInterface(aEvent->target);
            if (xulControl) {
              bool disabled;
              xulControl->GetDisabled(&disabled);
              suppressBlur = disabled;
            }
          }
        }

        if (!suppressBlur) {
          suppressBlur = nsContentUtils::IsUserFocusIgnored(activeContent);
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
          nsIDocument *doc = newFocus->GetCrossShadowCurrentDoc();
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
          const nsStyleDisplay* display = currFrame->StyleDisplay();
          if (display->mDisplay == NS_STYLE_DISPLAY_POPUP) {
            newFocus = nullptr;
            break;
          }

          int32_t tabIndexUnused;
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
              if (!activeContent || !activeContent->IsXULElement())
#endif
                fm->ClearFocus(mDocument->GetWindow());
              fm->SetFocusedWindow(mDocument->GetWindow());
            }
          }
        }

        // The rest is left button-specific.
        if (mouseEvent->button != WidgetMouseEvent::eLeftButton) {
          break;
        }

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
          if (currentWindow && mDocument->GetWindow() &&
              currentWindow != mDocument->GetWindow() &&
              !nsContentUtils::IsChromeDoc(mDocument)) {
            nsCOMPtr<nsIDOMWindow> currentTop;
            nsCOMPtr<nsIDOMWindow> newTop;
            currentWindow->GetTop(getter_AddRefs(currentTop));
            mDocument->GetWindow()->GetTop(getter_AddRefs(newTop));
            nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(currentWindow);
            nsCOMPtr<nsIDocument> currentDoc = win->GetExtantDoc();
            if (nsContentUtils::IsChromeDoc(currentDoc) ||
                (currentTop && newTop && currentTop != newTop)) {
              fm->SetFocusedWindow(mDocument->GetWindow());
            }
          }
        }
      }
      SetActiveManager(this, activeContent);
    }
    break;
  case ePointerCancel: {
    if(WidgetMouseEvent* mouseEvent = aEvent->AsMouseEvent()) {
      GenerateMouseEnterExit(mouseEvent);
    }
    // This break was commented specially
    // break;
  }
  case ePointerUp: {
    WidgetPointerEvent* pointerEvent = aEvent->AsPointerEvent();
    // After UP/Cancel Touch pointers become invalid so we can remove relevant helper from Table
    // Mouse/Pen pointers are valid all the time (not only between down/up)
    if (pointerEvent->inputSource == nsIDOMMouseEvent::MOZ_SOURCE_TOUCH) {
      mPointersEnterLeaveHelper.Remove(pointerEvent->pointerId);
      GenerateMouseEnterExit(pointerEvent);
    }
    break;
  }
  case eMouseUp:
    {
      ClearGlobalActiveContent(this);
      WidgetMouseEvent* mouseEvent = aEvent->AsMouseEvent();
      if (mouseEvent && mouseEvent->IsReal()) {
        if (!mCurrentTarget) {
          GetEventTarget();
        }
        // Make sure to dispatch the click even if there is no frame for
        // the current target element. This is required for Web compatibility.
        ret = CheckForAndDispatchClick(presContext, mouseEvent, aStatus);
      }

      nsIPresShell *shell = presContext->GetPresShell();
      if (shell) {
        nsRefPtr<nsFrameSelection> frameSelection = shell->FrameSelection();
        frameSelection->SetDragState(false);
      }
    }
    break;
  case NS_WHEEL_STOP:
    {
      MOZ_ASSERT(aEvent->mFlags.mIsTrusted);
      ScrollbarsForWheel::MayInactivate();
      WidgetWheelEvent* wheelEvent = aEvent->AsWheelEvent();
      nsIScrollableFrame* scrollTarget =
        ComputeScrollTarget(aTargetFrame, wheelEvent,
                            COMPUTE_DEFAULT_ACTION_TARGET);
      if (scrollTarget) {
        scrollTarget->ScrollSnap();
      }
    }
    break;
  case NS_WHEEL_WHEEL:
  case NS_WHEEL_START:
    {
      MOZ_ASSERT(aEvent->mFlags.mIsTrusted);

      if (*aStatus == nsEventStatus_eConsumeNoDefault) {
        ScrollbarsForWheel::Inactivate();
        break;
      }

      WidgetWheelEvent* wheelEvent = aEvent->AsWheelEvent();

      // When APZ is enabled, the actual scroll animation might be handled by
      // the compositor.
      WheelPrefs::Action action;
      if (wheelEvent->mFlags.mHandledByAPZ) {
        action = WheelPrefs::ACTION_NONE;
      } else {
        action = WheelPrefs::GetInstance()->ComputeActionFor(wheelEvent);
      }
      switch (action) {
        case WheelPrefs::ACTION_SCROLL: {
          // For scrolling of default action, we should honor the mouse wheel
          // transaction.

          ScrollbarsForWheel::PrepareToScrollText(this, aTargetFrame, wheelEvent);

          if (aEvent->mMessage != NS_WHEEL_WHEEL ||
              (!wheelEvent->deltaX && !wheelEvent->deltaY)) {
            break;
          }

          nsIScrollableFrame* scrollTarget =
            ComputeScrollTarget(aTargetFrame, wheelEvent,
                                COMPUTE_DEFAULT_ACTION_TARGET);

          ScrollbarsForWheel::SetActiveScrollTarget(scrollTarget);

          nsIFrame* rootScrollFrame = !aTargetFrame ? nullptr :
            aTargetFrame->PresContext()->PresShell()->GetRootScrollFrame();
          nsIScrollableFrame* rootScrollableFrame = nullptr;
          if (rootScrollFrame) {
            rootScrollableFrame = do_QueryFrame(rootScrollFrame);
          }
          if (!scrollTarget || scrollTarget == rootScrollableFrame) {
            wheelEvent->mViewPortIsOverscrolled = true;
          }
          wheelEvent->overflowDeltaX = wheelEvent->deltaX;
          wheelEvent->overflowDeltaY = wheelEvent->deltaY;
          WheelPrefs::GetInstance()->
            CancelApplyingUserPrefsFromOverflowDelta(wheelEvent);
          if (scrollTarget) {
            DoScrollText(scrollTarget, wheelEvent);
          } else {
            WheelTransaction::EndTransaction();
            ScrollbarsForWheel::Inactivate();
          }
          break;
        }
        case WheelPrefs::ACTION_HISTORY: {
          // If this event doesn't cause NS_MOUSE_SCROLL event or the direction
          // is oblique, don't perform history back/forward.
          int32_t intDelta = wheelEvent->GetPreferredIntDelta();
          if (!intDelta) {
            break;
          }
          DoScrollHistory(intDelta);
          break;
        }
        case WheelPrefs::ACTION_ZOOM: {
          // If this event doesn't cause NS_MOUSE_SCROLL event or the direction
          // is oblique, don't perform zoom in/out.
          int32_t intDelta = wheelEvent->GetPreferredIntDelta();
          if (!intDelta) {
            break;
          }
          DoScrollZoom(aTargetFrame, intDelta);
          break;
        }
        case WheelPrefs::ACTION_NONE:
        default:
          bool allDeltaOverflown = false;
          if (wheelEvent->mFlags.mHandledByAPZ) {
            if (wheelEvent->mCanTriggerSwipe) {
              // For events that can trigger swipes, APZ needs to know whether
              // scrolling is possible in the requested direction. It does this
              // by looking at the scroll overflow values on mCanTriggerSwipe
              // events after they have been processed.
              allDeltaOverflown =
                !ComputeScrollTarget(aTargetFrame, wheelEvent,
                                     COMPUTE_DEFAULT_ACTION_TARGET);
            }
          } else {
            // The event was processed neither by APZ nor by us, so all of the
            // delta values must be overflown delta values.
            allDeltaOverflown = true;
          }

          if (!allDeltaOverflown) {
            break;
          }
          wheelEvent->overflowDeltaX = wheelEvent->deltaX;
          wheelEvent->overflowDeltaY = wheelEvent->deltaY;
          WheelPrefs::GetInstance()->
            CancelApplyingUserPrefsFromOverflowDelta(wheelEvent);
          wheelEvent->mViewPortIsOverscrolled = true;
          break;
      }
      *aStatus = nsEventStatus_eConsumeNoDefault;
    }
    break;

  case NS_GESTURENOTIFY_EVENT_START:
    {
      if (nsEventStatus_eConsumeNoDefault != *aStatus) {
        DecideGestureEvent(aEvent->AsGestureNotifyEvent(), mCurrentTarget);
      }
    }
    break;

  case eDragEnter:
  case eDragOver:
    {
      NS_ASSERTION(aEvent->mClass == eDragEventClass, "Expected a drag event");

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

      WidgetDragEvent *dragEvent = aEvent->AsDragEvent();

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
      uint32_t dropEffect = nsIDragService::DRAGDROP_ACTION_NONE;
      uint32_t action = nsIDragService::DRAGDROP_ACTION_NONE;
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
          // initialized (which is done in DragEvent::GetDataTransfer),
          // so set it from the drag action. We'll still want to filter it
          // based on the effectAllowed below.
          dataTransfer = initialDataTransfer;

          dragSession->GetDragAction(&action);

          // filter the drop effect based on the action. Use UNINITIALIZED as
          // any effect is allowed.
          dropEffect = nsContentUtils::FilterDropEffect(action,
                         nsIDragService::DRAGDROP_ACTION_UNINITIALIZED);
        }

        // At this point, if the dataTransfer is null, it means that the
        // drag was originally started by directly calling the drag service.
        // Just assume that all effects are allowed.
        uint32_t effectAllowed = nsIDragService::DRAGDROP_ACTION_UNINITIALIZED;
        if (dataTransfer)
          dataTransfer->GetEffectAllowedInt(&effectAllowed);

        // set the drag action based on the drop effect and effect allowed.
        // The drop effect field on the drag transfer object specifies the
        // desired current drop effect. However, it cannot be used if the
        // effectAllowed state doesn't include that type of action. If the
        // dropEffect is "none", then the action will be 'none' so a drop will
        // not be allowed.
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
        if (aEvent->mMessage == eDragOver && !isChromeDoc) {
          // Someone has called preventDefault(), check whether is was on
          // content or chrome.
          dragSession->SetOnlyChromeDrop(
            !dragEvent->mDefaultPreventedOnContent);
        }
      } else if (aEvent->mMessage == eDragOver && !isChromeDoc) {
        // No one called preventDefault(), so handle drop only in chrome.
        dragSession->SetOnlyChromeDrop(true);
      }
      if (ContentChild* child = ContentChild::GetSingleton()) {
        child->SendUpdateDropEffect(action, dropEffect);
      }
      if (dispatchedToContentProcess) {
        dragSession->SetCanDrop(true);
      } else if (initialDataTransfer) {
        // Now set the drop effect in the initial dataTransfer. This ensures
        // that we can get the desired drop effect in the drop event. For events
        // dispatched to content, the content process will take care of setting
        // this.
        initialDataTransfer->SetDropEffectInt(dropEffect);
      }
    }
    break;

  case eDrop:
    {
      // now fire the dragdrop event, for compatibility with XUL
      if (mCurrentTarget && nsEventStatus_eConsumeNoDefault != *aStatus) {
        nsCOMPtr<nsIContent> targetContent;
        mCurrentTarget->GetContentForEvent(aEvent,
                                           getter_AddRefs(targetContent));

        nsCOMPtr<nsIWidget> widget = mCurrentTarget->GetNearestWidget();
        WidgetDragEvent event(aEvent->mFlags.mIsTrusted,
                              NS_DRAGDROP_DRAGDROP, widget);

        WidgetMouseEvent* mouseEvent = aEvent->AsMouseEvent();
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
      sLastDragOverFrame = nullptr;
      ClearGlobalActiveContent(this);
      break;
    }
  case NS_DRAGDROP_EXIT:
     // make sure to fire the enter and exit_synth events after the
     // NS_DRAGDROP_EXIT event, otherwise we'll clean up too early
    GenerateDragDropEnterExit(presContext, aEvent->AsDragEvent());
    break;

  case eBeforeKeyUp:
  case eKeyUp:
  case eAfterKeyUp:
    break;

  case eKeyPress:
    {
      WidgetKeyboardEvent* keyEvent = aEvent->AsKeyboardEvent();
      PostHandleKeyboardEvent(keyEvent, *aStatus, dispatchedToContentProcess);
    }
    break;

  case eMouseEnterIntoWidget:
    if (mCurrentTarget) {
      nsCOMPtr<nsIContent> targetContent;
      mCurrentTarget->GetContentForEvent(aEvent, getter_AddRefs(targetContent));
      SetContentState(targetContent, NS_EVENT_STATE_HOVER);
    }
    break;

#ifdef XP_MACOSX
  case eMouseActivate:
    if (mCurrentTarget) {
      nsCOMPtr<nsIContent> targetContent;
      mCurrentTarget->GetContentForEvent(aEvent, getter_AddRefs(targetContent));
      if (!NodeAllowsClickThrough(targetContent)) {
        *aStatus = nsEventStatus_eConsumeNoDefault;
      }
    }
    break;
#endif

  default:
    break;
  }

  //Reset target frame to null to avoid mistargeting after reentrant event
  mCurrentTarget = nullptr;
  mCurrentTargetContent = nullptr;

  return ret;
}

bool
EventStateManager::RemoteQueryContentEvent(WidgetEvent* aEvent)
{
  WidgetQueryContentEvent* queryEvent = aEvent->AsQueryContentEvent();
  if (!IsTargetCrossProcess(queryEvent)) {
    return false;
  }
  // Will not be handled locally, remote the event
  GetCrossProcessTarget()->HandleQueryContentEvent(*queryEvent);
  return true;
}

TabParent*
EventStateManager::GetCrossProcessTarget()
{
  return IMEStateManager::GetActiveTabParent();
}

bool
EventStateManager::IsTargetCrossProcess(WidgetGUIEvent* aEvent)
{
  // Check to see if there is a focused, editable content in chrome,
  // in that case, do not forward IME events to content
  nsIContent *focusedContent = GetFocusedContent();
  if (focusedContent && focusedContent->IsEditable())
    return false;
  return IMEStateManager::GetActiveTabParent() != nullptr;
}

void
EventStateManager::NotifyDestroyPresContext(nsPresContext* aPresContext)
{
  IMEStateManager::OnDestroyPresContext(aPresContext);
  if (mHoverContent) {
    // Bug 70855: Presentation is going away, possibly for a reframe.
    // Reset the hover state so that if we're recreating the presentation,
    // we won't have the old hover state still set in the new presentation,
    // as if the new presentation is resized, a new element may be hovered. 
    SetContentState(nullptr, NS_EVENT_STATE_HOVER);
  }
  mPointersEnterLeaveHelper.Clear();
}

void
EventStateManager::SetPresContext(nsPresContext* aPresContext)
{
  mPresContext = aPresContext;
}

void
EventStateManager::ClearFrameRefs(nsIFrame* aFrame)
{
  if (aFrame && aFrame == mCurrentTarget) {
    mCurrentTargetContent = aFrame->GetContent();
  }
}

void
EventStateManager::UpdateCursor(nsPresContext* aPresContext,
                                WidgetEvent* aEvent,
                                nsIFrame* aTargetFrame,
                                nsEventStatus* aStatus)
{
  if (aTargetFrame && IsRemoteTarget(aTargetFrame->GetContent())) {
    return;
  }

  int32_t cursor = NS_STYLE_CURSOR_DEFAULT;
  imgIContainer* container = nullptr;
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
    nsCOMPtr<nsIDocShell> docShell(aPresContext->GetDocShell());
    if (!docShell) return;
    uint32_t busyFlags = nsIDocShell::BUSY_FLAGS_NONE;
    docShell->GetBusyFlags(&busyFlags);

    // Show busy cursor everywhere before page loads
    // and just replace the arrow cursor after page starts loading
    if (busyFlags & nsIDocShell::BUSY_FLAGS_BUSY &&
          (cursor == NS_STYLE_CURSOR_AUTO || cursor == NS_STYLE_CURSOR_DEFAULT))
    {
      cursor = NS_STYLE_CURSOR_SPINNING;
      container = nullptr;
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

void
EventStateManager::ClearCachedWidgetCursor(nsIFrame* aTargetFrame)
{
  if (!aTargetFrame) {
    return;
  }
  nsIWidget* aWidget = aTargetFrame->GetNearestWidget();
  if (!aWidget) {
    return;
  }
  aWidget->ClearCachedCursor();
}

nsresult
EventStateManager::SetCursor(int32_t aCursor, imgIContainer* aContainer,
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
  case NS_STYLE_CURSOR_ZOOM_IN:
    c = eCursor_zoom_in;
    break;
  case NS_STYLE_CURSOR_ZOOM_OUT:
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
    uint32_t hotspotX, hotspotY;

    // css3-ui says to use the CSS-specified hotspot if present,
    // otherwise use the intrinsic hotspot, otherwise use the top left
    // corner.
    if (aHaveHotspot) {
      int32_t imgWidth, imgHeight;
      aContainer->GetWidth(&imgWidth);
      aContainer->GetHeight(&imgHeight);

      // XXX std::max(NS_lround(x), 0)?
      hotspotX = aHotspotX > 0.0f
                   ? uint32_t(aHotspotX + 0.5f) : uint32_t(0);
      if (hotspotX >= uint32_t(imgWidth))
        hotspotX = imgWidth - 1;
      hotspotY = aHotspotY > 0.0f
                   ? uint32_t(aHotspotY + 0.5f) : uint32_t(0);
      if (hotspotY >= uint32_t(imgHeight))
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

class MOZ_STACK_CLASS ESMEventCB : public EventDispatchingCallback
{
public:
  explicit ESMEventCB(nsIContent* aTarget) : mTarget(aTarget) {}

  virtual void HandleEvent(EventChainPostVisitor& aVisitor)
  {
    if (aVisitor.mPresContext) {
      nsIFrame* frame = aVisitor.mPresContext->GetPrimaryFrameFor(mTarget);
      if (frame) {
        frame->HandleEvent(aVisitor.mPresContext,
                           aVisitor.mEvent->AsGUIEvent(),
                           &aVisitor.mEventStatus);
      }
    }
  }

  nsCOMPtr<nsIContent> mTarget;
};

/*static*/ bool
EventStateManager::IsHandlingUserInput()
{
  if (sUserInputEventDepth <= 0) {
    return false;
  }

  TimeDuration timeout = nsContentUtils::HandlingUserInputTimeout();
  return timeout <= TimeDuration(0) ||
         (TimeStamp::Now() - sHandlingInputStart) <= timeout;
}

static void
CreateMouseOrPointerWidgetEvent(WidgetMouseEvent* aMouseEvent,
                                EventMessage aMessage,
                                nsIContent* aRelatedContent,
                                nsAutoPtr<WidgetMouseEvent>& aNewEvent)
{
  WidgetPointerEvent* sourcePointer = aMouseEvent->AsPointerEvent();
  if (sourcePointer) {
    PROFILER_LABEL("Input", "DispatchPointerEvent",
      js::ProfileEntry::Category::EVENTS);

    nsAutoPtr<WidgetPointerEvent> newPointerEvent;
    newPointerEvent =
      new WidgetPointerEvent(aMouseEvent->mFlags.mIsTrusted, aMessage,
                             aMouseEvent->widget);
    newPointerEvent->isPrimary = sourcePointer->isPrimary;
    newPointerEvent->pointerId = sourcePointer->pointerId;
    newPointerEvent->width = sourcePointer->width;
    newPointerEvent->height = sourcePointer->height;
    newPointerEvent->inputSource = sourcePointer->inputSource;
    newPointerEvent->relatedTarget =
      nsIPresShell::GetPointerCapturingContent(sourcePointer->pointerId)
        ? nullptr
        : aRelatedContent;
    aNewEvent = newPointerEvent.forget();
  } else {
    aNewEvent =
      new WidgetMouseEvent(aMouseEvent->mFlags.mIsTrusted, aMessage,
                           aMouseEvent->widget, WidgetMouseEvent::eReal);
    aNewEvent->relatedTarget = aRelatedContent;
  }
  aNewEvent->refPoint = aMouseEvent->refPoint;
  aNewEvent->modifiers = aMouseEvent->modifiers;
  aNewEvent->button = aMouseEvent->button;
  aNewEvent->buttons = aMouseEvent->buttons;
  aNewEvent->pressure = aMouseEvent->pressure;
  aNewEvent->mPluginEvent = aMouseEvent->mPluginEvent;
  aNewEvent->inputSource = aMouseEvent->inputSource;
}

nsIFrame*
EventStateManager::DispatchMouseOrPointerEvent(WidgetMouseEvent* aMouseEvent,
                                               EventMessage aMessage,
                                               nsIContent* aTargetContent,
                                               nsIContent* aRelatedContent)
{
  // http://dvcs.w3.org/hg/webevents/raw-file/default/mouse-lock.html#methods
  // "[When the mouse is locked on an element...e]vents that require the concept
  // of a mouse cursor must not be dispatched (for example: mouseover, mouseout).
  if (sIsPointerLocked &&
      (aMessage == eMouseLeave ||
       aMessage == eMouseEnter ||
       aMessage == eMouseOver ||
       aMessage == eMouseOut)) {
    mCurrentTargetContent = nullptr;
    nsCOMPtr<Element> pointerLockedElement =
      do_QueryReferent(EventStateManager::sPointerLockedElement);
    if (!pointerLockedElement) {
      NS_WARNING("Should have pointer locked element, but didn't.");
      return nullptr;
    }
    nsCOMPtr<nsIContent> content = do_QueryInterface(pointerLockedElement);
    return mPresContext->GetPrimaryFrameFor(content);
  }

  mCurrentTargetContent = nullptr;

  if (!aTargetContent) {
    return nullptr;
  }

  nsAutoPtr<WidgetMouseEvent> dispatchEvent;
  CreateMouseOrPointerWidgetEvent(aMouseEvent, aMessage,
                                  aRelatedContent, dispatchEvent);

  nsWeakFrame previousTarget = mCurrentTarget;
  mCurrentTargetContent = aTargetContent;

  nsIFrame* targetFrame = nullptr;

  if (aMouseEvent->AsMouseEvent()) {
    PROFILER_LABEL("Input", "DispatchMouseEvent",
      js::ProfileEntry::Category::EVENTS);
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  ESMEventCB callback(aTargetContent);
  EventDispatcher::Dispatch(aTargetContent, mPresContext, dispatchEvent, nullptr,
                            &status, &callback);

  if (mPresContext) {
    // Although the primary frame was checked in event callback, it may not be
    // the same object after event dispatch and handling, so refetch it.
    targetFrame = mPresContext->GetPrimaryFrameFor(aTargetContent);

    // If we are entering/leaving remote content, dispatch a mouse enter/exit
    // event to the remote frame.
    if (IsRemoteTarget(aTargetContent)) {
      if (aMessage == eMouseOut) {
        // For remote content, send a "top-level" widget mouse exit event.
        nsAutoPtr<WidgetMouseEvent> remoteEvent;
        CreateMouseOrPointerWidgetEvent(aMouseEvent, eMouseExitFromWidget,
                                        aRelatedContent, remoteEvent);
        remoteEvent->exit = WidgetMouseEvent::eTopLevel;

        // mCurrentTarget is set to the new target, so we must reset it to the
        // old target and then dispatch a cross-process event. (mCurrentTarget
        // will be set back below.) HandleCrossProcessEvent will query for the
        // proper target via GetEventTarget which will return mCurrentTarget.
        mCurrentTarget = targetFrame;
        HandleCrossProcessEvent(remoteEvent, &status);
      } else if (aMessage == eMouseOver) {
        nsAutoPtr<WidgetMouseEvent> remoteEvent;
        CreateMouseOrPointerWidgetEvent(aMouseEvent, eMouseEnterIntoWidget,
                                        aRelatedContent, remoteEvent);
        HandleCrossProcessEvent(remoteEvent, &status);
      }
    }
  }

  mCurrentTargetContent = nullptr;
  mCurrentTarget = previousTarget;

  return targetFrame;
}

class EnterLeaveDispatcher
{
public:
  EnterLeaveDispatcher(EventStateManager* aESM,
                       nsIContent* aTarget, nsIContent* aRelatedTarget,
                       WidgetMouseEvent* aMouseEvent,
                       EventMessage aEventMessage)
    : mESM(aESM)
    , mMouseEvent(aMouseEvent)
    , mEventMessage(aEventMessage)
  {
    nsPIDOMWindow* win =
      aTarget ? aTarget->OwnerDoc()->GetInnerWindow() : nullptr;
    if (aMouseEvent->AsPointerEvent() ? win && win->HasPointerEnterLeaveEventListeners() :
                                        win && win->HasMouseEnterLeaveEventListeners()) {
      mRelatedTarget = aRelatedTarget ?
        aRelatedTarget->FindFirstNonChromeOnlyAccessContent() : nullptr;
      nsINode* commonParent = nullptr;
      if (aTarget && aRelatedTarget) {
        commonParent =
          nsContentUtils::GetCommonAncestor(aTarget, aRelatedTarget);
      }
      nsIContent* current = aTarget;
      // Note, it is ok if commonParent is null!
      while (current && current != commonParent) {
        if (!current->ChromeOnlyAccess()) {
          mTargets.AppendObject(current);
        }
        // mouseenter/leave is fired only on elements.
        current = current->GetParent();
      }
    }
  }

  ~EnterLeaveDispatcher()
  {
    if (mEventMessage == eMouseEnter || mEventMessage == ePointerEnter) {
      for (int32_t i = mTargets.Count() - 1; i >= 0; --i) {
        mESM->DispatchMouseOrPointerEvent(mMouseEvent, mEventMessage,
                                          mTargets[i], mRelatedTarget);
      }
    } else {
      for (int32_t i = 0; i < mTargets.Count(); ++i) {
        mESM->DispatchMouseOrPointerEvent(mMouseEvent, mEventMessage,
                                          mTargets[i], mRelatedTarget);
      }
    }
  }

  EventStateManager* mESM;
  nsCOMArray<nsIContent> mTargets;
  nsCOMPtr<nsIContent> mRelatedTarget;
  WidgetMouseEvent* mMouseEvent;
  EventMessage mEventMessage;
};

void
EventStateManager::NotifyMouseOut(WidgetMouseEvent* aMouseEvent,
                                  nsIContent* aMovingInto)
{
  OverOutElementsWrapper* wrapper = GetWrapperByEventID(aMouseEvent);

  if (!wrapper->mLastOverElement)
    return;
  // Before firing mouseout, check for recursion
  if (wrapper->mLastOverElement == wrapper->mFirstOutEventElement)
    return;

  if (wrapper->mLastOverFrame) {
    // if the frame is associated with a subdocument,
    // tell the subdocument that we're moving out of it
    nsSubDocumentFrame* subdocFrame = do_QueryFrame(wrapper->mLastOverFrame.GetFrame());
    if (subdocFrame) {
      nsCOMPtr<nsIDocShell> docshell;
      subdocFrame->GetDocShell(getter_AddRefs(docshell));
      if (docshell) {
        nsRefPtr<nsPresContext> presContext;
        docshell->GetPresContext(getter_AddRefs(presContext));

        if (presContext) {
          EventStateManager* kidESM = presContext->EventStateManager();
          // Not moving into any element in this subdocument
          kidESM->NotifyMouseOut(aMouseEvent, nullptr);
        }
      }
    }
  }
  // That could have caused DOM events which could wreak havoc. Reverify
  // things and be careful.
  if (!wrapper->mLastOverElement)
    return;

  // Store the first mouseOut event we fire and don't refire mouseOut
  // to that element while the first mouseOut is still ongoing.
  wrapper->mFirstOutEventElement = wrapper->mLastOverElement;

  // Don't touch hover state if aMovingInto is non-null.  Caller will update
  // hover state itself, and we have optimizations for hover switching between
  // two nearby elements both deep in the DOM tree that would be defeated by
  // switching the hover state to null here.
  bool isPointer = aMouseEvent->mClass == ePointerEventClass;
  if (!aMovingInto && !isPointer) {
    // Unset :hover
    SetContentState(nullptr, NS_EVENT_STATE_HOVER);
  }

  // In case we go out from capturing element (retargetedByPointerCapture is true)
  // we should dispatch ePointerLeave event and only for capturing element.
  nsRefPtr<nsIContent> movingInto = aMouseEvent->retargetedByPointerCapture
                                    ? wrapper->mLastOverElement->GetParent()
                                    : aMovingInto;

  EnterLeaveDispatcher leaveDispatcher(this, wrapper->mLastOverElement,
                                       movingInto, aMouseEvent,
                                       isPointer ? ePointerLeave : eMouseLeave);

  // Fire mouseout
  DispatchMouseOrPointerEvent(aMouseEvent, isPointer ? ePointerOut : eMouseOut,
                              wrapper->mLastOverElement, aMovingInto);

  wrapper->mLastOverFrame = nullptr;
  wrapper->mLastOverElement = nullptr;

  // Turn recursion protection back off
  wrapper->mFirstOutEventElement = nullptr;
}

void
EventStateManager::NotifyMouseOver(WidgetMouseEvent* aMouseEvent,
                                   nsIContent* aContent)
{
  NS_ASSERTION(aContent, "Mouse must be over something");

  // If pointer capture is set, we should suppress pointerover/pointerenter events
  // for all elements except element which have pointer capture.
  bool dispatch = !aMouseEvent->retargetedByPointerCapture;

  OverOutElementsWrapper* wrapper = GetWrapperByEventID(aMouseEvent);

  if (wrapper->mLastOverElement == aContent && dispatch)
    return;

  // Before firing mouseover, check for recursion
  if (aContent == wrapper->mFirstOverEventElement)
    return;

  // Check to see if we're a subdocument and if so update the parent
  // document's ESM state to indicate that the mouse is over the
  // content associated with our subdocument.
  EnsureDocument(mPresContext);
  if (nsIDocument *parentDoc = mDocument->GetParentDocument()) {
    if (nsIContent *docContent = parentDoc->FindContentForSubDocument(mDocument)) {
      if (nsIPresShell *parentShell = parentDoc->GetShell()) {
        EventStateManager* parentESM =
          parentShell->GetPresContext()->EventStateManager();
        parentESM->NotifyMouseOver(aMouseEvent, docContent);
      }
    }
  }
  // Firing the DOM event in the parent document could cause all kinds
  // of havoc.  Reverify and take care.
  if (wrapper->mLastOverElement == aContent && dispatch)
    return;

  // Remember mLastOverElement as the related content for the
  // DispatchMouseOrPointerEvent() call below, since NotifyMouseOut() resets it, bug 298477.
  nsCOMPtr<nsIContent> lastOverElement = wrapper->mLastOverElement;

  bool isPointer = aMouseEvent->mClass == ePointerEventClass;
  
  Maybe<EnterLeaveDispatcher> enterDispatcher;
  if (dispatch) {
    enterDispatcher.emplace(this, aContent, lastOverElement, aMouseEvent,
                            isPointer ? ePointerEnter : eMouseEnter);
  }

  NotifyMouseOut(aMouseEvent, aContent);

  // Store the first mouseOver event we fire and don't refire mouseOver
  // to that element while the first mouseOver is still ongoing.
  wrapper->mFirstOverEventElement = aContent;

  if (!isPointer) {
    SetContentState(aContent, NS_EVENT_STATE_HOVER);
  }

  if (dispatch) {
    // Fire mouseover
    wrapper->mLastOverFrame = 
      DispatchMouseOrPointerEvent(aMouseEvent,
                                  isPointer ? ePointerOver : eMouseOver,
                                  aContent, lastOverElement);
    wrapper->mLastOverElement = aContent;
  } else {
    wrapper->mLastOverFrame = nullptr;
    wrapper->mLastOverElement = nullptr;
  }

  // Turn recursion protection back off
  wrapper->mFirstOverEventElement = nullptr;
}

// Returns the center point of the window's inner content area.
// This is in widget coordinates, i.e. relative to the widget's top
// left corner, not in screen coordinates, the same units that
// UIEvent::refPoint is in.
//
// XXX Hack alert: XXX
// However, we do the computation in integer CSS pixels, NOT device pix,
// in order to fudge around the one-pixel error in innerHeight in fullscreen
// mode (see bug 799523 comment 35, and bug 729011). Using integer CSS pix
// makes us throw away the fractional error that results, rather than having
// it manifest as a potential one-device-pix discrepancy.
static LayoutDeviceIntPoint
GetWindowInnerRectCenter(nsPIDOMWindow* aWindow,
                         nsIWidget* aWidget,
                         nsPresContext* aContext)
{
  NS_ENSURE_TRUE(aWindow && aWidget && aContext, LayoutDeviceIntPoint(0, 0));

  float cssInnerX = 0.0;
  aWindow->GetMozInnerScreenX(&cssInnerX);
  int32_t innerX = int32_t(NS_round(cssInnerX));

  float cssInnerY = 0.0;
  aWindow->GetMozInnerScreenY(&cssInnerY);
  int32_t innerY = int32_t(NS_round(cssInnerY));
 
  int32_t innerWidth = 0;
  aWindow->GetInnerWidth(&innerWidth);

  int32_t innerHeight = 0;
  aWindow->GetInnerHeight(&innerHeight);

  nsIntRect screen;
  aWidget->GetScreenBounds(screen);

  int32_t cssScreenX = aContext->DevPixelsToIntCSSPixels(screen.x);
  int32_t cssScreenY = aContext->DevPixelsToIntCSSPixels(screen.y);

  return LayoutDeviceIntPoint(
    aContext->CSSPixelsToDevPixels(innerX - cssScreenX + innerWidth / 2),
    aContext->CSSPixelsToDevPixels(innerY - cssScreenY + innerHeight / 2));
}

void
EventStateManager::GeneratePointerEnterExit(EventMessage aMessage,
                                            WidgetMouseEvent* aEvent)
{
  WidgetPointerEvent pointerEvent(*aEvent);
  pointerEvent.mMessage = aMessage;
  GenerateMouseEnterExit(&pointerEvent);
}

void
EventStateManager::GenerateMouseEnterExit(WidgetMouseEvent* aMouseEvent)
{
  EnsureDocument(mPresContext);
  if (!mDocument)
    return;

  // Hold onto old target content through the event and reset after.
  nsCOMPtr<nsIContent> targetBeforeEvent = mCurrentTargetContent;

  switch(aMouseEvent->mMessage) {
  case eMouseMove:
    {
      // Mouse movement is reported on the MouseEvent.movement{X,Y} fields.
      // Movement is calculated in UIEvent::GetMovementPoint() as:
      //   previous_mousemove_refPoint - current_mousemove_refPoint.
      if (sIsPointerLocked && aMouseEvent->widget) {
        // The pointer is locked. If the pointer is not located at the center of
        // the window, dispatch a synthetic mousemove to return the pointer there.
        // Doing this between "real" pointer moves gives the impression that the
        // (locked) pointer can continue moving and won't stop at the screen
        // boundary. We cancel the synthetic event so that we don't end up
        // dispatching the centering move event to content.
        LayoutDeviceIntPoint center =
          GetWindowInnerRectCenter(mDocument->GetWindow(), aMouseEvent->widget,
                                   mPresContext);
        aMouseEvent->lastRefPoint = center;
        if (aMouseEvent->refPoint != center) {
          // Mouse move doesn't finish at the center of the window. Dispatch a
          // synthetic native mouse event to move the pointer back to the center
          // of the window, to faciliate more movement. But first, record that
          // we've dispatched a synthetic mouse movement, so we can cancel it
          // in the other branch here.
          sSynthCenteringPoint = center;
          aMouseEvent->widget->SynthesizeNativeMouseMove(
            center + aMouseEvent->widget->WidgetToScreenOffset(), nullptr);
        } else if (aMouseEvent->refPoint == sSynthCenteringPoint) {
          // This is the "synthetic native" event we dispatched to re-center the
          // pointer. Cancel it so we don't expose the centering move to content.
          aMouseEvent->mFlags.mPropagationStopped = true;
          // Clear sSynthCenteringPoint so we don't cancel other events
          // targeted at the center.
          sSynthCenteringPoint = kInvalidRefPoint;
        }
      } else if (sLastRefPoint == kInvalidRefPoint) {
        // We don't have a valid previous mousemove refPoint. This is either
        // the first move we've encountered, or the mouse has just re-entered
        // the application window. We should report (0,0) movement for this
        // case, so make the current and previous refPoints the same.
        aMouseEvent->lastRefPoint = aMouseEvent->refPoint;
      } else {
        aMouseEvent->lastRefPoint = sLastRefPoint;
      }

      // Update the last known refPoint with the current refPoint.
      sLastRefPoint = aMouseEvent->refPoint;

    }
  case ePointerMove:
  case ePointerDown:
    {
      // Get the target content target (mousemove target == mouseover target)
      nsCOMPtr<nsIContent> targetElement = GetEventTargetContent(aMouseEvent);
      if (!targetElement) {
        // We're always over the document root, even if we're only
        // over dead space in a page (whose frame is not associated with
        // any content) or in print preview dead space
        targetElement = mDocument->GetRootElement();
      }
      if (targetElement) {
        NotifyMouseOver(aMouseEvent, targetElement);
      }
    }
    break;
  case ePointerUp:
    {
      // Get the target content target (mousemove target == mouseover target)
      nsCOMPtr<nsIContent> targetElement = GetEventTargetContent(aMouseEvent);
      if (!targetElement) {
        // We're always over the document root, even if we're only
        // over dead space in a page (whose frame is not associated with
        // any content) or in print preview dead space
        targetElement = mDocument->GetRootElement();
      }
      if (targetElement) {
        OverOutElementsWrapper* helper = GetWrapperByEventID(aMouseEvent);
        if (helper) {
          helper->mLastOverElement = targetElement;
        }
        NotifyMouseOut(aMouseEvent, nullptr);
      }
    }
    break;
  case ePointerLeave:
  case ePointerCancel:
  case eMouseExitFromWidget:
    {
      // This is actually the window mouse exit or pointer leave event. We're not moving
      // into any new element.

      OverOutElementsWrapper* helper = GetWrapperByEventID(aMouseEvent);
      if (helper->mLastOverFrame &&
          nsContentUtils::GetTopLevelWidget(aMouseEvent->widget) !=
          nsContentUtils::GetTopLevelWidget(helper->mLastOverFrame->GetNearestWidget())) {
        // the Mouse/PointerOut event widget doesn't have same top widget with
        // mLastOverFrame, it's a spurious event for mLastOverFrame
        break;
      }

      // Reset sLastRefPoint, so that we'll know not to report any
      // movement the next time we re-enter the window.
      sLastRefPoint = kInvalidRefPoint;

      NotifyMouseOut(aMouseEvent, nullptr);
    }
    break;
  default:
    break;
  }

  // reset mCurretTargetContent to what it was
  mCurrentTargetContent = targetBeforeEvent;
}

OverOutElementsWrapper*
EventStateManager::GetWrapperByEventID(WidgetMouseEvent* aEvent)
{
  WidgetPointerEvent* pointer = aEvent->AsPointerEvent();
  if (!pointer) {
    MOZ_ASSERT(aEvent->AsMouseEvent() != nullptr);
    if (!mMouseEnterLeaveHelper) {
      mMouseEnterLeaveHelper = new OverOutElementsWrapper();
    }
    return mMouseEnterLeaveHelper;
  }
  nsRefPtr<OverOutElementsWrapper> helper;
  if (!mPointersEnterLeaveHelper.Get(pointer->pointerId, getter_AddRefs(helper))) {
    helper = new OverOutElementsWrapper();
    mPointersEnterLeaveHelper.Put(pointer->pointerId, helper);
  }

  return helper;
}

void
EventStateManager::SetPointerLock(nsIWidget* aWidget,
                                  nsIContent* aElement)
{
  // NOTE: aElement will be nullptr when unlocking.
  sIsPointerLocked = !!aElement;

  if (!aWidget) {
    return;
  }

  // Reset mouse wheel transaction
  WheelTransaction::EndTransaction();

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
    aWidget->SynthesizeNativeMouseMove(sLastRefPoint + aWidget->WidgetToScreenOffset(),
                                       nullptr);

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
    aWidget->SynthesizeNativeMouseMove(mPreLockPoint + aWidget->WidgetToScreenOffset(),
                                       nullptr);

    // Don't retarget events to this element any more.
    nsIPresShell::SetCapturingContent(nullptr, CAPTURE_POINTERLOCK);

    // Unsuppress DnD
    if (dragService) {
      dragService->Unsuppress();
    }
  }
}

void
EventStateManager::GenerateDragDropEnterExit(nsPresContext* aPresContext,
                                             WidgetDragEvent* aDragEvent)
{
  //Hold onto old target content through the event and reset after.
  nsCOMPtr<nsIContent> targetBeforeEvent = mCurrentTargetContent;

  switch(aDragEvent->mMessage) {
  case eDragOver:
    {
      // when dragging from one frame to another, events are fired in the
      // order: dragexit, dragenter, dragleave
      if (sLastDragOverFrame != mCurrentTarget) {
        //We'll need the content, too, to check if it changed separately from the frames.
        nsCOMPtr<nsIContent> lastContent;
        nsCOMPtr<nsIContent> targetContent;
        mCurrentTarget->GetContentForEvent(aDragEvent,
                                           getter_AddRefs(targetContent));

        if (sLastDragOverFrame) {
          //The frame has changed but the content may not have. Check before dispatching to content
          sLastDragOverFrame->GetContentForEvent(aDragEvent,
                                                 getter_AddRefs(lastContent));

          FireDragEnterOrExit(sLastDragOverFrame->PresContext(),
                              aDragEvent, NS_DRAGDROP_EXIT,
                              targetContent, lastContent, sLastDragOverFrame);
        }

        FireDragEnterOrExit(aPresContext, aDragEvent, eDragEnter,
                            lastContent, targetContent, mCurrentTarget);

        if (sLastDragOverFrame) {
          FireDragEnterOrExit(sLastDragOverFrame->PresContext(),
                              aDragEvent, eDragLeave,
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
        sLastDragOverFrame->GetContentForEvent(aDragEvent,
                                               getter_AddRefs(lastContent));

        nsRefPtr<nsPresContext> lastDragOverFramePresContext = sLastDragOverFrame->PresContext();
        FireDragEnterOrExit(lastDragOverFramePresContext,
                            aDragEvent, NS_DRAGDROP_EXIT,
                            nullptr, lastContent, sLastDragOverFrame);
        FireDragEnterOrExit(lastDragOverFramePresContext,
                            aDragEvent, eDragLeave,
                            nullptr, lastContent, sLastDragOverFrame);

        sLastDragOverFrame = nullptr;
      }
    }
    break;

  default:
    break;
  }

  //reset mCurretTargetContent to what it was
  mCurrentTargetContent = targetBeforeEvent;

  // Now flush all pending notifications, for better responsiveness.
  FlushPendingEvents(aPresContext);
}

void
EventStateManager::FireDragEnterOrExit(nsPresContext* aPresContext,
                                       WidgetDragEvent* aDragEvent,
                                       EventMessage aMessage,
                                       nsIContent* aRelatedTarget,
                                       nsIContent* aTargetContent,
                                       nsWeakFrame& aTargetFrame)
{
  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetDragEvent event(aDragEvent->mFlags.mIsTrusted, aMessage,
                        aDragEvent->widget);
  event.refPoint = aDragEvent->refPoint;
  event.modifiers = aDragEvent->modifiers;
  event.buttons = aDragEvent->buttons;
  event.relatedTarget = aRelatedTarget;
  event.inputSource = aDragEvent->inputSource;

  mCurrentTargetContent = aTargetContent;

  if (aTargetContent != aRelatedTarget) {
    //XXX This event should still go somewhere!!
    if (aTargetContent) {
      EventDispatcher::Dispatch(aTargetContent, aPresContext, &event,
                                nullptr, &status);
    }

    // adjust the drag hover if the dragenter event was cancelled or this is a drag exit
    if (status == nsEventStatus_eConsumeNoDefault ||
        aMessage == NS_DRAGDROP_EXIT) {
      SetContentState((aMessage == eDragEnter) ? aTargetContent : nullptr,
                      NS_EVENT_STATE_DRAGOVER);
    }

    // collect any changes to moz cursor settings stored in the event's
    // data transfer.
    if (aMessage == eDragLeave || aMessage == NS_DRAGDROP_EXIT ||
        aMessage == eDragEnter) {
      UpdateDragDataTransfer(&event);
    }
  }

  // Finally dispatch the event to the frame
  if (aTargetFrame)
    aTargetFrame->HandleEvent(aPresContext, &event, &status);
}

void
EventStateManager::UpdateDragDataTransfer(WidgetDragEvent* dragEvent)
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
EventStateManager::SetClickCount(nsPresContext* aPresContext,
                                 WidgetMouseEvent* aEvent,
                                 nsEventStatus* aStatus)
{
  nsCOMPtr<nsIContent> mouseContent;
  nsIContent* mouseContentParent = nullptr;
  if (mCurrentTarget) {
    mCurrentTarget->GetContentForEvent(aEvent, getter_AddRefs(mouseContent));
  }
  if (mouseContent) {
    if (mouseContent->IsNodeOfType(nsINode::eTEXT)) {
      mouseContent = mouseContent->GetParent();
    }
    if (mouseContent && mouseContent->IsRootOfNativeAnonymousSubtree()) {
      mouseContentParent = mouseContent->GetParent();
    }
  }

  switch (aEvent->button) {
  case WidgetMouseEvent::eLeftButton:
    if (aEvent->mMessage == eMouseDown) {
      mLastLeftMouseDownContent = mouseContent;
      mLastLeftMouseDownContentParent = mouseContentParent;
    } else if (aEvent->mMessage == eMouseUp) {
      if (mLastLeftMouseDownContent == mouseContent ||
          mLastLeftMouseDownContentParent == mouseContent ||
          mLastLeftMouseDownContent == mouseContentParent) {
        aEvent->clickCount = mLClickCount;
        mLClickCount = 0;
      } else {
        aEvent->clickCount = 0;
      }
      mLastLeftMouseDownContent = nullptr;
      mLastLeftMouseDownContentParent = nullptr;
    }
    break;

  case WidgetMouseEvent::eMiddleButton:
    if (aEvent->mMessage == eMouseDown) {
      mLastMiddleMouseDownContent = mouseContent;
      mLastMiddleMouseDownContentParent = mouseContentParent;
    } else if (aEvent->mMessage == eMouseUp) {
      if (mLastMiddleMouseDownContent == mouseContent ||
          mLastMiddleMouseDownContentParent == mouseContent ||
          mLastMiddleMouseDownContent == mouseContentParent) {
        aEvent->clickCount = mMClickCount;
        mMClickCount = 0;
      } else {
        aEvent->clickCount = 0;
      }
      mLastMiddleMouseDownContent = nullptr;
      mLastMiddleMouseDownContentParent = nullptr;
    }
    break;

  case WidgetMouseEvent::eRightButton:
    if (aEvent->mMessage == eMouseDown) {
      mLastRightMouseDownContent = mouseContent;
      mLastRightMouseDownContentParent = mouseContentParent;
    } else if (aEvent->mMessage == eMouseUp) {
      if (mLastRightMouseDownContent == mouseContent ||
          mLastRightMouseDownContentParent == mouseContent ||
          mLastRightMouseDownContent == mouseContentParent) {
        aEvent->clickCount = mRClickCount;
        mRClickCount = 0;
      } else {
        aEvent->clickCount = 0;
      }
      mLastRightMouseDownContent = nullptr;
      mLastRightMouseDownContentParent = nullptr;
    }
    break;
  }

  return NS_OK;
}

nsresult
EventStateManager::CheckForAndDispatchClick(nsPresContext* aPresContext,
                                            WidgetMouseEvent* aEvent,
                                            nsEventStatus* aStatus)
{
  nsresult ret = NS_OK;

  //If mouse is still over same element, clickcount will be > 1.
  //If it has moved it will be zero, so no click.
  if (0 != aEvent->clickCount) {
    //Check that the window isn't disabled before firing a click
    //(see bug 366544).
    if (aEvent->widget && !aEvent->widget->IsEnabled()) {
      return ret;
    }
    //fire click
    bool notDispatchToContents =
     (aEvent->button == WidgetMouseEvent::eMiddleButton ||
      aEvent->button == WidgetMouseEvent::eRightButton);

    WidgetMouseEvent event(aEvent->mFlags.mIsTrusted, eMouseClick,
                           aEvent->widget, WidgetMouseEvent::eReal);
    event.refPoint = aEvent->refPoint;
    event.clickCount = aEvent->clickCount;
    event.modifiers = aEvent->modifiers;
    event.buttons = aEvent->buttons;
    event.time = aEvent->time;
    event.timeStamp = aEvent->timeStamp;
    event.mFlags.mNoContentDispatch = notDispatchToContents;
    event.button = aEvent->button;
    event.inputSource = aEvent->inputSource;

    nsCOMPtr<nsIPresShell> presShell = mPresContext->GetPresShell();
    if (presShell) {
      nsCOMPtr<nsIContent> mouseContent = GetEventTargetContent(aEvent);
      // Click events apply to *elements* not nodes. At this point the target
      // content may have been reset to some non-element content, and so we need
      // to walk up the closest ancestor element, just like we do in
      // nsPresShell::HandlePositionedEvent.
      while (mouseContent && !mouseContent->IsElement()) {
        mouseContent = mouseContent->GetParent();
      }

      if (!mouseContent && !mCurrentTarget) {
        return NS_OK;
      }

      // HandleEvent clears out mCurrentTarget which we might need again
      nsWeakFrame currentTarget = mCurrentTarget;
      ret = presShell->HandleEventWithTarget(&event, currentTarget,
                                             mouseContent, aStatus);
      if (NS_SUCCEEDED(ret) && aEvent->clickCount == 2) {
        //fire double click
        WidgetMouseEvent event2(aEvent->mFlags.mIsTrusted, eMouseDoubleClick,
                                aEvent->widget, WidgetMouseEvent::eReal);
        event2.refPoint = aEvent->refPoint;
        event2.clickCount = aEvent->clickCount;
        event2.modifiers = aEvent->modifiers;
        event2.buttons = aEvent->buttons;
        event2.mFlags.mNoContentDispatch = notDispatchToContents;
        event2.button = aEvent->button;
        event2.inputSource = aEvent->inputSource;

        ret = presShell->HandleEventWithTarget(&event2, currentTarget,
                                               mouseContent, aStatus);
      }
    }
  }
  return ret;
}

nsIFrame*
EventStateManager::GetEventTarget()
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
EventStateManager::GetEventTargetContent(WidgetEvent* aEvent)
{
  if (aEvent &&
      (aEvent->mMessage == eFocus || aEvent->mMessage == eBlur)) {
    nsCOMPtr<nsIContent> content = GetFocusedContent();
    return content.forget();
  }

  if (mCurrentTargetContent) {
    nsCOMPtr<nsIContent> content = mCurrentTargetContent;
    return content.forget();
  }

  nsCOMPtr<nsIContent> content;

  nsIPresShell *presShell = mPresContext->GetPresShell();
  if (presShell) {
    content = presShell->GetEventTargetContent(aEvent);
  }

  // Some events here may set mCurrentTarget but not set the corresponding
  // event target in the PresShell.
  if (!content && mCurrentTarget) {
    mCurrentTarget->GetContentForEvent(aEvent, getter_AddRefs(content));
  }

  return content.forget();
}

static Element*
GetLabelTarget(nsIContent* aPossibleLabel)
{
  mozilla::dom::HTMLLabelElement* label =
    mozilla::dom::HTMLLabelElement::FromContent(aPossibleLabel);
  if (!label)
    return nullptr;

  return label->GetLabeledElement();
}

static nsIContent* FindCommonAncestor(nsIContent *aNode1, nsIContent *aNode2)
{
  // Find closest common ancestor
  if (aNode1 && aNode2) {
    // Find the nearest common ancestor by counting the distance to the
    // root and then walking up again, in pairs.
    int32_t offset = 0;
    nsIContent *anc1 = aNode1;
    for (;;) {
      ++offset;
      nsIContent* parent = anc1->GetFlattenedTreeParent();
      if (!parent)
        break;
      anc1 = parent;
    }
    nsIContent *anc2 = aNode2;
    for (;;) {
      --offset;
      nsIContent* parent = anc2->GetFlattenedTreeParent();
      if (!parent)
        break;
      anc2 = parent;
    }
    if (anc1 == anc2) {
      anc1 = aNode1;
      anc2 = aNode2;
      while (offset > 0) {
        anc1 = anc1->GetFlattenedTreeParent();
        --offset;
      }
      while (offset < 0) {
        anc2 = anc2->GetFlattenedTreeParent();
        ++offset;
      }
      while (anc1 != anc2) {
        anc1 = anc1->GetFlattenedTreeParent();
        anc2 = anc2->GetFlattenedTreeParent();
      }
      return anc1;
    }
  }
  return nullptr;
}

static Element*
GetParentElement(Element* aElement)
{
  nsIContent* p = aElement->GetParent();
  return (p && p->IsElement()) ? p->AsElement() : nullptr;
}

/* static */
void
EventStateManager::SetFullScreenState(Element* aElement, bool aIsFullScreen)
{
  DoStateChange(aElement, NS_EVENT_STATE_FULL_SCREEN, aIsFullScreen);
  Element* ancestor = aElement;
  while ((ancestor = GetParentElement(ancestor))) {
    DoStateChange(ancestor, NS_EVENT_STATE_FULL_SCREEN_ANCESTOR, aIsFullScreen);
    if (ancestor->State().HasState(NS_EVENT_STATE_FULL_SCREEN)) {
      // If we meet another fullscreen element, stop here.
      break;
    }
  }
}

/* static */
inline void
EventStateManager::DoStateChange(Element* aElement, EventStates aState,
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
EventStateManager::DoStateChange(nsIContent* aContent, EventStates aState,
                                 bool aStateAdded)
{
  if (aContent->IsElement()) {
    DoStateChange(aContent->AsElement(), aState, aStateAdded);
  }
}

/* static */
void
EventStateManager::UpdateAncestorState(nsIContent* aStartNode,
                                       nsIContent* aStopBefore,
                                       EventStates aState,
                                       bool aAddState)
{
  for (; aStartNode && aStartNode != aStopBefore;
       aStartNode = aStartNode->GetFlattenedTreeParent()) {
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
    for ( ; aStartNode; aStartNode = aStartNode->GetFlattenedTreeParent()) {
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
EventStateManager::SetContentState(nsIContent* aContent, EventStates aState)
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
      const nsStyleUserInterface* ui = mCurrentTarget->StyleUserInterface();
      if (ui->mUserInput == NS_STYLE_USER_INPUT_NONE)
        return false;
    }

    if (aState == NS_EVENT_STATE_ACTIVE) {
      // Editable content can never become active since their default actions
      // are disabled.  Watch out for editable content in native anonymous
      // subtrees though, as they belong to text controls.
      if (aContent && aContent->IsEditable() &&
          !aContent->IsInNativeAnonymousSubtree()) {
        aContent = nullptr;
      }
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
                     aContent->GetCrossShadowCurrentDoc() ==
                       mPresContext->PresShell()->GetDocument(),
                     "Unexpected document");
        nsIFrame *frame = aContent ? aContent->GetPrimaryFrame() : nullptr;
        if (frame && nsLayoutUtils::IsViewportScrollbarFrame(frame)) {
          // The scrollbars of viewport should not ignore the hover state.
          // Because they are *not* the content of the web page.
          newHover = aContent;
        } else {
          // All contents of the web page should ignore the hover state.
          newHover = nullptr;
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
    notifyContent2 = nullptr;
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

PLDHashOperator
EventStateManager::ResetLastOverForContent(
                     const uint32_t& aIdx,
                     nsRefPtr<OverOutElementsWrapper>& aElemWrapper,
                     void* aClosure)
{
  nsIContent* content = static_cast<nsIContent*>(aClosure);
  if (aElemWrapper && aElemWrapper->mLastOverElement &&
      nsContentUtils::ContentIsDescendantOf(aElemWrapper->mLastOverElement, content)) {
    aElemWrapper->mLastOverElement = nullptr;
  }

  return PL_DHASH_NEXT;
}

void
EventStateManager::ContentRemoved(nsIDocument* aDocument, nsIContent* aContent)
{
  /*
   * Anchor and area elements when focused or hovered might make the UI to show
   * the current link. We want to make sure that the UI gets informed when they
   * are actually removed from the DOM.
   */
  if (aContent->IsAnyOfHTMLElements(nsGkAtoms::a, nsGkAtoms::area) &&
      (aContent->AsElement()->State().HasAtLeastOneOfStates(NS_EVENT_STATE_FOCUS |
                                                            NS_EVENT_STATE_HOVER))) {
    nsGenericHTMLElement* element = static_cast<nsGenericHTMLElement*>(aContent);
    element->LeaveLink(
      element->GetPresContext(nsGenericHTMLElement::eForComposedDoc));
  }

  IMEStateManager::OnRemoveContent(mPresContext, aContent);

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
    sDragOverContent = nullptr;
  }

  // See bug 292146 for why we want to null this out
  ResetLastOverForContent(0, mMouseEnterLeaveHelper, aContent);
  mPointersEnterLeaveHelper.Enumerate(
    &EventStateManager::ResetLastOverForContent, aContent);
}

bool
EventStateManager::EventStatusOK(WidgetGUIEvent* aEvent)
{
  return !(aEvent->mMessage == eMouseDown &&
           aEvent->AsMouseEvent()->button == WidgetMouseEvent::eLeftButton &&
           !sNormalLMouseEventInProcess);
}

//-------------------------------------------
// Access Key Registration
//-------------------------------------------
void
EventStateManager::RegisterAccessKey(nsIContent* aContent, uint32_t aKey)
{
  if (aContent && mAccessKeys.IndexOf(aContent) == -1)
    mAccessKeys.AppendObject(aContent);
}

void
EventStateManager::UnregisterAccessKey(nsIContent* aContent, uint32_t aKey)
{
  if (aContent)
    mAccessKeys.RemoveObject(aContent);
}

uint32_t
EventStateManager::GetRegisteredAccessKey(nsIContent* aContent)
{
  MOZ_ASSERT(aContent);

  if (mAccessKeys.IndexOf(aContent) == -1)
    return 0;

  nsAutoString accessKey;
  aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, accessKey);
  return accessKey.First();
}

void
EventStateManager::EnsureDocument(nsPresContext* aPresContext)
{
  if (!mDocument)
    mDocument = aPresContext->Document();
}

void
EventStateManager::FlushPendingEvents(nsPresContext* aPresContext)
{
  NS_PRECONDITION(nullptr != aPresContext, "nullptr ptr");
  nsIPresShell *shell = aPresContext->GetPresShell();
  if (shell) {
    shell->FlushPendingNotifications(Flush_InterruptibleLayout);
  }
}

nsIContent*
EventStateManager::GetFocusedContent()
{
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm || !mDocument)
    return nullptr;

  nsCOMPtr<nsPIDOMWindow> focusedWindow;
  return nsFocusManager::GetFocusedDescendant(mDocument->GetWindow(), false,
                                              getter_AddRefs(focusedWindow));
}

//-------------------------------------------------------
// Return true if the docshell is visible

bool
EventStateManager::IsShellVisible(nsIDocShell* aShell)
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
EventStateManager::DoContentCommandEvent(WidgetContentCommandEvent* aEvent)
{
  EnsureDocument(mPresContext);
  NS_ENSURE_TRUE(mDocument, NS_ERROR_FAILURE);
  nsCOMPtr<nsPIDOMWindow> window(mDocument->GetWindow());
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsCOMPtr<nsPIWindowRoot> root = window->GetTopWindowRoot();
  NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);
  const char* cmd;
  switch (aEvent->mMessage) {
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
      switch (aEvent->mMessage) {
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
EventStateManager::DoContentCommandScrollEvent(
                     WidgetContentCommandEvent* aEvent)
{
  NS_ENSURE_TRUE(mPresContext, NS_ERROR_NOT_AVAILABLE);
  nsIPresShell* ps = mPresContext->GetPresShell();
  NS_ENSURE_TRUE(ps, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(aEvent->mScroll.mAmount != 0, NS_ERROR_INVALID_ARG);

  nsIScrollableFrame::ScrollUnit scrollUnit;
  switch (aEvent->mScroll.mUnit) {
    case WidgetContentCommandEvent::eCmdScrollUnit_Line:
      scrollUnit = nsIScrollableFrame::LINES;
      break;
    case WidgetContentCommandEvent::eCmdScrollUnit_Page:
      scrollUnit = nsIScrollableFrame::PAGES;
      break;
    case WidgetContentCommandEvent::eCmdScrollUnit_Whole:
      scrollUnit = nsIScrollableFrame::WHOLE;
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }

  aEvent->mSucceeded = true;

  nsIScrollableFrame* sf =
    ps->GetFrameToScrollAsScrollable(nsIPresShell::eEither);
  aEvent->mIsEnabled = sf ?
    (aEvent->mScroll.mIsHorizontal ?
      WheelHandlingUtils::CanScrollOn(sf, aEvent->mScroll.mAmount, 0) :
      WheelHandlingUtils::CanScrollOn(sf, 0, aEvent->mScroll.mAmount)) : false;

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
EventStateManager::DoQuerySelectedText(WidgetQueryContentEvent* aEvent)
{
  if (RemoteQueryContentEvent(aEvent)) {
    return;
  }
  ContentEventHandler handler(mPresContext);
  handler.OnQuerySelectedText(aEvent);
}

void
EventStateManager::SetActiveManager(EventStateManager* aNewESM,
                                    nsIContent* aContent)
{
  if (sActiveESM && aNewESM != sActiveESM) {
    sActiveESM->SetContentState(nullptr, NS_EVENT_STATE_ACTIVE);
  }
  sActiveESM = aNewESM;
  if (sActiveESM && aContent) {
    sActiveESM->SetContentState(aContent, NS_EVENT_STATE_ACTIVE);
  }
}

void
EventStateManager::ClearGlobalActiveContent(EventStateManager* aClearer)
{
  if (aClearer) {
    aClearer->SetContentState(nullptr, NS_EVENT_STATE_ACTIVE);
    if (sDragOverContent) {
      aClearer->SetContentState(nullptr, NS_EVENT_STATE_DRAGOVER);
    }
  }
  if (sActiveESM && aClearer != sActiveESM) {
    sActiveESM->SetContentState(nullptr, NS_EVENT_STATE_ACTIVE);
  }
  sActiveESM = nullptr;
}

/******************************************************************/
/* mozilla::EventStateManager::DeltaAccumulator                   */
/******************************************************************/

void
EventStateManager::DeltaAccumulator::InitLineOrPageDelta(
                                       nsIFrame* aTargetFrame,
                                       EventStateManager* aESM,
                                       WidgetWheelEvent* aEvent)
{
  MOZ_ASSERT(aESM);
  MOZ_ASSERT(aEvent);

  // Reset if the previous wheel event is too old.
  if (!mLastTime.IsNull()) {
    TimeDuration duration = TimeStamp::Now() - mLastTime;
    if (duration.ToMilliseconds() > WheelTransaction::GetTimeoutTime()) {
      Reset();
    }
  }
  // If we have accumulated delta,  we may need to reset it.
  if (IsInTransaction()) {
    // If wheel event type is changed, reset the values.
    if (mHandlingDeltaMode != aEvent->deltaMode ||
        mIsNoLineOrPageDeltaDevice != aEvent->mIsNoLineOrPageDelta) {
      Reset();
    } else {
      // If the delta direction is changed, we should reset only the
      // accumulated values.
      if (mX && aEvent->deltaX && ((aEvent->deltaX > 0.0) != (mX > 0.0))) {
        mX = mPendingScrollAmountX = 0.0;
      }
      if (mY && aEvent->deltaY && ((aEvent->deltaY > 0.0) != (mY > 0.0))) {
        mY = mPendingScrollAmountY = 0.0;
      }
    }
  }

  mHandlingDeltaMode = aEvent->deltaMode;
  mIsNoLineOrPageDeltaDevice = aEvent->mIsNoLineOrPageDelta;

  // If it's handling neither a device that does not provide line or page deltas
  // nor delta values multiplied by prefs, we must not modify lineOrPageDelta
  // values.
  if (!mIsNoLineOrPageDeltaDevice &&
      !EventStateManager::WheelPrefs::GetInstance()->
        NeedToComputeLineOrPageDelta(aEvent)) {
    // Set the delta values to mX and mY.  They would be used when above block
    // resets mX/mY/mPendingScrollAmountX/mPendingScrollAmountY if the direction
    // is changed.
    // NOTE: We shouldn't accumulate the delta values, it might could cause
    //       overflow even though it's not a realistic situation.
    if (aEvent->deltaX) {
      mX = aEvent->deltaX;
    }
    if (aEvent->deltaY) {
      mY = aEvent->deltaY;
    }
    mLastTime = TimeStamp::Now();
    return;
  }

  mX += aEvent->deltaX;
  mY += aEvent->deltaY;

  if (mHandlingDeltaMode == nsIDOMWheelEvent::DOM_DELTA_PIXEL) {
    // Records pixel delta values and init lineOrPageDeltaX and
    // lineOrPageDeltaY for wheel events which are caused by pixel only
    // devices.  Ignore mouse wheel transaction for computing this.  The
    // lineOrPageDelta values will be used by dispatching legacy
    // eMouseScrollEventClass (DOMMouseScroll) but not be used for scrolling
    // of default action.  The transaction should be used only for the default
    // action.
    nsIScrollableFrame* scrollTarget =
      aESM->ComputeScrollTarget(aTargetFrame, aEvent,
                                COMPUTE_LEGACY_MOUSE_SCROLL_EVENT_TARGET);
    nsIFrame* frame = do_QueryFrame(scrollTarget);
    nsPresContext* pc =
      frame ? frame->PresContext() : aTargetFrame->PresContext();
    nsSize scrollAmount = aESM->GetScrollAmount(pc, aEvent, scrollTarget);
    nsIntSize scrollAmountInCSSPixels(
      nsPresContext::AppUnitsToIntCSSPixels(scrollAmount.width),
      nsPresContext::AppUnitsToIntCSSPixels(scrollAmount.height));

    aEvent->lineOrPageDeltaX = RoundDown(mX) / scrollAmountInCSSPixels.width;
    aEvent->lineOrPageDeltaY = RoundDown(mY) / scrollAmountInCSSPixels.height;

    mX -= aEvent->lineOrPageDeltaX * scrollAmountInCSSPixels.width;
    mY -= aEvent->lineOrPageDeltaY * scrollAmountInCSSPixels.height;
  } else {
    aEvent->lineOrPageDeltaX = RoundDown(mX);
    aEvent->lineOrPageDeltaY = RoundDown(mY);
    mX -= aEvent->lineOrPageDeltaX;
    mY -= aEvent->lineOrPageDeltaY;
  }

  mLastTime = TimeStamp::Now();
}

void
EventStateManager::DeltaAccumulator::Reset()
{
  mX = mY = 0.0;
  mPendingScrollAmountX = mPendingScrollAmountY = 0.0;
  mHandlingDeltaMode = UINT32_MAX;
  mIsNoLineOrPageDeltaDevice = false;
}

nsIntPoint
EventStateManager::DeltaAccumulator::ComputeScrollAmountForDefaultAction(
                     WidgetWheelEvent* aEvent,
                     const nsIntSize& aScrollAmountInDevPixels)
{
  MOZ_ASSERT(aEvent);

  // If the wheel event is line scroll and the delta value is computed from
  // system settings, allow to override the system speed.
  bool allowScrollSpeedOverride =
    (!aEvent->customizedByUserPrefs &&
     aEvent->deltaMode == nsIDOMWheelEvent::DOM_DELTA_LINE);
  DeltaValues acceleratedDelta =
    WheelTransaction::AccelerateWheelDelta(aEvent, allowScrollSpeedOverride);

  nsIntPoint result(0, 0);
  if (aEvent->deltaMode == nsIDOMWheelEvent::DOM_DELTA_PIXEL) {
    mPendingScrollAmountX += acceleratedDelta.deltaX;
    mPendingScrollAmountY += acceleratedDelta.deltaY;
  } else {
    mPendingScrollAmountX +=
      aScrollAmountInDevPixels.width * acceleratedDelta.deltaX;
    mPendingScrollAmountY +=
      aScrollAmountInDevPixels.height * acceleratedDelta.deltaY;
  }
  result.x = RoundDown(mPendingScrollAmountX);
  result.y = RoundDown(mPendingScrollAmountY);
  mPendingScrollAmountX -= result.x;
  mPendingScrollAmountY -= result.y;

  return result;
}

/******************************************************************/
/* mozilla::EventStateManager::WheelPrefs                         */
/******************************************************************/

// static
EventStateManager::WheelPrefs*
EventStateManager::WheelPrefs::GetInstance()
{
  if (!sInstance) {
    sInstance = new WheelPrefs();
  }
  return sInstance;
}

// static
void
EventStateManager::WheelPrefs::Shutdown()
{
  delete sInstance;
  sInstance = nullptr;
}

// static
void
EventStateManager::WheelPrefs::OnPrefChanged(const char* aPrefName,
                                             void* aClosure)
{
  // forget all prefs, it's not problem for performance.
  sInstance->Reset();
  DeltaAccumulator::GetInstance()->Reset();
}

EventStateManager::WheelPrefs::WheelPrefs()
{
  Reset();
  Preferences::RegisterCallback(OnPrefChanged, "mousewheel.", nullptr);
}

EventStateManager::WheelPrefs::~WheelPrefs()
{
  Preferences::UnregisterCallback(OnPrefChanged, "mousewheel.", nullptr);
}

void
EventStateManager::WheelPrefs::Reset()
{
  memset(mInit, 0, sizeof(mInit));

}

EventStateManager::WheelPrefs::Index
EventStateManager::WheelPrefs::GetIndexFor(WidgetWheelEvent* aEvent)
{
  if (!aEvent) {
    return INDEX_DEFAULT;
  }

  Modifiers modifiers =
    (aEvent->modifiers & (MODIFIER_ALT |
                          MODIFIER_CONTROL |
                          MODIFIER_META |
                          MODIFIER_SHIFT |
                          MODIFIER_OS));

  switch (modifiers) {
    case MODIFIER_ALT:
      return INDEX_ALT;
    case MODIFIER_CONTROL:
      return INDEX_CONTROL;
    case MODIFIER_META:
      return INDEX_META;
    case MODIFIER_SHIFT:
      return INDEX_SHIFT;
    case MODIFIER_OS:
      return INDEX_OS;
    default:
      // If two or more modifier keys are pressed, we should use default
      // settings.
      return INDEX_DEFAULT;
  }
}

void
EventStateManager::WheelPrefs::GetBasePrefName(
                     EventStateManager::WheelPrefs::Index aIndex,
                     nsACString& aBasePrefName)
{
  aBasePrefName.AssignLiteral("mousewheel.");
  switch (aIndex) {
    case INDEX_ALT:
      aBasePrefName.AppendLiteral("with_alt.");
      break;
    case INDEX_CONTROL:
      aBasePrefName.AppendLiteral("with_control.");
      break;
    case INDEX_META:
      aBasePrefName.AppendLiteral("with_meta.");
      break;
    case INDEX_SHIFT:
      aBasePrefName.AppendLiteral("with_shift.");
      break;
    case INDEX_OS:
      aBasePrefName.AppendLiteral("with_win.");
      break;
    case INDEX_DEFAULT:
    default:
      aBasePrefName.AppendLiteral("default.");
      break;
  }
}

void
EventStateManager::WheelPrefs::Init(EventStateManager::WheelPrefs::Index aIndex)
{
  if (mInit[aIndex]) {
    return;
  }
  mInit[aIndex] = true;

  nsAutoCString basePrefName;
  GetBasePrefName(aIndex, basePrefName);

  nsAutoCString prefNameX(basePrefName);
  prefNameX.AppendLiteral("delta_multiplier_x");
  mMultiplierX[aIndex] =
    static_cast<double>(Preferences::GetInt(prefNameX.get(), 100)) / 100;

  nsAutoCString prefNameY(basePrefName);
  prefNameY.AppendLiteral("delta_multiplier_y");
  mMultiplierY[aIndex] =
    static_cast<double>(Preferences::GetInt(prefNameY.get(), 100)) / 100;

  nsAutoCString prefNameZ(basePrefName);
  prefNameZ.AppendLiteral("delta_multiplier_z");
  mMultiplierZ[aIndex] =
    static_cast<double>(Preferences::GetInt(prefNameZ.get(), 100)) / 100;

  nsAutoCString prefNameAction(basePrefName);
  prefNameAction.AppendLiteral("action");
  int32_t action = Preferences::GetInt(prefNameAction.get(), ACTION_SCROLL);
  if (action < int32_t(ACTION_NONE) || action > int32_t(ACTION_LAST)) {
    NS_WARNING("Unsupported action pref value, replaced with 'Scroll'.");
    action = ACTION_SCROLL;
  }
  mActions[aIndex] = static_cast<Action>(action);

  // Compute action values overridden by .override_x pref.
  // At present, override is possible only for the x-direction
  // because this pref is introduced mainly for tilt wheels.
  prefNameAction.AppendLiteral(".override_x");
  int32_t actionOverrideX = Preferences::GetInt(prefNameAction.get(), -1);
  if (actionOverrideX < -1 || actionOverrideX > int32_t(ACTION_LAST)) {
    NS_WARNING("Unsupported action override pref value, didn't override.");
    actionOverrideX = -1;
  }
  mOverriddenActionsX[aIndex] = (actionOverrideX == -1)
                              ? static_cast<Action>(action)
                              : static_cast<Action>(actionOverrideX);
}

void
EventStateManager::WheelPrefs::ApplyUserPrefsToDelta(WidgetWheelEvent* aEvent)
{
  Index index = GetIndexFor(aEvent);
  Init(index);

  aEvent->deltaX *= mMultiplierX[index];
  aEvent->deltaY *= mMultiplierY[index];
  aEvent->deltaZ *= mMultiplierZ[index];

  // If the multiplier is 1.0 or -1.0, i.e., it doesn't change the absolute
  // value, we should use lineOrPageDelta values which were set by widget.
  // Otherwise, we need to compute them from accumulated delta values.
  if (!NeedToComputeLineOrPageDelta(aEvent)) {
    aEvent->lineOrPageDeltaX *= static_cast<int32_t>(mMultiplierX[index]);
    aEvent->lineOrPageDeltaY *= static_cast<int32_t>(mMultiplierY[index]);
  } else {
    aEvent->lineOrPageDeltaX = 0;
    aEvent->lineOrPageDeltaY = 0;
  }

  aEvent->customizedByUserPrefs =
    ((mMultiplierX[index] != 1.0) || (mMultiplierY[index] != 1.0) ||
     (mMultiplierZ[index] != 1.0));
}

void
EventStateManager::WheelPrefs::CancelApplyingUserPrefsFromOverflowDelta(
                                 WidgetWheelEvent* aEvent)
{
  Index index = GetIndexFor(aEvent);
  Init(index);

  // XXX If the multiplier pref value is negative, the scroll direction was
  //     changed and caused to scroll different direction.  In such case,
  //     this method reverts the sign of overflowDelta.  Does it make widget
  //     happy?  Although, widget can know the pref applied delta values by
  //     referrencing the deltaX and deltaY of the event.

  if (mMultiplierX[index]) {
    aEvent->overflowDeltaX /= mMultiplierX[index];
  }
  if (mMultiplierY[index]) {
    aEvent->overflowDeltaY /= mMultiplierY[index];
  }
}

EventStateManager::WheelPrefs::Action
EventStateManager::WheelPrefs::ComputeActionFor(WidgetWheelEvent* aEvent)
{
  Index index = GetIndexFor(aEvent);
  Init(index);

  bool deltaXPreferred =
    (Abs(aEvent->deltaX) > Abs(aEvent->deltaY) &&
     Abs(aEvent->deltaX) > Abs(aEvent->deltaZ));
  Action* actions = deltaXPreferred ? mOverriddenActionsX : mActions;
  if (actions[index] == ACTION_NONE || actions[index] == ACTION_SCROLL) {
    return actions[index];
  }

  // Momentum events shouldn't run special actions.
  if (aEvent->isMomentum) {
    // Use the default action.  Note that user might kill the wheel scrolling.
    Init(INDEX_DEFAULT);
    return (actions[INDEX_DEFAULT] == ACTION_SCROLL) ? ACTION_SCROLL :
                                                       ACTION_NONE;
  }

  return actions[index];
}

bool
EventStateManager::WheelPrefs::NeedToComputeLineOrPageDelta(
                                 WidgetWheelEvent* aEvent)
{
  Index index = GetIndexFor(aEvent);
  Init(index);

  return (mMultiplierX[index] != 1.0 && mMultiplierX[index] != -1.0) ||
         (mMultiplierY[index] != 1.0 && mMultiplierY[index] != -1.0);
}

bool
EventStateManager::WheelPrefs::HasUserPrefsForDelta(WidgetWheelEvent* aEvent)
{
  Index index = GetIndexFor(aEvent);
  Init(index);

  return mMultiplierX[index] != 1.0 ||
         mMultiplierY[index] != 1.0;
}

bool
EventStateManager::WheelEventIsScrollAction(WidgetWheelEvent* aEvent)
{
  return aEvent->mMessage == NS_WHEEL_WHEEL &&
         WheelPrefs::GetInstance()->ComputeActionFor(aEvent) == WheelPrefs::ACTION_SCROLL;
}

bool
EventStateManager::WheelEventNeedsDeltaMultipliers(WidgetWheelEvent* aEvent)
{
  return WheelPrefs::GetInstance()->HasUserPrefsForDelta(aEvent);
}

bool
EventStateManager::WheelPrefs::IsOverOnePageScrollAllowedX(
                                 WidgetWheelEvent* aEvent)
{
  Index index = GetIndexFor(aEvent);
  Init(index);
  return Abs(mMultiplierX[index]) >=
           MIN_MULTIPLIER_VALUE_ALLOWING_OVER_ONE_PAGE_SCROLL;
}

bool
EventStateManager::WheelPrefs::IsOverOnePageScrollAllowedY(
                                 WidgetWheelEvent* aEvent)
{
  Index index = GetIndexFor(aEvent);
  Init(index);
  return Abs(mMultiplierY[index]) >=
           MIN_MULTIPLIER_VALUE_ALLOWING_OVER_ONE_PAGE_SCROLL;
}

/******************************************************************/
/* mozilla::EventStateManager::Prefs                              */
/******************************************************************/

bool EventStateManager::Prefs::sKeyCausesActivation = true;
bool EventStateManager::Prefs::sClickHoldContextMenu = false;
int32_t EventStateManager::Prefs::sGenericAccessModifierKey = -1;
int32_t EventStateManager::Prefs::sChromeAccessModifierMask = 0;
int32_t EventStateManager::Prefs::sContentAccessModifierMask = 0;

// static
void
EventStateManager::Prefs::Init()
{
  DebugOnly<nsresult> rv = Preferences::RegisterCallback(OnChange, "dom.popup_allowed_events");
  MOZ_ASSERT(NS_SUCCEEDED(rv),
             "Failed to observe \"dom.popup_allowed_events\"");

  static bool sPrefsAlreadyCached = false;
  if (sPrefsAlreadyCached) {
    return;
  }

  rv = Preferences::AddBoolVarCache(&sKeyCausesActivation,
                                    "accessibility.accesskeycausesactivation",
                                    sKeyCausesActivation);
  MOZ_ASSERT(NS_SUCCEEDED(rv),
             "Failed to observe \"accessibility.accesskeycausesactivation\"");
  rv = Preferences::AddBoolVarCache(&sClickHoldContextMenu,
                                    "ui.click_hold_context_menus",
                                    sClickHoldContextMenu);
  MOZ_ASSERT(NS_SUCCEEDED(rv),
             "Failed to observe \"ui.click_hold_context_menus\"");
  rv = Preferences::AddIntVarCache(&sGenericAccessModifierKey,
                                   "ui.key.generalAccessKey",
                                   sGenericAccessModifierKey);
  MOZ_ASSERT(NS_SUCCEEDED(rv),
             "Failed to observe \"ui.key.generalAccessKey\"");
  rv = Preferences::AddIntVarCache(&sChromeAccessModifierMask,
                                   "ui.key.chromeAccess",
                                   sChromeAccessModifierMask);
  MOZ_ASSERT(NS_SUCCEEDED(rv),
             "Failed to observe \"ui.key.chromeAccess\"");
  rv = Preferences::AddIntVarCache(&sContentAccessModifierMask,
                                   "ui.key.contentAccess",
                                   sContentAccessModifierMask);
  MOZ_ASSERT(NS_SUCCEEDED(rv),
             "Failed to observe \"ui.key.contentAccess\"");
  sPrefsAlreadyCached = true;
}

// static
void
EventStateManager::Prefs::OnChange(const char* aPrefName, void*)
{
  nsDependentCString prefName(aPrefName);
  if (prefName.EqualsLiteral("dom.popup_allowed_events")) {
    Event::PopupAllowedEventsChanged();
  }
}

// static
void
EventStateManager::Prefs::Shutdown()
{
  Preferences::UnregisterCallback(OnChange, "dom.popup_allowed_events");
}

// static
int32_t
EventStateManager::Prefs::ChromeAccessModifierMask()
{
  return GetAccessModifierMask(nsIDocShellTreeItem::typeChrome);
}

// static
int32_t
EventStateManager::Prefs::ContentAccessModifierMask()
{
  return GetAccessModifierMask(nsIDocShellTreeItem::typeContent);
}

// static
int32_t
EventStateManager::Prefs::GetAccessModifierMask(int32_t aItemType)
{
  switch (sGenericAccessModifierKey) {
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
      return sChromeAccessModifierMask;
    case nsIDocShellTreeItem::typeContent:
      return sContentAccessModifierMask;
    default:
      return 0;
  }
}

/******************************************************************/
/* mozilla::AutoHandlingUserInputStatePusher                      */
/******************************************************************/

AutoHandlingUserInputStatePusher::AutoHandlingUserInputStatePusher(
                                    bool aIsHandlingUserInput,
                                    WidgetEvent* aEvent,
                                    nsIDocument* aDocument) :
  mIsHandlingUserInput(aIsHandlingUserInput),
  mIsMouseDown(aEvent && aEvent->mMessage == eMouseDown),
  mResetFMMouseButtonHandlingState(false)
{
  if (!aIsHandlingUserInput) {
    return;
  }
  EventStateManager::StartHandlingUserInput();
  if (mIsMouseDown) {
    nsIPresShell::SetCapturingContent(nullptr, 0);
    nsIPresShell::AllowMouseCapture(true);
  }
  if (!aDocument || !aEvent || !aEvent->mFlags.mIsTrusted) {
    return;
  }
  mResetFMMouseButtonHandlingState =
    (aEvent->mMessage == eMouseDown || aEvent->mMessage == eMouseUp);
  if (mResetFMMouseButtonHandlingState) {
    nsFocusManager* fm = nsFocusManager::GetFocusManager();
    NS_ENSURE_TRUE_VOID(fm);
    // If it's in modal state, mouse button event handling may be nested.
    // E.g., a modal dialog is opened at mousedown or mouseup event handler
    // and the dialog is clicked.  Therefore, we should store current
    // mouse button event handling document if nsFocusManager already has it.
    mMouseButtonEventHandlingDocument =
      fm->SetMouseButtonHandlingDocument(aDocument);
  }
}

AutoHandlingUserInputStatePusher::~AutoHandlingUserInputStatePusher()
{
  if (!mIsHandlingUserInput) {
    return;
  }
  EventStateManager::StopHandlingUserInput();
  if (mIsMouseDown) {
    nsIPresShell::AllowMouseCapture(false);
  }
  if (mResetFMMouseButtonHandlingState) {
    nsFocusManager* fm = nsFocusManager::GetFocusManager();
    NS_ENSURE_TRUE_VOID(fm);
    nsCOMPtr<nsIDocument> handlingDocument =
      fm->SetMouseButtonHandlingDocument(mMouseButtonEventHandlingDocument);
  }
}

} // namespace mozilla

