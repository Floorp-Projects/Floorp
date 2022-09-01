/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UtilityAudioDecoderChild.h"

#include "base/basictypes.h"
#include "mozilla/AppShutdown.h"
#include "mozilla/dom/ContentParent.h"

namespace mozilla::ipc {

static EnumeratedArray<SandboxingKind, SandboxingKind::COUNT,
                       StaticRefPtr<UtilityAudioDecoderChild>>
    sAudioDecoderChilds;

UtilityAudioDecoderChild::UtilityAudioDecoderChild(SandboxingKind aKind)
    : mSandbox(aKind) {
  MOZ_ASSERT(NS_IsMainThread());
}

void UtilityAudioDecoderChild::ActorDestroy(ActorDestroyReason aReason) {
  MOZ_ASSERT(NS_IsMainThread());
  sAudioDecoderChilds[mSandbox] = nullptr;
}

void UtilityAudioDecoderChild::Bind(
    Endpoint<PUtilityAudioDecoderChild>&& aEndpoint) {
  DebugOnly<bool> ok = aEndpoint.Bind(this);
  MOZ_ASSERT(ok);
}

/* static */
RefPtr<UtilityAudioDecoderChild> UtilityAudioDecoderChild::GetSingleton(
    SandboxingKind aKind) {
  MOZ_ASSERT(NS_IsMainThread());
  bool shutdown = AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMWillShutdown);
  if (!sAudioDecoderChilds[aKind] && !shutdown) {
    sAudioDecoderChilds[aKind] = new UtilityAudioDecoderChild(aKind);
  }
  return sAudioDecoderChilds[aKind];
}

mozilla::ipc::IPCResult
UtilityAudioDecoderChild::RecvUpdateMediaCodecsSupported(
    const RemoteDecodeIn& aLocation,
    const media::MediaCodecsSupported& aSupported) {
  dom::ContentParent::BroadcastMediaCodecsSupportedUpdate(aLocation,
                                                          aSupported);
  return IPC_OK();
}

}  // namespace mozilla::ipc
