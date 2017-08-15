/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sts=8 sw=2 ts=2 tw=99 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentProcessHost.h"

namespace mozilla {
namespace dom {

using namespace ipc;

ContentProcessHost::ContentProcessHost(ContentParent* aContentParent,
                                       ChildPrivileges aPrivileges)
 : GeckoChildProcessHost(GeckoProcessType_Content, aPrivileges),
   mHasLaunched(false),
   mContentParent(aContentParent)
{
  MOZ_COUNT_CTOR(ContentProcessHost);
}

ContentProcessHost::~ContentProcessHost()
{
  MOZ_COUNT_DTOR(ContentProcessHost);
}

bool
ContentProcessHost::Launch(StringVector aExtraOpts)
{
  MOZ_ASSERT(!mHasLaunched);
  MOZ_ASSERT(mContentParent);

  if (!GeckoChildProcessHost::AsyncLaunch(aExtraOpts)) {
    mHasLaunched = true;
    return false;
  }
  return true;
}

void
ContentProcessHost::OnChannelConnected(int32_t aPid)
{
  MOZ_ASSERT(!NS_IsMainThread());

  // This will wake up the main thread if it is waiting for the process to
  // launch.
  mContentParent->SetOtherProcessId(aPid);

  mHasLaunched = true;
  GeckoChildProcessHost::OnChannelConnected(aPid);
}

void
ContentProcessHost::OnChannelError()
{
  MOZ_ASSERT(!NS_IsMainThread());

  mHasLaunched = true;
  GeckoChildProcessHost::OnChannelError();
}

} // namespace dom
} // namespace mozilla
