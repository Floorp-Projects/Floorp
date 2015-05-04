/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EventDispatcher.h"
#include "mozilla/dom/TabMessageUtils.h"
#include "nsCOMPtr.h"
#include "nsIDOMEvent.h"

namespace mozilla {
namespace dom {

bool
ReadRemoteEvent(const IPC::Message* aMsg, void** aIter,
                RemoteDOMEvent* aResult)
{
  aResult->mEvent = nullptr;
  nsString type;
  NS_ENSURE_TRUE(ReadParam(aMsg, aIter, &type), false);

  nsCOMPtr<nsIDOMEvent> event;
  EventDispatcher::CreateEvent(nullptr, nullptr, nullptr, type,
                               getter_AddRefs(event));
  aResult->mEvent = do_QueryInterface(event);
  NS_ENSURE_TRUE(aResult->mEvent, false);

  return aResult->mEvent->Deserialize(aMsg, aIter);
}

}
}
