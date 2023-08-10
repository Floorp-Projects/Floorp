/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPProcessChild.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "mozilla/ipc/IOThreadChild.h"
#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/GeckoArgs.h"

using mozilla::ipc::IOThreadChild;

namespace mozilla::gmp {

GMPProcessChild::~GMPProcessChild() = default;

bool GMPProcessChild::Init(int aArgc, char* aArgv[]) {
  Maybe<const char*> parentBuildID =
      geckoargs::sParentBuildID.Get(aArgc, aArgv);
  if (NS_WARN_IF(parentBuildID.isNothing())) {
    return false;
  }

  Maybe<const char*> pluginPath = geckoargs::sPluginPath.Get(aArgc, aArgv);
  if (NS_WARN_IF(pluginPath.isNothing())) {
    return false;
  }

  NS_ConvertUTF8toUTF16 widePluginPath(*pluginPath);

  if (!ProcessChild::InitPrefs(aArgc, aArgv)) {
    return false;
  }

  return mPlugin->Init(widePluginPath, *parentBuildID, TakeInitialEndpoint());
}

void GMPProcessChild::CleanUp() { BackgroundHangMonitor::Shutdown(); }

}  // namespace mozilla::gmp
