/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_ipc_glue_UtilityAudioDecoderParent_h_
#define _include_ipc_glue_UtilityAudioDecoderParent_h_

#include "mozilla/PRemoteDecoderManagerParent.h"
#include "mozilla/UniquePtr.h"

#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/PUtilityAudioDecoderParent.h"

#include "mozilla/ipc/UtilityProcessSandboxing.h"

#include "nsThreadManager.h"

namespace mozilla::ipc {

// This is in charge of handling the utility child process side to perform
// audio decoding
class UtilityAudioDecoderParent final : public PUtilityAudioDecoderParent {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(UtilityAudioDecoderParent, override);

  UtilityAudioDecoderParent();

  static void GenericPreloadForSandbox();
  static void WMFPreloadForSandbox();

  void Start(Endpoint<PUtilityAudioDecoderParent>&& aEndpoint);

  mozilla::ipc::IPCResult RecvNewContentRemoteDecoderManager(
      Endpoint<PRemoteDecoderManagerParent>&& aEndpoint);

#ifdef MOZ_WMF_MEDIA_ENGINE
  mozilla::ipc::IPCResult RecvInitVideoBridge(
      Endpoint<PVideoBridgeChild>&& aEndpoint,
      nsTArray<mozilla::gfx::GfxVarUpdate>&& aUpdates,
      const ContentDeviceData& aContentDeviceData);

  IPCResult RecvUpdateVar(const mozilla::gfx::GfxVarUpdate& aUpdate);
#endif

  static SandboxingKind GetSandboxingKind();

 private:
  ~UtilityAudioDecoderParent() = default;
};

}  // namespace mozilla::ipc

#endif  // _include_ipc_glue_UtilityAudioDecoderParent_h_
