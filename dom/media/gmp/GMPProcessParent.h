/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPProcessParent_h
#define GMPProcessParent_h 1

#include "mozilla/Attributes.h"
#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/thread.h"
#include "base/waitable_event.h"
#include "chrome/common/child_process_host.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"

class nsIRunnable;

namespace mozilla {
namespace gmp {

class GMPProcessParent final : public mozilla::ipc::GeckoChildProcessHost
{
public:
  explicit GMPProcessParent(const std::string& aGMPPath);
  ~GMPProcessParent();

  // Synchronously launch the plugin process. If the process fails to launch
  // after timeoutMs, this method will return false.
  bool Launch(int32_t aTimeoutMs);

  void Delete(nsCOMPtr<nsIRunnable> aCallback = nullptr);

  virtual bool CanShutdown() override { return true; }
  const std::string& GetPluginFilePath() { return mGMPPath; }

  using mozilla::ipc::GeckoChildProcessHost::GetShutDownEvent;
  using mozilla::ipc::GeckoChildProcessHost::GetChannel;
  using mozilla::ipc::GeckoChildProcessHost::GetChildProcessHandle;

private:
  void DoDelete();

  std::string mGMPPath;
  nsCOMPtr<nsIRunnable> mDeletedCallback;

  DISALLOW_COPY_AND_ASSIGN(GMPProcessParent);
};

} // namespace gmp
} // namespace mozilla

#endif // ifndef GMPProcessParent_h
