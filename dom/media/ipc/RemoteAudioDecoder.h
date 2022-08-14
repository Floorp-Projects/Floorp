/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_media_ipc_RemoteAudioDecoderChild_h
#define include_dom_media_ipc_RemoteAudioDecoderChild_h
#include "RemoteDecoderChild.h"
#include "RemoteDecoderParent.h"

namespace mozilla {

using mozilla::ipc::IPCResult;

class RemoteAudioDecoderChild final : public RemoteDecoderChild {
 public:
  explicit RemoteAudioDecoderChild(RemoteDecodeIn aLocation);

  MOZ_IS_CLASS_INIT
  MediaResult InitIPDL(const AudioInfo& aAudioInfo,
                       const CreateDecoderParams::OptionSet& aOptions,
                       const Maybe<uint64_t>& aMediaEngineId);

  MediaResult ProcessOutput(DecodedOutputIPDL&& aDecodedData) override;
};

class RemoteAudioDecoderParent final : public RemoteDecoderParent {
 public:
  RemoteAudioDecoderParent(RemoteDecoderManagerParent* aParent,
                           const AudioInfo& aAudioInfo,
                           const CreateDecoderParams::OptionSet& aOptions,
                           nsISerialEventTarget* aManagerThread,
                           TaskQueue* aDecodeTaskQueue,
                           Maybe<uint64_t> aMediaEngineId);

 protected:
  IPCResult RecvConstruct(ConstructResolver&& aResolver) override;
  MediaResult ProcessDecodedData(MediaDataDecoder::DecodedData&& aData,
                                 DecodedOutputIPDL& aDecodedData) override;

 private:
  // Can only be accessed from the manager thread
  // Note: we can't keep a reference to the original AudioInfo here
  // because unlike in typical MediaDataDecoder situations, we're being
  // passed a deserialized AudioInfo from RecvPRemoteDecoderConstructor
  // which is destroyed when RecvPRemoteDecoderConstructor returns.
  const AudioInfo mAudioInfo;
  const CreateDecoderParams::OptionSet mOptions;
};

}  // namespace mozilla

#endif  // include_dom_media_ipc_RemoteAudioDecoderChild_h
