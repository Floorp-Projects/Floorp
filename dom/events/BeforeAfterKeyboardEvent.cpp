/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BeforeAfterKeyboardEvent.h"
#include "mozilla/TextEvents.h"
#include "prtime.h"

namespace mozilla {
namespace dom {

BeforeAfterKeyboardEvent::BeforeAfterKeyboardEvent(
                                       EventTarget* aOwner,
                                       nsPresContext* aPresContext,
                                       InternalBeforeAfterKeyboardEvent* aEvent)
  : KeyboardEvent(aOwner, aPresContext,
                  aEvent ? aEvent :
                           new InternalBeforeAfterKeyboardEvent(false,
                                                                NS_EVENT_NULL,
                                                                nullptr))
{
  MOZ_ASSERT(mEvent->mClass == eBeforeAfterKeyboardEventClass,
             "event type mismatch eBeforeAfterKeyboardEventClass");

  if (!aEvent) {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
  }
}

// static
already_AddRefed<BeforeAfterKeyboardEvent>
BeforeAfterKeyboardEvent::Constructor(
                            EventTarget* aOwner,
                            const nsAString& aType,
                            const BeforeAfterKeyboardEventInit& aParam)
{
  nsRefPtr<BeforeAfterKeyboardEvent> event =
    new BeforeAfterKeyboardEvent(aOwner, nullptr, nullptr);
  ErrorResult rv;
  event->InitWithKeyboardEventInit(aOwner, aType, aParam, rv);
  NS_WARN_IF(rv.Failed());

  event->mEvent->AsBeforeAfterKeyboardEvent()->mEmbeddedCancelled =
    aParam.mEmbeddedCancelled;

  return event.forget();
}

// static
already_AddRefed<BeforeAfterKeyboardEvent>
BeforeAfterKeyboardEvent::Constructor(
                            const GlobalObject& aGlobal,
                            const nsAString& aType,
                            const BeforeAfterKeyboardEventInit& aParam,
                            ErrorResult& aRv)
{
  nsCOMPtr<EventTarget> owner = do_QueryInterface(aGlobal.GetAsSupports());
  return Constructor(owner, aType, aParam);
}

Nullable<bool>
BeforeAfterKeyboardEvent::GetEmbeddedCancelled()
{
  nsAutoString type;
  GetType(type);
  if (type.EqualsLiteral("mozbrowserafterkeydown") ||
      type.EqualsLiteral("mozbrowserafterkeyup")) {
    return mEvent->AsBeforeAfterKeyboardEvent()->mEmbeddedCancelled;
  }
  return Nullable<bool>();
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<BeforeAfterKeyboardEvent>
NS_NewDOMBeforeAfterKeyboardEvent(EventTarget* aOwner,
                                  nsPresContext* aPresContext,
                                  InternalBeforeAfterKeyboardEvent* aEvent)
{
  nsRefPtr<BeforeAfterKeyboardEvent> it =
    new BeforeAfterKeyboardEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
