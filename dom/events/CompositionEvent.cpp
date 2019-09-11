/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CompositionEvent.h"
#include "mozilla/TextEvents.h"
#include "prtime.h"

namespace mozilla {
namespace dom {

CompositionEvent::CompositionEvent(EventTarget* aOwner,
                                   nsPresContext* aPresContext,
                                   WidgetCompositionEvent* aEvent)
    : UIEvent(aOwner, aPresContext,
              aEvent ? aEvent
                     : new WidgetCompositionEvent(false, eVoidEvent, nullptr)) {
  NS_ASSERTION(mEvent->mClass == eCompositionEventClass, "event type mismatch");

  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
    mEvent->mTime = PR_Now();

    // XXX compositionstart is cancelable in draft of DOM3 Events.
    //     However, it doesn't make sence for us, we cannot cancel composition
    //     when we sends compositionstart event.
    mEvent->mFlags.mCancelable = false;
  }

  // XXX Do we really need to duplicate the data value?
  mData = mEvent->AsCompositionEvent()->mData;
  // TODO: Native event should have locale information.
}

// static
already_AddRefed<CompositionEvent> CompositionEvent::Constructor(
    const GlobalObject& aGlobal, const nsAString& aType,
    const CompositionEventInit& aParam) {
  nsCOMPtr<EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<CompositionEvent> e = new CompositionEvent(t, nullptr, nullptr);
  bool trusted = e->Init(t);
  e->InitCompositionEvent(aType, aParam.mBubbles, aParam.mCancelable,
                          aParam.mView, aParam.mData, EmptyString());
  e->mDetail = aParam.mDetail;
  e->SetTrusted(trusted);
  e->SetComposed(aParam.mComposed);
  return e.forget();
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(CompositionEvent, UIEvent, mRanges)

NS_IMPL_ADDREF_INHERITED(CompositionEvent, UIEvent)
NS_IMPL_RELEASE_INHERITED(CompositionEvent, UIEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CompositionEvent)
NS_INTERFACE_MAP_END_INHERITING(UIEvent)

void CompositionEvent::GetData(nsAString& aData) const { aData = mData; }

void CompositionEvent::GetLocale(nsAString& aLocale) const {
  aLocale = mLocale;
}

void CompositionEvent::InitCompositionEvent(const nsAString& aType,
                                            bool aCanBubble, bool aCancelable,
                                            nsGlobalWindowInner* aView,
                                            const nsAString& aData,
                                            const nsAString& aLocale) {
  NS_ENSURE_TRUE_VOID(!mEvent->mFlags.mIsBeingDispatched);

  UIEvent::InitUIEvent(aType, aCanBubble, aCancelable, aView, 0);
  mData = aData;
  mLocale = aLocale;
}

void CompositionEvent::GetRanges(TextClauseArray& aRanges) {
  // If the mRanges is not empty, we return the cached value.
  if (!mRanges.IsEmpty()) {
    aRanges = mRanges;
    return;
  }
  RefPtr<TextRangeArray> textRangeArray = mEvent->AsCompositionEvent()->mRanges;
  if (!textRangeArray) {
    return;
  }
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(mOwner);
  const TextRange* targetRange = textRangeArray->GetTargetClause();
  for (size_t i = 0; i < textRangeArray->Length(); i++) {
    const TextRange& range = textRangeArray->ElementAt(i);
    mRanges.AppendElement(new TextClause(window, range, targetRange));
  }
  aRanges = mRanges;
}

}  // namespace dom
}  // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<CompositionEvent> NS_NewDOMCompositionEvent(
    EventTarget* aOwner, nsPresContext* aPresContext,
    WidgetCompositionEvent* aEvent) {
  RefPtr<CompositionEvent> event =
      new CompositionEvent(aOwner, aPresContext, aEvent);
  return event.forget();
}
