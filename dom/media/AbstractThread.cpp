/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AbstractThread.h"

#include "MediaTaskQueue.h"
#include "nsThreadUtils.h"

namespace mozilla {

template<>
nsresult
AbstractThreadImpl<MediaTaskQueue>::Dispatch(already_AddRefed<nsIRunnable> aRunnable)
{
  RefPtr<nsIRunnable> r(aRunnable);
  return mTarget->ForceDispatch(r);
}

template<>
nsresult
AbstractThreadImpl<nsIThread>::Dispatch(already_AddRefed<nsIRunnable> aRunnable)
{
  nsCOMPtr<nsIRunnable> r = aRunnable;
  return mTarget->Dispatch(r, NS_DISPATCH_NORMAL);
}

template<>
bool
AbstractThreadImpl<MediaTaskQueue>::IsCurrentThreadIn()
{
  return mTarget->IsCurrentThreadIn();
}

template<>
bool
AbstractThreadImpl<nsIThread>::IsCurrentThreadIn()
{
  return NS_GetCurrentThread() == mTarget;
}

} // namespace mozilla
