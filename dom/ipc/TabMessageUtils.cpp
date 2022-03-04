/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/TabMessageUtils.h"
#include "nsCOMPtr.h"

namespace mozilla::dom {

bool ReadRemoteEvent(IPC::MessageReader* aReader, RemoteDOMEvent* aResult) {
  aResult->mEvent = nullptr;
  nsString type;
  NS_ENSURE_TRUE(ReadParam(aReader, &type), false);

  aResult->mEvent =
      EventDispatcher::CreateEvent(nullptr, nullptr, nullptr, type);

  // In a controlled environment, we should always have a valid event.
  bool ret = false;
  if (aResult->mEvent) {
    ret = aResult->mEvent->Deserialize(aReader);
  }
  MOZ_ASSERT_UNLESS_FUZZING(ret);

  return ret;
}

}  // namespace mozilla::dom
