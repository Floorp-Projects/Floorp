/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPCrashHelperHolder_h_
#define GMPCrashHelperHolder_h_

#include "GMPService.h"
#include "mozilla/RefPtr.h"
#include "nsPIDOMWindow.h"
#include "mozilla/ipc/ProtocolUtils.h"

class GMPCrashHelper;

namespace mozilla {

// Disconnecting the GMPCrashHelpers at the right time is hard. We need to
// ensure that in the crashing case that we stay connected until the
// "gmp-plugin-crashed" message is processed in the content process.
//
// We have two channels connecting to the GMP; PGMP which connects from
// chrome to GMP process, and PGMPContent, which bridges between the content
// and GMP process. If the GMP crashes both PGMP and PGMPContent receive
// ActorDestroy messages and begin to shutdown at the same time.
//
// However the crash report mini dump must be generated in the chrome
// process' ActorDestroy, before the "gmp-plugin-crashed" message can be sent
// to the content process. We fire the "PluginCrashed" event when we handle
// the "gmp-plugin-crashed" message in the content process, and we need the
// crash helpers to do that.
//
// The PGMPContent's managed actors' ActorDestroy messages are the only shutdown
// notification we get in the content process, but we can't disconnect the
// crash handlers there in the crashing case, as ActorDestroy happens before
// we've received the "gmp-plugin-crashed" message and had a chance for the
// crash helpers to generate the window to dispatch PluginCrashed to initiate
// the crash report submission notification box.
//
// So we need to ensure that in the content process, the GMPCrashHelpers stay
// connected to the GMPService until after ActorDestroy messages are received
// if there's an abnormal shutdown. In the case where the GMP doesn't crash,
// we do actually want to disconnect GMPCrashHandlers in ActorDestroy, since
// we don't have any other signal that a GMP actor is shutting down. If we don't
// disconnect the crash helper there in the normal shutdown case, the helper
// will stick around forever and leak.
//
// In the crashing case, the GMPCrashHelpers are deallocated when the crash
// report is processed in GeckoMediaPluginService::RunPluginCrashCallbacks().
//
// It's a bit yuck that we have to have two paths for disconnecting the crash
// helpers, but there aren't really any better options.
class GMPCrashHelperHolder
{
public:

  void SetCrashHelper(GMPCrashHelper* aHelper)
  {
    mCrashHelper = aHelper;
  }

  GMPCrashHelper* GetCrashHelper()
  {
    return mCrashHelper;
  }

  void MaybeDisconnect(bool aAbnormalShutdown)
  {
    if (!aAbnormalShutdown) {
      RefPtr<gmp::GeckoMediaPluginService> service(gmp::GeckoMediaPluginService::GetGeckoMediaPluginService());
      service->DisconnectCrashHelper(GetCrashHelper());
    }
  }

private:
  RefPtr<GMPCrashHelper> mCrashHelper;
};

}

#endif // GMPCrashHelperHolder_h_
