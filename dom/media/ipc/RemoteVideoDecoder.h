/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_media_ipc_RemoteVideoDecoderChild_h
#define include_dom_media_ipc_RemoteVideoDecoderChild_h
#include "RemoteDecoderChild.h"
#include "RemoteDecoderManagerChild.h"
#include "RemoteDecoderParent.h"

namespace mozilla::layers {
class BufferRecycleBin;
}  // namespace mozilla::layers

namespace mozilla {

class KnowsCompositorVideo : public layers::KnowsCompositor {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(KnowsCompositorVideo, override)

  layers::TextureForwarder* GetTextureForwarder() override;
  layers::LayersIPCActor* GetLayersIPCActor() override;

  static already_AddRefed<KnowsCompositorVideo> TryCreateForIdentifier(
      const layers::TextureFactoryIdentifier& aIdentifier);

 private:
  KnowsCompositorVideo() = default;
  virtual ~KnowsCompositorVideo() = default;
};

using mozilla::ipc::IPCResult;

class RemoteVideoDecoderChild : public RemoteDecoderChild {
 public:
  explicit RemoteVideoDecoderChild(RemoteDecodeIn aLocation);

  MOZ_IS_CLASS_INIT MediaResult
  InitIPDL(const VideoInfo& aVideoInfo, float aFramerate,
           const CreateDecoderParams::OptionSet& aOptions,
           mozilla::Maybe<layers::TextureFactoryIdentifier> aIdentifier,
           const Maybe<uint64_t>& aMediaEngineId,
           const Maybe<TrackingId>& aTrackingId);

  MediaResult ProcessOutput(DecodedOutputIPDL&& aDecodedData) override;

 private:
  RefPtr<mozilla::layers::BufferRecycleBin> mBufferRecycleBin;
};

class RemoteVideoDecoderParent final : public RemoteDecoderParent {
 public:
  RemoteVideoDecoderParent(
      RemoteDecoderManagerParent* aParent, const VideoInfo& aVideoInfo,
      float aFramerate, const CreateDecoderParams::OptionSet& aOptions,
      const Maybe<layers::TextureFactoryIdentifier>& aIdentifier,
      nsISerialEventTarget* aManagerThread, TaskQueue* aDecodeTaskQueue,
      const Maybe<uint64_t>& aMediaEngineId, Maybe<TrackingId> aTrackingId);

 protected:
  IPCResult RecvConstruct(ConstructResolver&& aResolver) override;

  MediaResult ProcessDecodedData(MediaDataDecoder::DecodedData&& aData,
                                 DecodedOutputIPDL& aDecodedData) override;

 private:
  // Can only be accessed from the manager thread
  // Note: we can't keep a reference to the original VideoInfo here
  // because unlike in typical MediaDataDecoder situations, we're being
  // passed a deserialized VideoInfo from RecvPRemoteDecoderConstructor
  // which is destroyed when RecvPRemoteDecoderConstructor returns.
  const VideoInfo mVideoInfo;
  const float mFramerate;
  RefPtr<KnowsCompositorVideo> mKnowsCompositor;
};

}  // namespace mozilla

#endif  // include_dom_media_ipc_RemoteVideoDecoderChild_h
