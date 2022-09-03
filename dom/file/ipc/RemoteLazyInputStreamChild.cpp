/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteLazyInputStreamChild.h"
#include "RemoteLazyInputStreamThread.h"

namespace mozilla {

extern mozilla::LazyLogModule gRemoteLazyStreamLog;

RemoteLazyInputStreamChild::RemoteLazyInputStreamChild(const nsID& aID)
    : mID(aID) {}

RemoteLazyInputStreamChild::~RemoteLazyInputStreamChild() = default;

void RemoteLazyInputStreamChild::StreamCreated() {
  size_t count = ++mStreamCount;
  MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose,
          ("Child::StreamCreated %s = %zu", nsIDToCString(mID).get(), count));
}

void RemoteLazyInputStreamChild::StreamConsumed() {
  size_t count = --mStreamCount;
  MOZ_LOG(gRemoteLazyStreamLog, LogLevel::Verbose,
          ("Child::StreamConsumed %s = %zu", nsIDToCString(mID).get(), count));

  // When the count reaches zero, close the underlying actor.
  if (count == 0) {
    auto* t = RemoteLazyInputStreamThread::Get();
    if (t) {
      t->Dispatch(
          NS_NewRunnableFunction("RemoteLazyInputStreamChild::StreamConsumed",
                                 [self = RefPtr{this}]() {
                                   if (self->CanSend()) {
                                     self->SendGoodbye();
                                   }
                                 }));
    }  // else the xpcom thread shutdown has already started.
  }
}

void RemoteLazyInputStreamChild::ActorDestroy(ActorDestroyReason aReason) {
  if (mStreamCount != 0) {
    NS_WARNING(
        nsPrintfCString("RemoteLazyInputStreamChild disconnected unexpectedly "
                        "(%zu streams remaining)! %p %s",
                        size_t(mStreamCount), this, nsIDToCString(mID).get())
            .get());
  }
}

}  // namespace mozilla
