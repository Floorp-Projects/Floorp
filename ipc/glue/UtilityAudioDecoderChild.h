/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_ipc_glue_UtilityAudioDecoderChild_h__
#define _include_ipc_glue_UtilityAudioDecoderChild_h__

#include "mozilla/ProcInfo.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/RefPtr.h"

#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/UtilityProcessParent.h"
#include "mozilla/ipc/UtilityProcessSandboxing.h"
#include "mozilla/ipc/UtilityAudioDecoder.h"
#include "mozilla/ipc/PUtilityAudioDecoderChild.h"

#ifdef MOZ_WMF_MEDIA_ENGINE
#  include "mozilla/gfx/GPUProcessListener.h"
#  include "mozilla/gfx/gfxVarReceiver.h"
#endif

#include "PDMFactory.h"

namespace mozilla::ipc {

class UtilityAudioDecoderChildShutdownObserver : public nsIObserver {
 public:
  explicit UtilityAudioDecoderChildShutdownObserver(SandboxingKind aKind)
      : mSandbox(aKind){};

  NS_DECL_ISUPPORTS

  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override;

 private:
  virtual ~UtilityAudioDecoderChildShutdownObserver() = default;

  const SandboxingKind mSandbox;
};

// This controls performing audio decoding on the utility process and it is
// intended to live on the main process side
class UtilityAudioDecoderChild final : public PUtilityAudioDecoderChild
#ifdef MOZ_WMF_MEDIA_ENGINE
    ,
                                       public gfx::gfxVarReceiver,
                                       public gfx::GPUProcessListener
#endif
{
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(UtilityAudioDecoderChild, override);
  mozilla::ipc::IPCResult RecvUpdateMediaCodecsSupported(
      const RemoteDecodeIn& aLocation,
      const media::MediaCodecsSupported& aSupported);

  UtilityActorName GetActorName() { return GetAudioActorName(mSandbox); }

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

    PROFILER_MARKER_UNTYPED(
        "UtilityAudioDecoderChild::BindToUtilityProcess", IPC,
        MarkerOptions(
            MarkerTiming::IntervalUntilNowFrom(mAudioDecoderChildStart)));
    return NS_OK;
  }

  void ActorDestroy(ActorDestroyReason aReason) override;

  void Bind(Endpoint<PUtilityAudioDecoderChild>&& aEndpoint);

  static void Shutdown(SandboxingKind aKind);

  static RefPtr<UtilityAudioDecoderChild> GetSingleton(SandboxingKind aKind);

#ifdef MOZ_WMF_MEDIA_ENGINE
  mozilla::ipc::IPCResult RecvCompleteCreatedVideoBridge();

  bool HasCreatedVideoBridge() const;

  void OnVarChanged(const gfx::GfxVarUpdate& aVar) override;

  void OnCompositorUnexpectedShutdown() override;

  // True if creating a video bridge sucessfully. Currently only used for media
  // engine cdm.
  bool CreateVideoBridge();
#endif

#ifdef MOZ_WMF_CDM
  void GetKeySystemCapabilities(dom::Promise* aPromise);
#endif

 private:
  explicit UtilityAudioDecoderChild(SandboxingKind aKind);
  ~UtilityAudioDecoderChild() = default;

  const SandboxingKind mSandbox;

#ifdef MOZ_WMF_MEDIA_ENGINE
  // True if the utility process has created a video bridge with the GPU prcess.
  // Currently only used for media egine cdm. Main thread only.
  bool mHasCreatedVideoBridge = false;
#endif

  TimeStamp mAudioDecoderChildStart;
};

}  // namespace mozilla::ipc

#endif  // _include_ipc_glue_UtilityAudioDecoderChild_h__
