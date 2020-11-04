/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BeforeUnloadEvent.h"

namespace mozilla::dom {

void BeforeUnloadEvent::SetReturnValue(const nsAString& aReturnValue) {
  mText = aReturnValue;
}

void BeforeUnloadEvent::GetReturnValue(nsAString& aReturnValue) {
  aReturnValue = mText;
}

}  // namespace mozilla::dom

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<BeforeUnloadEvent> NS_NewDOMBeforeUnloadEvent(
    EventTarget* aOwner, nsPresContext* aPresContext, WidgetEvent* aEvent) {
  RefPtr<BeforeUnloadEvent> it =
      new BeforeUnloadEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
