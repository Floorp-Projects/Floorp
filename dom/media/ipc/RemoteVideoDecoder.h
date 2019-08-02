/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_media_ipc_RemoteVideoDecoderChild_h
#define include_dom_media_ipc_RemoteVideoDecoderChild_h
#include "RemoteDecoderChild.h"
#include "RemoteDecoderParent.h"

namespace mozilla {
namespace layers {
class BufferRecycleBin;
}  // namespace layers
}  // namespace mozilla

namespace mozilla {

class KnowsCompositorVideo;
using mozilla::ipc::IPCResult;

class RemoteVideoDecoderChild : public RemoteDecoderChild {
 public:
  explicit RemoteVideoDecoderChild(bool aRecreatedOnCrash = false);

  MOZ_IS_CLASS_INIT
  MediaResult InitIPDL(const VideoInfo& aVideoInfo, float aFramerate,
                       const CreateDecoderParams::OptionSet& aOptions,
                       const layers::TextureFactoryIdentifier* aIdentifier);

  IPCResult RecvOutput(const DecodedOutputIPDL& aDecodedData) override;

 private:
  RefPtr<mozilla::layers::Image> DeserializeImage(
      const SurfaceDescriptorBuffer& sdBuffer, const IntSize& aPicSize);

  RefPtr<mozilla::layers::BufferRecycleBin> mBufferRecycleBin;
};

class GpuRemoteVideoDecoderChild final : public RemoteVideoDecoderChild {
 public:
  explicit GpuRemoteVideoDecoderChild();

  MOZ_IS_CLASS_INIT
  MediaResult InitIPDL(const VideoInfo& aVideoInfo, float aFramerate,
                       const CreateDecoderParams::OptionSet& aOptions,
                       const layers::TextureFactoryIdentifier& aIdentifier);
};

class RemoteVideoDecoderParent final : public RemoteDecoderParent {
 public:
  RemoteVideoDecoderParent(
      RemoteDecoderManagerParent* aParent, const VideoInfo& aVideoInfo,
      float aFramerate, const CreateDecoderParams::OptionSet& aOptions,
      const Maybe<layers::TextureFactoryIdentifier>& aIdentifier,
      TaskQueue* aManagerTaskQueue, TaskQueue* aDecodeTaskQueue, bool* aSuccess,
      nsCString* aErrorDescription);

 protected:
  MediaResult ProcessDecodedData(
      const MediaDataDecoder::DecodedData& aData) override;

 private:
  // Can only be accessed from the manager thread
  // Note: we can't keep a reference to the original VideoInfo here
  // because unlike in typical MediaDataDecoder situations, we're being
  // passed a deserialized VideoInfo from RecvPRemoteDecoderConstructor
  // which is destroyed when RecvPRemoteDecoderConstructor returns.
  const VideoInfo mVideoInfo;

  RefPtr<KnowsCompositorVideo> mKnowsCompositor;
};

}  // namespace mozilla

#endif  // include_dom_media_ipc_RemoteVideoDecoderChild_h
