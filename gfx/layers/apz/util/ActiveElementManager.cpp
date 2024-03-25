/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ActiveElementManager.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_ui.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Document.h"
#include "mozilla/layers/APZEventState.h"
#include "mozilla/layers/APZUtils.h"
#include "nsITimer.h"

static mozilla::LazyLogModule sApzAemLog("apz.activeelement");
#define AEM_LOG(...) MOZ_LOG(sApzAemLog, LogLevel::Debug, (__VA_ARGS__))

namespace mozilla {
namespace layers {

class DelayedClearElementActivation final : public nsITimerCallback,
                                            public nsINamed {
 private:
  explicit DelayedClearElementActivation(RefPtr<dom::Element>& aTarget,
                                         const nsCOMPtr<nsITimer>& aTimer)
      : mTarget(aTarget)
        // Hold the reference count until we are called back.
        ,
        mTimer(aTimer),
        mProcessedSingleTap(false) {}

 public:
  NS_DECL_ISUPPORTS

  static RefPtr<DelayedClearElementActivation> Create(
      RefPtr<dom::Element>& aTarget);

  NS_IMETHOD Notify(nsITimer*) override;

  NS_IMETHOD GetName(nsACString& aName) override;

  void MarkSingleTapProcessed();

  bool ProcessedSingleTap() const { return mProcessedSingleTap; }

  void StartTimer();

  /**
   * Clear the Event State Manager's global active content.
   */
  void ClearGlobalActiveContent();

  void ClearTimer() {
    if (mTimer) {
      mTimer->Cancel();
      mTimer = nullptr;
    }
  }
  dom::Element* GetTarget() const { return mTarget; }

 private:
  ~DelayedClearElementActivation() = default;

  RefPtr<dom::Element> mTarget;
  nsCOMPtr<nsITimer> mTimer;
  bool mProcessedSingleTap;
};

static nsPresContext* GetPresContextFor(nsIContent* aContent) {
  if (!aContent) {
    return nullptr;
  }
  PresShell* presShell = aContent->OwnerDoc()->GetPresShell();
  if (!presShell) {
    return nullptr;
  }
  return presShell->GetPresContext();
}

RefPtr<DelayedClearElementActivation> DelayedClearElementActivation::Create(
    RefPtr<dom::Element>& aTarget) {
  nsCOMPtr<nsITimer> timer = NS_NewTimer();
  if (!timer) {
    return nullptr;
  }
  RefPtr<DelayedClearElementActivation> event =
      new DelayedClearElementActivation(aTarget, timer);
  return event;
}

NS_IMETHODIMP DelayedClearElementActivation::Notify(nsITimer*) {
  AEM_LOG("DelayedClearElementActivation notification ready=%d",
          mProcessedSingleTap);
  // If the single tap has been processed and the timer has expired,
  // clear the active element state.
  if (mProcessedSingleTap) {
    AEM_LOG("DelayedClearElementActivation clearing active content\n");
    ClearGlobalActiveContent();
  }
  mTimer = nullptr;
  return NS_OK;
}

NS_IMETHODIMP DelayedClearElementActivation::GetName(nsACString& aName) {
  aName.AssignLiteral("DelayedClearElementActivation");
  return NS_OK;
}

void DelayedClearElementActivation::StartTimer() {
  MOZ_ASSERT(mTimer);
  // If the timer is null, active content state has already been cleared.
  if (!mTimer) {
    return;
  }
  nsresult rv = mTimer->InitWithCallback(
      this, StaticPrefs::ui_touch_activation_duration_ms(),
      nsITimer::TYPE_ONE_SHOT);
  if (NS_FAILED(rv)) {
    ClearTimer();
  }
}

void DelayedClearElementActivation::MarkSingleTapProcessed() {
  mProcessedSingleTap = true;
  if (!mTimer) {
    AEM_LOG("Clear activation immediate!");
    ClearGlobalActiveContent();
  }
}

void DelayedClearElementActivation::ClearGlobalActiveContent() {
  if (nsPresContext* pc = GetPresContextFor(mTarget)) {
    EventStateManager::ClearGlobalActiveContent(pc->EventStateManager());
  }
  mTarget = nullptr;
}

NS_IMPL_ISUPPORTS(DelayedClearElementActivation, nsITimerCallback, nsINamed)

ActiveElementManager::ActiveElementManager()
    : mCanBePanOrZoom(false),
      mCanBePanOrZoomSet(false),
      mSingleTapBeforeActivation(false),
      mSingleTapState(apz::SingleTapState::NotClick),
      mSetActiveTask(nullptr) {}

ActiveElementManager::~ActiveElementManager() = default;

void ActiveElementManager::SetTargetElement(dom::EventTarget* aTarget) {
  if (mTarget) {
    // Multiple fingers on screen (since HandleTouchEnd clears mTarget).
    AEM_LOG("Multiple fingers on-screen, clearing target element\n");
    CancelTask();
    ResetActive();
    ResetTouchBlockState();
    return;
  }

  mTarget = dom::Element::FromEventTargetOrNull(aTarget);
  AEM_LOG("Setting target element to %p\n", mTarget.get());
  TriggerElementActivation();
}

void ActiveElementManager::HandleTouchStart(bool aCanBePanOrZoom) {
  AEM_LOG("Touch start, aCanBePanOrZoom: %d\n", aCanBePanOrZoom);
  if (mCanBePanOrZoomSet) {
    // Multiple fingers on screen (since HandleTouchEnd clears mCanBePanSet).
    AEM_LOG("Multiple fingers on-screen, clearing touch block state\n");
    CancelTask();
    ResetActive();
    ResetTouchBlockState();
    return;
  }

  mCanBePanOrZoom = aCanBePanOrZoom;
  mCanBePanOrZoomSet = true;
  TriggerElementActivation();
}

void ActiveElementManager::TriggerElementActivation() {
  // Reset mSingleTapState here either when HandleTouchStart() or
  // SetTargetElement() gets called.
  // NOTE: It's possible that ProcessSingleTap() gets called in between
  // HandleTouchStart() and SetTargetElement() calls. I.e.,
  // mSingleTapBeforeActivation is true, in such cases it doesn't matter that
  // mSingleTapState was reset once and referred it in ProcessSingleTap() and
  // then reset here again because in ProcessSingleTap() `NotYetDetermined` is
  // the only one state we need to care, and it should NOT happen in the
  // scenario. In other words the case where we need to care `NotYetDetermined`
  // is when ProcessSingleTap() gets called later than any other events and
  // notifications.
  mSingleTapState = apz::SingleTapState::NotClick;

  // Both HandleTouchStart() and SetTargetElement() call this. They can be
  // called in either order. One will set mCanBePanOrZoomSet, and the other,
  // mTarget. We want to actually trigger the activation once both are set.
  if (!(mTarget && mCanBePanOrZoomSet)) {
    return;
  }

  RefPtr<DelayedClearElementActivation> delayedEvent =
      DelayedClearElementActivation::Create(mTarget);
  if (mDelayedClearElementActivation) {
    mDelayedClearElementActivation->ClearTimer();
    mDelayedClearElementActivation->ClearGlobalActiveContent();
  }
  mDelayedClearElementActivation = delayedEvent;

  // If the touch cannot be a pan, make mTarget :active right away.
  // Otherwise, wait a bit to see if the user will pan or not.
  if (!mCanBePanOrZoom) {
    SetActive(mTarget);

    if (mDelayedClearElementActivation) {
      if (mSingleTapBeforeActivation) {
        mDelayedClearElementActivation->MarkSingleTapProcessed();
      }
      mDelayedClearElementActivation->StartTimer();
    }
  } else {
    CancelTask();  // this is only needed because of bug 1169802. Fixing that
                   // bug properly should make this unnecessary.
    MOZ_ASSERT(mSetActiveTask == nullptr);

    RefPtr<CancelableRunnable> task =
        NewCancelableRunnableMethod<nsCOMPtr<dom::Element>>(
            "layers::ActiveElementManager::SetActiveTask", this,
            &ActiveElementManager::SetActiveTask, mTarget);
    mSetActiveTask = task;
    NS_GetCurrentThread()->DelayedDispatch(
        task.forget(), StaticPrefs::ui_touch_activation_delay_ms());
    AEM_LOG("Scheduling mSetActiveTask %p\n", mSetActiveTask.get());
  }
  AEM_LOG(
      "Got both touch-end event and end touch notiication, clearing pan "
      "state\n");
  mCanBePanOrZoomSet = false;
}

void ActiveElementManager::ClearActivation() {
  AEM_LOG("Clearing element activation\n");
  CancelTask();
  ResetActive();
}

bool ActiveElementManager::HandleTouchEndEvent(apz::SingleTapState aState) {
  AEM_LOG("Touch end event, state: %hhu\n", static_cast<uint8_t>(aState));

  mTouchEndState += TouchEndState::GotTouchEndEvent;
  return MaybeChangeActiveState(aState);
}

bool ActiveElementManager::HandleTouchEnd(apz::SingleTapState aState) {
  AEM_LOG("Touch end\n");

  mTouchEndState += TouchEndState::GotTouchEndNotification;
  return MaybeChangeActiveState(aState);
}

bool ActiveElementManager::MaybeChangeActiveState(apz::SingleTapState aState) {
  if (mTouchEndState !=
      TouchEndStates(TouchEndState::GotTouchEndEvent,
                     TouchEndState::GotTouchEndNotification)) {
    return false;
  }

  CancelTask();

  mSingleTapState = aState;

  if (aState == apz::SingleTapState::WasClick) {
    // Scrollbar thumbs use a different mechanism for their active
    // highlight (the "active" attribute), so don't set the active state
    // on them because nothing will clear it.
    if (mCanBePanOrZoom &&
        !(mTarget && mTarget->IsXULElement(nsGkAtoms::thumb))) {
      SetActive(mTarget);
    }
  } else {
    // We might reach here if mCanBePanOrZoom was false on touch-start and
    // so we set the element active right away. Now it turns out the
    // action was not a click so we need to reset the active element.
    ResetActive();
  }

  ResetTouchBlockState();
  return true;
}

void ActiveElementManager::ProcessSingleTap() {
  if (!mDelayedClearElementActivation) {
    // We have not received touch-start notification yet. We will have to run
    // MarkSingleTapProcessed() when we receive the touch-start notification.
    mSingleTapBeforeActivation = true;
    return;
  }

  if (mSingleTapState == apz::SingleTapState::NotYetDetermined) {
    // If we got `NotYetDetermined`, which means at the moment we don't know for
    // sure whether double-tapping will be incoming or not, but now we are sure
    // that no double-tapping will happen, thus it's time to activate the target
    // element.
    if (auto* target = mDelayedClearElementActivation->GetTarget()) {
      SetActive(target);
    }
  }
  mDelayedClearElementActivation->MarkSingleTapProcessed();

  if (mCanBePanOrZoom) {
    // In the case that we have not started the delayed reset of the element
    // activation state, start the timer now.
    mDelayedClearElementActivation->StartTimer();
  }

  // We don't need to keep a reference to the element activation
  // clearing, because the event and its timer keep each other alive
  // until the timer expires
  mDelayedClearElementActivation = nullptr;
}

void ActiveElementManager::Destroy() {
  if (mDelayedClearElementActivation) {
    mDelayedClearElementActivation->ClearTimer();
    mDelayedClearElementActivation = nullptr;
  }
}

void ActiveElementManager::SetActive(dom::Element* aTarget) {
  AEM_LOG("Setting active %p\n", aTarget);

  if (nsPresContext* pc = GetPresContextFor(aTarget)) {
    pc->EventStateManager()->SetContentState(aTarget,
                                             dom::ElementState::ACTIVE);
  }
}

void ActiveElementManager::ResetActive() {
  AEM_LOG("Resetting active from %p\n", mTarget.get());

  // Clear the :active flag from mTarget by setting it on the document root.
  if (mTarget) {
    dom::Element* root = mTarget->OwnerDoc()->GetDocumentElement();
    if (root) {
      AEM_LOG("Found root %p, making active\n", root);
      SetActive(root);
    }
  }
}

void ActiveElementManager::ResetTouchBlockState() {
  mTarget = nullptr;
  mCanBePanOrZoomSet = false;
  mTouchEndState.clear();
  mSingleTapBeforeActivation = false;
  // NOTE: Do not reset mSingleTapState here since it will be necessary in
  // ProcessSingleTap() to tell whether we need to activate the target element
  // because on environments where double-tap is enabled ProcessSingleTap()
  // gets called after both of touch-end event and end touch notiication
  // arrived.
}

void ActiveElementManager::SetActiveTask(
    const nsCOMPtr<dom::Element>& aTarget) {
  AEM_LOG("mSetActiveTask %p running\n", mSetActiveTask.get());

  // This gets called from mSetActiveTask's Run() method. The message loop
  // deletes the task right after running it, so we need to null out
  // mSetActiveTask to make sure we're not left with a dangling pointer.
  mSetActiveTask = nullptr;
  SetActive(aTarget);
}

void ActiveElementManager::CancelTask() {
  AEM_LOG("Cancelling task %p\n", mSetActiveTask.get());

  if (mSetActiveTask) {
    mSetActiveTask->Cancel();
    mSetActiveTask = nullptr;
  }
}

}  // namespace layers
}  // namespace mozilla
