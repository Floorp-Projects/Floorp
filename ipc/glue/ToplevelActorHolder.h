/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_IPC_TOPLEVELACTORHOLDER_H
#define MOZILLA_IPC_TOPLEVELACTORHOLDER_H

#include "nsISupports.h"

namespace mozilla::ipc {

// Class to let us close the actor when we're not using it anymore.  You
// should create a single instance of this, and when you have no more
// references it will be destroyed and will Close() the underlying
// top-level channel.
// When you want to send something, you use something like
// aActor->Actor()->SendFoo()

// You can avoid calling Close() on an un-connected Actor (for example if
// Bind() fails) by calling RemoveActor();
template <typename T>
class ToplevelActorHolder final {
 public:
  NS_INLINE_DECL_REFCOUNTING_ONEVENTTARGET(ToplevelActorHolder)

  explicit ToplevelActorHolder(T* aActor) : mActor(aActor) {}

  constexpr T* Actor() const { return mActor; }
  inline void RemoveActor() { mActor = nullptr; }

 private:
  inline ~ToplevelActorHolder() {
    if (mActor) {
      mActor->Close();
    }
  }

  RefPtr<T> mActor;
};

}  // namespace mozilla::ipc

#endif  // MOZILLA_IPC_TOPLEVELACTORHOLDER_H
