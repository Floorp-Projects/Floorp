/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPProcessChild.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "mozilla/ipc/IOThreadChild.h"
#include "mozilla/BackgroundHangMonitor.h"

using mozilla::ipc::IOThreadChild;

namespace mozilla {
namespace gmp {

GMPProcessChild::GMPProcessChild(ProcessId aParentPid)
: ProcessChild(aParentPid)
{
}

GMPProcessChild::~GMPProcessChild()
{
}

bool
GMPProcessChild::Init()
{
  nsAutoString pluginFilename;
  nsAutoString voucherFilename;

#if defined(OS_POSIX)
  // NB: need to be very careful in ensuring that the first arg
  // (after the binary name) here is indeed the plugin module path.
  // Keep in sync with dom/plugins/PluginModuleParent.
  std::vector<std::string> values = CommandLine::ForCurrentProcess()->argv();
  MOZ_ASSERT(values.size() >= 3, "not enough args");
  pluginFilename = NS_ConvertUTF8toUTF16(nsDependentCString(values[1].c_str()));
  voucherFilename = NS_ConvertUTF8toUTF16(nsDependentCString(values[2].c_str()));
#elif defined(OS_WIN)
  std::vector<std::wstring> values = CommandLine::ForCurrentProcess()->GetLooseValues();
  MOZ_ASSERT(values.size() >= 2, "not enough loose args");
  pluginFilename = nsDependentString(values[0].c_str());
  voucherFilename = nsDependentString(values[1].c_str());
#else
#error Not implemented
#endif

  BackgroundHangMonitor::Startup();

  return mPlugin.Init(pluginFilename,
                      voucherFilename,
                      ParentPid(),
                      IOThreadChild::message_loop(),
                      IOThreadChild::channel());
}

void
GMPProcessChild::CleanUp()
{
  BackgroundHangMonitor::Shutdown();
}

GMPLoader* GMPProcessChild::mLoader = nullptr;

/* static */
void
GMPProcessChild::SetGMPLoader(GMPLoader* aLoader)
{
  mLoader = aLoader;
}

/* static */
GMPLoader*
GMPProcessChild::GetGMPLoader()
{
  return mLoader;
}

} // namespace gmp
} // namespace mozilla
