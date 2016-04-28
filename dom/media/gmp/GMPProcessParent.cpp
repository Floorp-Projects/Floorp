/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPProcessParent.h"
#include "GMPUtils.h"
#include "nsIFile.h"
#include "nsIRunnable.h"
#if defined(XP_WIN) && defined(MOZ_SANDBOX)
#include "WinUtils.h"
#endif

#include "base/string_util.h"
#include "base/process_util.h"

#include <string>

using std::vector;
using std::string;

using mozilla::gmp::GMPProcessParent;
using mozilla::ipc::GeckoChildProcessHost;
using base::ProcessArchitecture;

namespace mozilla {
namespace gmp {

GMPProcessParent::GMPProcessParent(const std::string& aGMPPath)
: GeckoChildProcessHost(GeckoProcessType_GMPlugin),
  mGMPPath(aGMPPath)
{
  MOZ_COUNT_CTOR(GMPProcessParent);
}

GMPProcessParent::~GMPProcessParent()
{
  MOZ_COUNT_DTOR(GMPProcessParent);
}

bool
GMPProcessParent::Launch(int32_t aTimeoutMs)
{
  nsCOMPtr<nsIFile> path;
  if (!GetEMEVoucherPath(getter_AddRefs(path))) {
    NS_WARNING("GMPProcessParent can't get EME voucher path!");
    return false;
  }
  nsAutoCString voucherPath;
  path->GetNativePath(voucherPath);

  vector<string> args;

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
  std::wstring wGMPPath = UTF8ToWide(mGMPPath.c_str());

  // The sandbox doesn't allow file system rules where the paths contain
  // symbolic links or junction points. Sometimes the Users folder has been
  // moved to another drive using a junction point, so allow for this specific
  // case. See bug 1236680 for details.
  if (!widget::WinUtils::ResolveMovedUsersFolder(wGMPPath)) {
    NS_WARNING("ResolveMovedUsersFolder failed for GMP path.");
    return false;
  }
  mAllowedFilesRead.push_back(wGMPPath + L"\\*");
  args.push_back(WideToUTF8(wGMPPath));
#else
  args.push_back(mGMPPath);
#endif

  args.push_back(string(voucherPath.BeginReading(), voucherPath.EndReading()));

  return SyncLaunch(args, aTimeoutMs, base::GetCurrentProcessArchitecture());
}

void
GMPProcessParent::Delete(nsCOMPtr<nsIRunnable> aCallback)
{
  mDeletedCallback = aCallback;
  RefPtr<mozilla::Runnable> task = NS_NewNonOwningRunnableMethod(this, &GMPProcessParent::DoDelete);
  XRE_GetIOMessageLoop()->PostTask(task.forget());
}

void
GMPProcessParent::DoDelete()
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  Join();

  if (mDeletedCallback) {
    mDeletedCallback->Run();
  }

  delete this;
}

} // namespace gmp
} // namespace mozilla
