/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TabMessageUtils.h"
#include "nsCOMPtr.h"
#include "nsIDOMEvent.h"

#ifdef CreateEvent
#undef CreateEvent
#endif

#include "nsEventDispatcher.h"

namespace mozilla {
namespace dom {

bool
ReadRemoteEvent(const IPC::Message* aMsg, void** aIter,
                RemoteDOMEvent* aResult)
{
  aResult->mEvent = nsnull;
  nsString type;
  NS_ENSURE_TRUE(ReadParam(aMsg, aIter, &type), false);

  nsCOMPtr<nsIDOMEvent> event;
  nsEventDispatcher::CreateEvent(nsnull, nsnull, type, getter_AddRefs(event));
  aResult->mEvent = do_QueryInterface(event);
  NS_ENSURE_TRUE(aResult->mEvent, false);

  return aResult->mEvent->Deserialize(aMsg, aIter);
}

}
}
