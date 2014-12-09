/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaPromise.h"
#include "MediaTaskQueue.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace detail {

nsresult
DispatchMediaPromiseRunnable(MediaTaskQueue* aTaskQueue, nsIRunnable* aRunnable)
{
  return aTaskQueue->ForceDispatch(aRunnable);
}

nsresult
DispatchMediaPromiseRunnable(nsIEventTarget* aEventTarget, nsIRunnable* aRunnable)
{
  return aEventTarget->Dispatch(aRunnable, NS_DISPATCH_NORMAL);
}

}
} // namespace mozilla
