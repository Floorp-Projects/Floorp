/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncIOThreadHolder.h"

namespace mozilla {
namespace gfx {

VsyncIOThreadHolder::VsyncIOThreadHolder()
{
}

VsyncIOThreadHolder::~VsyncIOThreadHolder()
{
  if (!mThread) {
    return;
  }

  NS_DispatchToMainThread(NewRunnableMethod(mThread, &nsIThread::AsyncShutdown));
}

bool
VsyncIOThreadHolder::Start()
{
  nsresult rv = NS_NewNamedThread("VsyncIOThread", getter_AddRefs(mThread));
  return NS_SUCCEEDED(rv);
}

RefPtr<nsIThread>
VsyncIOThreadHolder::GetThread() const
{
  return mThread;
}

} // namespace gfx
} // namespace mozilla
