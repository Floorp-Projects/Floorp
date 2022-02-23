/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_BackgroundStarterChild_h
#define mozilla_ipc_BackgroundStarterChild_h

#include "mozilla/ipc/PBackgroundStarterChild.h"
#include "mozilla/dom/ContentChild.h"

namespace mozilla::ipc {

class BackgroundStarterChild final : public PBackgroundStarterChild {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BackgroundStarterChild, override)

  BackgroundStarterChild(base::ProcessId aOtherPid,
                         nsISerialEventTarget* aTaskQueue)
      : mOtherPid(aOtherPid), mTaskQueue(aTaskQueue) {}

  // Unlike the methods on `IToplevelProtocol`, may be accessed on any thread
  // and will not be modified after construction.
  const base::ProcessId mOtherPid;
  const nsCOMPtr<nsISerialEventTarget> mTaskQueue;

 private:
  friend class PBackgroundStarterChild;
  ~BackgroundStarterChild() = default;
};

}  // namespace mozilla::ipc

#endif  // mozilla_ipc_BackgroundStarterChild_h
