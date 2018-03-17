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
                                       bool aIsFileContent)
 : GeckoChildProcessHost(GeckoProcessType_Content, aIsFileContent),
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

  bool res = GeckoChildProcessHost::AsyncLaunch(aExtraOpts);
  MOZ_RELEASE_ASSERT(res);
  return true;
}

void
ContentProcessHost::OnProcessHandleReady(ProcessHandle aProcessHandle)
{
  MOZ_ASSERT(!NS_IsMainThread());

  // This will wake up the main thread if it is waiting for the process to
  // launch.
  mContentParent->SetOtherProcessId(base::GetProcId(aProcessHandle));

  mHasLaunched = true;
  GeckoChildProcessHost::OnProcessHandleReady(aProcessHandle);
}

void
ContentProcessHost::OnProcessLaunchError()
{
  MOZ_ASSERT(!NS_IsMainThread());
  mContentParent->SetOtherProcessId(mozilla::ipc::kInvalidProcessId,
                                    ContentParent::ProcessIdState::eError);
  mHasLaunched = true;
  GeckoChildProcessHost::OnProcessLaunchError();
}

} // namespace dom
} // namespace mozilla
