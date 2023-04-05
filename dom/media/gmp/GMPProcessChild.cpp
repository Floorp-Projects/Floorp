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

namespace mozilla::gmp {

GMPProcessChild::~GMPProcessChild() = default;

bool GMPProcessChild::Init(int aArgc, char* aArgv[]) {
  nsAutoString pluginFilename;

#if defined(OS_POSIX)
  // NB: need to be very careful in ensuring that the first arg
  // (after the binary name) here is indeed the plugin module path.
  // Keep in sync with dom/plugins/PluginModuleParent.
  std::vector<std::string> values = CommandLine::ForCurrentProcess()->argv();
  MOZ_ASSERT(values.size() >= 2, "not enough args");
  CopyUTF8toUTF16(nsDependentCString(values[1].c_str()), pluginFilename);
#elif defined(OS_WIN)
  std::vector<std::wstring> values =
      CommandLine::ForCurrentProcess()->GetLooseValues();
  MOZ_ASSERT(values.size() >= 1, "not enough loose args");
  pluginFilename = nsDependentString(values[0].c_str());
#else
#  error Not implemented
#endif

  BackgroundHangMonitor::Startup();

  return mPlugin.Init(pluginFilename, TakeInitialEndpoint());
}

void GMPProcessChild::CleanUp() { BackgroundHangMonitor::Shutdown(); }

}  // namespace mozilla::gmp
