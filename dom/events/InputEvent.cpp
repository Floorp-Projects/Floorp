/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/InputEvent.h"
#include "mozilla/TextEvents.h"
#include "mozilla/StaticPrefs_dom.h"
#include "prtime.h"

namespace mozilla::dom {

InputEvent::InputEvent(EventTarget* aOwner, nsPresContext* aPresContext,
                       InternalEditorInputEvent* aEvent)
    : UIEvent(aOwner, aPresContext,
              aEvent
                  ? aEvent
                  : new InternalEditorInputEvent(false, eVoidEvent, nullptr)) {
  NS_ASSERTION(mEvent->mClass == eEditorInputEventClass, "event type mismatch");

  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
    mEvent->mTime = PR_Now();
  }
}

void InputEvent::GetData(nsAString& aData, CallerType aCallerType) {
  InternalEditorInputEvent* editorInputEvent = mEvent->AsEditorInputEvent();
  MOZ_ASSERT(editorInputEvent);
  // If clipboard event is disabled, user may not want to leak clipboard
  // information via DOM events.  If so, we should return empty string instead.
  if (mEvent->IsTrusted() && aCallerType != CallerType::System &&
      !StaticPrefs::dom_event_clipboardevents_enabled() &&
      ExposesClipboardDataOrDataTransfer(editorInputEvent->mInputType)) {
    aData = editorInputEvent->mData.IsVoid() ? VoidString() : u""_ns;
    return;
  }
  aData = editorInputEvent->mData;
}

already_AddRefed<DataTransfer> InputEvent::GetDataTransfer(
    CallerType aCallerType) {
  InternalEditorInputEvent* editorInputEvent = mEvent->AsEditorInputEvent();
  MOZ_ASSERT(editorInputEvent);
  // If clipboard event is disabled, user may not want to leak clipboard
  // information via DOM events.  If so, we should return DataTransfer which
  // has empty string instead.  The reason why we make it have empty string is,
  // web apps may not expect that InputEvent.dataTransfer returns empty and
  // non-null DataTransfer instance.
  if (mEvent->IsTrusted() && aCallerType != CallerType::System &&
      !StaticPrefs::dom_event_clipboardevents_enabled() &&
      ExposesClipboardDataOrDataTransfer(editorInputEvent->mInputType)) {
    if (!editorInputEvent->mDataTransfer) {
      return nullptr;
    }
    return do_AddRef(
        new DataTransfer(editorInputEvent->mDataTransfer->GetParentObject(),
                         editorInputEvent->mMessage, u""_ns));
  }
  return do_AddRef(editorInputEvent->mDataTransfer);
}

void InputEvent::GetInputType(nsAString& aInputType) {
  InternalEditorInputEvent* editorInputEvent = mEvent->AsEditorInputEvent();
  MOZ_ASSERT(editorInputEvent);
  if (editorInputEvent->mInputType == EditorInputType::eUnknown) {
    aInputType = mInputTypeValue;
  } else {
    editorInputEvent->GetDOMInputTypeName(aInputType);
  }
}

void InputEvent::GetTargetRanges(nsTArray<RefPtr<StaticRange>>& aTargetRanges) {
  MOZ_ASSERT(aTargetRanges.IsEmpty());
  MOZ_ASSERT(mEvent->AsEditorInputEvent());
  aTargetRanges.AppendElements(mEvent->AsEditorInputEvent()->mTargetRanges);
}

bool InputEvent::IsComposing() {
  return mEvent->AsEditorInputEvent()->mIsComposing;
}

already_AddRefed<InputEvent> InputEvent::Constructor(
    const GlobalObject& aGlobal, const nsAString& aType,
    const InputEventInit& aParam) {
  nsCOMPtr<EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<InputEvent> e = new InputEvent(t, nullptr, nullptr);
  bool trusted = e->Init(t);
  e->InitUIEvent(aType, aParam.mBubbles, aParam.mCancelable, aParam.mView,
                 aParam.mDetail);
  InternalEditorInputEvent* internalEvent = e->mEvent->AsEditorInputEvent();
  internalEvent->mInputType =
      InternalEditorInputEvent::GetEditorInputType(aParam.mInputType);
  if (internalEvent->mInputType == EditorInputType::eUnknown) {
    e->mInputTypeValue = aParam.mInputType;
  }
  internalEvent->mData = aParam.mData;
  internalEvent->mDataTransfer = aParam.mDataTransfer;
  internalEvent->mTargetRanges = aParam.mTargetRanges;
  internalEvent->mIsComposing = aParam.mIsComposing;
  e->SetTrusted(trusted);
  e->SetComposed(aParam.mComposed);
  return e.forget();
}

}  // namespace mozilla::dom

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<InputEvent> NS_NewDOMInputEvent(
    EventTarget* aOwner, nsPresContext* aPresContext,
    InternalEditorInputEvent* aEvent) {
  RefPtr<InputEvent> it = new InputEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
