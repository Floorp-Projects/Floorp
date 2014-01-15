/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/EventTarget.h"
#include "nsEventListenerManager.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {

void
EventTarget::RemoveEventListener(const nsAString& aType,
                                 EventListener* aListener,
                                 bool aUseCapture,
                                 ErrorResult& aRv)
{
  nsEventListenerManager* elm = GetExistingListenerManager();
  if (elm) {
    elm->RemoveEventListener(aType, aListener, aUseCapture);
  }
}

EventHandlerNonNull*
EventTarget::GetEventHandler(nsIAtom* aType, const nsAString& aTypeString)
{
  nsEventListenerManager* elm = GetExistingListenerManager();
  return elm ? elm->GetEventHandler(aType, aTypeString) : nullptr;
}

void
EventTarget::SetEventHandler(const nsAString& aType,
                             EventHandlerNonNull* aHandler,
                             ErrorResult& aRv)
{
  if (!StringBeginsWith(aType, NS_LITERAL_STRING("on"))) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }
  if (NS_IsMainThread()) {
    nsCOMPtr<nsIAtom> type = do_GetAtom(aType);
    SetEventHandler(type, EmptyString(), aHandler);
    return;
  }
  SetEventHandler(nullptr,
                  Substring(aType, 2), // Remove "on"
                  aHandler);
}

void
EventTarget::SetEventHandler(nsIAtom* aType, const nsAString& aTypeString,
                             EventHandlerNonNull* aHandler)
{
  GetOrCreateListenerManager()->SetEventHandler(aType, aTypeString, aHandler);
}

} // namespace dom
} // namespace mozilla
