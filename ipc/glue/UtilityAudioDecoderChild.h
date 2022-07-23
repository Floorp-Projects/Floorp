/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_ipc_glue_UtilityAudioDecoderChild_h__
#define _include_ipc_glue_UtilityAudioDecoderChild_h__

#include "mozilla/ProcInfo.h"
#include "mozilla/RefPtr.h"

#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/UtilityProcessParent.h"
#include "mozilla/ipc/PUtilityAudioDecoderChild.h"

#include "PDMFactory.h"

namespace mozilla::ipc {

// This controls performing audio decoding on the utility process and it is
// intended to live on the main process side
class UtilityAudioDecoderChild final : public PUtilityAudioDecoderChild {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(UtilityAudioDecoderChild, override);

  mozilla::ipc::IPCResult RecvUpdateMediaCodecsSupported(
      const media::MediaCodecsSupported& aSupported);

  UtilityActorName GetActorName() { return UtilityActorName::AudioDecoder; }

  nsresult BindToUtilityProcess(RefPtr<UtilityProcessParent> aUtilityParent) {
    Endpoint<PUtilityAudioDecoderChild> utilityAudioDecoderChildEnd;
    Endpoint<PUtilityAudioDecoderParent> utilityAudioDecoderParentEnd;
    nsresult rv = PUtilityAudioDecoder::CreateEndpoints(
        aUtilityParent->OtherPid(), base::GetCurrentProcId(),
        &utilityAudioDecoderParentEnd, &utilityAudioDecoderChildEnd);

    if (NS_FAILED(rv)) {
      MOZ_ASSERT(false, "Protocol endpoints failure");
      return NS_ERROR_FAILURE;
    }

    if (!aUtilityParent->SendStartUtilityAudioDecoderService(
            std::move(utilityAudioDecoderParentEnd))) {
      MOZ_ASSERT(false, "StartUtilityAudioDecoder service failure");
      return NS_ERROR_FAILURE;
    }

    Bind(std::move(utilityAudioDecoderChildEnd));
    return NS_OK;
  }

  void ActorDestroy(ActorDestroyReason aReason) override;

  void Bind(Endpoint<PUtilityAudioDecoderChild>&& aEndpoint);

  static RefPtr<UtilityAudioDecoderChild> GetSingleton();

 private:
  UtilityAudioDecoderChild();
  ~UtilityAudioDecoderChild() = default;
};

}  // namespace mozilla::ipc

#endif  // _include_ipc_glue_UtilityAudioDecoderChild_h__
