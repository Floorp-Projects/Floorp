/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPProcessChild.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "chrome/common/chrome_switches.h"
#include "mozilla/ipc/IOThreadChild.h"
#include "mozilla/BackgroundHangMonitor.h"

using mozilla::ipc::IOThreadChild;

namespace mozilla {
namespace gmp {

GMPProcessChild::GMPProcessChild(ProcessHandle parentHandle)
: ProcessChild(parentHandle)
{
}

GMPProcessChild::~GMPProcessChild()
{
}

bool
GMPProcessChild::Init()
{
  std::string pluginFilename;
  std::string voucherFilename;

#if defined(OS_POSIX)
  // NB: need to be very careful in ensuring that the first arg
  // (after the binary name) here is indeed the plugin module path.
  // Keep in sync with dom/plugins/PluginModuleParent.
  std::vector<std::string> values = CommandLine::ForCurrentProcess()->argv();
  MOZ_ASSERT(values.size() >= 3, "not enough args");
  pluginFilename = values[1];
  voucherFilename = values[2];
#elif defined(OS_WIN)
  std::vector<std::wstring> values = CommandLine::ForCurrentProcess()->GetLooseValues();
  MOZ_ASSERT(values.size() >= 2, "not enough loose args");
  pluginFilename = WideToUTF8(values[0]);
  voucherFilename = WideToUTF8(values[1]);
#else
#error Not implemented
#endif

  BackgroundHangMonitor::Startup();

  return mPlugin.Init(pluginFilename,
                      voucherFilename,
                      ParentHandle(),
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
