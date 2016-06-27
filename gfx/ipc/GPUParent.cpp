/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GPUParent.h"
#include "gfxConfig.h"
#include "gfxPrefs.h"
#include "GPUProcessHost.h"
#include "mozilla/Assertions.h"
#include "mozilla/ipc/ProcessChild.h"

namespace mozilla {
namespace gfx {

using namespace ipc;

GPUParent::GPUParent()
{
}

GPUParent::~GPUParent()
{
}

bool
GPUParent::Init(base::ProcessId aParentPid,
                MessageLoop* aIOLoop,
                IPC::Channel* aChannel)
{
  if (NS_WARN_IF(!Open(aChannel, aParentPid, aIOLoop))) {
    return false;
  }

  // Ensure gfxPrefs are initialized.
  gfxPrefs::GetSingleton();

  return true;
}

bool
GPUParent::RecvInit(nsTArray<GfxPrefSetting>&& prefs)
{
  for (auto setting : prefs) {
    gfxPrefs::Pref* pref = gfxPrefs::all()[setting.index()];
    pref->SetCachedValue(setting.value());
  }

  return true;
}

bool
GPUParent::RecvUpdatePref(const GfxPrefSetting& setting)
{
  gfxPrefs::Pref* pref = gfxPrefs::all()[setting.index()];
  pref->SetCachedValue(setting.value());
  return true;
}

void
GPUParent::ActorDestroy(ActorDestroyReason aWhy)
{
  if (AbnormalShutdown == aWhy) {
    NS_WARNING("Shutting down GPU process early due to a crash!");
    ProcessChild::QuickExit();
  }

#ifndef NS_FREE_PERMANENT_DATA
  // No point in going through XPCOM shutdown because we don't keep persistent
  // state. Currently we quick-exit in RecvBeginShutdown so this should be
  // unreachable.
  ProcessChild::QuickExit();
#else
  XRE_ShutdownChildProcess();
#endif
}

} // namespace gfx
} // namespace mozilla
