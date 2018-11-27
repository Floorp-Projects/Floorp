/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sts=8 sw=2 ts=2 tw=99 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _include_mozilla_dom_ipc_ContentProcessHost_h_
#define _include_mozilla_dom_ipc_ContentProcessHost_h_

#include "mozilla/ipc/GeckoChildProcessHost.h"

namespace mozilla {
namespace dom {

class ContentParent;

// ContentProcessHost is the "parent process" container for a subprocess handle
// and IPC connection. It owns the parent process IPDL actor, which in this
// case, is a ContentParent.
class ContentProcessHost final : public ::mozilla::ipc::GeckoChildProcessHost
{
  friend class ContentParent;

public:
  explicit
  ContentProcessHost(ContentParent* aContentParent,
                     bool aIsFileContent = false);
  ~ContentProcessHost();

  // Launch the subprocess asynchronously. On failure, false is returned.
  // Otherwise, true is returned, and either the OnProcessHandleReady method is
  // called when the process is created, or OnProcessLaunchError will be called
  // if the process could not be spawned due to an asynchronous error.
  bool Launch(StringVector aExtraOpts);

  // Called on the IO thread.
  void OnProcessHandleReady(ProcessHandle aProcessHandle) override;
  void OnProcessLaunchError() override;

private:
  DISALLOW_COPY_AND_ASSIGN(ContentProcessHost);

  bool mHasLaunched;

  ContentParent* mContentParent; // weak
};

} // namespace dom
} // namespace mozilla

#endif // _include_mozilla_dom_ipc_ContentProcessHost_h_
