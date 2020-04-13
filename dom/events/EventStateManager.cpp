/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/PresShell.h"
#include "mozilla/ScrollTypes.h"
#include "mozilla/TextComposition.h"
#include "mozilla/TextEditor.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/Telemetry.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/DragEvent.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/FrameLoaderBinding.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/UIEvent.h"
#include "mozilla/dom/UIEventBinding.h"
#include "mozilla/dom/UserActivation.h"
#include "mozilla/dom/WheelEventBinding.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/StaticPrefs_mousewheel.h"
#include "mozilla/StaticPrefs_ui.h"
#include "mozilla/StaticPrefs_zoom.h"

#include "ContentEventHandler.h"
#include "IMEContentObserver.h"
#include "WheelHandlingHelper.h"
#include "RemoteDragStartData.h"

#include "nsCommandParams.h"
#include "nsCOMPtr.h"
#include "nsCopySupport.h"
#include "nsFocusManager.h"
#include "nsGenericHTMLElement.h"
#include "nsIClipboard.h"
#include "nsIContent.h"
#include "nsIContentInlines.h"
#include "mozilla/dom/Document.h"
#include "nsIFrame.h"
#include "nsFrameLoaderOwner.h"
#include "nsIWidget.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsIFormControl.h"
#include "nsComboboxControlFrame.h"
#include "nsIScrollableFrame.h"
#include "nsIDOMXULControlElement.h"
#include "nsNameSpaceManager.h"
#include "nsIBaseWindow.h"
#include "nsFrameSelection.h"
#include "nsPIDOMWindow.h"
#include "nsPIWindowRoot.h"
#include "nsIWebNavigation.h"
#include "nsIContentViewer.h"
#include "nsFrameManager.h"
#include "nsIBrowserChild.h"
#include "nsPluginFrame.h"
#include "nsMenuPopupFrame.h"

#include "nsIObserverService.h"
#include "nsIDocShell.h"

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
#include "nsIDragService.h"
#include "nsIDragSession.h"
#include "mozilla/dom/DataTransfer.h"
#include "nsContentAreaDragDrop.h"
#ifdef MOZ_XUL
#  include "nsTreeBodyFrame.h"
#endif
#include "nsIController.h"
#include "mozilla/Services.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/HTMLLabelElement.h"
#include "mozilla/dom/Selection.h"

#include "mozilla/Preferences.h"
#include "mozilla/LookAndFeel.h"
#include "GeckoProfiler.h"
#include "Units.h"

#ifdef XP_MACOSX
#  import <ApplicationServices/ApplicationServices.h>
#endif

namespace mozilla {

using namespace dom;

static const LayoutDeviceIntPoint kInvalidRefPoint =
    LayoutDeviceIntPoint(-1, -1);

static uint32_t gMouseOrKeyboardEventCounter = 0;
static nsITimer* gUserInteractionTimer = nullptr;
static nsITimerCallback* gUserInteractionTimerCallback = nullptr;

static const double kCursorLoadingTimeout = 1000;  // ms
static AutoWeakFrame gLastCursorSourceFrame;
static TimeStamp gLastCursorUpdateTime;

static inline int32_t RoundDown(double aDouble) {
  return (aDouble > 0) ? static_cast<int32_t>(floor(aDouble))
                       : static_cast<int32_t>(ceil(aDouble));
}

/******************************************************************/
/* mozilla::UITimerCallback                                       */
/******************************************************************/

class UITimerCallback final : public nsITimerCallback, public nsINamed {
 public:
  UITimerCallback() : mPreviousCount(0) {}
  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED
 private:
  ~UITimerCallback() = default;
  uint32_t mPreviousCount;
};

NS_IMPL_ISUPPORTS(UITimerCallback, nsITimerCallback, nsINamed)

// If aTimer is nullptr, this method always sends "user-interaction-inactive"
// notification.
NS_IMETHODIMP
UITimerCallback::Notify(nsITimer* aTimer) {
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) return NS_ERROR_FAILURE;
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

NS_IMETHODIMP
UITimerCallback::GetName(nsACString& aName) {
  aName.AssignLiteral("UITimerCallback_timer");
  return NS_OK;
}

/******************************************************************/
/* mozilla::OverOutElementsWrapper                                */
/******************************************************************/

OverOutElementsWrapper::OverOutElementsWrapper() : mLastOverFrame(nullptr) {}

OverOutElementsWrapper::~OverOutElementsWrapper() = default;

NS_IMPL_CYCLE_COLLECTION(OverOutElementsWrapper, mLastOverElement,
                         mFirstOverEventElement, mFirstOutEventElement)
NS_IMPL_CYCLE_COLLECTING_ADDREF(OverOutElementsWrapper)
NS_IMPL_CYCLE_COLLECTING_RELEASE(OverOutElementsWrapper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(OverOutElementsWrapper)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/******************************************************************/
/* mozilla::EventStateManager                                     */
/******************************************************************/

static uint32_t sESMInstanceCount = 0;

bool EventStateManager::sNormalLMouseEventInProcess = false;
int16_t EventStateManager::sCurrentMouseBtn = MouseButton::eNotPressed;
EventStateManager* EventStateManager::sActiveESM = nullptr;
Document* EventStateManager::sMouseOverDocument = nullptr;
AutoWeakFrame EventStateManager::sLastDragOverFrame = nullptr;
LayoutDeviceIntPoint EventStateManager::sPreLockPoint =
    LayoutDeviceIntPoint(0, 0);
LayoutDeviceIntPoint EventStateManager::sLastRefPoint = kInvalidRefPoint;
CSSIntPoint EventStateManager::sLastScreenPoint = CSSIntPoint(0, 0);
LayoutDeviceIntPoint EventStateManager::sSynthCenteringPoint = kInvalidRefPoint;
CSSIntPoint EventStateManager::sLastClientPoint = CSSIntPoint(0, 0);
bool EventStateManager::sIsPointerLocked = false;
// Reference to the pointer locked element.
nsWeakPtr EventStateManager::sPointerLockedElement;
// Reference to the document which requested pointer lock.
nsWeakPtr EventStateManager::sPointerLockedDoc;
nsCOMPtr<nsIContent> EventStateManager::sDragOverContent = nullptr;

EventStateManager::WheelPrefs* EventStateManager::WheelPrefs::sInstance =
    nullptr;
bool EventStateManager::WheelPrefs::sWheelEventsEnabledOnPlugins = true;
bool EventStateManager::WheelPrefs::sIsAutoDirEnabled = false;
bool EventStateManager::WheelPrefs::sHonoursRootForAutoDir = false;
EventStateManager::DeltaAccumulator*
    EventStateManager::DeltaAccumulator::sInstance = nullptr;

constexpr const StyleCursorKind kInvalidCursorKind =
    static_cast<StyleCursorKind>(255);

EventStateManager::EventStateManager()
    : mLockCursor(kInvalidCursorKind),
      mLastFrameConsumedSetCursor(false),
      mCurrentTarget(nullptr),
      // init d&d gesture state machine variables
      mGestureDownPoint(0, 0),
      mGestureModifiers(0),
      mGestureDownButtons(0),
      mPresContext(nullptr),
      mLClickCount(0),
      mMClickCount(0),
      mRClickCount(0),
      mInTouchDrag(false),
      m_haveShutdown(false) {
  if (sESMInstanceCount == 0) {
    gUserInteractionTimerCallback = new UITimerCallback();
    if (gUserInteractionTimerCallback) NS_ADDREF(gUserInteractionTimerCallback);
    UpdateUserActivityTimer();
  }
  ++sESMInstanceCount;
}

nsresult EventStateManager::UpdateUserActivityTimer() {
  if (!gUserInteractionTimerCallback) return NS_OK;

  if (!gUserInteractionTimer) {
    gUserInteractionTimer = NS_NewTimer().take();
  }

  if (gUserInteractionTimer) {
    gUserInteractionTimer->InitWithCallback(
        gUserInteractionTimerCallback,
        StaticPrefs::dom_events_user_interaction_interval(),
        nsITimer::TYPE_ONE_SHOT);
  }
  return NS_OK;
}

nsresult EventStateManager::Init() {
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (!observerService) return NS_ERROR_FAILURE;

  observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, true);

  if (sESMInstanceCount == 1) {
    Prefs::Init();
  }

  return NS_OK;
}

EventStateManager::~EventStateManager() {
  ReleaseCurrentIMEContentObserver();

  if (sActiveESM == this) {
    sActiveESM = nullptr;
  }
  if (Prefs::ClickHoldContextMenu()) KillClickHoldTimer();

  if (mDocument == sMouseOverDocument) sMouseOverDocument = nullptr;

  --sESMInstanceCount;
  if (sESMInstanceCount == 0) {
    WheelTransaction::Shutdown();
    if (gUserInteractionTimerCallback) {
      gUserInteractionTimerCallback->Notify(nullptr);
      NS_RELEASE(gUserInteractionTimerCallback);
    }
    if (gUserInteractionTimer) {
      gUserInteractionTimer->Cancel();
      NS_RELEASE(gUserInteractionTimer);
    }
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

nsresult EventStateManager::Shutdown() {
  m_haveShutdown = true;
  return NS_OK;
}

NS_IMETHODIMP
EventStateManager::Observe(nsISupports* aSubject, const char* aTopic,
                           const char16_t* someData) {
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

NS_IMPL_CYCLE_COLLECTION_WEAK(EventStateManager, mCurrentTargetContent,
                              mGestureDownContent, mGestureDownFrameOwner,
                              mLastLeftMouseDownContent,
                              mLastMiddleMouseDownContent,
                              mLastRightMouseDownContent, mActiveContent,
                              mHoverContent, mURLTargetContent,
                              mMouseEnterLeaveHelper, mPointersEnterLeaveHelper,
                              mDocument, mIMEContentObserver, mAccessKeys)

void EventStateManager::ReleaseCurrentIMEContentObserver() {
  if (mIMEContentObserver) {
    mIMEContentObserver->DisconnectFromEventStateManager();
  }
  mIMEContentObserver = nullptr;
}

void EventStateManager::OnStartToObserveContent(
    IMEContentObserver* aIMEContentObserver) {
  if (mIMEContentObserver == aIMEContentObserver) {
    return;
  }
  ReleaseCurrentIMEContentObserver();
  mIMEContentObserver = aIMEContentObserver;
}

void EventStateManager::OnStopObservingContent(
    IMEContentObserver* aIMEContentObserver) {
  aIMEContentObserver->DisconnectFromEventStateManager();
  NS_ENSURE_TRUE_VOID(mIMEContentObserver == aIMEContentObserver);
  mIMEContentObserver = nullptr;
}

void EventStateManager::TryToFlushPendingNotificationsToIME() {
  if (mIMEContentObserver) {
    mIMEContentObserver->TryToFlushPendingNotifications(true);
  }
}

static bool IsMessageMouseUserActivity(EventMessage aMessage) {
  return aMessage == eMouseMove || aMessage == eMouseUp ||
         aMessage == eMouseDown || aMessage == eMouseAuxClick ||
         aMessage == eMouseDoubleClick || aMessage == eMouseClick ||
         aMessage == eMouseActivate || aMessage == eMouseLongTap;
}

static bool IsMessageGamepadUserActivity(EventMessage aMessage) {
  return aMessage == eGamepadButtonDown || aMessage == eGamepadButtonUp ||
         aMessage == eGamepadAxisMove;
}

// We ignore things that shouldn't cause popups, but also things that look
// like shortcut presses. In some obscure cases these may actually be
// website input, but any meaningful website will have other input anyway,
// and we can't very well tell whether shortcut input was supposed to be
// directed at chrome or the document.
static bool IsKeyboardEventUserActivity(WidgetEvent* aEvent) {
  WidgetKeyboardEvent* keyEvent = aEvent->AsKeyboardEvent();
  // Access keys should be treated as page interaction.
  if (keyEvent->ModifiersMatchWithAccessKey(AccessKeyType::eContent)) {
    return true;
  }
  if (!keyEvent->CanTreatAsUserInput() || keyEvent->IsControl() ||
      keyEvent->IsMeta() || keyEvent->IsOS() || keyEvent->IsAlt()) {
    return false;
  }
  // Deal with function keys:
  switch (keyEvent->mKeyNameIndex) {
    case KEY_NAME_INDEX_F1:
    case KEY_NAME_INDEX_F2:
    case KEY_NAME_INDEX_F3:
    case KEY_NAME_INDEX_F4:
    case KEY_NAME_INDEX_F5:
    case KEY_NAME_INDEX_F6:
    case KEY_NAME_INDEX_F7:
    case KEY_NAME_INDEX_F8:
    case KEY_NAME_INDEX_F9:
    case KEY_NAME_INDEX_F10:
    case KEY_NAME_INDEX_F11:
    case KEY_NAME_INDEX_F12:
    case KEY_NAME_INDEX_F13:
    case KEY_NAME_INDEX_F14:
    case KEY_NAME_INDEX_F15:
    case KEY_NAME_INDEX_F16:
    case KEY_NAME_INDEX_F17:
    case KEY_NAME_INDEX_F18:
    case KEY_NAME_INDEX_F19:
    case KEY_NAME_INDEX_F20:
    case KEY_NAME_INDEX_F21:
    case KEY_NAME_INDEX_F22:
    case KEY_NAME_INDEX_F23:
    case KEY_NAME_INDEX_F24:
      return false;
    default:
      return true;
  }
}

nsresult EventStateManager::PreHandleEvent(nsPresContext* aPresContext,
                                           WidgetEvent* aEvent,
                                           nsIFrame* aTargetFrame,
                                           nsIContent* aTargetContent,
                                           nsEventStatus* aStatus,
                                           nsIContent* aOverrideClickTarget) {
  NS_ENSURE_ARG_POINTER(aStatus);
  NS_ENSURE_ARG(aPresContext);
  if (!aEvent) {
    NS_ERROR("aEvent is null.  This should never happen.");
    return NS_ERROR_NULL_POINTER;
  }

  NS_WARNING_ASSERTION(
      !aTargetFrame || !aTargetFrame->GetContent() ||
          aTargetFrame->GetContent() == aTargetContent ||
          aTargetFrame->GetContent()->GetFlattenedTreeParent() ==
              aTargetContent ||
          aTargetFrame->IsGeneratedContentFrame(),
      "aTargetFrame should be related with aTargetContent");
#if DEBUG
  if (aTargetFrame && aTargetFrame->IsGeneratedContentFrame()) {
    nsCOMPtr<nsIContent> targetContent;
    aTargetFrame->GetContentForEvent(aEvent, getter_AddRefs(targetContent));
    MOZ_ASSERT(aTargetContent == targetContent,
               "Unexpected target for generated content frame!");
  }
#endif

  mCurrentTarget = aTargetFrame;
  mCurrentTargetContent = nullptr;

  // Do not take account eMouseEnterIntoWidget/ExitFromWidget so that loading
  // a page when user is not active doesn't change the state to active.
  WidgetMouseEvent* mouseEvent = aEvent->AsMouseEvent();
  if (aEvent->IsTrusted() &&
      ((mouseEvent && mouseEvent->IsReal() &&
        IsMessageMouseUserActivity(mouseEvent->mMessage)) ||
       aEvent->mClass == eWheelEventClass ||
       aEvent->mClass == ePointerEventClass ||
       aEvent->mClass == eTouchEventClass ||
       aEvent->mClass == eKeyboardEventClass ||
       (aEvent->mClass == eDragEventClass && aEvent->mMessage == eDrop) ||
       IsMessageGamepadUserActivity(aEvent->mMessage))) {
    if (gMouseOrKeyboardEventCounter == 0) {
      nsCOMPtr<nsIObserverService> obs =
          mozilla::services::GetObserverService();
      if (obs) {
        obs->NotifyObservers(nullptr, "user-interaction-active", nullptr);
        UpdateUserActivityTimer();
      }
    }
    ++gMouseOrKeyboardEventCounter;

    nsCOMPtr<nsINode> node = aTargetContent;
    if (node &&
        ((aEvent->mMessage == eKeyUp && IsKeyboardEventUserActivity(aEvent)) ||
         aEvent->mMessage == eMouseUp || aEvent->mMessage == eWheel ||
         aEvent->mMessage == eTouchEnd || aEvent->mMessage == ePointerUp ||
         aEvent->mMessage == eDrop)) {
      Document* doc = node->OwnerDoc();
      while (doc) {
        doc->SetUserHasInteracted();
        doc = nsContentUtils::IsChildOfSameType(doc)
                  ? doc->GetInProcessParentDocument()
                  : nullptr;
      }
    }
  }

  WheelTransaction::OnEvent(aEvent);

  // Focus events don't necessarily need a frame.
  if (!mCurrentTarget && !aTargetContent) {
    NS_ERROR("mCurrentTarget and aTargetContent are null");
    return NS_ERROR_NULL_POINTER;
  }
#ifdef DEBUG
  if (aEvent->HasDragEventMessage() && sIsPointerLocked) {
    NS_ASSERTION(
        sIsPointerLocked,
        "sIsPointerLocked is true. Drag events should be suppressed when "
        "the pointer is locked.");
  }
#endif
  // Store last known screenPoint and clientPoint so pointer lock
  // can use these values as constants.
  if (aEvent->IsTrusted() &&
      ((mouseEvent && mouseEvent->IsReal()) ||
       aEvent->mClass == eWheelEventClass) &&
      !sIsPointerLocked) {
    sLastScreenPoint =
        Event::GetScreenCoords(aPresContext, aEvent, aEvent->mRefPoint);
    sLastClientPoint = Event::GetClientCoords(
        aPresContext, aEvent, aEvent->mRefPoint, CSSIntPoint(0, 0));
  }

  *aStatus = nsEventStatus_eIgnore;

  if (aEvent->mClass == eQueryContentEventClass) {
    HandleQueryContentEvent(aEvent->AsQueryContentEvent());
    return NS_OK;
  }

  WidgetTouchEvent* touchEvent = aEvent->AsTouchEvent();
  if (touchEvent && mInTouchDrag) {
    if (touchEvent->mMessage == eTouchMove) {
      GenerateDragGesture(aPresContext, touchEvent);
    } else {
      mInTouchDrag = false;
      StopTrackingDragGesture(true);
    }
  }

  PointerEventHandler::UpdateActivePointerState(mouseEvent);

  switch (aEvent->mMessage) {
    case eContextMenu:
      if (sIsPointerLocked) {
        return NS_ERROR_DOM_INVALID_STATE_ERR;
      }
      break;
    case eMouseTouchDrag:
      mInTouchDrag = true;
      BeginTrackingDragGesture(aPresContext, mouseEvent, aTargetFrame);
      break;
    case eMouseDown: {
      switch (mouseEvent->mButton) {
        case MouseButton::eLeft:
          BeginTrackingDragGesture(aPresContext, mouseEvent, aTargetFrame);
          mLClickCount = mouseEvent->mClickCount;
          SetClickCount(mouseEvent, aStatus);
          sNormalLMouseEventInProcess = true;
          break;
        case MouseButton::eMiddle:
          mMClickCount = mouseEvent->mClickCount;
          SetClickCount(mouseEvent, aStatus);
          break;
        case MouseButton::eRight:
          mRClickCount = mouseEvent->mClickCount;
          SetClickCount(mouseEvent, aStatus);
          break;
      }
      NotifyTargetUserActivation(aEvent, aTargetContent);
      break;
    }
    case eMouseUp: {
      switch (mouseEvent->mButton) {
        case MouseButton::eLeft:
          if (Prefs::ClickHoldContextMenu()) {
            KillClickHoldTimer();
          }
          mInTouchDrag = false;
          StopTrackingDragGesture(true);
          sNormalLMouseEventInProcess = false;
          // then fall through...
          [[fallthrough]];
        case MouseButton::eRight:
        case MouseButton::eMiddle:
          RefPtr<EventStateManager> esm =
              ESMFromContentOrThis(aOverrideClickTarget);
          esm->SetClickCount(mouseEvent, aStatus, aOverrideClickTarget);
          break;
      }
      break;
    }
    case eMouseEnterIntoWidget:
      // In some cases on e10s eMouseEnterIntoWidget
      // event was sent twice into child process of content.
      // (From specific widget code (sending is not permanent) and
      // from ESM::DispatchMouseOrPointerEvent (sending is permanent)).
      // IsCrossProcessForwardingStopped() helps to suppress sending accidental
      // event from widget code.
      aEvent->StopCrossProcessForwarding();
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

      // IsCrossProcessForwardingStopped() helps to suppress double event
      // sending into process of content. For more information see comment
      // above, at eMouseEnterIntoWidget case.
      aEvent->StopCrossProcessForwarding();

      // If the event is not a top-level window exit, then it's not
      // really an exit --- we may have traversed widget boundaries but
      // we're still in our toplevel window.
      if (mouseEvent->mExitFrom != WidgetMouseEvent::eTopLevel) {
        // Treat it as a synthetic move so we don't generate spurious
        // "exit" or "move" events.  Any necessary "out" or "over" events
        // will be generated by GenerateMouseEnterExit
        mouseEvent->mMessage = eMouseMove;
        mouseEvent->mReason = WidgetMouseEvent::eSynthesized;
        // then fall through...
      } else {
        // We should synthetize corresponding pointer events
        GeneratePointerEnterExit(ePointerLeave, mouseEvent);
        GenerateMouseEnterExit(mouseEvent);
        // This is a window level mouse exit event and should stop here
        aEvent->mMessage = eVoidEvent;
        break;
      }
      [[fallthrough]];
    case eMouseMove:
    case ePointerDown:
      if (aEvent->mMessage == ePointerDown) {
        PointerEventHandler::ImplicitlyCapturePointer(aTargetFrame, aEvent);
        if (mouseEvent->mInputSource != MouseEvent_Binding::MOZ_SOURCE_TOUCH) {
          NotifyTargetUserActivation(aEvent, aTargetContent);
        }
      }
      [[fallthrough]];
    case ePointerMove: {
      // on the Mac, GenerateDragGesture() may not return until the drag
      // has completed and so |aTargetFrame| may have been deleted (moving
      // a bookmark, for example).  If this is the case, however, we know
      // that ClearFrameRefs() has been called and it cleared out
      // |mCurrentTarget|. As a result, we should pass |mCurrentTarget|
      // into UpdateCursor().
      if (!mInTouchDrag) {
        GenerateDragGesture(aPresContext, mouseEvent);
      }
      UpdateCursor(aPresContext, aEvent, mCurrentTarget, aStatus);

      UpdateLastRefPointOfMouseEvent(mouseEvent);
      if (sIsPointerLocked) {
        ResetPointerToWindowCenterWhilePointerLocked(mouseEvent);
      }
      UpdateLastPointerPosition(mouseEvent);

      GenerateMouseEnterExit(mouseEvent);
      // Flush pending layout changes, so that later mouse move events
      // will go to the right nodes.
      FlushLayout(aPresContext);
      break;
    }
    case ePointerGotCapture:
      GenerateMouseEnterExit(mouseEvent);
      break;
    case eDragStart:
      if (Prefs::ClickHoldContextMenu()) {
        // an external drag gesture event came in, not generated internally
        // by Gecko. Make sure we get rid of the click-hold timer.
        KillClickHoldTimer();
      }
      break;
    case eDragOver: {
      WidgetDragEvent* dragEvent = aEvent->AsDragEvent();
      MOZ_ASSERT(dragEvent);
      if (dragEvent->mFlags.mIsSynthesizedForTests) {
        dragEvent->InitDropEffectForTests();
      }
      // Send the enter/exit events before eDrop.
      GenerateDragDropEnterExit(aPresContext, dragEvent);
      break;
    }
    case eDrop:
      if (aEvent->mFlags.mIsSynthesizedForTests) {
        MOZ_ASSERT(aEvent->AsDragEvent());
        aEvent->AsDragEvent()->InitDropEffectForTests();
      }
      break;

    case eKeyPress: {
      WidgetKeyboardEvent* keyEvent = aEvent->AsKeyboardEvent();
      if (keyEvent->ModifiersMatchWithAccessKey(AccessKeyType::eChrome) ||
          keyEvent->ModifiersMatchWithAccessKey(AccessKeyType::eContent)) {
        // If the eKeyPress event will be sent to a remote process, this
        // process needs to wait reply from the remote process for checking if
        // preceding eKeyDown event is consumed.  If preceding eKeyDown event
        // is consumed in the remote process, BrowserChild won't send the event
        // back to this process.  So, only when this process receives a reply
        // eKeyPress event in BrowserParent, we should handle accesskey in this
        // process.
        if (IsRemoteTarget(GetFocusedContent())) {
          // However, if there is no accesskey target for the key combination,
          // we don't need to wait reply from the remote process.  Otherwise,
          // Mark the event as waiting reply from remote process and stop
          // propagation in this process.
          if (CheckIfEventMatchesAccessKey(keyEvent, aPresContext)) {
            keyEvent->StopPropagation();
            keyEvent->MarkAsWaitingReplyFromRemoteProcess();
          }
        }
        // If the event target is in this process, we can handle accesskey now
        // since if preceding eKeyDown event was consumed, eKeyPress event
        // won't be dispatched by widget.  So, coming eKeyPress event means
        // that the preceding eKeyDown event wasn't consumed in this case.
        else {
          AutoTArray<uint32_t, 10> accessCharCodes;
          keyEvent->GetAccessKeyCandidates(accessCharCodes);

          if (HandleAccessKey(keyEvent, aPresContext, accessCharCodes)) {
            *aStatus = nsEventStatus_eConsumeNoDefault;
          }
        }
      }
    }
      // then fall through...
      [[fallthrough]];
    case eKeyDown:
      if (aEvent->mMessage == eKeyDown) {
        NotifyTargetUserActivation(aEvent, aTargetContent);
      }
      [[fallthrough]];
    case eKeyUp: {
      nsIContent* content = GetFocusedContent();
      if (content) mCurrentTargetContent = content;

      // NOTE: Don't refer TextComposition::IsComposing() since UI Events
      //       defines that KeyboardEvent.isComposing is true when it's
      //       dispatched after compositionstart and compositionend.
      //       TextComposition::IsComposing() is false even before
      //       compositionend if there is no composing string.
      //       And also don't expose other document's composition state.
      //       A native IME context is typically shared by multiple documents.
      //       So, don't use GetTextCompositionFor(nsIWidget*) here.
      RefPtr<TextComposition> composition =
          IMEStateManager::GetTextCompositionFor(aPresContext);
      aEvent->AsKeyboardEvent()->mIsComposing = !!composition;

      // Widget may need to perform default action for specific keyboard
      // event if it's not consumed.  In this case, widget has already marked
      // the event as "waiting reply from remote process".  However, we need
      // to reset it if the target (focused content) isn't in a remote process
      // because PresShell needs to check if it's marked as so before
      // dispatching events into the DOM tree.
      if (aEvent->IsWaitingReplyFromRemoteProcess() &&
          !aEvent->PropagationStopped() && !IsRemoteTarget(content)) {
        aEvent->ResetWaitingReplyFromRemoteProcessState();
      }
    } break;
    case eWheel:
    case eWheelOperationStart:
    case eWheelOperationEnd: {
      NS_ASSERTION(aEvent->IsTrusted(),
                   "Untrusted wheel event shouldn't be here");

      nsIContent* content = GetFocusedContent();
      if (content) {
        mCurrentTargetContent = content;
      }

      if (aEvent->mMessage != eWheel) {
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
      DeltaAccumulator::GetInstance()->InitLineOrPageDelta(aTargetFrame, this,
                                                           wheelEvent);
    } break;
    case eSetSelection: {
      nsCOMPtr<nsIContent> focusedContent = GetFocusedContent();
      IMEStateManager::HandleSelectionEvent(aPresContext, focusedContent,
                                            aEvent->AsSelectionEvent());
      break;
    }
    case eContentCommandCut:
    case eContentCommandCopy:
    case eContentCommandPaste:
    case eContentCommandDelete:
    case eContentCommandUndo:
    case eContentCommandRedo:
    case eContentCommandPasteTransferable:
    case eContentCommandLookUpDictionary:
      DoContentCommandEvent(aEvent->AsContentCommandEvent());
      break;
    case eContentCommandScroll:
      DoContentCommandScrollEvent(aEvent->AsContentCommandEvent());
      break;
    case eCompositionStart:
      if (aEvent->IsTrusted()) {
        // If the event is trusted event, set the selected text to data of
        // composition event.
        WidgetCompositionEvent* compositionEvent = aEvent->AsCompositionEvent();
        WidgetQueryContentEvent selectedText(true, eQuerySelectedText,
                                             compositionEvent->mWidget);
        HandleQueryContentEvent(&selectedText);
        NS_ASSERTION(selectedText.mSucceeded, "Failed to get selected text");
        compositionEvent->mData = selectedText.mReply.mString;
      }
      break;
    case eTouchStart:
      SetGestureDownPoint(aEvent->AsTouchEvent());
      break;
    case eTouchEnd:
      NotifyTargetUserActivation(aEvent, aTargetContent);
      break;
    default:
      break;
  }
  return NS_OK;
}

void EventStateManager::NotifyTargetUserActivation(WidgetEvent* aEvent,
                                                   nsIContent* aTargetContent) {
  if (!aEvent->IsTrusted()) {
    return;
  }

  WidgetMouseEvent* mouseEvent = aEvent->AsMouseEvent();
  if (mouseEvent && !mouseEvent->IsReal()) {
    return;
  }

  nsCOMPtr<nsINode> node = aTargetContent;
  if (!node) {
    return;
  }

  Document* doc = node->OwnerDoc();
  if (!doc) {
    return;
  }

  // Don't gesture activate for key events for keys which are likely
  // to be interaction with the browser, OS.
  WidgetKeyboardEvent* keyEvent = aEvent->AsKeyboardEvent();
  if (keyEvent && !keyEvent->CanUserGestureActivateTarget()) {
    return;
  }

  // Touch gestures that end outside the drag target were touches that turned
  // into scroll/pan/swipe actions. We don't want to gesture activate on such
  // actions, we want to only gesture activate on touches that are taps.
  // That is, touches that end in roughly the same place that they started.
  if (aEvent->mMessage == eTouchEnd && aEvent->AsTouchEvent() &&
      IsEventOutsideDragThreshold(aEvent->AsTouchEvent())) {
    return;
  }

  MOZ_ASSERT(aEvent->mMessage == eKeyDown || aEvent->mMessage == eMouseDown ||
             aEvent->mMessage == ePointerDown || aEvent->mMessage == eTouchEnd);
  doc->NotifyUserGestureActivation();
}

already_AddRefed<EventStateManager> EventStateManager::ESMFromContentOrThis(
    nsIContent* aContent) {
  if (aContent) {
    PresShell* presShell = aContent->OwnerDoc()->GetPresShell();
    if (presShell) {
      nsPresContext* prescontext = presShell->GetPresContext();
      if (prescontext) {
        RefPtr<EventStateManager> esm = prescontext->EventStateManager();
        if (esm) {
          return esm.forget();
        }
      }
    }
  }

  RefPtr<EventStateManager> esm = this;
  return esm.forget();
}

void EventStateManager::HandleQueryContentEvent(
    WidgetQueryContentEvent* aEvent) {
  switch (aEvent->mMessage) {
    case eQuerySelectedText:
    case eQueryTextContent:
    case eQueryCaretRect:
    case eQueryTextRect:
    case eQueryEditorRect:
      if (!IsTargetCrossProcess(aEvent)) {
        break;
      }
      // Will not be handled locally, remote the event
      GetCrossProcessTarget()->HandleQueryContentEvent(*aEvent);
      return;
    // Following events have not been supported in e10s mode yet.
    case eQueryContentState:
    case eQuerySelectionAsTransferable:
    case eQueryCharacterAtPoint:
    case eQueryDOMWidgetHittest:
    case eQueryTextRectArray:
      break;
    default:
      return;
  }

  // If there is an IMEContentObserver, we need to handle QueryContentEvent
  // with it.
  if (mIMEContentObserver) {
    RefPtr<IMEContentObserver> contentObserver = mIMEContentObserver;
    contentObserver->HandleQueryContentEvent(aEvent);
    return;
  }

  ContentEventHandler handler(mPresContext);
  handler.HandleQueryContentEvent(aEvent);
}

static AccessKeyType GetAccessKeyTypeFor(nsISupports* aDocShell) {
  nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(aDocShell));
  if (!treeItem) {
    return AccessKeyType::eNone;
  }

  switch (treeItem->ItemType()) {
    case nsIDocShellTreeItem::typeChrome:
      return AccessKeyType::eChrome;
    case nsIDocShellTreeItem::typeContent:
      return AccessKeyType::eContent;
    default:
      return AccessKeyType::eNone;
  }
}

static bool IsAccessKeyTarget(nsIContent* aContent, nsIFrame* aFrame,
                              nsAString& aKey) {
  // Use GetAttr because we want Unicode case=insensitive matching
  // XXXbz shouldn't this be case-sensitive, per spec?
  nsString contentKey;
  if (!aContent->IsElement() ||
      !aContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey,
                                      contentKey) ||
      !contentKey.Equals(aKey, nsCaseInsensitiveStringComparator()))
    return false;

  if (!aContent->IsXULElement()) return true;

  // For XUL we do visibility checks.
  if (!aFrame) return false;

  if (aFrame->IsFocusable()) return true;

  if (!aFrame->IsVisibleConsideringAncestors()) return false;

  // XUL controls can be activated.
  nsCOMPtr<nsIDOMXULControlElement> control =
      aContent->AsElement()->AsXULControl();
  if (control) return true;

  // HTML area, label and legend elements are never focusable, so
  // we need to check for them explicitly before giving up.
  if (aContent->IsAnyOfHTMLElements(nsGkAtoms::area, nsGkAtoms::label,
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

bool EventStateManager::CheckIfEventMatchesAccessKey(
    WidgetKeyboardEvent* aEvent, nsPresContext* aPresContext) {
  AutoTArray<uint32_t, 10> accessCharCodes;
  aEvent->GetAccessKeyCandidates(accessCharCodes);
  return WalkESMTreeToHandleAccessKey(const_cast<WidgetKeyboardEvent*>(aEvent),
                                      aPresContext, accessCharCodes, nullptr,
                                      eAccessKeyProcessingNormal, false);
}

bool EventStateManager::LookForAccessKeyAndExecute(
    nsTArray<uint32_t>& aAccessCharCodes, bool aIsTrustedEvent, bool aIsRepeat,
    bool aExecute) {
  int32_t count, start = -1;
  if (nsIContent* focusedContent = GetFocusedContent()) {
    start = mAccessKeys.IndexOf(focusedContent);
    if (start == -1 && focusedContent->IsInNativeAnonymousSubtree()) {
      start = mAccessKeys.IndexOf(
          focusedContent->GetClosestNativeAnonymousSubtreeRootParent());
    }
  }
  RefPtr<Element> element;
  nsIFrame* frame;
  int32_t length = mAccessKeys.Count();
  for (uint32_t i = 0; i < aAccessCharCodes.Length(); ++i) {
    uint32_t ch = aAccessCharCodes[i];
    nsAutoString accessKey;
    AppendUCS4ToUTF16(ch, accessKey);
    for (count = 1; count <= length; ++count) {
      // mAccessKeys always stores Element instances.
      element = mAccessKeys[(start + count) % length]->AsElement();
      frame = element->GetPrimaryFrame();
      if (IsAccessKeyTarget(element, frame, accessKey)) {
        if (!aExecute) {
          return true;
        }
        bool shouldActivate = Prefs::KeyCausesActivation();

        if (aIsRepeat && nsContentUtils::IsChromeDoc(element->OwnerDoc())) {
          shouldActivate = false;
        }

        while (shouldActivate && ++count <= length) {
          nsIContent* oc = mAccessKeys[(start + count) % length];
          nsIFrame* of = oc->GetPrimaryFrame();
          if (IsAccessKeyTarget(oc, of, accessKey)) shouldActivate = false;
        }

        bool focusChanged = false;
        if (shouldActivate) {
          focusChanged =
              element->PerformAccesskey(shouldActivate, aIsTrustedEvent);
        } else if (nsFocusManager* fm = nsFocusManager::GetFocusManager()) {
          fm->SetFocus(element, nsIFocusManager::FLAG_BYKEY);
          focusChanged = true;
        }

        if (focusChanged && aIsTrustedEvent) {
          // If this is a child process, inform the parent that we want the
          // focus, but pass false since we don't want to change the window
          // order.
          nsIDocShell* docShell = mPresContext->GetDocShell();
          nsCOMPtr<nsIBrowserChild> child =
              docShell ? docShell->GetBrowserChild() : nullptr;
          if (child) {
            child->SendRequestFocus(false, CallerType::System);
          }
        }

        return true;
      }
    }
  }
  return false;
}

// static
void EventStateManager::GetAccessKeyLabelPrefix(Element* aElement,
                                                nsAString& aPrefix) {
  aPrefix.Truncate();
  nsAutoString separator, modifierText;
  nsContentUtils::GetModifierSeparatorText(separator);

  AccessKeyType accessKeyType =
      GetAccessKeyTypeFor(aElement->OwnerDoc()->GetDocShell());
  if (accessKeyType == AccessKeyType::eNone) {
    return;
  }
  Modifiers modifiers = WidgetKeyboardEvent::AccessKeyModifiers(accessKeyType);
  if (modifiers == MODIFIER_NONE) {
    return;
  }

  if (modifiers & MODIFIER_CONTROL) {
    nsContentUtils::GetControlText(modifierText);
    aPrefix.Append(modifierText + separator);
  }
  if (modifiers & MODIFIER_META) {
    nsContentUtils::GetMetaText(modifierText);
    aPrefix.Append(modifierText + separator);
  }
  if (modifiers & MODIFIER_OS) {
    nsContentUtils::GetOSText(modifierText);
    aPrefix.Append(modifierText + separator);
  }
  if (modifiers & MODIFIER_ALT) {
    nsContentUtils::GetAltText(modifierText);
    aPrefix.Append(modifierText + separator);
  }
  if (modifiers & MODIFIER_SHIFT) {
    nsContentUtils::GetShiftText(modifierText);
    aPrefix.Append(modifierText + separator);
  }
}

struct MOZ_STACK_CLASS AccessKeyInfo {
  WidgetKeyboardEvent* event;
  nsTArray<uint32_t>& charCodes;

  AccessKeyInfo(WidgetKeyboardEvent* aEvent, nsTArray<uint32_t>& aCharCodes)
      : event(aEvent), charCodes(aCharCodes) {}
};

bool EventStateManager::WalkESMTreeToHandleAccessKey(
    WidgetKeyboardEvent* aEvent, nsPresContext* aPresContext,
    nsTArray<uint32_t>& aAccessCharCodes, nsIDocShellTreeItem* aBubbledFrom,
    ProcessingAccessKeyState aAccessKeyState, bool aExecute) {
  EnsureDocument(mPresContext);
  nsCOMPtr<nsIDocShell> docShell = aPresContext->GetDocShell();
  if (NS_WARN_IF(!docShell) || NS_WARN_IF(!mDocument)) {
    return false;
  }
  AccessKeyType accessKeyType = GetAccessKeyTypeFor(docShell);
  if (accessKeyType == AccessKeyType::eNone) {
    return false;
  }
  // Alt or other accesskey modifier is down, we may need to do an accesskey.
  if (mAccessKeys.Count() > 0 &&
      aEvent->ModifiersMatchWithAccessKey(accessKeyType)) {
    // Someone registered an accesskey.  Find and activate it.
    if (LookForAccessKeyAndExecute(aAccessCharCodes, aEvent->IsTrusted(),
                                   aEvent->mIsRepeat, aExecute)) {
      return true;
    }
  }

  int32_t childCount;
  docShell->GetInProcessChildCount(&childCount);
  for (int32_t counter = 0; counter < childCount; counter++) {
    // Not processing the child which bubbles up the handling
    nsCOMPtr<nsIDocShellTreeItem> subShellItem;
    docShell->GetInProcessChildAt(counter, getter_AddRefs(subShellItem));
    if (aAccessKeyState == eAccessKeyProcessingUp &&
        subShellItem == aBubbledFrom) {
      continue;
    }

    nsCOMPtr<nsIDocShell> subDS = do_QueryInterface(subShellItem);
    if (subDS && IsShellVisible(subDS)) {
      // Guarantee subPresShell lifetime while we're handling access key
      // since somebody may assume that it won't be deleted before the
      // corresponding nsPresContext and EventStateManager.
      RefPtr<PresShell> subPresShell = subDS->GetPresShell();

      // Docshells need not have a presshell (eg. display:none
      // iframes, docshells in transition between documents, etc).
      if (!subPresShell) {
        // Oh, well.  Just move on to the next child
        continue;
      }

      RefPtr<nsPresContext> subPresContext = subPresShell->GetPresContext();

      RefPtr<EventStateManager> esm =
          static_cast<EventStateManager*>(subPresContext->EventStateManager());

      if (esm && esm->WalkESMTreeToHandleAccessKey(
                     aEvent, subPresContext, aAccessCharCodes, nullptr,
                     eAccessKeyProcessingDown, aExecute)) {
        return true;
      }
    }
  }  // if end . checking all sub docshell ends here.

  // bubble up the process to the parent docshell if necessary
  if (eAccessKeyProcessingDown != aAccessKeyState) {
    nsCOMPtr<nsIDocShellTreeItem> parentShellItem;
    docShell->GetInProcessParent(getter_AddRefs(parentShellItem));
    nsCOMPtr<nsIDocShell> parentDS = do_QueryInterface(parentShellItem);
    if (parentDS) {
      // Guarantee parentPresShell lifetime while we're handling access key
      // since somebody may assume that it won't be deleted before the
      // corresponding nsPresContext and EventStateManager.
      RefPtr<PresShell> parentPresShell = parentDS->GetPresShell();
      NS_ASSERTION(parentPresShell,
                   "Our PresShell exists but the parent's does not?");

      RefPtr<nsPresContext> parentPresContext =
          parentPresShell->GetPresContext();
      NS_ASSERTION(parentPresContext, "PresShell without PresContext");

      RefPtr<EventStateManager> esm = static_cast<EventStateManager*>(
          parentPresContext->EventStateManager());
      if (esm && esm->WalkESMTreeToHandleAccessKey(
                     aEvent, parentPresContext, aAccessCharCodes, docShell,
                     eAccessKeyProcessingDown, aExecute)) {
        return true;
      }
    }
  }  // if end. bubble up process

  // If the content access key modifier is pressed, try remote children
  if (aExecute &&
      aEvent->ModifiersMatchWithAccessKey(AccessKeyType::eContent) &&
      mDocument && mDocument->GetWindow()) {
    // If the focus is currently on a node with a BrowserParent, the key event
    // should've gotten forwarded to the child process and HandleAccessKey
    // called from there.
    if (BrowserParent::GetFrom(GetFocusedContent())) {
      // If access key may be only in remote contents, this method won't handle
      // access key synchronously.  In this case, only reply event should reach
      // here.
      MOZ_ASSERT(aEvent->IsHandledInRemoteProcess() ||
                 !aEvent->IsWaitingReplyFromRemoteProcess());
    }
    // If focus is somewhere else, then we need to check the remote children.
    // However, if the event has already been handled in a remote process,
    // then, focus is moved from the remote process after posting the event.
    // In such case, we shouldn't retry to handle access keys in remote
    // processes.
    else if (!aEvent->IsHandledInRemoteProcess()) {
      AccessKeyInfo accessKeyInfo(aEvent, aAccessCharCodes);
      nsContentUtils::CallOnAllRemoteChildren(
          mDocument->GetWindow(),
          [&accessKeyInfo](BrowserParent* aBrowserParent) -> CallState {
            // Only forward accesskeys for the active tab.
            if (aBrowserParent->GetDocShellIsActive()) {
              // Even if there is no target for the accesskey in this process,
              // the event may match with a content accesskey.  If so, the
              // keyboard event should be handled with reply event for
              // preventing double action. (e.g., Alt+Shift+F on Windows may
              // focus a content in remote and open "File" menu.)
              accessKeyInfo.event->StopPropagation();
              accessKeyInfo.event->MarkAsWaitingReplyFromRemoteProcess();
              aBrowserParent->HandleAccessKey(*accessKeyInfo.event,
                                              accessKeyInfo.charCodes);
              return CallState::Stop;
            }

            return CallState::Continue;
          });
    }
  }

  return false;
}  // end of HandleAccessKey

void EventStateManager::DispatchCrossProcessEvent(WidgetEvent* aEvent,
                                                  nsFrameLoader* aFrameLoader,
                                                  nsEventStatus* aStatus) {
  BrowserParent* remote = BrowserParent::GetFrom(aFrameLoader);
  if (!remote) {
    return;
  }

  WidgetMouseEvent* mouseEvent = aEvent->AsMouseEvent();
  bool isContextMenuKey = mouseEvent && mouseEvent->IsContextMenuKeyEvent();
  if (aEvent->mClass == eKeyboardEventClass || isContextMenuKey) {
    // APZ attaches a LayersId to hit-testable events, for keyboard events,
    // we use focus.
    BrowserParent* preciseRemote = BrowserParent::GetFocused();
    if (preciseRemote) {
      remote = preciseRemote;
    }
    // else there is a race between layout and focus tracking,
    // so fall back to delivering the event to the topmost child process.
  } else if (aEvent->mLayersId.IsValid()) {
    BrowserParent* preciseRemote =
        BrowserParent::GetBrowserParentFromLayersId(aEvent->mLayersId);
    if (preciseRemote) {
      remote = preciseRemote;
    }
    // else there is a race between APZ and the LayersId to BrowserParent
    // mapping, so fall back to delivering the event to the topmost child
    // process.
  }

  switch (aEvent->mClass) {
    case eMouseEventClass: {
      remote->SendRealMouseEvent(*mouseEvent);
      return;
    }
    case eKeyboardEventClass: {
      remote->SendRealKeyEvent(*aEvent->AsKeyboardEvent());
      return;
    }
    case eWheelEventClass: {
      remote->SendMouseWheelEvent(*aEvent->AsWheelEvent());
      return;
    }
    case eTouchEventClass: {
      // Let the child process synthesize a mouse event if needed, and
      // ensure we don't synthesize one in this process.
      *aStatus = nsEventStatus_eConsumeNoDefault;
      remote->SendRealTouchEvent(*aEvent->AsTouchEvent());
      return;
    }
    case eDragEventClass: {
      RefPtr<BrowserParent> browserParent = remote;
      browserParent->Manager()->MaybeInvokeDragSession(browserParent);

      nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
      uint32_t dropEffect = nsIDragService::DRAGDROP_ACTION_NONE;
      uint32_t action = nsIDragService::DRAGDROP_ACTION_NONE;
      nsCOMPtr<nsIPrincipal> principal;
      nsCOMPtr<nsIContentSecurityPolicy> csp;

      if (dragSession) {
        dragSession->DragEventDispatchedToChildProcess();
        dragSession->GetDragAction(&action);
        dragSession->GetTriggeringPrincipal(getter_AddRefs(principal));
        dragSession->GetCsp(getter_AddRefs(csp));
        RefPtr<DataTransfer> initialDataTransfer =
            dragSession->GetDataTransfer();
        if (initialDataTransfer) {
          dropEffect = initialDataTransfer->DropEffectInt();
        }
      }

      browserParent->SendRealDragEvent(*aEvent->AsDragEvent(), action,
                                       dropEffect, principal, csp);
      return;
    }
    case ePluginEventClass: {
      *aStatus = nsEventStatus_eConsumeNoDefault;
      remote->SendPluginEvent(*aEvent->AsPluginEvent());
      return;
    }
    default: {
      MOZ_CRASH("Attempt to send non-whitelisted event?");
    }
  }
}

bool EventStateManager::IsRemoteTarget(nsIContent* target) {
  return !!BrowserParent::GetFrom(target);
}

bool EventStateManager::HandleCrossProcessEvent(WidgetEvent* aEvent,
                                                nsEventStatus* aStatus) {
  if (!aEvent->CanBeSentToRemoteProcess()) {
    return false;
  }

  MOZ_ASSERT(!aEvent->HasBeenPostedToRemoteProcess(),
             "Why do we need to post same event to remote processes again?");

  // Collect the remote event targets we're going to forward this
  // event to.
  //
  // NB: the elements of |targets| must be unique, for correctness.
  AutoTArray<nsCOMPtr<nsIContent>, 1> targets;
  if (aEvent->mClass != eTouchEventClass || aEvent->mMessage == eTouchStart) {
    // If this event only has one target, and it's remote, add it to
    // the array.
    nsIFrame* frame = aEvent->mMessage == eDragExit
                          ? sLastDragOverFrame.GetFrame()
                          : GetEventTarget();
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
        aEvent->AsTouchEvent()->mTouches;
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
  for (uint32_t i = 0; i < targets.Length(); ++i) {
    nsIContent* target = targets[i];
    RefPtr<nsFrameLoaderOwner> loaderOwner = do_QueryObject(target);
    if (!loaderOwner) {
      continue;
    }

    RefPtr<nsFrameLoader> frameLoader = loaderOwner->GetFrameLoader();
    if (!frameLoader) {
      continue;
    }

    DispatchCrossProcessEvent(aEvent, frameLoader, aStatus);
  }
  return aEvent->HasBeenPostedToRemoteProcess();
}

//
// CreateClickHoldTimer
//
// Fire off a timer for determining if the user wants click-hold. This timer
// is a one-shot that will be cancelled when the user moves enough to fire
// a drag.
//
void EventStateManager::CreateClickHoldTimer(nsPresContext* inPresContext,
                                             nsIFrame* inDownFrame,
                                             WidgetGUIEvent* inMouseDownEvent) {
  if (!inMouseDownEvent->IsTrusted() || IsRemoteTarget(mGestureDownContent) ||
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
  if (mGestureDownContent &&
      nsContentUtils::HasNonEmptyAttr(mGestureDownContent, kNameSpaceID_None,
                                      nsGkAtoms::popup)) {
    return;
  }

  int32_t clickHoldDelay =
      Preferences::GetInt("ui.click_hold_context_menus.delay", 500);
  NS_NewTimerWithFuncCallback(
      getter_AddRefs(mClickHoldTimer), sClickHoldCallback, this, clickHoldDelay,
      nsITimer::TYPE_ONE_SHOT, "EventStateManager::CreateClickHoldTimer");
}  // CreateClickHoldTimer

//
// KillClickHoldTimer
//
// Stop the timer that would show the context menu dead in its tracks
//
void EventStateManager::KillClickHoldTimer() {
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
void EventStateManager::sClickHoldCallback(nsITimer* aTimer, void* aESM) {
  RefPtr<EventStateManager> self = static_cast<EventStateManager*>(aESM);
  if (self) {
    self->FireContextClick();
  }

  // NOTE: |aTimer| and |self->mAutoHideTimer| are invalid after calling
  // ClosePopup();

}  // sAutoHideCallback

//
// FireContextClick
//
// If we're this far, our timer has fired, which means the mouse has been down
// for a certain period of time and has not moved enough to generate a
// dragGesture. We can be certain the user wants a context-click at this stage,
// so generate a dom event and fire it in.
//
// After the event fires, check if PreventDefault() has been set on the event
// which means that someone either ate the event or put up a context menu. This
// is our cue to stop tracking the drag gesture. If we always did this,
// draggable items w/out a context menu wouldn't be draggable after a certain
// length of time, which is _not_ what we want.
//
void EventStateManager::FireContextClick() {
  if (!mGestureDownContent || !mPresContext || sIsPointerLocked) {
    return;
  }

#ifdef XP_MACOSX
  // Hack to ensure that we don't show a context menu when the user
  // let go of the mouse after a long cpu-hogging operation prevented
  // us from handling any OS events. See bug 117589.
  if (!CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState,
                                kCGMouseButtonLeft))
    return;
#endif

  nsEventStatus status = nsEventStatus_eIgnore;

  // Dispatch to the DOM. We have to fake out the ESM and tell it that the
  // current target frame is actually where the mouseDown occurred, otherwise it
  // will use the frame the mouse is currently over which may or may not be
  // the same. (Note: saari and I have decided that we don't have to reset
  // |mCurrentTarget| when we're through because no one else is doing anything
  // more with this event and it will get reset on the very next event to the
  // correct frame).
  mCurrentTarget = mPresContext->GetPrimaryFrameFor(mGestureDownContent);
  // make sure the widget sticks around
  nsCOMPtr<nsIWidget> targetWidget;
  if (mCurrentTarget && (targetWidget = mCurrentTarget->GetNearestWidget())) {
    NS_ASSERTION(
        mPresContext == mCurrentTarget->PresContext(),
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
      if (nsContentUtils::HasNonEmptyAttr(
              mGestureDownContent, kNameSpaceID_None, nsGkAtoms::container)) {
        allowedToDispatch = false;
      } else {
        // If the toolbar button has an open menu, don't attempt to open
        // a second menu
        if (mGestureDownContent->IsElement() &&
            mGestureDownContent->AsElement()->AttrValueIs(
                kNameSpaceID_None, nsGkAtoms::open, nsGkAtoms::_true,
                eCaseMatters)) {
          allowedToDispatch = false;
        }
      }
    } else if (mGestureDownContent->IsHTMLElement()) {
      nsCOMPtr<nsIFormControl> formCtrl(do_QueryInterface(mGestureDownContent));

      if (formCtrl) {
        allowedToDispatch =
            formCtrl->IsTextControl(/*aExcludePassword*/ false) ||
            formCtrl->ControlType() == NS_FORM_INPUT_FILE;
      } else if (mGestureDownContent->IsAnyOfHTMLElements(
                     nsGkAtoms::embed, nsGkAtoms::object, nsGkAtoms::label)) {
        allowedToDispatch = false;
      }
    }

    if (allowedToDispatch) {
      // init the event while mCurrentTarget is still good
      WidgetMouseEvent event(true, eContextMenu, targetWidget,
                             WidgetMouseEvent::eReal);
      event.mClickCount = 1;
      FillInEventFromGestureDown(&event);

      // stop selection tracking, we're in control now
      if (mCurrentTarget) {
        RefPtr<nsFrameSelection> frameSel = mCurrentTarget->GetFrameSelection();

        if (frameSel && frameSel->GetDragState()) {
          // note that this can cause selection changed events to fire if we're
          // in a text field, which will null out mCurrentTarget
          frameSel->SetDragState(false);
        }
      }

      AutoHandlingUserInputStatePusher userInpStatePusher(true, &event);

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
    StopTrackingDragGesture(true);
  }

  KillClickHoldTimer();

}  // FireContextClick

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
void EventStateManager::BeginTrackingDragGesture(nsPresContext* aPresContext,
                                                 WidgetMouseEvent* inDownEvent,
                                                 nsIFrame* inDownFrame) {
  if (!inDownEvent->mWidget) {
    return;
  }

  // Note that |inDownEvent| could be either a mouse down event or a
  // synthesized mouse move event.
  SetGestureDownPoint(inDownEvent);

  if (inDownFrame) {
    inDownFrame->GetContentForEvent(inDownEvent,
                                    getter_AddRefs(mGestureDownContent));

    mGestureDownFrameOwner = inDownFrame->GetContent();
    if (!mGestureDownFrameOwner) {
      mGestureDownFrameOwner = mGestureDownContent;
    }
  }
  mGestureModifiers = inDownEvent->mModifiers;
  mGestureDownButtons = inDownEvent->mButtons;

  if (inDownEvent->mMessage != eMouseTouchDrag &&
      Prefs::ClickHoldContextMenu()) {
    // fire off a timer to track click-hold
    CreateClickHoldTimer(aPresContext, inDownFrame, inDownEvent);
  }
}

void EventStateManager::SetGestureDownPoint(WidgetGUIEvent* aEvent) {
  mGestureDownPoint =
      GetEventRefPoint(aEvent) + aEvent->mWidget->WidgetToScreenOffset();
}

LayoutDeviceIntPoint EventStateManager::GetEventRefPoint(
    WidgetEvent* aEvent) const {
  auto touchEvent = aEvent->AsTouchEvent();
  return (touchEvent && !touchEvent->mTouches.IsEmpty())
             ? aEvent->AsTouchEvent()->mTouches[0]->mRefPoint
             : aEvent->mRefPoint;
}

void EventStateManager::BeginTrackingRemoteDragGesture(
    nsIContent* aContent, RemoteDragStartData* aDragStartData) {
  mGestureDownContent = aContent;
  mGestureDownFrameOwner = aContent;
  mGestureDownDragStartData = aDragStartData;
}

//
// StopTrackingDragGesture
//
// Record that the mouse has gone back up so that we should leave the TRACKING
// state of d&d gesture tracker and return to the START state.
//
void EventStateManager::StopTrackingDragGesture(bool aClearInChildProcesses) {
  mGestureDownContent = nullptr;
  mGestureDownFrameOwner = nullptr;
  mGestureDownDragStartData = nullptr;

  // If a content process starts a drag but the mouse is released before the
  // parent starts the actual drag, the content process will think a drag is
  // still happening. Inform any child processes with active drags that the drag
  // should be stopped.
  if (aClearInChildProcesses) {
    nsCOMPtr<nsIDragService> dragService =
        do_GetService("@mozilla.org/widget/dragservice;1");
    if (dragService) {
      nsCOMPtr<nsIDragSession> dragSession;
      dragService->GetCurrentSession(getter_AddRefs(dragSession));
      if (!dragSession) {
        // Only notify if there isn't a drag session active.
        dragService->RemoveAllChildProcesses();
      }
    }
  }
}

void EventStateManager::FillInEventFromGestureDown(WidgetMouseEvent* aEvent) {
  NS_ASSERTION(aEvent->mWidget == mCurrentTarget->GetNearestWidget(),
               "Incorrect widget in event");

  // Set the coordinates in the new event to the coordinates of
  // the old event, adjusted for the fact that the widget might be
  // different
  aEvent->mRefPoint =
      mGestureDownPoint - aEvent->mWidget->WidgetToScreenOffset();
  aEvent->mModifiers = mGestureModifiers;
  aEvent->mButtons = mGestureDownButtons;
}

void EventStateManager::MaybeFirePointerCancel(WidgetInputEvent* aEvent) {
  RefPtr<PresShell> presShell = mPresContext->GetPresShell();
  AutoWeakFrame targetFrame = mCurrentTarget;

  if (!StaticPrefs::dom_w3c_pointer_events_enabled() || !presShell ||
      !targetFrame) {
    return;
  }

  nsCOMPtr<nsIContent> content;
  targetFrame->GetContentForEvent(aEvent, getter_AddRefs(content));
  if (!content) {
    return;
  }

  nsEventStatus status = nsEventStatus_eIgnore;

  if (WidgetMouseEvent* aMouseEvent = aEvent->AsMouseEvent()) {
    WidgetPointerEvent event(*aMouseEvent);
    PointerEventHandler::InitPointerEventFromMouse(&event, aMouseEvent,
                                                   ePointerCancel);

    event.convertToPointer = false;
    presShell->HandleEventWithTarget(&event, targetFrame, content, &status);
  } else if (WidgetTouchEvent* aTouchEvent = aEvent->AsTouchEvent()) {
    WidgetPointerEvent event(aTouchEvent->IsTrusted(), ePointerCancel,
                             aTouchEvent->mWidget);

    PointerEventHandler::InitPointerEventFromTouch(
        &event, aTouchEvent, aTouchEvent->mTouches[0], true);

    event.convertToPointer = false;
    presShell->HandleEventWithTarget(&event, targetFrame, content, &status);
  } else {
    MOZ_ASSERT(false);
  }

  // HandleEventWithTarget clears out mCurrentTarget, which may be used in the
  // caller GenerateDragGesture. We have to restore mCurrentTarget.
  mCurrentTarget = targetFrame;
}

bool EventStateManager::IsEventOutsideDragThreshold(
    WidgetInputEvent* aEvent) const {
  static int32_t sPixelThresholdX = 0;
  static int32_t sPixelThresholdY = 0;

  if (!sPixelThresholdX) {
    sPixelThresholdX =
        LookAndFeel::GetInt(LookAndFeel::eIntID_DragThresholdX, 0);
    sPixelThresholdY =
        LookAndFeel::GetInt(LookAndFeel::eIntID_DragThresholdY, 0);
    if (!sPixelThresholdX) sPixelThresholdX = 5;
    if (!sPixelThresholdY) sPixelThresholdY = 5;
  }

  LayoutDeviceIntPoint pt =
      aEvent->mWidget->WidgetToScreenOffset() + GetEventRefPoint(aEvent);
  LayoutDeviceIntPoint distance = pt - mGestureDownPoint;
  return Abs(distance.x) > AssertedCast<uint32_t>(sPixelThresholdX) ||
         Abs(distance.y) > AssertedCast<uint32_t>(sPixelThresholdY);
}

//
// GenerateDragGesture
//
// If we're in the TRACKING state of the d&d gesture tracker, check the current
// position of the mouse in relation to the old one. If we've moved a sufficient
// amount from the mouse down, then fire off a drag gesture event.
void EventStateManager::GenerateDragGesture(nsPresContext* aPresContext,
                                            WidgetInputEvent* aEvent) {
  NS_ASSERTION(aPresContext, "This shouldn't happen.");
  if (!IsTrackingDragGesture()) {
    return;
  }

  mCurrentTarget = mGestureDownFrameOwner->GetPrimaryFrame();

  if (!mCurrentTarget || !mCurrentTarget->GetNearestWidget()) {
    StopTrackingDragGesture(true);
    return;
  }

  // Check if selection is tracking drag gestures, if so
  // don't interfere!
  if (mCurrentTarget) {
    RefPtr<nsFrameSelection> frameSel = mCurrentTarget->GetFrameSelection();
    if (frameSel && frameSel->GetDragState()) {
      StopTrackingDragGesture(true);
      return;
    }
  }

  // If non-native code is capturing the mouse don't start a drag.
  if (PresShell::IsMouseCapturePreventingDrag()) {
    StopTrackingDragGesture(true);
    return;
  }

  if (!IsEventOutsideDragThreshold(aEvent)) {
    // To keep the old behavior, flush layout even if we don't start dnd.
    FlushLayout(aPresContext);
    return;
  }

  if (Prefs::ClickHoldContextMenu()) {
    // stop the click-hold before we fire off the drag gesture, in case
    // it takes a long time
    KillClickHoldTimer();
  }

  nsCOMPtr<nsIDocShell> docshell = aPresContext->GetDocShell();
  if (!docshell) {
    return;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = docshell->GetWindow();
  if (!window) return;

  RefPtr<DataTransfer> dataTransfer =
      new DataTransfer(window, eDragStart, false, -1);
  auto protectDataTransfer = MakeScopeExit([&] {
    if (dataTransfer) {
      dataTransfer->Disconnect();
    }
  });

  RefPtr<Selection> selection;
  RefPtr<RemoteDragStartData> remoteDragStartData;
  nsCOMPtr<nsIContent> eventContent, targetContent;
  nsCOMPtr<nsIPrincipal> principal;
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  bool allowEmptyDataTransfer = false;
  mCurrentTarget->GetContentForEvent(aEvent, getter_AddRefs(eventContent));
  if (eventContent) {
    // If the content is a text node in a password field, we shouldn't
    // allow to drag its raw text.  Note that we've supported drag from
    // password fields but dragging data was masked text.  So, it doesn't
    // make sense anyway.
    if (eventContent->IsText() && eventContent->HasFlag(NS_MAYBE_MASKED)) {
      // However, it makes sense to allow to drag selected password text
      // when copying selected password is allowed because users may want
      // to use drag and drop rather than copy and paste when web apps
      // request to input password twice for conforming new password but
      // they used password generator.
      TextEditor* textEditor =
          nsContentUtils::GetTextEditorFromAnonymousNodeWithoutCreation(
              eventContent);
      if (!textEditor || !textEditor->IsCopyToClipboardAllowed()) {
        StopTrackingDragGesture(true);
        return;
      }
    }
    DetermineDragTargetAndDefaultData(
        window, eventContent, dataTransfer, &allowEmptyDataTransfer,
        getter_AddRefs(selection), getter_AddRefs(remoteDragStartData),
        getter_AddRefs(targetContent), getter_AddRefs(principal),
        getter_AddRefs(csp));
  }

  // Stop tracking the drag gesture now. This should stop us from
  // reentering GenerateDragGesture inside DOM event processing.
  // Pass false to avoid clearing the child process state since a real
  // drag should be starting.
  StopTrackingDragGesture(false);

  if (!targetContent) return;

  // Use our targetContent, now that we've determined it, as the
  // parent object of the DataTransfer.
  nsCOMPtr<nsIContent> parentContent =
      targetContent->FindFirstNonChromeOnlyAccessContent();
  dataTransfer->SetParentObject(parentContent);

  sLastDragOverFrame = nullptr;
  nsCOMPtr<nsIWidget> widget = mCurrentTarget->GetNearestWidget();

  // get the widget from the target frame
  WidgetDragEvent startEvent(aEvent->IsTrusted(), eDragStart, widget);
  startEvent.mFlags.mIsSynthesizedForTests =
      aEvent->mFlags.mIsSynthesizedForTests;
  FillInEventFromGestureDown(&startEvent);

  startEvent.mDataTransfer = dataTransfer;
  if (aEvent->AsMouseEvent()) {
    startEvent.mInputSource = aEvent->AsMouseEvent()->mInputSource;
  } else if (aEvent->AsTouchEvent()) {
    startEvent.mInputSource = MouseEvent_Binding::MOZ_SOURCE_TOUCH;
  } else {
    MOZ_ASSERT(false);
  }

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

  // Dispatch the dragstart event to the DOM.
  nsEventStatus status = nsEventStatus_eIgnore;
  EventDispatcher::Dispatch(targetContent, aPresContext, &startEvent, nullptr,
                            &status);

  WidgetDragEvent* event = &startEvent;

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  // Emit observer event to allow addons to modify the DataTransfer
  // object.
  if (observerService) {
    observerService->NotifyObservers(dataTransfer, "on-datatransfer-available",
                                     nullptr);
  }

  if (status != nsEventStatus_eConsumeNoDefault) {
    bool dragStarted = DoDefaultDragStart(
        aPresContext, event, dataTransfer, allowEmptyDataTransfer,
        targetContent, selection, remoteDragStartData, principal, csp);
    if (dragStarted) {
      sActiveESM = nullptr;
      MaybeFirePointerCancel(aEvent);
      aEvent->StopPropagation();
    }
  }

  // Reset mCurretTargetContent to what it was
  mCurrentTargetContent = targetBeforeEvent;

  // Now flush all pending notifications, for better responsiveness
  // while dragging.
  FlushLayout(aPresContext);
}  // GenerateDragGesture

void EventStateManager::DetermineDragTargetAndDefaultData(
    nsPIDOMWindowOuter* aWindow, nsIContent* aSelectionTarget,
    DataTransfer* aDataTransfer, bool* aAllowEmptyDataTransfer,
    Selection** aSelection, RemoteDragStartData** aRemoteDragStartData,
    nsIContent** aTargetNode, nsIPrincipal** aPrincipal,
    nsIContentSecurityPolicy** aCsp) {
  *aTargetNode = nullptr;
  *aAllowEmptyDataTransfer = false;
  nsCOMPtr<nsIContent> dragDataNode;

  nsIContent* editingElement = aSelectionTarget->IsEditable()
                                   ? aSelectionTarget->GetEditingHost()
                                   : nullptr;

  // In chrome, only allow dragging inside editable areas.
  bool isChromeContext = !aWindow->GetBrowsingContext()->IsContent();
  if (isChromeContext && !editingElement) {
    if (mGestureDownDragStartData) {
      // A child process started a drag so use any data it assigned for the dnd
      // session.
      mGestureDownDragStartData->AddInitialDnDDataTo(aDataTransfer, aPrincipal,
                                                     aCsp);
      mGestureDownDragStartData.forget(aRemoteDragStartData);
      *aAllowEmptyDataTransfer = true;
    }
  } else {
    mGestureDownDragStartData = nullptr;

    // GetDragData determines if a selection, link or image in the content
    // should be dragged, and places the data associated with the drag in the
    // data transfer.
    // mGestureDownContent is the node where the mousedown event for the drag
    // occurred, and aSelectionTarget is the node to use when a selection is
    // used
    bool canDrag;
    bool wasAlt = (mGestureModifiers & MODIFIER_ALT) != 0;
    nsresult rv = nsContentAreaDragDrop::GetDragData(
        aWindow, mGestureDownContent, aSelectionTarget, wasAlt, aDataTransfer,
        &canDrag, aSelection, getter_AddRefs(dragDataNode), aPrincipal, aCsp);
    if (NS_FAILED(rv) || !canDrag) {
      return;
    }
  }

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
      if (auto htmlElement = nsGenericHTMLElement::FromNode(dragContent)) {
        if (htmlElement->Draggable()) {
          // We let draggable elements to trigger dnd even if there is no data
          // in the DataTransfer.
          *aAllowEmptyDataTransfer = true;
          break;
        }
      } else {
        if (dragContent->IsXULElement()) {
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
      dragContent = dragContent->GetFlattenedTreeParent();
    }
  }

  // if no node in the hierarchy was found to drag, but the GetDragData method
  // returned a node, use that returned node. Otherwise, nothing is draggable.
  if (!dragContent && dragDataNode) dragContent = dragDataNode;

  if (dragContent) {
    // if an ancestor node was used instead, clear the drag data
    // XXXndeakin rework this a bit. Find a way to just not call GetDragData if
    // we don't need to.
    if (dragContent != originalDragContent) aDataTransfer->ClearAll();
    *aTargetNode = dragContent;
    NS_ADDREF(*aTargetNode);
  }
}

bool EventStateManager::DoDefaultDragStart(
    nsPresContext* aPresContext, WidgetDragEvent* aDragEvent,
    DataTransfer* aDataTransfer, bool aAllowEmptyDataTransfer,
    nsIContent* aDragTarget, Selection* aSelection,
    RemoteDragStartData* aDragStartData, nsIPrincipal* aPrincipal,
    nsIContentSecurityPolicy* aCsp) {
  nsCOMPtr<nsIDragService> dragService =
      do_GetService("@mozilla.org/widget/dragservice;1");
  if (!dragService) return false;

  // Default handling for the dragstart event.
  //
  // First, check if a drag session already exists. This means that the drag
  // service was called directly within a draggesture handler. In this case,
  // don't do anything more, as it is assumed that the handler is managing
  // drag and drop manually. Make sure to return true to indicate that a drag
  // began.  However, if we're handling drag session for synthesized events,
  // we need to initialize some information of the session.  Therefore, we
  // need to keep going for synthesized case.
  nsCOMPtr<nsIDragSession> dragSession;
  dragService->GetCurrentSession(getter_AddRefs(dragSession));
  if (dragSession && !dragSession->IsSynthesizedForTests()) {
    return true;
  }

  // No drag session is currently active, so check if a handler added
  // any items to be dragged. If not, there isn't anything to drag.
  uint32_t count = 0;
  if (aDataTransfer) {
    count = aDataTransfer->MozItemCount();
  }
  if (!aAllowEmptyDataTransfer && !count) {
    return false;
  }

  // Get the target being dragged, which may not be the same as the
  // target of the mouse event. If one wasn't set in the
  // aDataTransfer during the event handler, just use the original
  // target instead.
  nsCOMPtr<nsIContent> dragTarget = aDataTransfer->GetDragTarget();
  if (!dragTarget) {
    dragTarget = aDragTarget;
    if (!dragTarget) {
      return false;
    }
  }

  // check which drag effect should initially be used. If the effect was not
  // set, just use all actions, otherwise Windows won't allow a drop.
  uint32_t action = aDataTransfer->EffectAllowedInt();
  if (action == nsIDragService::DRAGDROP_ACTION_UNINITIALIZED) {
    action = nsIDragService::DRAGDROP_ACTION_COPY |
             nsIDragService::DRAGDROP_ACTION_MOVE |
             nsIDragService::DRAGDROP_ACTION_LINK;
  }

  // get any custom drag image that was set
  int32_t imageX, imageY;
  RefPtr<Element> dragImage = aDataTransfer->GetDragImage(&imageX, &imageY);

  nsCOMPtr<nsIArray> transArray = aDataTransfer->GetTransferables(dragTarget);
  if (!transArray) {
    return false;
  }

  RefPtr<DataTransfer> dataTransfer;
  if (!dragSession) {
    // After this function returns, the DataTransfer will be cleared so it
    // appears empty to content. We need to pass a DataTransfer into the Drag
    // Session, so we need to make a copy.
    aDataTransfer->Clone(aDragTarget, eDrop, aDataTransfer->MozUserCancelled(),
                         false, getter_AddRefs(dataTransfer));

    // Copy over the drop effect, as Clone doesn't copy it for us.
    dataTransfer->SetDropEffectInt(aDataTransfer->DropEffectInt());
  } else {
    MOZ_ASSERT(dragSession->IsSynthesizedForTests());
    MOZ_ASSERT(aDragEvent->mFlags.mIsSynthesizedForTests);
    // If we're initializing synthesized drag session, we should use given
    // DataTransfer as is because it'll be used with following drag events
    // in any tests, therefore it should be set to nsIDragSession.dataTransfer
    // because it and DragEvent.dataTransfer should be same instance.
    dataTransfer = aDataTransfer;
  }

  // XXXndeakin don't really want to create a new drag DOM event
  // here, but we need something to pass to the InvokeDragSession
  // methods.
  RefPtr<DragEvent> event =
      NS_NewDOMDragEvent(dragTarget, aPresContext, aDragEvent);

  // Use InvokeDragSessionWithSelection if a selection is being dragged,
  // such that the image can be generated from the selected text. However,
  // use InvokeDragSessionWithImage if a custom image was set or something
  // other than a selection is being dragged.
  if (!dragImage && aSelection) {
    dragService->InvokeDragSessionWithSelection(
        aSelection, aPrincipal, aCsp, transArray, action, event, dataTransfer);
  } else if (aDragStartData) {
    MOZ_ASSERT(XRE_IsParentProcess());
    dragService->InvokeDragSessionWithRemoteImage(
        dragTarget, aPrincipal, aCsp, transArray, action, aDragStartData, event,
        dataTransfer);
  } else {
    dragService->InvokeDragSessionWithImage(
        dragTarget, aPrincipal, aCsp, transArray, action, dragImage, imageX,
        imageY, event, dataTransfer);
  }

  return true;
}

nsresult EventStateManager::ChangeZoom(int32_t change) {
  MOZ_ASSERT(change == 1 || change == -1, "Can only change by +/- 10%.");

  // Send the zoom change as a chrome event so it will be handled
  // by the front end actors in the same way as other zoom actions.
  // This excludes documents hosted in non-browser containers, like
  // in a WebExtension.
  nsContentUtils::DispatchChromeEvent(
      mDocument, ToSupports(mDocument),
      (change == 1 ? NS_LITERAL_STRING("DoZoomEnlargeBy10")
                   : NS_LITERAL_STRING("DoZoomReduceBy10")),
      CanBubble::eYes, Cancelable::eYes);

  return NS_OK;
}

void EventStateManager::DoScrollHistory(int32_t direction) {
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

void EventStateManager::DoScrollZoom(nsIFrame* aTargetFrame,
                                     int32_t adjustment) {
  // Exclude content in chrome docshells.
  nsIContent* content = aTargetFrame->GetContent();
  if (content && !nsContentUtils::IsInChromeDocshell(content->OwnerDoc())) {
    // positive adjustment to decrease zoom, negative to increase
    int32_t change = (adjustment > 0) ? -1 : 1;

    EnsureDocument(mPresContext);
    ChangeZoom(change);
    nsContentUtils::DispatchChromeEvent(
        mDocument, ToSupports(mDocument),
        NS_LITERAL_STRING("ZoomChangeUsingMouseWheel"), CanBubble::eYes,
        Cancelable::eYes);
  }
}

static nsIFrame* GetParentFrameToScroll(nsIFrame* aFrame) {
  if (!aFrame) return nullptr;

  if (aFrame->StyleDisplay()->mPosition == StylePositionProperty::Fixed &&
      nsLayoutUtils::IsReallyFixedPos(aFrame))
    return aFrame->PresContext()->GetPresShell()->GetRootScrollFrame();

  return aFrame->GetParent();
}

void EventStateManager::DispatchLegacyMouseScrollEvents(
    nsIFrame* aTargetFrame, WidgetWheelEvent* aEvent, nsEventStatus* aStatus) {
  MOZ_ASSERT(aEvent);
  MOZ_ASSERT(aStatus);

  if (!aTargetFrame || *aStatus == nsEventStatus_eConsumeNoDefault) {
    return;
  }

  // Ignore mouse wheel transaction for computing legacy mouse wheel
  // events' delta value.
  nsIFrame* scrollFrame = ComputeScrollTargetAndMayAdjustWheelEvent(
      aTargetFrame, aEvent, COMPUTE_LEGACY_MOUSE_SCROLL_EVENT_TARGET);

  nsIScrollableFrame* scrollTarget = do_QueryFrame(scrollFrame);
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
  switch (aEvent->mDeltaMode) {
    case WheelEvent_Binding::DOM_DELTA_PAGE:
      scrollDeltaX = !aEvent->mLineOrPageDeltaX
                         ? 0
                         : (aEvent->mLineOrPageDeltaX > 0
                                ? UIEvent_Binding::SCROLL_PAGE_DOWN
                                : UIEvent_Binding::SCROLL_PAGE_UP);
      scrollDeltaY = !aEvent->mLineOrPageDeltaY
                         ? 0
                         : (aEvent->mLineOrPageDeltaY > 0
                                ? UIEvent_Binding::SCROLL_PAGE_DOWN
                                : UIEvent_Binding::SCROLL_PAGE_UP);
      pixelDeltaX = RoundDown(aEvent->mDeltaX * scrollAmountInCSSPixels.width);
      pixelDeltaY = RoundDown(aEvent->mDeltaY * scrollAmountInCSSPixels.height);
      break;

    case WheelEvent_Binding::DOM_DELTA_LINE:
      scrollDeltaX = aEvent->mLineOrPageDeltaX;
      scrollDeltaY = aEvent->mLineOrPageDeltaY;
      pixelDeltaX = RoundDown(aEvent->mDeltaX * scrollAmountInCSSPixels.width);
      pixelDeltaY = RoundDown(aEvent->mDeltaY * scrollAmountInCSSPixels.height);
      break;

    case WheelEvent_Binding::DOM_DELTA_PIXEL:
      scrollDeltaX = aEvent->mLineOrPageDeltaX;
      scrollDeltaY = aEvent->mLineOrPageDeltaY;
      pixelDeltaX = RoundDown(aEvent->mDeltaX);
      pixelDeltaY = RoundDown(aEvent->mDeltaY);
      break;

    default:
      MOZ_CRASH("Invalid deltaMode value comes");
  }

  // Send the legacy events in following order:
  // 1. Vertical scroll
  // 2. Vertical pixel scroll (even if #1 isn't consumed)
  // 3. Horizontal scroll (even if #1 and/or #2 are consumed)
  // 4. Horizontal pixel scroll (even if #3 isn't consumed)

  AutoWeakFrame targetFrame(aTargetFrame);

  MOZ_ASSERT(*aStatus != nsEventStatus_eConsumeNoDefault &&
                 !aEvent->DefaultPrevented(),
             "If you make legacy events dispatched for default prevented wheel "
             "event, you need to initialize stateX and stateY");
  EventState stateX, stateY;
  if (scrollDeltaY) {
    SendLineScrollEvent(aTargetFrame, aEvent, stateY, scrollDeltaY,
                        DELTA_DIRECTION_Y);
    if (!targetFrame.IsAlive()) {
      *aStatus = nsEventStatus_eConsumeNoDefault;
      return;
    }
  }

  if (pixelDeltaY) {
    SendPixelScrollEvent(aTargetFrame, aEvent, stateY, pixelDeltaY,
                         DELTA_DIRECTION_Y);
    if (!targetFrame.IsAlive()) {
      *aStatus = nsEventStatus_eConsumeNoDefault;
      return;
    }
  }

  if (scrollDeltaX) {
    SendLineScrollEvent(aTargetFrame, aEvent, stateX, scrollDeltaX,
                        DELTA_DIRECTION_X);
    if (!targetFrame.IsAlive()) {
      *aStatus = nsEventStatus_eConsumeNoDefault;
      return;
    }
  }

  if (pixelDeltaX) {
    SendPixelScrollEvent(aTargetFrame, aEvent, stateX, pixelDeltaX,
                         DELTA_DIRECTION_X);
    if (!targetFrame.IsAlive()) {
      *aStatus = nsEventStatus_eConsumeNoDefault;
      return;
    }
  }

  if (stateY.mDefaultPrevented) {
    *aStatus = nsEventStatus_eConsumeNoDefault;
    aEvent->PreventDefault(!stateY.mDefaultPreventedByContent);
  }

  if (stateX.mDefaultPrevented) {
    *aStatus = nsEventStatus_eConsumeNoDefault;
    aEvent->PreventDefault(!stateX.mDefaultPreventedByContent);
  }
}

void EventStateManager::SendLineScrollEvent(nsIFrame* aTargetFrame,
                                            WidgetWheelEvent* aEvent,
                                            EventState& aState, int32_t aDelta,
                                            DeltaDirection aDeltaDirection) {
  nsCOMPtr<nsIContent> targetContent = aTargetFrame->GetContent();
  if (!targetContent) targetContent = GetFocusedContent();
  if (!targetContent) return;

  while (targetContent->IsText()) {
    targetContent = targetContent->GetFlattenedTreeParent();
  }

  WidgetMouseScrollEvent event(aEvent->IsTrusted(),
                               eLegacyMouseLineOrPageScroll, aEvent->mWidget);
  event.mFlags.mDefaultPrevented = aState.mDefaultPrevented;
  event.mFlags.mDefaultPreventedByContent = aState.mDefaultPreventedByContent;
  event.mRefPoint = aEvent->mRefPoint;
  event.mTime = aEvent->mTime;
  event.mTimeStamp = aEvent->mTimeStamp;
  event.mModifiers = aEvent->mModifiers;
  event.mButtons = aEvent->mButtons;
  event.mIsHorizontal = (aDeltaDirection == DELTA_DIRECTION_X);
  event.mDelta = aDelta;
  event.mInputSource = aEvent->mInputSource;

  nsEventStatus status = nsEventStatus_eIgnore;
  EventDispatcher::Dispatch(targetContent, aTargetFrame->PresContext(), &event,
                            nullptr, &status);
  aState.mDefaultPrevented =
      event.DefaultPrevented() || status == nsEventStatus_eConsumeNoDefault;
  aState.mDefaultPreventedByContent = event.DefaultPreventedByContent();
}

void EventStateManager::SendPixelScrollEvent(nsIFrame* aTargetFrame,
                                             WidgetWheelEvent* aEvent,
                                             EventState& aState,
                                             int32_t aPixelDelta,
                                             DeltaDirection aDeltaDirection) {
  nsCOMPtr<nsIContent> targetContent = aTargetFrame->GetContent();
  if (!targetContent) {
    targetContent = GetFocusedContent();
    if (!targetContent) return;
  }

  while (targetContent->IsText()) {
    targetContent = targetContent->GetFlattenedTreeParent();
  }

  WidgetMouseScrollEvent event(aEvent->IsTrusted(), eLegacyMousePixelScroll,
                               aEvent->mWidget);
  event.mFlags.mDefaultPrevented = aState.mDefaultPrevented;
  event.mFlags.mDefaultPreventedByContent = aState.mDefaultPreventedByContent;
  event.mRefPoint = aEvent->mRefPoint;
  event.mTime = aEvent->mTime;
  event.mTimeStamp = aEvent->mTimeStamp;
  event.mModifiers = aEvent->mModifiers;
  event.mButtons = aEvent->mButtons;
  event.mIsHorizontal = (aDeltaDirection == DELTA_DIRECTION_X);
  event.mDelta = aPixelDelta;
  event.mInputSource = aEvent->mInputSource;

  nsEventStatus status = nsEventStatus_eIgnore;
  EventDispatcher::Dispatch(targetContent, aTargetFrame->PresContext(), &event,
                            nullptr, &status);
  aState.mDefaultPrevented =
      event.DefaultPrevented() || status == nsEventStatus_eConsumeNoDefault;
  aState.mDefaultPreventedByContent = event.DefaultPreventedByContent();
}

nsIFrame* EventStateManager::ComputeScrollTargetAndMayAdjustWheelEvent(
    nsIFrame* aTargetFrame, WidgetWheelEvent* aEvent,
    ComputeScrollTargetOptions aOptions) {
  return ComputeScrollTargetAndMayAdjustWheelEvent(
      aTargetFrame, aEvent->mDeltaX, aEvent->mDeltaY, aEvent, aOptions);
}

// Overload ComputeScrollTargetAndMayAdjustWheelEvent method to allow passing
// "test" dx and dy when looking for which scrollbarmediators to activate when
// two finger down on trackpad and before any actual motion
nsIFrame* EventStateManager::ComputeScrollTargetAndMayAdjustWheelEvent(
    nsIFrame* aTargetFrame, double aDirectionX, double aDirectionY,
    WidgetWheelEvent* aEvent, ComputeScrollTargetOptions aOptions) {
  if ((aOptions & INCLUDE_PLUGIN_AS_TARGET) &&
      !WheelPrefs::WheelEventsEnabledOnPlugins()) {
    aOptions = RemovePluginFromTarget(aOptions);
  }

  bool isAutoDir = false;
  bool honoursRoot = false;
  if (MAY_BE_ADJUSTED_BY_AUTO_DIR & aOptions) {
    // If the scroll is respected as auto-dir, aDirection* should always be
    // equivalent to the event's delta vlaues(Currently, there are only one case
    // where aDirection*s have different values from the widget wheel event's
    // original delta values and the only case isn't auto-dir, see
    // ScrollbarsForWheel::TemporarilyActivateAllPossibleScrollTargets).
    MOZ_ASSERT(aDirectionX == aEvent->mDeltaX &&
               aDirectionY == aEvent->mDeltaY);

    WheelDeltaAdjustmentStrategy strategy =
        GetWheelDeltaAdjustmentStrategy(*aEvent);
    switch (strategy) {
      case WheelDeltaAdjustmentStrategy::eAutoDir:
        isAutoDir = true;
        honoursRoot = false;
        break;
      case WheelDeltaAdjustmentStrategy::eAutoDirWithRootHonour:
        isAutoDir = true;
        honoursRoot = true;
        break;
      default:
        break;
    }
  }

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
      if (aOptions & INCLUDE_PLUGIN_AS_TARGET) {
        nsPluginFrame* pluginFrame = do_QueryFrame(lastScrollFrame);
        if (pluginFrame &&
            pluginFrame->WantsToHandleWheelEventAsDefaultAction()) {
          return lastScrollFrame;
        }
      }
      nsIScrollableFrame* scrollableFrame =
          lastScrollFrame->GetScrollTargetFrame();
      if (scrollableFrame) {
        nsIFrame* frameToScroll = do_QueryFrame(scrollableFrame);
        MOZ_ASSERT(frameToScroll);
        if (isAutoDir) {
          ESMAutoDirWheelDeltaAdjuster adjuster(*aEvent, *lastScrollFrame,
                                                honoursRoot);
          // Note that calling this function will not always cause the delta to
          // be adjusted, it only adjusts the delta when it should, because
          // Adjust() internally calls ShouldBeAdjusted() before making
          // adjustment.
          adjuster.Adjust();
        }
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

  bool checkIfScrollableX;
  bool checkIfScrollableY;
  if (isAutoDir) {
    // Always check the frame's scrollability in both the two directions for an
    // auto-dir scroll. That is, for an auto-dir scroll,
    // PREFER_ACTUAL_SCROLLABLE_TARGET_ALONG_X_AXIS and
    // PREFER_ACTUAL_SCROLLABLE_TARGET_ALONG_Y_AXIS should be ignored.
    checkIfScrollableX = true;
    checkIfScrollableY = true;
  } else {
    checkIfScrollableX =
        aDirectionX &&
        (aOptions & PREFER_ACTUAL_SCROLLABLE_TARGET_ALONG_X_AXIS);
    checkIfScrollableY =
        aDirectionY &&
        (aOptions & PREFER_ACTUAL_SCROLLABLE_TARGET_ALONG_Y_AXIS);
  }

  nsIFrame* scrollFrame = !(aOptions & START_FROM_PARENT)
                              ? aTargetFrame
                              : GetParentFrameToScroll(aTargetFrame);
  for (; scrollFrame; scrollFrame = GetParentFrameToScroll(scrollFrame)) {
    // Check whether the frame wants to provide us with a scrollable view.
    nsIScrollableFrame* scrollableFrame = scrollFrame->GetScrollTargetFrame();
    if (!scrollableFrame) {
      // If the frame is a plugin frame, then, the plugin content may handle
      // wheel events.  Only when the caller computes the scroll target for
      // default action handling, we should assume the plugin frame as
      // scrollable if the plugin wants to handle wheel events as default
      // action.
      if (aOptions & INCLUDE_PLUGIN_AS_TARGET) {
        nsPluginFrame* pluginFrame = do_QueryFrame(scrollFrame);
        if (pluginFrame &&
            pluginFrame->WantsToHandleWheelEventAsDefaultAction()) {
          return scrollFrame;
        }
      }
      nsMenuPopupFrame* menuPopupFrame = do_QueryFrame(scrollFrame);
      if (menuPopupFrame) {
        return nullptr;
      }
      continue;
    }

    nsIFrame* frameToScroll = do_QueryFrame(scrollableFrame);
    MOZ_ASSERT(frameToScroll);

    if (!checkIfScrollableX && !checkIfScrollableY) {
      return frameToScroll;
    }

    // If the frame disregards the direction the user is trying to scroll, then
    // it should just bubbles the scroll event up to its parental scroll frame
    Maybe<layers::ScrollDirection> disregardedDirection =
        WheelHandlingUtils::GetDisregardedWheelScrollDirection(scrollFrame);
    if (disregardedDirection) {
      switch (disregardedDirection.ref()) {
        case layers::ScrollDirection::eHorizontal:
          if (checkIfScrollableX) {
            continue;
          }
          break;
        case layers::ScrollDirection::eVertical:
          if (checkIfScrollableY) {
            continue;
          }
          break;
      }
    }

    uint32_t directions =
        scrollableFrame->GetAvailableScrollingDirectionsForUserInputEvents();
    if ((!(directions & nsIScrollableFrame::VERTICAL) &&
         !(directions & nsIScrollableFrame::HORIZONTAL)) ||
        (checkIfScrollableY && !checkIfScrollableX &&
         !(directions & nsIScrollableFrame::VERTICAL)) ||
        (checkIfScrollableX && !checkIfScrollableY &&
         !(directions & nsIScrollableFrame::HORIZONTAL))) {
      continue;
    }

    // Computes whether the currently checked frame is scrollable by this wheel
    // event.
    bool canScroll = false;
    if (isAutoDir) {
      ESMAutoDirWheelDeltaAdjuster adjuster(*aEvent, *scrollFrame, honoursRoot);
      if (adjuster.ShouldBeAdjusted()) {
        adjuster.Adjust();
        canScroll = true;
      } else if (WheelHandlingUtils::CanScrollOn(scrollableFrame, aDirectionX,
                                                 aDirectionY)) {
        canScroll = true;
      }
    } else if (WheelHandlingUtils::CanScrollOn(scrollableFrame, aDirectionX,
                                               aDirectionY)) {
      canScroll = true;
    }

    // Comboboxes need special care.
    nsComboboxControlFrame* comboBox = do_QueryFrame(scrollFrame);
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

    // Where we are at is the block ending in a for loop.
    // The current frame has been checked to be unscrollable by this wheel
    // event, continue the loop to check its parent, if any.
  }

  nsIFrame* newFrame = nsLayoutUtils::GetCrossDocParentFrame(
      aTargetFrame->PresShell()->GetRootFrame());
  aOptions =
      static_cast<ComputeScrollTargetOptions>(aOptions & ~START_FROM_PARENT);
  if (!newFrame) {
    return nullptr;
  }
  return ComputeScrollTargetAndMayAdjustWheelEvent(newFrame, aEvent, aOptions);
}

nsSize EventStateManager::GetScrollAmount(
    nsPresContext* aPresContext, WidgetWheelEvent* aEvent,
    nsIScrollableFrame* aScrollableFrame) {
  MOZ_ASSERT(aPresContext);
  MOZ_ASSERT(aEvent);

  bool isPage = (aEvent->mDeltaMode == WheelEvent_Binding::DOM_DELTA_PAGE);
  if (aScrollableFrame) {
    return isPage ? aScrollableFrame->GetPageScrollAmount()
                  : aScrollableFrame->GetLineScrollAmount();
  }

  // If there is no scrollable frame and page scrolling, use viewport size.
  if (isPage) {
    return aPresContext->GetVisibleArea().Size();
  }

  // If there is no scrollable frame, we should use root frame's information.
  nsIFrame* rootFrame = aPresContext->PresShell()->GetRootFrame();
  if (!rootFrame) {
    return nsSize(0, 0);
  }
  RefPtr<nsFontMetrics> fm =
      nsLayoutUtils::GetInflatedFontMetricsForFrame(rootFrame);
  NS_ENSURE_TRUE(fm, nsSize(0, 0));
  return nsSize(fm->AveCharWidth(), fm->MaxHeight());
}

void EventStateManager::DoScrollText(nsIScrollableFrame* aScrollableFrame,
                                     WidgetWheelEvent* aEvent) {
  MOZ_ASSERT(aScrollableFrame);
  MOZ_ASSERT(aEvent);

  nsIFrame* scrollFrame = do_QueryFrame(aScrollableFrame);
  MOZ_ASSERT(scrollFrame);

  AutoWeakFrame scrollFrameWeak(scrollFrame);
  if (!WheelTransaction::WillHandleDefaultAction(aEvent, scrollFrameWeak)) {
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
      DeltaAccumulator::GetInstance()->ComputeScrollAmountForDefaultAction(
          aEvent, scrollAmountInDevPixels);

  // Don't scroll around the axis whose overflow style is hidden.
  ScrollStyles overflowStyle = aScrollableFrame->GetScrollStyles();
  if (overflowStyle.mHorizontal == StyleOverflow::Hidden) {
    actualDevPixelScrollAmount.x = 0;
  }
  if (overflowStyle.mVertical == StyleOverflow::Hidden) {
    actualDevPixelScrollAmount.y = 0;
  }

  nsIScrollbarMediator::ScrollSnapMode snapMode =
      nsIScrollbarMediator::DISABLE_SNAP;
  nsAtom* origin = nullptr;
  switch (aEvent->mDeltaMode) {
    case WheelEvent_Binding::DOM_DELTA_LINE:
      origin = nsGkAtoms::mouseWheel;
      snapMode = nsIScrollableFrame::ENABLE_SNAP;
      break;
    case WheelEvent_Binding::DOM_DELTA_PAGE:
      origin = nsGkAtoms::pages;
      snapMode = nsIScrollableFrame::ENABLE_SNAP;
      break;
    case WheelEvent_Binding::DOM_DELTA_PIXEL:
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
    actualDevPixelScrollAmount.x = (actualDevPixelScrollAmount.x >= 0)
                                       ? devPixelPageSize.width
                                       : -devPixelPageSize.width;
  }

  if (!WheelPrefs::GetInstance()->IsOverOnePageScrollAllowedY(aEvent) &&
      DeprecatedAbs(actualDevPixelScrollAmount.y) > devPixelPageSize.height) {
    actualDevPixelScrollAmount.y = (actualDevPixelScrollAmount.y >= 0)
                                       ? devPixelPageSize.height
                                       : -devPixelPageSize.height;
  }

  bool isDeltaModePixel =
      (aEvent->mDeltaMode == WheelEvent_Binding::DOM_DELTA_PIXEL);

  ScrollMode mode;
  switch (aEvent->mScrollType) {
    case WidgetWheelEvent::SCROLL_DEFAULT:
      if (isDeltaModePixel) {
        mode = ScrollMode::Normal;
      } else if (aEvent->mFlags.mHandledByAPZ) {
        mode = ScrollMode::SmoothMsd;
      } else {
        mode = ScrollMode::Smooth;
      }
      break;
    case WidgetWheelEvent::SCROLL_SYNCHRONOUSLY:
      mode = ScrollMode::Instant;
      break;
    case WidgetWheelEvent::SCROLL_ASYNCHRONOUSELY:
      mode = ScrollMode::Normal;
      break;
    case WidgetWheelEvent::SCROLL_SMOOTHLY:
      mode = ScrollMode::Smooth;
      break;
    default:
      MOZ_CRASH("Invalid mScrollType value comes");
  }

  nsIScrollableFrame::ScrollMomentum momentum =
      aEvent->mIsMomentum ? nsIScrollableFrame::SYNTHESIZED_MOMENTUM_EVENT
                          : nsIScrollableFrame::NOT_MOMENTUM;

  nsIntPoint overflow;
  aScrollableFrame->ScrollBy(actualDevPixelScrollAmount,
                             ScrollUnit::DEVICE_PIXELS, mode, &overflow, origin,
                             momentum, snapMode);

  if (!scrollFrameWeak.IsAlive()) {
    // If the scroll causes changing the layout, we can think that the event
    // has been completely consumed by the content.  Then, users probably don't
    // want additional action.
    aEvent->mOverflowDeltaX = aEvent->mOverflowDeltaY = 0;
  } else if (isDeltaModePixel) {
    aEvent->mOverflowDeltaX = overflow.x;
    aEvent->mOverflowDeltaY = overflow.y;
  } else {
    aEvent->mOverflowDeltaX =
        static_cast<double>(overflow.x) / scrollAmountInDevPixels.width;
    aEvent->mOverflowDeltaY =
        static_cast<double>(overflow.y) / scrollAmountInDevPixels.height;
  }

  // If CSS overflow properties caused not to scroll, the overflowDelta* values
  // should be same as delta* values since they may be used as gesture event by
  // widget.  However, if there is another scrollable element in the ancestor
  // along the axis, probably users don't want the operation to cause
  // additional action such as moving history.  In such case, overflowDelta
  // values should stay zero.
  if (scrollFrameWeak.IsAlive()) {
    if (aEvent->mDeltaX && overflowStyle.mHorizontal == StyleOverflow::Hidden &&
        !ComputeScrollTargetAndMayAdjustWheelEvent(
            scrollFrame, aEvent,
            COMPUTE_SCROLLABLE_ANCESTOR_ALONG_X_AXIS_WITH_AUTO_DIR)) {
      aEvent->mOverflowDeltaX = aEvent->mDeltaX;
    }
    if (aEvent->mDeltaY && overflowStyle.mVertical == StyleOverflow::Hidden &&
        !ComputeScrollTargetAndMayAdjustWheelEvent(
            scrollFrame, aEvent,
            COMPUTE_SCROLLABLE_ANCESTOR_ALONG_Y_AXIS_WITH_AUTO_DIR)) {
      aEvent->mOverflowDeltaY = aEvent->mDeltaY;
    }
  }

  NS_ASSERTION(
      aEvent->mOverflowDeltaX == 0 ||
          (aEvent->mOverflowDeltaX > 0) == (aEvent->mDeltaX > 0),
      "The sign of mOverflowDeltaX is different from the scroll direction");
  NS_ASSERTION(
      aEvent->mOverflowDeltaY == 0 ||
          (aEvent->mOverflowDeltaY > 0) == (aEvent->mDeltaY > 0),
      "The sign of mOverflowDeltaY is different from the scroll direction");

  WheelPrefs::GetInstance()->CancelApplyingUserPrefsFromOverflowDelta(aEvent);
}

void EventStateManager::DecideGestureEvent(WidgetGestureNotifyEvent* aEvent,
                                           nsIFrame* targetFrame) {
  NS_ASSERTION(aEvent->mMessage == eGestureNotify,
               "DecideGestureEvent called with a non-gesture event");

  /* Check the ancestor tree to decide if any frame is willing* to receive
   * a MozPixelScroll event. If that's the case, the current touch gesture
   * will be used as a pan gesture; otherwise it will be a regular
   * mousedown/mousemove/click event.
   *
   * *willing: determine if it makes sense to pan the element using scroll
   * events:
   *  - For web content: if there are any visible scrollbars on the touch point
   *  - For XUL: if it's an scrollable element that can currently scroll in some
   *    direction.
   *
   * Note: we'll have to one-off various cases to ensure a good usable behavior
   */
  WidgetGestureNotifyEvent::PanDirection panDirection =
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

    LayoutFrameType currentFrameType = current->Type();

    // Scrollbars should always be draggable
    if (currentFrameType == LayoutFrameType::Scrollbar) {
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

        if (targetFrame->IsMenuFrame()) {
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
      } else {  // Not a XUL box
        uint32_t scrollbarVisibility =
            scrollableFrame->GetScrollbarVisibility();

        // Check if we have visible scrollbars
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
    }  // scrollableFrame
  }    // ancestor chain
  aEvent->mDisplayPanFeedback = displayPanFeedback;
  aEvent->mPanDirection = panDirection;
}

#ifdef XP_MACOSX
static bool NodeAllowsClickThrough(nsINode* aNode) {
  while (aNode) {
    if (aNode->IsXULElement(nsGkAtoms::browser)) {
      return false;
    }
    if (aNode->IsXULElement()) {
      mozilla::dom::Element* element = aNode->AsElement();
      static Element::AttrValuesArray strings[] = {nsGkAtoms::always,
                                                   nsGkAtoms::never, nullptr};
      switch (element->FindAttrValueIn(
          kNameSpaceID_None, nsGkAtoms::clickthrough, strings, eCaseMatters)) {
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

void EventStateManager::PostHandleKeyboardEvent(
    WidgetKeyboardEvent* aKeyboardEvent, nsIFrame* aTargetFrame,
    nsEventStatus& aStatus) {
  if (aStatus == nsEventStatus_eConsumeNoDefault) {
    return;
  }

  if (!aKeyboardEvent->HasBeenPostedToRemoteProcess()) {
    if (aKeyboardEvent->IsWaitingReplyFromRemoteProcess()) {
      RefPtr<BrowserParent> remote =
          aTargetFrame ? BrowserParent::GetFrom(aTargetFrame->GetContent())
                       : nullptr;
      if (remote) {
        // remote is null-checked above in order to let pre-existing event
        // targeting code's chrome vs. content decision override in case of
        // disagreement in order not to disrupt non-Fission e10s mode in case
        // there are still bugs in the Fission-mode code. That is, if remote
        // is nullptr, the pre-existing event targeting code has deemed this
        // event to belong to chrome rather than content.
        BrowserParent* preciseRemote = BrowserParent::GetFocused();
        if (preciseRemote) {
          remote = preciseRemote;
        }
        // else there was a race between layout and focus tracking
      }
      if (remote && !remote->IsReadyToHandleInputEvents()) {
        // We need to dispatch the event to the browser element again if we were
        // waiting for the key reply but the event wasn't sent to the content
        // process due to the remote browser wasn't ready.
        WidgetKeyboardEvent keyEvent(*aKeyboardEvent);
        aKeyboardEvent->MarkAsHandledInRemoteProcess();
        EventDispatcher::Dispatch(remote->GetOwnerElement(), mPresContext,
                                  &keyEvent);
        if (keyEvent.DefaultPrevented()) {
          aKeyboardEvent->PreventDefault(!keyEvent.DefaultPreventedByContent());
          aStatus = nsEventStatus_eConsumeNoDefault;
          return;
        }
      }
    }
    // The widget expects a reply for every keyboard event. If the event wasn't
    // dispatched to a content process (non-e10s or no content process
    // running), we need to short-circuit here. Otherwise, we need to wait for
    // the content process to handle the event.
    if (aKeyboardEvent->mWidget) {
      aKeyboardEvent->mWidget->PostHandleKeyEvent(aKeyboardEvent);
    }
    if (aKeyboardEvent->DefaultPrevented()) {
      aStatus = nsEventStatus_eConsumeNoDefault;
      return;
    }
  }

  // XXX Currently, our automated tests don't support mKeyNameIndex.
  //     Therefore, we still need to handle this with keyCode.
  switch (aKeyboardEvent->mKeyCode) {
    case NS_VK_TAB:
    case NS_VK_F6:
      // This is to prevent keyboard scrolling while alt modifier in use.
      if (!aKeyboardEvent->IsAlt()) {
        aStatus = nsEventStatus_eConsumeNoDefault;

        // Handling the tab event after it was sent to content is bad,
        // because to the FocusManager the remote-browser looks like one
        // element, so we would just move the focus to the next element
        // in chrome, instead of handling it in content.
        if (aKeyboardEvent->HasBeenPostedToRemoteProcess()) {
          break;
        }

        EnsureDocument(mPresContext);
        nsIFocusManager* fm = nsFocusManager::GetFocusManager();
        if (fm && mDocument) {
          // Shift focus forward or back depending on shift key
          bool isDocMove = aKeyboardEvent->IsControl() ||
                           aKeyboardEvent->mKeyCode == NS_VK_F6;
          uint32_t dir =
              aKeyboardEvent->IsShift()
                  ? (isDocMove ? static_cast<uint32_t>(
                                     nsIFocusManager::MOVEFOCUS_BACKWARDDOC)
                               : static_cast<uint32_t>(
                                     nsIFocusManager::MOVEFOCUS_BACKWARD))
                  : (isDocMove ? static_cast<uint32_t>(
                                     nsIFocusManager::MOVEFOCUS_FORWARDDOC)
                               : static_cast<uint32_t>(
                                     nsIFocusManager::MOVEFOCUS_FORWARD));
          RefPtr<Element> result;
          fm->MoveFocus(mDocument->GetWindow(), nullptr, dir,
                        nsIFocusManager::FLAG_BYKEY, getter_AddRefs(result));
        }
      }
      return;
    case 0:
      // We handle keys with no specific keycode value below.
      break;
    default:
      return;
  }

  switch (aKeyboardEvent->mKeyNameIndex) {
    case KEY_NAME_INDEX_ZoomIn:
    case KEY_NAME_INDEX_ZoomOut:
      ChangeZoom(aKeyboardEvent->mKeyNameIndex == KEY_NAME_INDEX_ZoomIn ? 1
                                                                        : -1);
      aStatus = nsEventStatus_eConsumeNoDefault;
      break;
    default:
      break;
  }
}

nsresult EventStateManager::PostHandleEvent(nsPresContext* aPresContext,
                                            WidgetEvent* aEvent,
                                            nsIFrame* aTargetFrame,
                                            nsEventStatus* aStatus,
                                            nsIContent* aOverrideClickTarget) {
  NS_ENSURE_ARG(aPresContext);
  NS_ENSURE_ARG_POINTER(aStatus);

  mCurrentTarget = aTargetFrame;
  mCurrentTargetContent = nullptr;

  HandleCrossProcessEvent(aEvent, aStatus);
  // NOTE: the above call may have destroyed aTargetFrame, please use
  // mCurrentTarget henceforth.  This is to avoid using it accidentally:
  aTargetFrame = nullptr;

  // Most of the events we handle below require a frame.
  // Add special cases here.
  if (!mCurrentTarget && aEvent->mMessage != eMouseUp &&
      aEvent->mMessage != eMouseDown && aEvent->mMessage != eDragEnter &&
      aEvent->mMessage != eDragOver) {
    return NS_OK;
  }

  // Keep the prescontext alive, we might need it after event dispatch
  RefPtr<nsPresContext> presContext = aPresContext;
  nsresult ret = NS_OK;

  switch (aEvent->mMessage) {
    case eMouseDown: {
      WidgetMouseEvent* mouseEvent = aEvent->AsMouseEvent();
      if (mouseEvent->mButton == MouseButton::eLeft &&
          !sNormalLMouseEventInProcess) {
        // We got a mouseup event while a mousedown event was being processed.
        // Make sure that the capturing content is cleared.
        PresShell::ReleaseCapturingContent();
        break;
      }

      // For remote content, capture the event in the parent process at the
      // <xul:browser remote> element. This will ensure that subsequent
      // mousemove/mouseup events will continue to be dispatched to this element
      // and therefore forwarded to the child.
      if (aEvent->HasBeenPostedToRemoteProcess() &&
          !PresShell::GetCapturingContent()) {
        if (nsIContent* content =
                mCurrentTarget ? mCurrentTarget->GetContent() : nullptr) {
          PresShell::SetCapturingContent(content, CaptureFlags::None);
        } else {
          PresShell::ReleaseCapturingContent();
        }
      }

      nsCOMPtr<nsIContent> activeContent;
      // When content calls PreventDefault on pointerdown, we also call
      // PreventDefault on the subsequent mouse events to suppress default
      // behaviors. Normally, aStatus should be nsEventStatus_eConsumeNoDefault
      // when the event is DefaultPrevented but it's reset to
      // nsEventStatus_eIgnore in EventStateManager::PreHandleEvent. So we also
      // check if the event is DefaultPrevented.
      if (nsEventStatus_eConsumeNoDefault != *aStatus &&
          !aEvent->DefaultPrevented()) {
        nsCOMPtr<nsIContent> newFocus;
        bool suppressBlur = false;
        if (mCurrentTarget) {
          mCurrentTarget->GetContentForEvent(aEvent, getter_AddRefs(newFocus));
          const nsStyleUI* ui = mCurrentTarget->StyleUI();
          activeContent = mCurrentTarget->GetContent();

          // In some cases, we do not want to even blur the current focused
          // element. Those cases are:
          // 1. -moz-user-focus CSS property is set to 'ignore';
          // 2. XUL control element has the disabled property set to 'true'.
          //
          // We can't use nsIFrame::IsFocusable() because we want to blur when
          // we click on a visibility: none element.
          // We can't use nsIContent::IsFocusable() because we want to blur when
          // we click on a non-focusable element like a <div>.
          // We have to use |aEvent->mTarget| to not make sure we do not check
          // an anonymous node of the targeted element.
          suppressBlur = (ui->mUserFocus == StyleUserFocus::Ignore);

          nsCOMPtr<Element> element = do_QueryInterface(aEvent->mTarget);
          if (!suppressBlur && element) {
            nsCOMPtr<nsIDOMXULControlElement> xulControl =
                element->AsXULControl();
            if (xulControl) {
              bool disabled = false;
              xulControl->GetDisabled(&disabled);
              suppressBlur = disabled;
            }
          }
        }

        if (!suppressBlur) {
          suppressBlur = nsContentUtils::IsUserFocusIgnored(activeContent);
        }

        // When a root content which isn't editable but has an editable HTML
        // <body> element is clicked, we should redirect the focus to the
        // the <body> element.  E.g., when an user click bottom of the editor
        // where is outside of the <body> element, the <body> should be focused
        // and the user can edit immediately after that.
        //
        // NOTE: The newFocus isn't editable that also means it's not in
        // designMode.  In designMode, all contents are not focusable.
        if (newFocus && !newFocus->IsEditable()) {
          Document* doc = newFocus->GetComposedDoc();
          if (doc && newFocus == doc->GetRootElement()) {
            nsIContent* bodyContent =
                nsLayoutUtils::GetEditableRootContentByContentEditable(doc);
            if (bodyContent && bodyContent->GetPrimaryFrame()) {
              newFocus = bodyContent;
            }
          }
        }

        // When the mouse is pressed, the default action is to focus the
        // target. Look for the nearest enclosing focusable frame.
        //
        // TODO: Probably this should be moved to Element::PostHandleEvent.
        for (; newFocus; newFocus = newFocus->GetFlattenedTreeParent()) {
          if (!newFocus->IsElement()) {
            continue;
          }

          nsIFrame* frame = newFocus->GetPrimaryFrame();
          if (!frame) {
            continue;
          }

          // If the mousedown happened inside a popup, don't try to set focus on
          // one of its containing elements
          if (frame->StyleDisplay()->mDisplay == StyleDisplay::MozPopup) {
            newFocus = nullptr;
            break;
          }

          int32_t tabIndexUnused;
          if (frame->IsFocusable(&tabIndexUnused, true)) {
            break;
          }
        }

        MOZ_ASSERT_IF(newFocus, newFocus->IsElement());

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
          if (newFocus) {
            // use the mouse flag and the noscroll flag so that the content
            // doesn't unexpectedly scroll when clicking an element that is
            // only half visible
            uint32_t flags =
                nsIFocusManager::FLAG_BYMOUSE | nsIFocusManager::FLAG_NOSCROLL;
            // If this was a touch-generated event, pass that information:
            if (mouseEvent->mInputSource ==
                MouseEvent_Binding::MOZ_SOURCE_TOUCH) {
              flags |= nsIFocusManager::FLAG_BYTOUCH;
            }
            fm->SetFocus(newFocus->AsElement(), flags);
          } else if (!suppressBlur) {
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
        if (mouseEvent->mButton != MouseButton::eLeft) {
          break;
        }

        // The nearest enclosing element goes into the :active state.  If we're
        // not an element (so we're text or something) we need to obtain
        // our parent element and put it into :active instead.
        if (activeContent && !activeContent->IsElement()) {
          if (nsIContent* par = activeContent->GetFlattenedTreeParent()) {
            activeContent = par;
          }
        }
      } else {
        // if we're here, the event handler returned false, so stop
        // any of our own processing of a drag. Workaround for bug 43258.
        StopTrackingDragGesture(true);

        // When the event was cancelled, there is currently a chrome document
        // focused and a mousedown just occurred on a content document, ensure
        // that the window that was clicked is focused.
        EnsureDocument(mPresContext);
        nsIFocusManager* fm = nsFocusManager::GetFocusManager();
        if (mDocument && fm) {
          nsCOMPtr<mozIDOMWindowProxy> window;
          fm->GetFocusedWindow(getter_AddRefs(window));
          auto* currentWindow = nsPIDOMWindowOuter::From(window);
          if (currentWindow && mDocument->GetWindow() &&
              currentWindow != mDocument->GetWindow() &&
              !nsContentUtils::IsChromeDoc(mDocument)) {
            nsCOMPtr<nsPIDOMWindowOuter> currentTop;
            nsCOMPtr<nsPIDOMWindowOuter> newTop;
            currentTop = currentWindow->GetInProcessTop();
            newTop = mDocument->GetWindow()->GetInProcessTop();
            nsCOMPtr<Document> currentDoc = currentWindow->GetExtantDoc();
            if (nsContentUtils::IsChromeDoc(currentDoc) ||
                (currentTop && newTop && currentTop != newTop)) {
              fm->SetFocusedWindow(mDocument->GetWindow());
            }
          }
        }
      }
      SetActiveManager(this, activeContent);
    } break;
    case ePointerCancel:
    case ePointerUp: {
      WidgetPointerEvent* pointerEvent = aEvent->AsPointerEvent();
      MOZ_ASSERT(pointerEvent);
      // Implicitly releasing capture for given pointer. ePointerLostCapture
      // should be send after ePointerUp or ePointerCancel.
      PointerEventHandler::ImplicitlyReleasePointerCapture(pointerEvent);

      if (pointerEvent->mMessage == ePointerCancel ||
          pointerEvent->mInputSource == MouseEvent_Binding::MOZ_SOURCE_TOUCH) {
        // After pointercancel, pointer becomes invalid so we can remove
        // relevant helper from table. Regarding pointerup with non-hoverable
        // device, the pointer also becomes invalid. Hoverable (mouse/pen)
        // pointers are valid all the time (not only between down/up).
        GenerateMouseEnterExit(pointerEvent);
        mPointersEnterLeaveHelper.Remove(pointerEvent->pointerId);
      }
      break;
    }
    case eMouseUp: {
      ClearGlobalActiveContent(this);
      WidgetMouseEvent* mouseUpEvent = aEvent->AsMouseEvent();
      if (mouseUpEvent && EventCausesClickEvents(*mouseUpEvent)) {
        // Make sure to dispatch the click even if there is no frame for
        // the current target element. This is required for Web compatibility.
        RefPtr<EventStateManager> esm =
            ESMFromContentOrThis(aOverrideClickTarget);
        ret =
            esm->PostHandleMouseUp(mouseUpEvent, aStatus, aOverrideClickTarget);
      }

      if (PresShell* presShell = presContext->GetPresShell()) {
        RefPtr<nsFrameSelection> frameSelection = presShell->FrameSelection();
        frameSelection->SetDragState(false);
      }
    } break;
    case eWheelOperationEnd: {
      MOZ_ASSERT(aEvent->IsTrusted());
      ScrollbarsForWheel::MayInactivate();
      WidgetWheelEvent* wheelEvent = aEvent->AsWheelEvent();
      nsIScrollableFrame* scrollTarget =
          do_QueryFrame(ComputeScrollTargetAndMayAdjustWheelEvent(
              mCurrentTarget, wheelEvent,
              COMPUTE_DEFAULT_ACTION_TARGET_WITH_AUTO_DIR));
      if (scrollTarget) {
        scrollTarget->ScrollSnap();
      }
    } break;
    case eWheel:
    case eWheelOperationStart: {
      MOZ_ASSERT(aEvent->IsTrusted());

      if (*aStatus == nsEventStatus_eConsumeNoDefault) {
        ScrollbarsForWheel::Inactivate();
        break;
      }

      WidgetWheelEvent* wheelEvent = aEvent->AsWheelEvent();
      MOZ_ASSERT(wheelEvent);

      // When APZ is enabled, the actual scroll animation might be handled by
      // the compositor.
      WheelPrefs::Action action =
          wheelEvent->mFlags.mHandledByAPZ
              ? WheelPrefs::ACTION_NONE
              : WheelPrefs::GetInstance()->ComputeActionFor(wheelEvent);

      WheelDeltaAdjustmentStrategy strategy =
          GetWheelDeltaAdjustmentStrategy(*wheelEvent);
      // Adjust the delta values of the wheel event if the current default
      // action is to horizontalize scrolling. I.e., deltaY values are set to
      // deltaX and deltaY and deltaZ values are set to 0.
      // If horizontalized, the delta values will be restored and its overflow
      // deltaX will become 0 when the WheelDeltaHorizontalizer instance is
      // being destroyed.
      WheelDeltaHorizontalizer horizontalizer(*wheelEvent);
      if (WheelDeltaAdjustmentStrategy::eHorizontalize == strategy) {
        horizontalizer.Horizontalize();
      }

      // Since ComputeScrollTargetAndMayAdjustWheelEvent() may adjust the delta
      // if the event is auto-dir. So we use |ESMAutoDirWheelDeltaRestorer|
      // here.
      // An instance of |ESMAutoDirWheelDeltaRestorer| is used to monitor
      // auto-dir adjustment which may happen during its lifetime. If the delta
      // values is adjusted during its lifetime, the instance will restore the
      // adjusted delta when it's being destrcuted.
      ESMAutoDirWheelDeltaRestorer restorer(*wheelEvent);
      // Check if the frame to scroll before checking the default action
      // because if the scroll target is a plugin, the default action should be
      // chosen by the plugin rather than by our prefs.
      nsIFrame* frameToScroll = ComputeScrollTargetAndMayAdjustWheelEvent(
          mCurrentTarget, wheelEvent,
          COMPUTE_DEFAULT_ACTION_TARGET_WITH_AUTO_DIR);
      nsPluginFrame* pluginFrame = do_QueryFrame(frameToScroll);
      if (pluginFrame) {
        MOZ_ASSERT(pluginFrame->WantsToHandleWheelEventAsDefaultAction());
        // Plugins should receive original values instead of adjusted values.
        horizontalizer.CancelHorizontalization();
        action = WheelPrefs::ACTION_SEND_TO_PLUGIN;
      }

      switch (action) {
        case WheelPrefs::ACTION_SCROLL:
        case WheelPrefs::ACTION_HORIZONTALIZED_SCROLL: {
          // For scrolling of default action, we should honor the mouse wheel
          // transaction.

          ScrollbarsForWheel::PrepareToScrollText(this, mCurrentTarget,
                                                  wheelEvent);

          if (aEvent->mMessage != eWheel ||
              (!wheelEvent->mDeltaX && !wheelEvent->mDeltaY)) {
            break;
          }

          nsIScrollableFrame* scrollTarget = do_QueryFrame(frameToScroll);
          ScrollbarsForWheel::SetActiveScrollTarget(scrollTarget);

          nsIFrame* rootScrollFrame =
              !mCurrentTarget
                  ? nullptr
                  : mCurrentTarget->PresShell()->GetRootScrollFrame();
          nsIScrollableFrame* rootScrollableFrame = nullptr;
          if (rootScrollFrame) {
            rootScrollableFrame = do_QueryFrame(rootScrollFrame);
          }
          if (!scrollTarget || scrollTarget == rootScrollableFrame) {
            wheelEvent->mViewPortIsOverscrolled = true;
          }
          wheelEvent->mOverflowDeltaX = wheelEvent->mDeltaX;
          wheelEvent->mOverflowDeltaY = wheelEvent->mDeltaY;
          WheelPrefs::GetInstance()->CancelApplyingUserPrefsFromOverflowDelta(
              wheelEvent);
          if (scrollTarget) {
            DoScrollText(scrollTarget, wheelEvent);
          } else {
            WheelTransaction::EndTransaction();
            ScrollbarsForWheel::Inactivate();
          }
          break;
        }
        case WheelPrefs::ACTION_HISTORY: {
          // If this event doesn't cause eLegacyMouseLineOrPageScroll event or
          // the direction is oblique, don't perform history back/forward.
          int32_t intDelta = wheelEvent->GetPreferredIntDelta();
          if (!intDelta) {
            break;
          }
          DoScrollHistory(intDelta);
          break;
        }
        case WheelPrefs::ACTION_ZOOM: {
          // If this event doesn't cause eLegacyMouseLineOrPageScroll event or
          // the direction is oblique, don't perform zoom in/out.
          int32_t intDelta = wheelEvent->GetPreferredIntDelta();
          if (!intDelta) {
            break;
          }
          DoScrollZoom(mCurrentTarget, intDelta);
          break;
        }
        case WheelPrefs::ACTION_SEND_TO_PLUGIN:
          MOZ_ASSERT(pluginFrame);

          if (wheelEvent->mMessage != eWheel ||
              (!wheelEvent->mDeltaX && !wheelEvent->mDeltaY)) {
            break;
          }

          MOZ_ASSERT(static_cast<void*>(frameToScroll) ==
                     static_cast<void*>(pluginFrame));
          if (!WheelTransaction::WillHandleDefaultAction(wheelEvent,
                                                         frameToScroll)) {
            break;
          }

          pluginFrame->HandleWheelEventAsDefaultAction(wheelEvent);
          break;
        case WheelPrefs::ACTION_NONE:
        default:
          bool allDeltaOverflown = false;
          if (wheelEvent->mFlags.mHandledByAPZ) {
            if (wheelEvent->mCanTriggerSwipe) {
              // For events that can trigger swipes, APZ needs to know whether
              // scrolling is possible in the requested direction. It does this
              // by looking at the scroll overflow values on mCanTriggerSwipe
              // events after they have been processed.
              allDeltaOverflown = !ComputeScrollTarget(
                  mCurrentTarget, wheelEvent, COMPUTE_DEFAULT_ACTION_TARGET);
            }
          } else {
            // The event was processed neither by APZ nor by us, so all of the
            // delta values must be overflown delta values.
            allDeltaOverflown = true;
          }

          if (!allDeltaOverflown) {
            break;
          }
          wheelEvent->mOverflowDeltaX = wheelEvent->mDeltaX;
          wheelEvent->mOverflowDeltaY = wheelEvent->mDeltaY;
          WheelPrefs::GetInstance()->CancelApplyingUserPrefsFromOverflowDelta(
              wheelEvent);
          wheelEvent->mViewPortIsOverscrolled = true;
          break;
      }
      *aStatus = nsEventStatus_eConsumeNoDefault;
    } break;

    case eGestureNotify: {
      if (nsEventStatus_eConsumeNoDefault != *aStatus) {
        DecideGestureEvent(aEvent->AsGestureNotifyEvent(), mCurrentTarget);
      }
    } break;

    case eDragEnter:
    case eDragOver: {
      NS_ASSERTION(aEvent->mClass == eDragEventClass, "Expected a drag event");

      // Check if the drag is occurring inside a scrollable area. If so, scroll
      // the area when the mouse is near the edges.
      if (mCurrentTarget && aEvent->mMessage == eDragOver) {
        nsIFrame* checkFrame = mCurrentTarget;
        while (checkFrame) {
          nsIScrollableFrame* scrollFrame = do_QueryFrame(checkFrame);
          // Break out so only the innermost scrollframe is scrolled.
          if (scrollFrame && scrollFrame->DragScroll(aEvent)) {
            break;
          }
          checkFrame = checkFrame->GetParent();
        }
      }

      nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
      if (!dragSession) break;

      // Reset the flag.
      dragSession->SetOnlyChromeDrop(false);
      if (mPresContext) {
        EnsureDocument(mPresContext);
      }
      bool isChromeDoc = nsContentUtils::IsChromeDoc(mDocument);

      // the initial dataTransfer is the one from the dragstart event that
      // was set on the dragSession when the drag began.
      RefPtr<DataTransfer> dataTransfer;
      RefPtr<DataTransfer> initialDataTransfer = dragSession->GetDataTransfer();

      WidgetDragEvent* dragEvent = aEvent->AsDragEvent();

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
        // If the event has initialized its mDataTransfer, use it.
        // Or the event has not been initialized its mDataTransfer, but
        // it's set before dispatch because of synthesized, but without
        // testing session (e.g., emulating drag from another app), use it
        // coming from outside.
        // XXX Perhaps, for the latter case, we need new API because we don't
        //     have a chance to initialize allowed effects of the session.
        if (dragEvent->mDataTransfer) {
          // get the dataTransfer and the dropEffect that was set on it
          dataTransfer = dragEvent->mDataTransfer;
          dropEffect = dataTransfer->DropEffectInt();
        } else {
          // if dragEvent->mDataTransfer is null, it means that no attempt was
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
          dropEffect = nsContentUtils::FilterDropEffect(
              action, nsIDragService::DRAGDROP_ACTION_UNINITIALIZED);
        }

        // At this point, if the dataTransfer is null, it means that the
        // drag was originally started by directly calling the drag service.
        // Just assume that all effects are allowed.
        uint32_t effectAllowed = nsIDragService::DRAGDROP_ACTION_UNINITIALIZED;
        if (dataTransfer) {
          effectAllowed = dataTransfer->EffectAllowedInt();
        }

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
        // XXXsmaug dragenter needs some more work.
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
      if (aEvent->HasBeenPostedToRemoteProcess()) {
        dragSession->SetCanDrop(true);
      } else if (initialDataTransfer) {
        // Now set the drop effect in the initial dataTransfer. This ensures
        // that we can get the desired drop effect in the drop event. For events
        // dispatched to content, the content process will take care of setting
        // this.
        initialDataTransfer->SetDropEffectInt(dropEffect);
      }
    } break;

    case eDrop: {
      if (aEvent->mFlags.mIsSynthesizedForTests) {
        if (nsCOMPtr<nsIDragSession> dragSession =
                nsContentUtils::GetDragSession()) {
          MOZ_ASSERT(dragSession->IsSynthesizedForTests());
          RefPtr<Document> sourceDocument;
          DebugOnly<nsresult> rvIgnored =
              dragSession->GetSourceDocument(getter_AddRefs(sourceDocument));
          NS_WARNING_ASSERTION(
              NS_SUCCEEDED(rvIgnored),
              "nsIDragSession::GetSourceDocument() failed, but ignored");
          // If source document hasn't been initialized, i.e., dragstart was
          // consumed by the test, the test needs to dispatch "dragend" event
          // instead of the drag session.  Therefore, it does not make sense
          // to set drag end point in such case (you hit assersion if you do
          // it).
          if (sourceDocument) {
            CSSIntPoint dropPointInScreen =
                Event::GetScreenCoords(aPresContext, aEvent, aEvent->mRefPoint);
            dragSession->SetDragEndPointForTests(dropPointInScreen.x,
                                                 dropPointInScreen.y);
          }
        }
      }
      sLastDragOverFrame = nullptr;
      ClearGlobalActiveContent(this);
      break;
    }
    case eDragExit:
      // make sure to fire the enter and exit_synth events after the
      // eDragExit event, otherwise we'll clean up too early
      GenerateDragDropEnterExit(presContext, aEvent->AsDragEvent());
      if (ContentChild* child = ContentChild::GetSingleton()) {
        // SendUpdateDropEffect to prevent nsIDragService from waiting for
        // response of forwarded dragexit event.
        child->SendUpdateDropEffect(nsIDragService::DRAGDROP_ACTION_NONE,
                                    nsIDragService::DRAGDROP_ACTION_NONE);
      }
      break;

    case eKeyUp:
      break;

    case eKeyPress: {
      WidgetKeyboardEvent* keyEvent = aEvent->AsKeyboardEvent();
      PostHandleKeyboardEvent(keyEvent, mCurrentTarget, *aStatus);
    } break;

    case eMouseEnterIntoWidget:
      if (mCurrentTarget) {
        nsCOMPtr<nsIContent> targetContent;
        mCurrentTarget->GetContentForEvent(aEvent,
                                           getter_AddRefs(targetContent));
        SetContentState(targetContent, NS_EVENT_STATE_HOVER);
      }
      break;

#ifdef XP_MACOSX
    case eMouseActivate:
      if (mCurrentTarget) {
        nsCOMPtr<nsIContent> targetContent;
        mCurrentTarget->GetContentForEvent(aEvent,
                                           getter_AddRefs(targetContent));
        if (!NodeAllowsClickThrough(targetContent)) {
          *aStatus = nsEventStatus_eConsumeNoDefault;
        }
      }
      break;
#endif

    default:
      break;
  }

  // Reset target frame to null to avoid mistargeting after reentrant event
  mCurrentTarget = nullptr;
  mCurrentTargetContent = nullptr;

  return ret;
}

BrowserParent* EventStateManager::GetCrossProcessTarget() {
  return IMEStateManager::GetActiveBrowserParent();
}

bool EventStateManager::IsTargetCrossProcess(WidgetGUIEvent* aEvent) {
  // Check to see if there is a focused, editable content in chrome,
  // in that case, do not forward IME events to content
  nsIContent* focusedContent = GetFocusedContent();
  if (focusedContent && focusedContent->IsEditable()) return false;
  return IMEStateManager::GetActiveBrowserParent() != nullptr;
}

void EventStateManager::NotifyDestroyPresContext(nsPresContext* aPresContext) {
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

void EventStateManager::SetPresContext(nsPresContext* aPresContext) {
  mPresContext = aPresContext;
}

void EventStateManager::ClearFrameRefs(nsIFrame* aFrame) {
  if (aFrame && aFrame == mCurrentTarget) {
    mCurrentTargetContent = aFrame->GetContent();
  }
}

struct CursorImage {
  gfx::IntPoint mHotspot;
  nsCOMPtr<imgIContainer> mContainer;
  bool mEarlierCursorLoading = false;
};

// Given the event that we're processing, and the computed cursor and hotspot,
// determine whether the custom CSS cursor should be blocked (that is, not
// honored).
//
// We will not honor it all of the following are true:
//
//  * layout.cursor.block.enabled is true.
//  * the size of the custom cursor is bigger than layout.cursor.block.max-size.
//  * the bounds of the cursor would end up outside of the viewport of the
//    top-level content document.
//
// This is done in order to prevent hijacking the cursor, see bug 1445844 and
// co.
static bool ShouldBlockCustomCursor(nsPresContext* aPresContext,
                                    WidgetEvent* aEvent,
                                    const CursorImage& aCursor) {
  if (!StaticPrefs::layout_cursor_block_enabled()) {
    return false;
  }

  int32_t width = 0;
  int32_t height = 0;
  aCursor.mContainer->GetWidth(&width);
  aCursor.mContainer->GetHeight(&height);

  int32_t maxSize = StaticPrefs::layout_cursor_block_max_size();

  if (width <= maxSize && height <= maxSize) {
    return false;
  }

  // We don't want to deal with iframes, just let them do their thing unless
  // they intersect UI.
  //
  // TODO(emilio, bug 1525561): In a fission world, we should have a better way
  // to find the event coordinates relative to the content area.
  nsPresContext* topLevel =
      aPresContext->GetToplevelContentDocumentPresContext();
  if (!topLevel) {
    return false;
  }

  nsPoint point = nsLayoutUtils::GetEventCoordinatesRelativeTo(
      aEvent, topLevel->PresShell()->GetRootFrame());

  nsSize size(CSSPixel::ToAppUnits(width), CSSPixel::ToAppUnits(height));
  nsPoint hotspot(CSSPixel::ToAppUnits(aCursor.mHotspot.x),
                  CSSPixel::ToAppUnits(aCursor.mHotspot.y));

  nsRect cursorRect(point - hotspot, size);
  return !topLevel->GetVisibleArea().Contains(cursorRect);
}

static gfx::IntPoint ComputeHotspot(imgIContainer* aContainer,
                                    const Maybe<gfx::Point>& aHotspot) {
  MOZ_ASSERT(aContainer);

  // css3-ui says to use the CSS-specified hotspot if present,
  // otherwise use the intrinsic hotspot, otherwise use the top left
  // corner.
  if (aHotspot) {
    int32_t imgWidth, imgHeight;
    aContainer->GetWidth(&imgWidth);
    aContainer->GetHeight(&imgHeight);
    auto hotspot = gfx::IntPoint::Round(*aHotspot);
    return {std::max(std::min(hotspot.x, imgWidth - 1), 0),
            std::max(std::min(hotspot.y, imgHeight - 1), 0)};
  }

  gfx::IntPoint hotspot;
  aContainer->GetHotspotX(&hotspot.x);
  aContainer->GetHotspotY(&hotspot.y);
  return hotspot;
}

static CursorImage ComputeCustomCursor(nsPresContext* aPresContext,
                                       WidgetEvent* aEvent,
                                       const nsIFrame& aFrame,
                                       const nsIFrame::Cursor& aCursor) {
  if (aCursor.mAllowCustomCursor == nsIFrame::AllowCustomCursorImage::No) {
    return {};
  }
  const ComputedStyle& style =
      aCursor.mStyle ? *aCursor.mStyle : *aFrame.Style();

  // If we are falling back because any cursor before us is loading, let the
  // consumer know.
  bool loading = false;
  for (const auto& image : style.StyleUI()->mCursor.images.AsSpan()) {
    uint32_t status;
    imgRequestProxy* req = image.url.GetImage();
    if (!req || NS_FAILED(req->GetImageStatus(&status))) {
      continue;
    }
    if (!(status & imgIRequest::STATUS_LOAD_COMPLETE)) {
      loading = true;
      continue;
    }
    if (status & imgIRequest::STATUS_ERROR) {
      continue;
    }
    nsCOMPtr<imgIContainer> container;
    req->GetImage(getter_AddRefs(container));
    if (!container) {
      continue;
    }
    Maybe<gfx::Point> specifiedHotspot =
        image.has_hotspot ? Some(gfx::Point{image.hotspot_x, image.hotspot_y})
                          : Nothing();
    gfx::IntPoint hotspot = ComputeHotspot(container, specifiedHotspot);
    CursorImage result{hotspot, std::move(container), loading};
    if (ShouldBlockCustomCursor(aPresContext, aEvent, result)) {
      continue;
    }
    // This is the one we want!
    return result;
  }
  return {{}, nullptr, loading};
}

void EventStateManager::UpdateCursor(nsPresContext* aPresContext,
                                     WidgetEvent* aEvent,
                                     nsIFrame* aTargetFrame,
                                     nsEventStatus* aStatus) {
  if (aTargetFrame && IsRemoteTarget(aTargetFrame->GetContent())) {
    return;
  }

  auto cursor = StyleCursorKind::Default;
  nsCOMPtr<imgIContainer> container;
  Maybe<gfx::IntPoint> hotspot;

  // If cursor is locked just use the locked one
  if (mLockCursor != kInvalidCursorKind) {
    cursor = mLockCursor;
  }
  // If not locked, look for correct cursor
  else if (aTargetFrame) {
    nsPoint pt =
        nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, aTargetFrame);
    Maybe<nsIFrame::Cursor> framecursor = aTargetFrame->GetCursor(pt);
    // Avoid setting cursor when the mouse is over a windowless plugin.
    if (!framecursor) {
      if (XRE_IsContentProcess()) {
        mLastFrameConsumedSetCursor = true;
      }
      return;
    }
    // Make sure cursors get reset after the mouse leaves a
    // windowless plugin frame.
    if (mLastFrameConsumedSetCursor) {
      ClearCachedWidgetCursor(aTargetFrame);
      mLastFrameConsumedSetCursor = false;
    }

    CursorImage customCursor =
        ComputeCustomCursor(aPresContext, aEvent, *aTargetFrame, *framecursor);

    // If the current cursor is from the same frame, and it is now
    // loading some new image for the cursor, we should wait for a
    // while rather than taking its fallback cursor directly.
    if (customCursor.mEarlierCursorLoading &&
        gLastCursorSourceFrame == aTargetFrame &&
        TimeStamp::NowLoRes() - gLastCursorUpdateTime <
            TimeDuration::FromMilliseconds(kCursorLoadingTimeout)) {
      return;
    }
    cursor = framecursor->mCursor;
    container = std::move(customCursor.mContainer);
    hotspot = Some(customCursor.mHotspot);
  }

  if (StaticPrefs::ui_use_activity_cursor()) {
    // Check whether or not to show the busy cursor
    nsCOMPtr<nsIDocShell> docShell(aPresContext->GetDocShell());
    if (!docShell) return;
    auto busyFlags = docShell->GetBusyFlags();

    // Show busy cursor everywhere before page loads
    // and just replace the arrow cursor after page starts loading
    if (busyFlags & nsIDocShell::BUSY_FLAGS_BUSY &&
        (cursor == StyleCursorKind::Auto ||
         cursor == StyleCursorKind::Default)) {
      cursor = StyleCursorKind::Progress;
      container = nullptr;
    }
  }

  if (aTargetFrame) {
    SetCursor(cursor, container, hotspot, aTargetFrame->GetNearestWidget(),
              false);
    gLastCursorSourceFrame = aTargetFrame;
    gLastCursorUpdateTime = TimeStamp::NowLoRes();
  }

  if (mLockCursor != kInvalidCursorKind || StyleCursorKind::Auto != cursor) {
    *aStatus = nsEventStatus_eConsumeDoDefault;
  }
}

void EventStateManager::ClearCachedWidgetCursor(nsIFrame* aTargetFrame) {
  if (!aTargetFrame) {
    return;
  }
  nsIWidget* aWidget = aTargetFrame->GetNearestWidget();
  if (!aWidget) {
    return;
  }
  aWidget->ClearCachedCursor();
}

nsresult EventStateManager::SetCursor(StyleCursorKind aCursor,
                                      imgIContainer* aContainer,
                                      const Maybe<gfx::IntPoint>& aHotspot,
                                      nsIWidget* aWidget, bool aLockCursor) {
  EnsureDocument(mPresContext);
  NS_ENSURE_TRUE(mDocument, NS_ERROR_FAILURE);
  sMouseOverDocument = mDocument.get();

  NS_ENSURE_TRUE(aWidget, NS_ERROR_FAILURE);
  if (aLockCursor) {
    if (StyleCursorKind::Auto != aCursor) {
      mLockCursor = aCursor;
    } else {
      // If cursor style is set to auto we unlock the cursor again.
      mLockCursor = kInvalidCursorKind;
    }
  }
  nsCursor c;
  switch (aCursor) {
    case StyleCursorKind::Auto:
    case StyleCursorKind::Default:
      c = eCursor_standard;
      break;
    case StyleCursorKind::Pointer:
      c = eCursor_hyperlink;
      break;
    case StyleCursorKind::Crosshair:
      c = eCursor_crosshair;
      break;
    case StyleCursorKind::Move:
      c = eCursor_move;
      break;
    case StyleCursorKind::Text:
      c = eCursor_select;
      break;
    case StyleCursorKind::Wait:
      c = eCursor_wait;
      break;
    case StyleCursorKind::Help:
      c = eCursor_help;
      break;
    case StyleCursorKind::NResize:
      c = eCursor_n_resize;
      break;
    case StyleCursorKind::SResize:
      c = eCursor_s_resize;
      break;
    case StyleCursorKind::WResize:
      c = eCursor_w_resize;
      break;
    case StyleCursorKind::EResize:
      c = eCursor_e_resize;
      break;
    case StyleCursorKind::NwResize:
      c = eCursor_nw_resize;
      break;
    case StyleCursorKind::SeResize:
      c = eCursor_se_resize;
      break;
    case StyleCursorKind::NeResize:
      c = eCursor_ne_resize;
      break;
    case StyleCursorKind::SwResize:
      c = eCursor_sw_resize;
      break;
    case StyleCursorKind::Copy:  // CSS3
      c = eCursor_copy;
      break;
    case StyleCursorKind::Alias:
      c = eCursor_alias;
      break;
    case StyleCursorKind::ContextMenu:
      c = eCursor_context_menu;
      break;
    case StyleCursorKind::Cell:
      c = eCursor_cell;
      break;
    case StyleCursorKind::Grab:
      c = eCursor_grab;
      break;
    case StyleCursorKind::Grabbing:
      c = eCursor_grabbing;
      break;
    case StyleCursorKind::Progress:
      c = eCursor_spinning;
      break;
    case StyleCursorKind::ZoomIn:
      c = eCursor_zoom_in;
      break;
    case StyleCursorKind::ZoomOut:
      c = eCursor_zoom_out;
      break;
    case StyleCursorKind::NotAllowed:
      c = eCursor_not_allowed;
      break;
    case StyleCursorKind::ColResize:
      c = eCursor_col_resize;
      break;
    case StyleCursorKind::RowResize:
      c = eCursor_row_resize;
      break;
    case StyleCursorKind::NoDrop:
      c = eCursor_no_drop;
      break;
    case StyleCursorKind::VerticalText:
      c = eCursor_vertical_text;
      break;
    case StyleCursorKind::AllScroll:
      c = eCursor_all_scroll;
      break;
    case StyleCursorKind::NeswResize:
      c = eCursor_nesw_resize;
      break;
    case StyleCursorKind::NwseResize:
      c = eCursor_nwse_resize;
      break;
    case StyleCursorKind::NsResize:
      c = eCursor_ns_resize;
      break;
    case StyleCursorKind::EwResize:
      c = eCursor_ew_resize;
      break;
    case StyleCursorKind::None:
      c = eCursor_none;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown cursor kind");
      c = eCursor_standard;
      break;
  }

  int32_t x = aHotspot ? aHotspot->x : 0;
  int32_t y = aHotspot ? aHotspot->y : 0;
  aWidget->SetCursor(c, aContainer, x, y);
  return NS_OK;
}

class MOZ_STACK_CLASS ESMEventCB : public EventDispatchingCallback {
 public:
  explicit ESMEventCB(nsIContent* aTarget) : mTarget(aTarget) {}

  MOZ_CAN_RUN_SCRIPT
  void HandleEvent(EventChainPostVisitor& aVisitor) override {
    if (aVisitor.mPresContext) {
      nsIFrame* frame = aVisitor.mPresContext->GetPrimaryFrameFor(mTarget);
      if (frame) {
        frame->HandleEvent(MOZ_KnownLive(aVisitor.mPresContext),
                           aVisitor.mEvent->AsGUIEvent(),
                           &aVisitor.mEventStatus);
      }
    }
  }

  nsCOMPtr<nsIContent> mTarget;
};

static UniquePtr<WidgetMouseEvent> CreateMouseOrPointerWidgetEvent(
    WidgetMouseEvent* aMouseEvent, EventMessage aMessage,
    nsIContent* aRelatedContent) {
  WidgetPointerEvent* sourcePointer = aMouseEvent->AsPointerEvent();
  UniquePtr<WidgetMouseEvent> newEvent;
  if (sourcePointer) {
    AUTO_PROFILER_LABEL("CreateMouseOrPointerWidgetEvent", OTHER);

    WidgetPointerEvent* newPointerEvent = new WidgetPointerEvent(
        aMouseEvent->IsTrusted(), aMessage, aMouseEvent->mWidget);
    newPointerEvent->mIsPrimary = sourcePointer->mIsPrimary;
    newPointerEvent->mWidth = sourcePointer->mWidth;
    newPointerEvent->mHeight = sourcePointer->mHeight;
    newPointerEvent->mInputSource = sourcePointer->mInputSource;
    newPointerEvent->mRelatedTarget = aRelatedContent;

    newEvent = WrapUnique(newPointerEvent);
  } else {
    newEvent = MakeUnique<WidgetMouseEvent>(aMouseEvent->IsTrusted(), aMessage,
                                            aMouseEvent->mWidget,
                                            WidgetMouseEvent::eReal);
    newEvent->mRelatedTarget = aRelatedContent;
  }
  newEvent->mRefPoint = aMouseEvent->mRefPoint;
  newEvent->mModifiers = aMouseEvent->mModifiers;
  newEvent->mButton = aMouseEvent->mButton;
  newEvent->mButtons = aMouseEvent->mButtons;
  newEvent->mPressure = aMouseEvent->mPressure;
  newEvent->mPluginEvent = aMouseEvent->mPluginEvent;
  newEvent->mInputSource = aMouseEvent->mInputSource;
  newEvent->pointerId = aMouseEvent->pointerId;

  return newEvent;
}

nsIFrame* EventStateManager::DispatchMouseOrPointerEvent(
    WidgetMouseEvent* aMouseEvent, EventMessage aMessage,
    nsIContent* aTargetContent, nsIContent* aRelatedContent) {
  // http://dvcs.w3.org/hg/webevents/raw-file/default/mouse-lock.html#methods
  // "[When the mouse is locked on an element...e]vents that require the concept
  // of a mouse cursor must not be dispatched (for example: mouseover,
  // mouseout).
  if (sIsPointerLocked && (aMessage == eMouseLeave || aMessage == eMouseEnter ||
                           aMessage == eMouseOver || aMessage == eMouseOut)) {
    mCurrentTargetContent = nullptr;
    nsCOMPtr<Element> pointerLockedElement =
        do_QueryReferent(EventStateManager::sPointerLockedElement);
    if (!pointerLockedElement) {
      NS_WARNING("Should have pointer locked element, but didn't.");
      return nullptr;
    }
    return mPresContext->GetPrimaryFrameFor(pointerLockedElement);
  }

  mCurrentTargetContent = nullptr;

  if (!aTargetContent) {
    return nullptr;
  }

  nsCOMPtr<nsIContent> targetContent = aTargetContent;
  nsCOMPtr<nsIContent> relatedContent = aRelatedContent;

  UniquePtr<WidgetMouseEvent> dispatchEvent =
      CreateMouseOrPointerWidgetEvent(aMouseEvent, aMessage, relatedContent);

  AutoWeakFrame previousTarget = mCurrentTarget;
  mCurrentTargetContent = targetContent;

  nsIFrame* targetFrame = nullptr;

  nsEventStatus status = nsEventStatus_eIgnore;
  ESMEventCB callback(targetContent);
  EventDispatcher::Dispatch(targetContent, mPresContext, dispatchEvent.get(),
                            nullptr, &status, &callback);

  if (mPresContext) {
    // Although the primary frame was checked in event callback, it may not be
    // the same object after event dispatch and handling, so refetch it.
    targetFrame = mPresContext->GetPrimaryFrameFor(targetContent);

    // If we are entering/leaving remote content, dispatch a mouse enter/exit
    // event to the remote frame.
    if (IsRemoteTarget(targetContent)) {
      if (aMessage == eMouseOut) {
        // For remote content, send a "top-level" widget mouse exit event.
        UniquePtr<WidgetMouseEvent> remoteEvent =
            CreateMouseOrPointerWidgetEvent(aMouseEvent, eMouseExitFromWidget,
                                            relatedContent);
        remoteEvent->mExitFrom = WidgetMouseEvent::eTopLevel;

        // mCurrentTarget is set to the new target, so we must reset it to the
        // old target and then dispatch a cross-process event. (mCurrentTarget
        // will be set back below.) HandleCrossProcessEvent will query for the
        // proper target via GetEventTarget which will return mCurrentTarget.
        mCurrentTarget = targetFrame;
        HandleCrossProcessEvent(remoteEvent.get(), &status);
      } else if (aMessage == eMouseOver) {
        UniquePtr<WidgetMouseEvent> remoteEvent =
            CreateMouseOrPointerWidgetEvent(aMouseEvent, eMouseEnterIntoWidget,
                                            relatedContent);
        HandleCrossProcessEvent(remoteEvent.get(), &status);
      }
    }
  }

  mCurrentTargetContent = nullptr;
  mCurrentTarget = previousTarget;

  return targetFrame;
}

static nsIContent* FindCommonAncestor(nsIContent* aNode1, nsIContent* aNode2) {
  if (!aNode1 || !aNode2) {
    return nullptr;
  }
  return nsContentUtils::GetCommonFlattenedTreeAncestor(aNode1, aNode2);
}

class EnterLeaveDispatcher {
 public:
  EnterLeaveDispatcher(EventStateManager* aESM, nsIContent* aTarget,
                       nsIContent* aRelatedTarget,
                       WidgetMouseEvent* aMouseEvent,
                       EventMessage aEventMessage)
      : mESM(aESM), mMouseEvent(aMouseEvent), mEventMessage(aEventMessage) {
    nsPIDOMWindowInner* win =
        aTarget ? aTarget->OwnerDoc()->GetInnerWindow() : nullptr;
    if (aMouseEvent->AsPointerEvent()
            ? win && win->HasPointerEnterLeaveEventListeners()
            : win && win->HasMouseEnterLeaveEventListeners()) {
      mRelatedTarget =
          aRelatedTarget ? aRelatedTarget->FindFirstNonChromeOnlyAccessContent()
                         : nullptr;
      nsINode* commonParent = FindCommonAncestor(aTarget, aRelatedTarget);
      nsIContent* current = aTarget;
      // Note, it is ok if commonParent is null!
      while (current && current != commonParent) {
        if (!current->ChromeOnlyAccess()) {
          mTargets.AppendObject(current);
        }
        // mouseenter/leave is fired only on elements.
        current = current->GetFlattenedTreeParent();
      }
    }
  }

  void Dispatch() {
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

void EventStateManager::NotifyMouseOut(WidgetMouseEvent* aMouseEvent,
                                       nsIContent* aMovingInto) {
  RefPtr<OverOutElementsWrapper> wrapper = GetWrapperByEventID(aMouseEvent);

  if (!wrapper || !wrapper->mLastOverElement) return;
  // Before firing mouseout, check for recursion
  if (wrapper->mLastOverElement == wrapper->mFirstOutEventElement) return;

  if (wrapper->mLastOverFrame) {
    // if the frame is associated with a subdocument,
    // tell the subdocument that we're moving out of it
    nsSubDocumentFrame* subdocFrame =
        do_QueryFrame(wrapper->mLastOverFrame.GetFrame());
    if (subdocFrame) {
      nsIDocShell* docshell = subdocFrame->GetDocShell();
      if (docshell) {
        RefPtr<nsPresContext> presContext = docshell->GetPresContext();

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
  if (!wrapper->mLastOverElement) return;

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

  EnterLeaveDispatcher leaveDispatcher(this, wrapper->mLastOverElement,
                                       aMovingInto, aMouseEvent,
                                       isPointer ? ePointerLeave : eMouseLeave);

  // Fire mouseout
  DispatchMouseOrPointerEvent(aMouseEvent, isPointer ? ePointerOut : eMouseOut,
                              wrapper->mLastOverElement, aMovingInto);
  leaveDispatcher.Dispatch();

  wrapper->mLastOverFrame = nullptr;
  wrapper->mLastOverElement = nullptr;

  // Turn recursion protection back off
  wrapper->mFirstOutEventElement = nullptr;
}

void EventStateManager::RecomputeMouseEnterStateForRemoteFrame(
    Element& aElement) {
  if (!mMouseEnterLeaveHelper ||
      mMouseEnterLeaveHelper->mLastOverElement != &aElement) {
    return;
  }

  if (BrowserParent* remote = BrowserParent::GetFrom(&aElement)) {
    remote->MouseEnterIntoWidget();
  }
}

void EventStateManager::NotifyMouseOver(WidgetMouseEvent* aMouseEvent,
                                        nsIContent* aContent) {
  NS_ASSERTION(aContent, "Mouse must be over something");

  RefPtr<OverOutElementsWrapper> wrapper = GetWrapperByEventID(aMouseEvent);

  if (!wrapper || wrapper->mLastOverElement == aContent) return;

  // Before firing mouseover, check for recursion
  if (aContent == wrapper->mFirstOverEventElement) return;

  // Check to see if we're a subdocument and if so update the parent
  // document's ESM state to indicate that the mouse is over the
  // content associated with our subdocument.
  EnsureDocument(mPresContext);
  if (Document* parentDoc = mDocument->GetInProcessParentDocument()) {
    if (nsCOMPtr<nsIContent> docContent =
            parentDoc->FindContentForSubDocument(mDocument)) {
      if (PresShell* parentPresShell = parentDoc->GetPresShell()) {
        RefPtr<EventStateManager> parentESM =
            parentPresShell->GetPresContext()->EventStateManager();
        parentESM->NotifyMouseOver(aMouseEvent, docContent);
      }
    }
  }
  // Firing the DOM event in the parent document could cause all kinds
  // of havoc.  Reverify and take care.
  if (wrapper->mLastOverElement == aContent) return;

  // Remember mLastOverElement as the related content for the
  // DispatchMouseOrPointerEvent() call below, since NotifyMouseOut() resets it,
  // bug 298477.
  nsCOMPtr<nsIContent> lastOverElement = wrapper->mLastOverElement;

  bool isPointer = aMouseEvent->mClass == ePointerEventClass;

  EnterLeaveDispatcher enterDispatcher(this, aContent, lastOverElement,
                                       aMouseEvent,
                                       isPointer ? ePointerEnter : eMouseEnter);

  if (!isPointer) {
    SetContentState(aContent, NS_EVENT_STATE_HOVER);
  }

  NotifyMouseOut(aMouseEvent, aContent);

  // Store the first mouseOver event we fire and don't refire mouseOver
  // to that element while the first mouseOver is still ongoing.
  wrapper->mFirstOverEventElement = aContent;

  // Fire mouseover
  wrapper->mLastOverFrame = DispatchMouseOrPointerEvent(
      aMouseEvent, isPointer ? ePointerOver : eMouseOver, aContent,
      lastOverElement);
  enterDispatcher.Dispatch();
  wrapper->mLastOverElement = aContent;

  // Turn recursion protection back off
  wrapper->mFirstOverEventElement = nullptr;
}

// Returns the center point of the window's client area. This is
// in widget coordinates, i.e. relative to the widget's top-left
// corner, not in screen coordinates, the same units that UIEvent::
// refpoint is in. It may not be the exact center of the window if
// the platform requires rounding the coordinate.
static LayoutDeviceIntPoint GetWindowClientRectCenter(nsIWidget* aWidget) {
  NS_ENSURE_TRUE(aWidget, LayoutDeviceIntPoint(0, 0));

  LayoutDeviceIntRect rect = aWidget->GetClientBounds();
  LayoutDeviceIntPoint point(rect.x + rect.width / 2, rect.y + rect.height / 2);
  int32_t round = aWidget->RoundsWidgetCoordinatesTo();
  point.x = point.x / round * round;
  point.y = point.y / round * round;
  return point - aWidget->WidgetToScreenOffset();
}

void EventStateManager::GeneratePointerEnterExit(EventMessage aMessage,
                                                 WidgetMouseEvent* aEvent) {
  if (!StaticPrefs::dom_w3c_pointer_events_enabled()) {
    return;
  }
  WidgetPointerEvent pointerEvent(*aEvent);
  pointerEvent.mMessage = aMessage;
  GenerateMouseEnterExit(&pointerEvent);
}

/* static */
void EventStateManager::UpdateLastRefPointOfMouseEvent(
    WidgetMouseEvent* aMouseEvent) {
  if (aMouseEvent->mMessage != eMouseMove &&
      aMouseEvent->mMessage != ePointerMove) {
    return;
  }

  // Mouse movement is reported on the MouseEvent.movement{X,Y} fields.
  // Movement is calculated in UIEvent::GetMovementPoint() as:
  //   previous_mousemove_mRefPoint - current_mousemove_mRefPoint.
  if (sIsPointerLocked && aMouseEvent->mWidget) {
    // The pointer is locked. If the pointer is not located at the center of
    // the window, dispatch a synthetic mousemove to return the pointer there.
    // Doing this between "real" pointer moves gives the impression that the
    // (locked) pointer can continue moving and won't stop at the screen
    // boundary. We cancel the synthetic event so that we don't end up
    // dispatching the centering move event to content.
    aMouseEvent->mLastRefPoint =
        GetWindowClientRectCenter(aMouseEvent->mWidget);

  } else if (sLastRefPoint == kInvalidRefPoint) {
    // We don't have a valid previous mousemove mRefPoint. This is either
    // the first move we've encountered, or the mouse has just re-entered
    // the application window. We should report (0,0) movement for this
    // case, so make the current and previous mRefPoints the same.
    aMouseEvent->mLastRefPoint = aMouseEvent->mRefPoint;
  } else {
    aMouseEvent->mLastRefPoint = sLastRefPoint;
  }
}

/* static */
void EventStateManager::ResetPointerToWindowCenterWhilePointerLocked(
    WidgetMouseEvent* aMouseEvent) {
  MOZ_ASSERT(sIsPointerLocked);
  if ((aMouseEvent->mMessage != eMouseMove &&
       aMouseEvent->mMessage != ePointerMove) ||
      !aMouseEvent->mWidget) {
    return;
  }

  // We generate pointermove from mousemove event, so only synthesize native
  // mouse move and update sSynthCenteringPoint by mousemove event.
  bool updateSynthCenteringPoint = aMouseEvent->mMessage == eMouseMove;

  // The pointer is locked. If the pointer is not located at the center of
  // the window, dispatch a synthetic mousemove to return the pointer there.
  // Doing this between "real" pointer moves gives the impression that the
  // (locked) pointer can continue moving and won't stop at the screen
  // boundary. We cancel the synthetic event so that we don't end up
  // dispatching the centering move event to content.
  LayoutDeviceIntPoint center = GetWindowClientRectCenter(aMouseEvent->mWidget);

  if (aMouseEvent->mRefPoint != center && updateSynthCenteringPoint) {
    // Mouse move doesn't finish at the center of the window. Dispatch a
    // synthetic native mouse event to move the pointer back to the center
    // of the window, to faciliate more movement. But first, record that
    // we've dispatched a synthetic mouse movement, so we can cancel it
    // in the other branch here.
    sSynthCenteringPoint = center;
    aMouseEvent->mWidget->SynthesizeNativeMouseMove(
        center + aMouseEvent->mWidget->WidgetToScreenOffset(), nullptr);
  } else if (aMouseEvent->mRefPoint == sSynthCenteringPoint) {
    // This is the "synthetic native" event we dispatched to re-center the
    // pointer. Cancel it so we don't expose the centering move to content.
    aMouseEvent->StopPropagation();
    // Clear sSynthCenteringPoint so we don't cancel other events
    // targeted at the center.
    if (updateSynthCenteringPoint) {
      sSynthCenteringPoint = kInvalidRefPoint;
    }
  }
}

/* static */
void EventStateManager::UpdateLastPointerPosition(
    WidgetMouseEvent* aMouseEvent) {
  if (aMouseEvent->mMessage != eMouseMove) {
    return;
  }
  sLastRefPoint = aMouseEvent->mRefPoint;
}

void EventStateManager::GenerateMouseEnterExit(WidgetMouseEvent* aMouseEvent) {
  EnsureDocument(mPresContext);
  if (!mDocument) return;

  // Hold onto old target content through the event and reset after.
  nsCOMPtr<nsIContent> targetBeforeEvent = mCurrentTargetContent;

  switch (aMouseEvent->mMessage) {
    case eMouseMove:
    case ePointerMove:
    case ePointerDown:
    case ePointerGotCapture: {
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
    } break;
    case ePointerUp: {
      // Get the target content target (mousemove target == mouseover target)
      nsCOMPtr<nsIContent> targetElement = GetEventTargetContent(aMouseEvent);
      if (!targetElement) {
        // We're always over the document root, even if we're only
        // over dead space in a page (whose frame is not associated with
        // any content) or in print preview dead space
        targetElement = mDocument->GetRootElement();
      }
      if (targetElement) {
        RefPtr<OverOutElementsWrapper> helper =
            GetWrapperByEventID(aMouseEvent);
        if (helper) {
          helper->mLastOverElement = targetElement;
        }
        NotifyMouseOut(aMouseEvent, nullptr);
      }
    } break;
    case ePointerLeave:
    case ePointerCancel:
    case eMouseExitFromWidget: {
      // This is actually the window mouse exit or pointer leave event. We're
      // not moving into any new element.

      RefPtr<OverOutElementsWrapper> helper = GetWrapperByEventID(aMouseEvent);
      if (helper && helper->mLastOverFrame &&
          nsContentUtils::GetTopLevelWidget(aMouseEvent->mWidget) !=
              nsContentUtils::GetTopLevelWidget(
                  helper->mLastOverFrame->GetNearestWidget())) {
        // the Mouse/PointerOut event widget doesn't have same top widget with
        // mLastOverFrame, it's a spurious event for mLastOverFrame
        break;
      }

      // Reset sLastRefPoint, so that we'll know not to report any
      // movement the next time we re-enter the window.
      sLastRefPoint = kInvalidRefPoint;

      NotifyMouseOut(aMouseEvent, nullptr);
    } break;
    default:
      break;
  }

  // reset mCurretTargetContent to what it was
  mCurrentTargetContent = targetBeforeEvent;
}

OverOutElementsWrapper* EventStateManager::GetWrapperByEventID(
    WidgetMouseEvent* aEvent) {
  WidgetPointerEvent* pointer = aEvent->AsPointerEvent();
  if (!pointer) {
    MOZ_ASSERT(aEvent->AsMouseEvent() != nullptr);
    if (!mMouseEnterLeaveHelper) {
      mMouseEnterLeaveHelper = new OverOutElementsWrapper();
    }
    return mMouseEnterLeaveHelper;
  }
  return mPointersEnterLeaveHelper.LookupForAdd(pointer->pointerId)
      .OrInsert([]() { return new OverOutElementsWrapper(); });
}

/* static */
void EventStateManager::SetPointerLock(nsIWidget* aWidget,
                                       nsIContent* aElement) {
  // NOTE: aElement will be nullptr when unlocking.
  sIsPointerLocked = !!aElement;

  // Reset mouse wheel transaction
  WheelTransaction::EndTransaction();

  // Deal with DnD events
  nsCOMPtr<nsIDragService> dragService =
      do_GetService("@mozilla.org/widget/dragservice;1");

  if (sIsPointerLocked) {
    MOZ_ASSERT(aWidget, "Locking pointer requires a widget");

    // Release all pointer capture when a pointer lock is successfully applied
    // on an element.
    PointerEventHandler::ReleaseAllPointerCapture();

    // Store the last known ref point so we can reposition the pointer after
    // unlock.
    sPreLockPoint = sLastRefPoint;

    // Fire a synthetic mouse move to ensure event state is updated. We first
    // set the mouse to the center of the window, so that the mouse event
    // doesn't report any movement.
    sLastRefPoint = GetWindowClientRectCenter(aWidget);
    aWidget->SynthesizeNativeMouseMove(
        sLastRefPoint + aWidget->WidgetToScreenOffset(), nullptr);

    // Suppress DnD
    if (dragService) {
      dragService->Suppress();
    }
  } else {
    // Unlocking, so return pointer to the original position by firing a
    // synthetic mouse event. We first reset sLastRefPoint to its
    // pre-pointerlock position, so that the synthetic mouse event reports
    // no movement.
    sLastRefPoint = sPreLockPoint;
    // Reset SynthCenteringPoint to invalid so that next time we start
    // locking pointer, it has its initial value.
    sSynthCenteringPoint = kInvalidRefPoint;
    if (aWidget) {
      aWidget->SynthesizeNativeMouseMove(
          sPreLockPoint + aWidget->WidgetToScreenOffset(), nullptr);
    }

    // Unsuppress DnD
    if (dragService) {
      dragService->Unsuppress();
    }
  }
}

void EventStateManager::GenerateDragDropEnterExit(nsPresContext* aPresContext,
                                                  WidgetDragEvent* aDragEvent) {
  // Hold onto old target content through the event and reset after.
  nsCOMPtr<nsIContent> targetBeforeEvent = mCurrentTargetContent;

  switch (aDragEvent->mMessage) {
    case eDragOver: {
      // when dragging from one frame to another, events are fired in the
      // order: dragexit, dragenter, dragleave
      if (sLastDragOverFrame != mCurrentTarget) {
        // We'll need the content, too, to check if it changed separately from
        // the frames.
        nsCOMPtr<nsIContent> lastContent;
        nsCOMPtr<nsIContent> targetContent;
        mCurrentTarget->GetContentForEvent(aDragEvent,
                                           getter_AddRefs(targetContent));

        if (sLastDragOverFrame) {
          // The frame has changed but the content may not have. Check before
          // dispatching to content
          sLastDragOverFrame->GetContentForEvent(aDragEvent,
                                                 getter_AddRefs(lastContent));

          FireDragEnterOrExit(sLastDragOverFrame->PresContext(), aDragEvent,
                              eDragExit, targetContent, lastContent,
                              sLastDragOverFrame);
          nsIContent* target = sLastDragOverFrame
                                   ? sLastDragOverFrame.GetFrame()->GetContent()
                                   : nullptr;
          if (IsRemoteTarget(target)) {
            // Dragging something and moving from web content to chrome only
            // fires dragexit and dragleave to xul:browser. We have to forward
            // dragexit to sLastDragOverFrame when its content is a remote
            // target. We don't forward dragleave since it's generated from
            // dragexit.
            WidgetDragEvent remoteEvent(aDragEvent->IsTrusted(), eDragExit,
                                        aDragEvent->mWidget);
            remoteEvent.AssignDragEventData(*aDragEvent, true);
            remoteEvent.mFlags.mIsSynthesizedForTests =
                aDragEvent->mFlags.mIsSynthesizedForTests;
            nsEventStatus remoteStatus = nsEventStatus_eIgnore;
            HandleCrossProcessEvent(&remoteEvent, &remoteStatus);
          }
        }

        AutoWeakFrame currentTraget = mCurrentTarget;
        FireDragEnterOrExit(aPresContext, aDragEvent, eDragEnter, lastContent,
                            targetContent, currentTraget);

        if (sLastDragOverFrame) {
          FireDragEnterOrExit(sLastDragOverFrame->PresContext(), aDragEvent,
                              eDragLeave, targetContent, lastContent,
                              sLastDragOverFrame);
        }

        sLastDragOverFrame = mCurrentTarget;
      }
    } break;

    case eDragExit: {
      // This is actually the window mouse exit event.
      if (sLastDragOverFrame) {
        nsCOMPtr<nsIContent> lastContent;
        sLastDragOverFrame->GetContentForEvent(aDragEvent,
                                               getter_AddRefs(lastContent));

        RefPtr<nsPresContext> lastDragOverFramePresContext =
            sLastDragOverFrame->PresContext();
        FireDragEnterOrExit(lastDragOverFramePresContext, aDragEvent, eDragExit,
                            nullptr, lastContent, sLastDragOverFrame);
        FireDragEnterOrExit(lastDragOverFramePresContext, aDragEvent,
                            eDragLeave, nullptr, lastContent,
                            sLastDragOverFrame);

        sLastDragOverFrame = nullptr;
      }
    } break;

    default:
      break;
  }

  // reset mCurretTargetContent to what it was
  mCurrentTargetContent = targetBeforeEvent;

  // Now flush all pending notifications, for better responsiveness.
  FlushLayout(aPresContext);
}

void EventStateManager::FireDragEnterOrExit(nsPresContext* aPresContext,
                                            WidgetDragEvent* aDragEvent,
                                            EventMessage aMessage,
                                            nsIContent* aRelatedTarget,
                                            nsIContent* aTargetContent,
                                            AutoWeakFrame& aTargetFrame) {
  MOZ_ASSERT(aMessage == eDragLeave || aMessage == eDragExit ||
             aMessage == eDragEnter);
  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetDragEvent event(aDragEvent->IsTrusted(), aMessage, aDragEvent->mWidget);
  event.AssignDragEventData(*aDragEvent, false);
  event.mFlags.mIsSynthesizedForTests =
      aDragEvent->mFlags.mIsSynthesizedForTests;
  event.mRelatedTarget = aRelatedTarget;
  mCurrentTargetContent = aTargetContent;

  if (aTargetContent != aRelatedTarget) {
    // XXX This event should still go somewhere!!
    if (aTargetContent) {
      EventDispatcher::Dispatch(aTargetContent, aPresContext, &event, nullptr,
                                &status);
    }

    // adjust the drag hover if the dragenter event was cancelled or this is a
    // drag exit
    if (status == nsEventStatus_eConsumeNoDefault || aMessage == eDragExit) {
      SetContentState((aMessage == eDragEnter) ? aTargetContent : nullptr,
                      NS_EVENT_STATE_DRAGOVER);
    }

    // collect any changes to moz cursor settings stored in the event's
    // data transfer.
    UpdateDragDataTransfer(&event);
  }

  // Finally dispatch the event to the frame
  if (aTargetFrame) {
    aTargetFrame->HandleEvent(aPresContext, &event, &status);
  }
}

void EventStateManager::UpdateDragDataTransfer(WidgetDragEvent* dragEvent) {
  NS_ASSERTION(dragEvent, "drag event is null in UpdateDragDataTransfer!");
  if (!dragEvent->mDataTransfer) {
    return;
  }

  nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();

  if (dragSession) {
    // the initial dataTransfer is the one from the dragstart event that
    // was set on the dragSession when the drag began.
    RefPtr<DataTransfer> initialDataTransfer = dragSession->GetDataTransfer();
    if (initialDataTransfer) {
      // retrieve the current moz cursor setting and save it.
      nsAutoString mozCursor;
      dragEvent->mDataTransfer->GetMozCursor(mozCursor);
      initialDataTransfer->SetMozCursor(mozCursor);
    }
  }
}

nsresult EventStateManager::SetClickCount(WidgetMouseEvent* aEvent,
                                          nsEventStatus* aStatus,
                                          nsIContent* aOverrideClickTarget) {
  nsCOMPtr<nsIContent> mouseContent = aOverrideClickTarget;
  if (!mouseContent && mCurrentTarget) {
    mCurrentTarget->GetContentForEvent(aEvent, getter_AddRefs(mouseContent));
  }
  if (mouseContent && mouseContent->IsText()) {
    nsINode* parent = mouseContent->GetFlattenedTreeParentNode();
    if (parent && parent->IsContent()) {
      mouseContent = parent->AsContent();
    }
  }

  switch (aEvent->mButton) {
    case MouseButton::eLeft:
      if (aEvent->mMessage == eMouseDown) {
        mLastLeftMouseDownContent = mouseContent;
      } else if (aEvent->mMessage == eMouseUp) {
        aEvent->mClickTarget =
            nsContentUtils::GetCommonAncestorUnderInteractiveContent(
                mouseContent, mLastLeftMouseDownContent);
        if (aEvent->mClickTarget) {
          aEvent->mClickCount = mLClickCount;
          mLClickCount = 0;
        } else {
          aEvent->mClickCount = 0;
        }
        mLastLeftMouseDownContent = nullptr;
      }
      break;

    case MouseButton::eMiddle:
      if (aEvent->mMessage == eMouseDown) {
        mLastMiddleMouseDownContent = mouseContent;
      } else if (aEvent->mMessage == eMouseUp) {
        aEvent->mClickTarget =
            nsContentUtils::GetCommonAncestorUnderInteractiveContent(
                mouseContent, mLastMiddleMouseDownContent);
        if (aEvent->mClickTarget) {
          aEvent->mClickCount = mMClickCount;
          mMClickCount = 0;
        } else {
          aEvent->mClickCount = 0;
        }
        mLastMiddleMouseDownContent = nullptr;
      }
      break;

    case MouseButton::eRight:
      if (aEvent->mMessage == eMouseDown) {
        mLastRightMouseDownContent = mouseContent;
      } else if (aEvent->mMessage == eMouseUp) {
        aEvent->mClickTarget =
            nsContentUtils::GetCommonAncestorUnderInteractiveContent(
                mouseContent, mLastRightMouseDownContent);
        if (aEvent->mClickTarget) {
          aEvent->mClickCount = mRClickCount;
          mRClickCount = 0;
        } else {
          aEvent->mClickCount = 0;
        }
        mLastRightMouseDownContent = nullptr;
      }
      break;
  }

  return NS_OK;
}

// static
bool EventStateManager::EventCausesClickEvents(
    const WidgetMouseEvent& aMouseEvent) {
  if (NS_WARN_IF(aMouseEvent.mMessage != eMouseUp)) {
    return false;
  }
  // If the mouseup event is synthesized event, we don't need to dispatch
  // click events.
  if (!aMouseEvent.IsReal()) {
    return false;
  }
  // If mouse is still over same element, clickcount will be > 1.
  // If it has moved it will be zero, so no click.
  if (!aMouseEvent.mClickCount || !aMouseEvent.mClickTarget) {
    return false;
  }
  // Check that the window isn't disabled before firing a click
  // (see bug 366544).
  return !(aMouseEvent.mWidget && !aMouseEvent.mWidget->IsEnabled());
}

nsresult EventStateManager::InitAndDispatchClickEvent(
    WidgetMouseEvent* aMouseUpEvent, nsEventStatus* aStatus,
    EventMessage aMessage, PresShell* aPresShell, nsIContent* aMouseUpContent,
    AutoWeakFrame aCurrentTarget, bool aNoContentDispatch,
    nsIContent* aOverrideClickTarget) {
  MOZ_ASSERT(aMouseUpEvent);
  MOZ_ASSERT(EventCausesClickEvents(*aMouseUpEvent));
  MOZ_ASSERT(aMouseUpContent || aCurrentTarget || aOverrideClickTarget);

  WidgetMouseEvent event(aMouseUpEvent->IsTrusted(), aMessage,
                         aMouseUpEvent->mWidget, WidgetMouseEvent::eReal);

  event.mRefPoint = aMouseUpEvent->mRefPoint;
  event.mClickCount = aMouseUpEvent->mClickCount;
  event.mModifiers = aMouseUpEvent->mModifiers;
  event.mButtons = aMouseUpEvent->mButtons;
  event.mTime = aMouseUpEvent->mTime;
  event.mTimeStamp = aMouseUpEvent->mTimeStamp;
  event.mFlags.mOnlyChromeDispatch =
      aNoContentDispatch && !aMouseUpEvent->mUseLegacyNonPrimaryDispatch;
  event.mFlags.mNoContentDispatch = aNoContentDispatch;
  event.mButton = aMouseUpEvent->mButton;
  event.pointerId = aMouseUpEvent->pointerId;
  event.mInputSource = aMouseUpEvent->mInputSource;
  nsIContent* target = aMouseUpContent;
  nsIFrame* targetFrame = aCurrentTarget;
  if (aOverrideClickTarget) {
    target = aOverrideClickTarget;
    targetFrame = aOverrideClickTarget->GetPrimaryFrame();
  }

  if (!target->IsInComposedDoc()) {
    return NS_OK;
  }

  // Use local event status for each click event dispatching since it'll be
  // cleared by EventStateManager::PreHandleEvent().  Therefore, dispatching
  // an event means that previous event status will be ignored.
  nsEventStatus status = nsEventStatus_eIgnore;
  nsresult rv = aPresShell->HandleEventWithTarget(
      &event, targetFrame, MOZ_KnownLive(target), &status);

  if (event.mFlags.mHadNonPrivilegedClickListeners && !aNoContentDispatch) {
    Telemetry::AccumulateCategorical(
        Telemetry::LABELS_TYPES_OF_USER_CLICKS::Has_JS_Listener);
  }
  // Copy mMultipleActionsPrevented flag from a click event to the mouseup
  // event only when it's set to true.  It may be set to true if an editor has
  // already handled it.  This is important to avoid two or more default
  // actions handled here.
  aMouseUpEvent->mFlags.mMultipleActionsPrevented |=
      event.mFlags.mMultipleActionsPrevented;
  // If current status is nsEventStatus_eConsumeNoDefault, we don't need to
  // overwrite it.
  if (*aStatus == nsEventStatus_eConsumeNoDefault) {
    return rv;
  }
  // If new status is nsEventStatus_eConsumeNoDefault or
  // nsEventStatus_eConsumeDoDefault, use it.
  if (status == nsEventStatus_eConsumeNoDefault ||
      status == nsEventStatus_eConsumeDoDefault) {
    *aStatus = status;
    return rv;
  }
  // Otherwise, keep the original status.
  return rv;
}

nsresult EventStateManager::PostHandleMouseUp(
    WidgetMouseEvent* aMouseUpEvent, nsEventStatus* aStatus,
    nsIContent* aOverrideClickTarget) {
  MOZ_ASSERT(aMouseUpEvent);
  MOZ_ASSERT(EventCausesClickEvents(*aMouseUpEvent));
  MOZ_ASSERT(aStatus);

  RefPtr<PresShell> presShell = mPresContext->GetPresShell();
  if (!presShell) {
    return NS_OK;
  }

  nsCOMPtr<nsIContent> clickTarget =
      do_QueryInterface(aMouseUpEvent->mClickTarget);
  NS_ENSURE_STATE(clickTarget);

  // Fire click events if the event target is still available.
  // Note that do not include the eMouseUp event's status since we ignore it
  // for compatibility with the other browsers.
  nsEventStatus status = nsEventStatus_eIgnore;
  nsresult rv = DispatchClickEvents(presShell, aMouseUpEvent, &status,
                                    clickTarget, aOverrideClickTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Do not do anything if preceding click events are consumed.
  // Note that Chromium dispatches "paste" event and actually pates clipboard
  // text into focused editor even if the preceding click events are consumed.
  // However, this is different from our traditional behavior and does not
  // conform to DOM events.  If we need to keep compatibility with Chromium,
  // we should change it later.
  if (status == nsEventStatus_eConsumeNoDefault) {
    *aStatus = nsEventStatus_eConsumeNoDefault;
    return NS_OK;
  }

  // Handle middle click paste if it's enabled and the mouse button is middle.
  if (aMouseUpEvent->mButton != MouseButton::eMiddle ||
      !WidgetMouseEvent::IsMiddleClickPasteEnabled()) {
    return NS_OK;
  }
  DebugOnly<nsresult> rvIgnored =
      HandleMiddleClickPaste(presShell, aMouseUpEvent, &status, nullptr);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "Failed to paste for a middle click");

  // If new status is nsEventStatus_eConsumeNoDefault or
  // nsEventStatus_eConsumeDoDefault, use it.
  if (*aStatus != nsEventStatus_eConsumeNoDefault &&
      (status == nsEventStatus_eConsumeNoDefault ||
       status == nsEventStatus_eConsumeDoDefault)) {
    *aStatus = status;
  }

  // Don't return error even if middle mouse paste fails since we haven't
  // handled it here.
  return NS_OK;
}

nsresult EventStateManager::DispatchClickEvents(
    PresShell* aPresShell, WidgetMouseEvent* aMouseUpEvent,
    nsEventStatus* aStatus, nsIContent* aClickTarget,
    nsIContent* aOverrideClickTarget) {
  MOZ_ASSERT(aPresShell);
  MOZ_ASSERT(aMouseUpEvent);
  MOZ_ASSERT(EventCausesClickEvents(*aMouseUpEvent));
  MOZ_ASSERT(aStatus);
  MOZ_ASSERT(aClickTarget || aOverrideClickTarget);

  bool notDispatchToContents =
      (aMouseUpEvent->mButton == MouseButton::eMiddle ||
       aMouseUpEvent->mButton == MouseButton::eRight);

  bool fireAuxClick = notDispatchToContents;

  AutoWeakFrame currentTarget = aClickTarget->GetPrimaryFrame();
  nsresult rv = InitAndDispatchClickEvent(
      aMouseUpEvent, aStatus, eMouseClick, aPresShell, aClickTarget,
      currentTarget, notDispatchToContents, aOverrideClickTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Fire auxclick event if necessary.
  if (fireAuxClick && *aStatus != nsEventStatus_eConsumeNoDefault &&
      aClickTarget && aClickTarget->IsInComposedDoc()) {
    rv = InitAndDispatchClickEvent(aMouseUpEvent, aStatus, eMouseAuxClick,
                                   aPresShell, aClickTarget, currentTarget,
                                   false, aOverrideClickTarget);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to dispatch eMouseAuxClick");
  }

  // Fire double click event if click count is 2.
  if (aMouseUpEvent->mClickCount == 2 && !fireAuxClick && aClickTarget &&
      aClickTarget->IsInComposedDoc()) {
    rv = InitAndDispatchClickEvent(aMouseUpEvent, aStatus, eMouseDoubleClick,
                                   aPresShell, aClickTarget, currentTarget,
                                   notDispatchToContents, aOverrideClickTarget);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // notDispatchToContents is used here because we don't want to
  // count auxclicks.
  if (XRE_IsParentProcess() && !IsRemoteTarget(aClickTarget) &&
      !notDispatchToContents) {
    Telemetry::AccumulateCategorical(
        Telemetry::LABELS_TYPES_OF_USER_CLICKS::Browser_Chrome);
  }

  return rv;
}

nsresult EventStateManager::HandleMiddleClickPaste(
    PresShell* aPresShell, WidgetMouseEvent* aMouseEvent,
    nsEventStatus* aStatus, TextEditor* aTextEditor) {
  MOZ_ASSERT(aPresShell);
  MOZ_ASSERT(aMouseEvent);
  MOZ_ASSERT((aMouseEvent->mMessage == eMouseAuxClick &&
              aMouseEvent->mButton == MouseButton::eMiddle) ||
             EventCausesClickEvents(*aMouseEvent));
  MOZ_ASSERT(aStatus);
  MOZ_ASSERT(*aStatus != nsEventStatus_eConsumeNoDefault);

  // Even if we're called twice or more for a mouse operation, we should
  // handle only once.  Although mMultipleActionsPrevented may be set to
  // true by different event handler in the future, we can use it for now.
  if (aMouseEvent->mFlags.mMultipleActionsPrevented) {
    return NS_OK;
  }
  aMouseEvent->mFlags.mMultipleActionsPrevented = true;

  RefPtr<Selection> selection;
  if (aTextEditor) {
    selection = aTextEditor->GetSelection();
    if (NS_WARN_IF(!selection)) {
      return NS_ERROR_FAILURE;
    }
  } else {
    Document* document = aPresShell->GetDocument();
    if (NS_WARN_IF(!document)) {
      return NS_ERROR_FAILURE;
    }
    selection = nsCopySupport::GetSelectionForCopy(document);
    if (NS_WARN_IF(!selection)) {
      return NS_ERROR_FAILURE;
    }
  }

  // Move selection to the clicked point.
  nsCOMPtr<nsIContent> container;
  int32_t offset;
  nsLayoutUtils::GetContainerAndOffsetAtEvent(
      aPresShell, aMouseEvent, getter_AddRefs(container), &offset);
  if (container) {
    // XXX If readonly or disabled <input> or <textarea> in contenteditable
    //     designMode editor is clicked, the point is in the editor.
    //     However, outer HTMLEditor and Selection should handle it.
    //     So, in such case, Selection::Collapse() will fail.
    DebugOnly<nsresult> rv = selection->Collapse(container, offset);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Failed to collapse Selection at middle clicked");
  }

  int32_t clipboardType = nsIClipboard::kGlobalClipboard;
  nsresult rv = NS_OK;
  nsCOMPtr<nsIClipboard> clipboardService =
      do_GetService("@mozilla.org/widget/clipboard;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    bool selectionSupported;
    rv = clipboardService->SupportsSelectionClipboard(&selectionSupported);
    if (NS_SUCCEEDED(rv) && selectionSupported) {
      clipboardType = nsIClipboard::kSelectionClipboard;
    }
  }

  // Fire ePaste event by ourselves since we need to dispatch "paste" event
  // even if the middle click event was consumed for compatibility with
  // Chromium.
  if (!nsCopySupport::FireClipboardEvent(ePaste, clipboardType, aPresShell,
                                         selection)) {
    *aStatus = nsEventStatus_eConsumeNoDefault;
    return NS_OK;
  }

  // Although we've fired "paste" event, there is no editor to accept the
  // clipboard content.
  if (!aTextEditor) {
    return NS_OK;
  }

  // Check if the editor is still the good target to paste.
  if (aTextEditor->Destroyed() || aTextEditor->IsReadonly()) {
    // XXX Should we consume the event when the editor is readonly and/or
    //     disabled?
    return NS_OK;
  }

  // The selection may have been modified during reflow.  Therefore, we
  // should adjust event target to pass IsAcceptableInputEvent().
  nsRange* range = selection->GetRangeAt(0);
  if (!range) {
    return NS_OK;
  }
  WidgetMouseEvent mouseEvent(*aMouseEvent);
  mouseEvent.mOriginalTarget = range->GetStartContainer();
  if (NS_WARN_IF(!mouseEvent.mOriginalTarget) ||
      !aTextEditor->IsAcceptableInputEvent(&mouseEvent)) {
    return NS_OK;
  }

  // If Control key is pressed, we should paste clipboard content as
  // quotation.  Otherwise, paste it as is.
  if (aMouseEvent->IsControl()) {
    DebugOnly<nsresult> rv =
        aTextEditor->PasteAsQuotationAsAction(clipboardType, false);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to paste as quotation");
  } else {
    DebugOnly<nsresult> rv = aTextEditor->PasteAsAction(clipboardType, false);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to paste");
  }
  *aStatus = nsEventStatus_eConsumeNoDefault;

  return NS_OK;
}

nsIFrame* EventStateManager::GetEventTarget() {
  PresShell* presShell;
  if (mCurrentTarget || !mPresContext ||
      !(presShell = mPresContext->GetPresShell())) {
    return mCurrentTarget;
  }

  if (mCurrentTargetContent) {
    mCurrentTarget = mPresContext->GetPrimaryFrameFor(mCurrentTargetContent);
    if (mCurrentTarget) {
      return mCurrentTarget;
    }
  }

  nsIFrame* frame = presShell->GetCurrentEventFrame();
  return (mCurrentTarget = frame);
}

already_AddRefed<nsIContent> EventStateManager::GetEventTargetContent(
    WidgetEvent* aEvent) {
  if (aEvent && (aEvent->mMessage == eFocus || aEvent->mMessage == eBlur)) {
    nsCOMPtr<nsIContent> content = GetFocusedContent();
    return content.forget();
  }

  if (mCurrentTargetContent) {
    nsCOMPtr<nsIContent> content = mCurrentTargetContent;
    return content.forget();
  }

  nsCOMPtr<nsIContent> content;
  if (PresShell* presShell = mPresContext->GetPresShell()) {
    content = presShell->GetEventTargetContent(aEvent);
  }

  // Some events here may set mCurrentTarget but not set the corresponding
  // event target in the PresShell.
  if (!content && mCurrentTarget) {
    mCurrentTarget->GetContentForEvent(aEvent, getter_AddRefs(content));
  }

  return content.forget();
}

static Element* GetLabelTarget(nsIContent* aPossibleLabel) {
  mozilla::dom::HTMLLabelElement* label =
      mozilla::dom::HTMLLabelElement::FromNode(aPossibleLabel);
  if (!label) return nullptr;

  return label->GetLabeledElement();
}

/* static */
void EventStateManager::SetFullscreenState(Element* aElement,
                                           bool aIsFullscreen) {
  DoStateChange(aElement, NS_EVENT_STATE_FULLSCREEN, aIsFullscreen);
}

/* static */
inline void EventStateManager::DoStateChange(Element* aElement,
                                             EventStates aState,
                                             bool aAddState) {
  if (aAddState) {
    aElement->AddStates(aState);
  } else {
    aElement->RemoveStates(aState);
  }
}

/* static */
inline void EventStateManager::DoStateChange(nsIContent* aContent,
                                             EventStates aState,
                                             bool aStateAdded) {
  if (aContent->IsElement()) {
    DoStateChange(aContent->AsElement(), aState, aStateAdded);
  }
}

/* static */
void EventStateManager::UpdateAncestorState(nsIContent* aStartNode,
                                            nsIContent* aStopBefore,
                                            EventStates aState,
                                            bool aAddState) {
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
    for (; aStartNode; aStartNode = aStartNode->GetFlattenedTreeParent()) {
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

bool EventStateManager::SetContentState(nsIContent* aContent,
                                        EventStates aState) {
  MOZ_ASSERT(ManagesState(aState), "Unexpected state");

  nsCOMPtr<nsIContent> notifyContent1;
  nsCOMPtr<nsIContent> notifyContent2;
  bool updateAncestors;

  if (aState == NS_EVENT_STATE_HOVER || aState == NS_EVENT_STATE_ACTIVE) {
    // Hover and active are hierarchical
    updateAncestors = true;

    // check to see that this state is allowed by style. Check dragover too?
    // XXX Is this even what we want?
    if (mCurrentTarget) {
      const nsStyleUI* ui = mCurrentTarget->StyleUI();
      if (ui->mUserInput == StyleUserInput::None) {
        return false;
      }
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
        NS_ASSERTION(!aContent || aContent->GetComposedDoc() ==
                                      mPresContext->PresShell()->GetDocument(),
                     "Unexpected document");
        nsIFrame* frame = aContent ? aContent->GetPrimaryFrame() : nullptr;
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

void EventStateManager::ResetLastOverForContent(
    const uint32_t& aIdx, RefPtr<OverOutElementsWrapper>& aElemWrapper,
    nsIContent* aContent) {
  if (aElemWrapper && aElemWrapper->mLastOverElement &&
      nsContentUtils::ContentIsFlattenedTreeDescendantOf(
          aElemWrapper->mLastOverElement, aContent)) {
    aElemWrapper->mLastOverElement = nullptr;
  }
}

void EventStateManager::RemoveNodeFromChainIfNeeded(EventStates aState,
                                                    nsIContent* aContentRemoved,
                                                    bool aNotify) {
  MOZ_ASSERT(aState == NS_EVENT_STATE_HOVER || aState == NS_EVENT_STATE_ACTIVE);
  if (!aContentRemoved->IsElement() ||
      !aContentRemoved->AsElement()->State().HasState(aState)) {
    return;
  }

  nsCOMPtr<nsIContent>& leaf =
      aState == NS_EVENT_STATE_HOVER ? mHoverContent : mActiveContent;

  MOZ_ASSERT(leaf);
  // These two NS_ASSERTIONS below can fail for Shadow DOM sometimes, and it's
  // not clear how to best handle it, see
  // https://github.com/whatwg/html/issues/4795 and bug 1551621.
  NS_ASSERTION(
      nsContentUtils::ContentIsFlattenedTreeDescendantOf(leaf, aContentRemoved),
      "Flat tree and active / hover chain got out of sync");

  nsIContent* newLeaf = aContentRemoved->GetFlattenedTreeParent();
  MOZ_ASSERT(!newLeaf || newLeaf->IsElement());
  NS_ASSERTION(!newLeaf || newLeaf->AsElement()->State().HasState(aState),
               "State got out of sync because of shadow DOM");
  if (aNotify) {
    SetContentState(newLeaf, aState);
  } else {
    // We don't update the removed content's state here, since removing NAC
    // happens from layout and we don't really want to notify at that point or
    // what not.
    //
    // Also, NAC is not observable and NAC being removed will go away soon.
    leaf = newLeaf;
  }
  MOZ_ASSERT(leaf == newLeaf);
}

void EventStateManager::NativeAnonymousContentRemoved(nsIContent* aContent) {
  MOZ_ASSERT(aContent->IsRootOfNativeAnonymousSubtree());
  RemoveNodeFromChainIfNeeded(NS_EVENT_STATE_HOVER, aContent, false);
  RemoveNodeFromChainIfNeeded(NS_EVENT_STATE_ACTIVE, aContent, false);

  if (mLastLeftMouseDownContent &&
      nsContentUtils::ContentIsFlattenedTreeDescendantOf(
          mLastLeftMouseDownContent, aContent)) {
    mLastLeftMouseDownContent = aContent->GetFlattenedTreeParent();
  }

  if (mLastMiddleMouseDownContent &&
      nsContentUtils::ContentIsFlattenedTreeDescendantOf(
          mLastMiddleMouseDownContent, aContent)) {
    mLastMiddleMouseDownContent = aContent->GetFlattenedTreeParent();
  }

  if (mLastRightMouseDownContent &&
      nsContentUtils::ContentIsFlattenedTreeDescendantOf(
          mLastRightMouseDownContent, aContent)) {
    mLastRightMouseDownContent = aContent->GetFlattenedTreeParent();
  }
}

void EventStateManager::ContentRemoved(Document* aDocument,
                                       nsIContent* aContent) {
  /*
   * Anchor and area elements when focused or hovered might make the UI to show
   * the current link. We want to make sure that the UI gets informed when they
   * are actually removed from the DOM.
   */
  if (aContent->IsAnyOfHTMLElements(nsGkAtoms::a, nsGkAtoms::area) &&
      (aContent->AsElement()->State().HasAtLeastOneOfStates(
          NS_EVENT_STATE_FOCUS | NS_EVENT_STATE_HOVER))) {
    nsGenericHTMLElement* element =
        static_cast<nsGenericHTMLElement*>(aContent);
    element->LeaveLink(
        element->GetPresContext(nsGenericHTMLElement::eForComposedDoc));
  }

  IMEStateManager::OnRemoveContent(mPresContext, aContent);

  // inform the focus manager that the content is being removed. If this
  // content is focused, the focus will be removed without firing events.
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) fm->ContentRemoved(aDocument, aContent);

  RemoveNodeFromChainIfNeeded(NS_EVENT_STATE_HOVER, aContent, true);
  RemoveNodeFromChainIfNeeded(NS_EVENT_STATE_ACTIVE, aContent, true);

  if (sDragOverContent &&
      sDragOverContent->OwnerDoc() == aContent->OwnerDoc() &&
      nsContentUtils::ContentIsFlattenedTreeDescendantOf(sDragOverContent,
                                                         aContent)) {
    sDragOverContent = nullptr;
  }

  PointerEventHandler::ReleaseIfCaptureByDescendant(aContent);

  // See bug 292146 for why we want to null this out
  ResetLastOverForContent(0, mMouseEnterLeaveHelper, aContent);
  for (auto iter = mPointersEnterLeaveHelper.Iter(); !iter.Done();
       iter.Next()) {
    ResetLastOverForContent(iter.Key(), iter.Data(), aContent);
  }
}

bool EventStateManager::EventStatusOK(WidgetGUIEvent* aEvent) {
  return !(aEvent->mMessage == eMouseDown &&
           aEvent->AsMouseEvent()->mButton == MouseButton::eLeft &&
           !sNormalLMouseEventInProcess);
}

//-------------------------------------------
// Access Key Registration
//-------------------------------------------
void EventStateManager::RegisterAccessKey(Element* aElement, uint32_t aKey) {
  if (aElement && mAccessKeys.IndexOf(aElement) == -1)
    mAccessKeys.AppendObject(aElement);
}

void EventStateManager::UnregisterAccessKey(Element* aElement, uint32_t aKey) {
  if (aElement) mAccessKeys.RemoveObject(aElement);
}

uint32_t EventStateManager::GetRegisteredAccessKey(Element* aElement) {
  MOZ_ASSERT(aElement);

  if (mAccessKeys.IndexOf(aElement) == -1) return 0;

  nsAutoString accessKey;
  aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, accessKey);
  return accessKey.First();
}

void EventStateManager::EnsureDocument(nsPresContext* aPresContext) {
  if (!mDocument) mDocument = aPresContext->Document();
}

void EventStateManager::FlushLayout(nsPresContext* aPresContext) {
  MOZ_ASSERT(aPresContext, "nullptr ptr");
  if (RefPtr<PresShell> presShell = aPresContext->GetPresShell()) {
    presShell->FlushPendingNotifications(FlushType::InterruptibleLayout);
  }
}

nsIContent* EventStateManager::GetFocusedContent() {
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  EnsureDocument(mPresContext);
  if (!fm || !mDocument) return nullptr;

  nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
  return nsFocusManager::GetFocusedDescendant(
      mDocument->GetWindow(), nsFocusManager::eOnlyCurrentWindow,
      getter_AddRefs(focusedWindow));
}

//-------------------------------------------------------
// Return true if the docshell is visible

bool EventStateManager::IsShellVisible(nsIDocShell* aShell) {
  NS_ASSERTION(aShell, "docshell is null");

  nsCOMPtr<nsIBaseWindow> basewin = do_QueryInterface(aShell);
  if (!basewin) return true;

  bool isVisible = true;
  basewin->GetVisibility(&isVisible);

  // We should be doing some additional checks here so that
  // we don't tab into hidden tabs of tabbrowser.  -bryner

  return isVisible;
}

nsresult EventStateManager::DoContentCommandEvent(
    WidgetContentCommandEvent* aEvent) {
  EnsureDocument(mPresContext);
  NS_ENSURE_TRUE(mDocument, NS_ERROR_FAILURE);
  nsCOMPtr<nsPIDOMWindowOuter> window(mDocument->GetWindow());
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsCOMPtr<nsPIWindowRoot> root = window->GetTopWindowRoot();
  NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);
  const char* cmd;
  switch (aEvent->mMessage) {
    case eContentCommandCut:
      cmd = "cmd_cut";
      break;
    case eContentCommandCopy:
      cmd = "cmd_copy";
      break;
    case eContentCommandPaste:
      cmd = "cmd_paste";
      break;
    case eContentCommandDelete:
      cmd = "cmd_delete";
      break;
    case eContentCommandUndo:
      cmd = "cmd_undo";
      break;
    case eContentCommandRedo:
      cmd = "cmd_redo";
      break;
    case eContentCommandPasteTransferable:
      cmd = "cmd_pasteTransferable";
      break;
    case eContentCommandLookUpDictionary:
      cmd = "cmd_lookUpDictionary";
      break;
    default:
      return NS_ERROR_NOT_IMPLEMENTED;
  }
  // If user tries to do something, user must try to do it in visible window.
  // So, let's retrieve controller of visible window.
  nsCOMPtr<nsIController> controller;
  nsresult rv =
      root->GetControllerForCommand(cmd, true, getter_AddRefs(controller));
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
        case eContentCommandPasteTransferable: {
          BrowserParent* remote = BrowserParent::GetFocused();
          if (remote) {
            nsCOMPtr<nsITransferable> transferable = aEvent->mTransferable;
            IPCDataTransfer ipcDataTransfer;
            nsContentUtils::TransferableToIPCTransferable(
                transferable, &ipcDataTransfer, false, nullptr,
                remote->Manager());
            bool isPrivateData = transferable->GetIsPrivateData();
            nsCOMPtr<nsIPrincipal> requestingPrincipal =
                transferable->GetRequestingPrincipal();
            nsContentPolicyType contentPolicyType =
                transferable->GetContentPolicyType();
            remote->SendPasteTransferable(ipcDataTransfer, isPrivateData,
                                          requestingPrincipal,
                                          contentPolicyType);
            rv = NS_OK;
          } else {
            nsCOMPtr<nsICommandController> commandController =
                do_QueryInterface(controller);
            NS_ENSURE_STATE(commandController);

            RefPtr<nsCommandParams> params = new nsCommandParams();
            rv = params->SetISupports("transferable", aEvent->mTransferable);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              return rv;
            }
            rv = commandController->DoCommandWithParams(cmd, params);
          }
          break;
        }

        case eContentCommandLookUpDictionary: {
          nsCOMPtr<nsICommandController> commandController =
              do_QueryInterface(controller);
          if (NS_WARN_IF(!commandController)) {
            return NS_ERROR_FAILURE;
          }

          RefPtr<nsCommandParams> params = new nsCommandParams();
          rv = params->SetInt("x", aEvent->mRefPoint.x);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }

          rv = params->SetInt("y", aEvent->mRefPoint.y);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }

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

nsresult EventStateManager::DoContentCommandScrollEvent(
    WidgetContentCommandEvent* aEvent) {
  NS_ENSURE_TRUE(mPresContext, NS_ERROR_NOT_AVAILABLE);
  PresShell* presShell = mPresContext->GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(aEvent->mScroll.mAmount != 0, NS_ERROR_INVALID_ARG);

  ScrollUnit scrollUnit;
  switch (aEvent->mScroll.mUnit) {
    case WidgetContentCommandEvent::eCmdScrollUnit_Line:
      scrollUnit = ScrollUnit::LINES;
      break;
    case WidgetContentCommandEvent::eCmdScrollUnit_Page:
      scrollUnit = ScrollUnit::PAGES;
      break;
    case WidgetContentCommandEvent::eCmdScrollUnit_Whole:
      scrollUnit = ScrollUnit::WHOLE;
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }

  aEvent->mSucceeded = true;

  nsIScrollableFrame* sf =
      presShell->GetScrollableFrameToScroll(ScrollableDirection::Either);
  aEvent->mIsEnabled =
      sf ? (aEvent->mScroll.mIsHorizontal ? WheelHandlingUtils::CanScrollOn(
                                                sf, aEvent->mScroll.mAmount, 0)
                                          : WheelHandlingUtils::CanScrollOn(
                                                sf, 0, aEvent->mScroll.mAmount))
         : false;

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
  sf->ScrollBy(pt, scrollUnit, ScrollMode::Instant);
  return NS_OK;
}

void EventStateManager::SetActiveManager(EventStateManager* aNewESM,
                                         nsIContent* aContent) {
  if (sActiveESM && aNewESM != sActiveESM) {
    sActiveESM->SetContentState(nullptr, NS_EVENT_STATE_ACTIVE);
  }
  sActiveESM = aNewESM;
  if (sActiveESM && aContent) {
    sActiveESM->SetContentState(aContent, NS_EVENT_STATE_ACTIVE);
  }
}

void EventStateManager::ClearGlobalActiveContent(EventStateManager* aClearer) {
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

void EventStateManager::DeltaAccumulator::InitLineOrPageDelta(
    nsIFrame* aTargetFrame, EventStateManager* aESM, WidgetWheelEvent* aEvent) {
  MOZ_ASSERT(aESM);
  MOZ_ASSERT(aEvent);

  // Reset if the previous wheel event is too old.
  if (!mLastTime.IsNull()) {
    TimeDuration duration = TimeStamp::Now() - mLastTime;
    if (duration.ToMilliseconds() >
        StaticPrefs::mousewheel_transaction_timeout()) {
      Reset();
    }
  }
  // If we have accumulated delta,  we may need to reset it.
  if (IsInTransaction()) {
    // If wheel event type is changed, reset the values.
    if (mHandlingDeltaMode != aEvent->mDeltaMode ||
        mIsNoLineOrPageDeltaDevice != aEvent->mIsNoLineOrPageDelta) {
      Reset();
    } else {
      // If the delta direction is changed, we should reset only the
      // accumulated values.
      if (mX && aEvent->mDeltaX && ((aEvent->mDeltaX > 0.0) != (mX > 0.0))) {
        mX = mPendingScrollAmountX = 0.0;
      }
      if (mY && aEvent->mDeltaY && ((aEvent->mDeltaY > 0.0) != (mY > 0.0))) {
        mY = mPendingScrollAmountY = 0.0;
      }
    }
  }

  mHandlingDeltaMode = aEvent->mDeltaMode;
  mIsNoLineOrPageDeltaDevice = aEvent->mIsNoLineOrPageDelta;

  // If it's handling neither a device that does not provide line or page deltas
  // nor delta values multiplied by prefs, we must not modify lineOrPageDelta
  // values.
  if (!mIsNoLineOrPageDeltaDevice &&
      !EventStateManager::WheelPrefs::GetInstance()
           ->NeedToComputeLineOrPageDelta(aEvent)) {
    // Set the delta values to mX and mY.  They would be used when above block
    // resets mX/mY/mPendingScrollAmountX/mPendingScrollAmountY if the direction
    // is changed.
    // NOTE: We shouldn't accumulate the delta values, it might could cause
    //       overflow even though it's not a realistic situation.
    if (aEvent->mDeltaX) {
      mX = aEvent->mDeltaX;
    }
    if (aEvent->mDeltaY) {
      mY = aEvent->mDeltaY;
    }
    mLastTime = TimeStamp::Now();
    return;
  }

  mX += aEvent->mDeltaX;
  mY += aEvent->mDeltaY;

  if (mHandlingDeltaMode == WheelEvent_Binding::DOM_DELTA_PIXEL) {
    // Records pixel delta values and init mLineOrPageDeltaX and
    // mLineOrPageDeltaY for wheel events which are caused by pixel only
    // devices.  Ignore mouse wheel transaction for computing this.  The
    // lineOrPageDelta values will be used by dispatching legacy
    // eMouseScrollEventClass (DOMMouseScroll) but not be used for scrolling
    // of default action.  The transaction should be used only for the default
    // action.
    nsIFrame* frame = aESM->ComputeScrollTarget(
        aTargetFrame, aEvent, COMPUTE_LEGACY_MOUSE_SCROLL_EVENT_TARGET);
    nsPresContext* pc =
        frame ? frame->PresContext() : aTargetFrame->PresContext();
    nsIScrollableFrame* scrollTarget = do_QueryFrame(frame);
    nsSize scrollAmount = aESM->GetScrollAmount(pc, aEvent, scrollTarget);
    nsIntSize scrollAmountInCSSPixels(
        nsPresContext::AppUnitsToIntCSSPixels(scrollAmount.width),
        nsPresContext::AppUnitsToIntCSSPixels(scrollAmount.height));

    aEvent->mLineOrPageDeltaX = RoundDown(mX) / scrollAmountInCSSPixels.width;
    aEvent->mLineOrPageDeltaY = RoundDown(mY) / scrollAmountInCSSPixels.height;

    mX -= aEvent->mLineOrPageDeltaX * scrollAmountInCSSPixels.width;
    mY -= aEvent->mLineOrPageDeltaY * scrollAmountInCSSPixels.height;
  } else {
    aEvent->mLineOrPageDeltaX = RoundDown(mX);
    aEvent->mLineOrPageDeltaY = RoundDown(mY);
    mX -= aEvent->mLineOrPageDeltaX;
    mY -= aEvent->mLineOrPageDeltaY;
  }

  mLastTime = TimeStamp::Now();
}

void EventStateManager::DeltaAccumulator::Reset() {
  mX = mY = 0.0;
  mPendingScrollAmountX = mPendingScrollAmountY = 0.0;
  mHandlingDeltaMode = UINT32_MAX;
  mIsNoLineOrPageDeltaDevice = false;
}

nsIntPoint
EventStateManager::DeltaAccumulator::ComputeScrollAmountForDefaultAction(
    WidgetWheelEvent* aEvent, const nsIntSize& aScrollAmountInDevPixels) {
  MOZ_ASSERT(aEvent);

  // If the wheel event is line scroll and the delta value is computed from
  // system settings, allow to override the system speed.
  bool allowScrollSpeedOverride =
      (!aEvent->mCustomizedByUserPrefs &&
       aEvent->mDeltaMode == WheelEvent_Binding::DOM_DELTA_LINE);
  DeltaValues acceleratedDelta =
      WheelTransaction::AccelerateWheelDelta(aEvent, allowScrollSpeedOverride);

  nsIntPoint result(0, 0);
  if (aEvent->mDeltaMode == WheelEvent_Binding::DOM_DELTA_PIXEL) {
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
EventStateManager::WheelPrefs* EventStateManager::WheelPrefs::GetInstance() {
  if (!sInstance) {
    sInstance = new WheelPrefs();
  }
  return sInstance;
}

// static
void EventStateManager::WheelPrefs::Shutdown() {
  delete sInstance;
  sInstance = nullptr;
}

// static
void EventStateManager::WheelPrefs::OnPrefChanged(const char* aPrefName,
                                                  void* aClosure) {
  // forget all prefs, it's not problem for performance.
  sInstance->Reset();
  DeltaAccumulator::GetInstance()->Reset();
}

EventStateManager::WheelPrefs::WheelPrefs() {
  Reset();
  Preferences::RegisterPrefixCallback(OnPrefChanged, "mousewheel.");
  Preferences::AddBoolVarCache(&sWheelEventsEnabledOnPlugins,
                               "plugin.mousewheel.enabled", true);
  Preferences::AddBoolVarCache(&sIsAutoDirEnabled, "mousewheel.autodir.enabled",
                               true);
  Preferences::AddBoolVarCache(&sHonoursRootForAutoDir,
                               "mousewheel.autodir.honourroot", false);
}

EventStateManager::WheelPrefs::~WheelPrefs() {
  Preferences::UnregisterPrefixCallback(OnPrefChanged, "mousewheel.");
}

void EventStateManager::WheelPrefs::Reset() { memset(mInit, 0, sizeof(mInit)); }

EventStateManager::WheelPrefs::Index EventStateManager::WheelPrefs::GetIndexFor(
    const WidgetWheelEvent* aEvent) {
  if (!aEvent) {
    return INDEX_DEFAULT;
  }

  Modifiers modifiers =
      (aEvent->mModifiers & (MODIFIER_ALT | MODIFIER_CONTROL | MODIFIER_META |
                             MODIFIER_SHIFT | MODIFIER_OS));

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

void EventStateManager::WheelPrefs::GetBasePrefName(
    EventStateManager::WheelPrefs::Index aIndex, nsACString& aBasePrefName) {
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

void EventStateManager::WheelPrefs::Init(
    EventStateManager::WheelPrefs::Index aIndex) {
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
  // Note that ACTION_HORIZONTALIZED_SCROLL isn't a valid value for this pref
  // because it affects only to deltaY.
  prefNameAction.AppendLiteral(".override_x");
  int32_t actionOverrideX = Preferences::GetInt(prefNameAction.get(), -1);
  if (actionOverrideX < -1 || actionOverrideX > int32_t(ACTION_LAST) ||
      actionOverrideX == ACTION_HORIZONTALIZED_SCROLL) {
    NS_WARNING("Unsupported action override pref value, didn't override.");
    actionOverrideX = -1;
  }
  mOverriddenActionsX[aIndex] = (actionOverrideX == -1)
                                    ? static_cast<Action>(action)
                                    : static_cast<Action>(actionOverrideX);
}

void EventStateManager::WheelPrefs::GetMultiplierForDeltaXAndY(
    const WidgetWheelEvent* aEvent, Index aIndex, double* aMultiplierForDeltaX,
    double* aMultiplierForDeltaY) {
  *aMultiplierForDeltaX = mMultiplierX[aIndex];
  *aMultiplierForDeltaY = mMultiplierY[aIndex];
  // If the event has been horizontalized(I.e. treated as a horizontal wheel
  // scroll for a vertical wheel scroll), then we should swap mMultiplierX and
  // mMultiplierY. By doing this, multipliers will still apply to the delta
  // values they origianlly corresponded to.
  if (aEvent->mDeltaValuesHorizontalizedForDefaultHandler &&
      ComputeActionFor(aEvent) == ACTION_HORIZONTALIZED_SCROLL) {
    std::swap(*aMultiplierForDeltaX, *aMultiplierForDeltaY);
  }
}

void EventStateManager::WheelPrefs::ApplyUserPrefsToDelta(
    WidgetWheelEvent* aEvent) {
  if (aEvent->mCustomizedByUserPrefs) {
    return;
  }

  Index index = GetIndexFor(aEvent);
  Init(index);

  double multiplierForDeltaX = 1.0, multiplierForDeltaY = 1.0;
  GetMultiplierForDeltaXAndY(aEvent, index, &multiplierForDeltaX,
                             &multiplierForDeltaY);
  aEvent->mDeltaX *= multiplierForDeltaX;
  aEvent->mDeltaY *= multiplierForDeltaY;
  aEvent->mDeltaZ *= mMultiplierZ[index];

  // If the multiplier is 1.0 or -1.0, i.e., it doesn't change the absolute
  // value, we should use lineOrPageDelta values which were set by widget.
  // Otherwise, we need to compute them from accumulated delta values.
  if (!NeedToComputeLineOrPageDelta(aEvent)) {
    aEvent->mLineOrPageDeltaX *= static_cast<int32_t>(multiplierForDeltaX);
    aEvent->mLineOrPageDeltaY *= static_cast<int32_t>(multiplierForDeltaY);
  } else {
    aEvent->mLineOrPageDeltaX = 0;
    aEvent->mLineOrPageDeltaY = 0;
  }

  aEvent->mCustomizedByUserPrefs =
      ((mMultiplierX[index] != 1.0) || (mMultiplierY[index] != 1.0) ||
       (mMultiplierZ[index] != 1.0));
}

void EventStateManager::WheelPrefs::CancelApplyingUserPrefsFromOverflowDelta(
    WidgetWheelEvent* aEvent) {
  Index index = GetIndexFor(aEvent);
  Init(index);

  // XXX If the multiplier pref value is negative, the scroll direction was
  //     changed and caused to scroll different direction.  In such case,
  //     this method reverts the sign of overflowDelta.  Does it make widget
  //     happy?  Although, widget can know the pref applied delta values by
  //     referrencing the deltaX and deltaY of the event.

  double multiplierForDeltaX = 1.0, multiplierForDeltaY = 1.0;
  GetMultiplierForDeltaXAndY(aEvent, index, &multiplierForDeltaX,
                             &multiplierForDeltaY);
  if (multiplierForDeltaX) {
    aEvent->mOverflowDeltaX /= multiplierForDeltaX;
  }
  if (multiplierForDeltaY) {
    aEvent->mOverflowDeltaY /= multiplierForDeltaY;
  }
}

EventStateManager::WheelPrefs::Action
EventStateManager::WheelPrefs::ComputeActionFor(
    const WidgetWheelEvent* aEvent) {
  Index index = GetIndexFor(aEvent);
  Init(index);

  bool deltaXPreferred = (Abs(aEvent->mDeltaX) > Abs(aEvent->mDeltaY) &&
                          Abs(aEvent->mDeltaX) > Abs(aEvent->mDeltaZ));
  Action* actions = deltaXPreferred ? mOverriddenActionsX : mActions;
  if (actions[index] == ACTION_NONE || actions[index] == ACTION_SCROLL ||
      actions[index] == ACTION_HORIZONTALIZED_SCROLL) {
    return actions[index];
  }

  // Momentum events shouldn't run special actions.
  if (aEvent->mIsMomentum) {
    // Use the default action.  Note that user might kill the wheel scrolling.
    Init(INDEX_DEFAULT);
    if (actions[INDEX_DEFAULT] == ACTION_SCROLL ||
        actions[INDEX_DEFAULT] == ACTION_HORIZONTALIZED_SCROLL) {
      return actions[INDEX_DEFAULT];
    }
    return ACTION_NONE;
  }

  return actions[index];
}

bool EventStateManager::WheelPrefs::NeedToComputeLineOrPageDelta(
    const WidgetWheelEvent* aEvent) {
  Index index = GetIndexFor(aEvent);
  Init(index);

  return (mMultiplierX[index] != 1.0 && mMultiplierX[index] != -1.0) ||
         (mMultiplierY[index] != 1.0 && mMultiplierY[index] != -1.0);
}

void EventStateManager::WheelPrefs::GetUserPrefsForEvent(
    const WidgetWheelEvent* aEvent, double* aOutMultiplierX,
    double* aOutMultiplierY) {
  Index index = GetIndexFor(aEvent);
  Init(index);

  double multiplierForDeltaX = 1.0, multiplierForDeltaY = 1.0;
  GetMultiplierForDeltaXAndY(aEvent, index, &multiplierForDeltaX,
                             &multiplierForDeltaY);
  *aOutMultiplierX = multiplierForDeltaX;
  *aOutMultiplierY = multiplierForDeltaY;
}

// static
bool EventStateManager::WheelPrefs::WheelEventsEnabledOnPlugins() {
  if (!sInstance) {
    GetInstance();  // initializing sWheelEventsEnabledOnPlugins
  }
  return sWheelEventsEnabledOnPlugins;
}

// static
bool EventStateManager::WheelPrefs::IsAutoDirEnabled() {
  if (!sInstance) {
    GetInstance();  // initializing sIsAutoDirEnabled
  }
  return sIsAutoDirEnabled;
}

// static
bool EventStateManager::WheelPrefs::HonoursRootForAutoDir() {
  if (!sInstance) {
    GetInstance();  // initializing sHonoursRootForAutoDir
  }
  return sHonoursRootForAutoDir;
}

// static
Maybe<layers::APZWheelAction> EventStateManager::APZWheelActionFor(
    const WidgetWheelEvent* aEvent) {
  if (aEvent->mMessage != eWheel) {
    return Nothing();
  }
  WheelPrefs::Action action =
      WheelPrefs::GetInstance()->ComputeActionFor(aEvent);
  switch (action) {
    case WheelPrefs::ACTION_SCROLL:
    case WheelPrefs::ACTION_HORIZONTALIZED_SCROLL:
      return Some(layers::APZWheelAction::Scroll);
    case WheelPrefs::ACTION_PINCH_ZOOM:
      return Some(layers::APZWheelAction::PinchZoom);
    default:
      return Nothing();
  }
}

// static
WheelDeltaAdjustmentStrategy EventStateManager::GetWheelDeltaAdjustmentStrategy(
    const WidgetWheelEvent& aEvent) {
  if (aEvent.mMessage != eWheel) {
    return WheelDeltaAdjustmentStrategy::eNone;
  }
  switch (WheelPrefs::GetInstance()->ComputeActionFor(&aEvent)) {
    case WheelPrefs::ACTION_SCROLL:
      if (WheelPrefs::IsAutoDirEnabled() && 0 == aEvent.mDeltaZ) {
        if (WheelPrefs::HonoursRootForAutoDir()) {
          return WheelDeltaAdjustmentStrategy::eAutoDirWithRootHonour;
        }
        return WheelDeltaAdjustmentStrategy::eAutoDir;
      }
      return WheelDeltaAdjustmentStrategy::eNone;
    case WheelPrefs::ACTION_HORIZONTALIZED_SCROLL:
      return WheelDeltaAdjustmentStrategy::eHorizontalize;
    default:
      break;
  }
  return WheelDeltaAdjustmentStrategy::eNone;
}

void EventStateManager::GetUserPrefsForWheelEvent(
    const WidgetWheelEvent* aEvent, double* aOutMultiplierX,
    double* aOutMultiplierY) {
  WheelPrefs::GetInstance()->GetUserPrefsForEvent(aEvent, aOutMultiplierX,
                                                  aOutMultiplierY);
}

bool EventStateManager::WheelPrefs::IsOverOnePageScrollAllowedX(
    const WidgetWheelEvent* aEvent) {
  Index index = GetIndexFor(aEvent);
  Init(index);
  return Abs(mMultiplierX[index]) >=
         MIN_MULTIPLIER_VALUE_ALLOWING_OVER_ONE_PAGE_SCROLL;
}

bool EventStateManager::WheelPrefs::IsOverOnePageScrollAllowedY(
    const WidgetWheelEvent* aEvent) {
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

// static
void EventStateManager::Prefs::Init() {
  static bool sPrefsAlreadyCached = false;
  if (sPrefsAlreadyCached) {
    return;
  }

  Preferences::AddBoolVarCache(&sKeyCausesActivation,
                               "accessibility.accesskeycausesactivation",
                               sKeyCausesActivation);
  Preferences::AddBoolVarCache(&sClickHoldContextMenu,
                               "ui.click_hold_context_menus",
                               sClickHoldContextMenu);
  sPrefsAlreadyCached = true;
}

}  // namespace mozilla
