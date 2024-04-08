/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BasePrincipal.h"  // for nsIPrincipal::IsSystemPrincipal()
#include "mozilla/EventForwards.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/TextEvent.h"
#include "nsGlobalWindowInner.h"
#include "nsIPrincipal.h"
#include "nsPresContext.h"

namespace mozilla::dom {

TextEvent::TextEvent(EventTarget* aOwner, nsPresContext* aPresContext,
                     InternalLegacyTextEvent* aEvent)
    : UIEvent(aOwner, aPresContext,
              aEvent
                  ? aEvent
                  : new InternalLegacyTextEvent(false, eVoidEvent, nullptr)) {
  NS_ASSERTION(mEvent->mClass == eLegacyTextEventClass, "event type mismatch");
  mEventIsInternal = !aEvent;
}

void TextEvent::InitTextEvent(const nsAString& typeArg, bool canBubbleArg,
                              bool cancelableArg, nsGlobalWindowInner* viewArg,
                              const nsAString& dataArg) {
  if (NS_WARN_IF(mEvent->mFlags.mIsBeingDispatched)) {
    return;
  }

  UIEvent::InitUIEvent(typeArg, canBubbleArg, cancelableArg, viewArg, 0);

  static_cast<InternalLegacyTextEvent*>(mEvent)->mData = dataArg;
}

void TextEvent::GetData(nsAString& aData,
                        nsIPrincipal& aSubjectPrincipal) const {
  InternalLegacyTextEvent* textEvent = mEvent->AsLegacyTextEvent();
  MOZ_ASSERT(textEvent);
  if (mEvent->IsTrusted() && !aSubjectPrincipal.IsSystemPrincipal() &&
      !StaticPrefs::dom_event_clipboardevents_enabled() &&
      ExposesClipboardDataOrDataTransfer(textEvent->mInputType)) {
    aData.Truncate();
    return;
  }
  if (!textEvent->mDataTransfer) {
    aData = textEvent->mData;
    return;
  }
  textEvent->mDataTransfer->GetData(u"text/plain"_ns, aData, aSubjectPrincipal,
                                    IgnoreErrors());
}

}  // namespace mozilla::dom

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<TextEvent> NS_NewDOMTextEvent(
    EventTarget* aOwner, nsPresContext* aPresContext,
    InternalLegacyTextEvent* aEvent) {
  RefPtr<TextEvent> it = new TextEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
