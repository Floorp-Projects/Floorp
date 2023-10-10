/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncIOThreadHolder.h"

#include "mozilla/SchedulerGroup.h"

namespace mozilla {
namespace gfx {

VsyncIOThreadHolder::VsyncIOThreadHolder() {
  MOZ_COUNT_CTOR(VsyncIOThreadHolder);
}

VsyncIOThreadHolder::~VsyncIOThreadHolder() {
  MOZ_COUNT_DTOR(VsyncIOThreadHolder);

  if (!mThread) {
    return;
  }

  if (NS_IsMainThread()) {
    mThread->AsyncShutdown();
  } else {
    SchedulerGroup::Dispatch(NewRunnableMethod(
        "nsIThread::AsyncShutdown", mThread, &nsIThread::AsyncShutdown));
  }
}

bool VsyncIOThreadHolder::Start() {
  /* "VsyncIOThread" is used as the thread we send/recv IPC messages on.  We
   * don't use the "WindowsVsyncThread" directly because it isn't servicing an
   * nsThread event loop which is needed for IPC to return results/notify us
   * about shutdown etc.
   *
   * It would be better if we could notify the IPC IO thread directly to avoid
   * the extra ping-ponging but that doesn't seem possible. */
  nsresult rv = NS_NewNamedThread("VsyncIOThread", getter_AddRefs(mThread));
  return NS_SUCCEEDED(rv);
}

RefPtr<nsIThread> VsyncIOThreadHolder::GetThread() const { return mThread; }

}  // namespace gfx
}  // namespace mozilla
