/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteAudioDecoder.h"

#include "MediaDataDecoderProxy.h"
#include "OpusDecoder.h"
#include "RemoteDecoderManagerChild.h"
#include "VorbisDecoder.h"
#include "WAVDecoder.h"
#include "mozilla/PodOperations.h"

namespace mozilla {

RemoteAudioDecoderChild::RemoteAudioDecoderChild() : RemoteDecoderChild() {}

MediaResult RemoteAudioDecoderChild::ProcessOutput(
    DecodedOutputIPDL&& aDecodedData) {
  AssertOnManagerThread();
  MOZ_ASSERT(aDecodedData.type() ==
             DecodedOutputIPDL::TArrayOfRemoteAudioDataIPDL);
  nsTArray<RemoteAudioDataIPDL>& arrayData =
      aDecodedData.get_ArrayOfRemoteAudioDataIPDL();

  for (auto&& data : arrayData) {
    AlignedAudioBuffer alignedAudioBuffer;
    // Use std::min to make sure we can't overrun our buffer in case someone is
    // fibbing about buffer sizes.
    if (!alignedAudioBuffer.SetLength(
            std::min((unsigned long)data.audioDataBufferSize(),
                     (unsigned long)data.buffer().Size<AudioDataValue>()))) {
      // OOM
      return MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__);
    }
    PodCopy(alignedAudioBuffer.Data(), data.buffer().get<AudioDataValue>(),
            alignedAudioBuffer.Length());

    RefPtr<AudioData> audio = new AudioData(
        data.base().offset(), data.base().time(), std::move(alignedAudioBuffer),
        data.channels(), data.rate(), data.channelMap());

    mDecodedData.AppendElement(std::move(audio));
  }
  return NS_OK;
}

MediaResult RemoteAudioDecoderChild::InitIPDL(
    const AudioInfo& aAudioInfo,
    const CreateDecoderParams::OptionSet& aOptions) {
  RefPtr<RemoteDecoderManagerChild> manager =
      RemoteDecoderManagerChild::GetRDDProcessSingleton();

  // The manager isn't available because RemoteDecoderManagerChild has been
  // initialized with null end points and we don't want to decode video on RDD
  // process anymore. Return false here so that we can fallback to other PDMs.
  if (!manager) {
    return MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                       RESULT_DETAIL("RemoteDecoderManager is not available."));
  }

  if (!manager->CanSend()) {
    return MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                       RESULT_DETAIL("RemoteDecoderManager unable to send."));
  }

  mIPDLSelfRef = this;
  bool success = false;
  nsCString errorDescription;
  Unused << manager->SendPRemoteDecoderConstructor(
      this, aAudioInfo, aOptions, Nothing(), &success, &errorDescription);
  return success ? MediaResult(NS_OK)
                 : MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR, errorDescription);
}

RemoteAudioDecoderParent::RemoteAudioDecoderParent(
    RemoteDecoderManagerParent* aParent, const AudioInfo& aAudioInfo,
    const CreateDecoderParams::OptionSet& aOptions,
    nsISerialEventTarget* aManagerThread, TaskQueue* aDecodeTaskQueue,
    bool* aSuccess, nsCString* aErrorDescription)
    : RemoteDecoderParent(aParent, aManagerThread, aDecodeTaskQueue),
      mAudioInfo(aAudioInfo) {
  CreateDecoderParams params(mAudioInfo);
  params.mOptions = aOptions;
  MediaResult error(NS_OK);
  params.mError = &error;

  RefPtr<MediaDataDecoder> decoder;
  if (VorbisDataDecoder::IsVorbis(params.mConfig.mMimeType)) {
    decoder = new VorbisDataDecoder(params);
  } else if (OpusDataDecoder::IsOpus(params.mConfig.mMimeType)) {
    decoder = new OpusDataDecoder(params);
  } else if (WaveDataDecoder::IsWave(params.mConfig.mMimeType)) {
    decoder = new WaveDataDecoder(params);
  }

  if (NS_FAILED(error)) {
    MOZ_ASSERT(aErrorDescription);
    *aErrorDescription = error.Description();
  }

  if (decoder) {
    mDecoder = new MediaDataDecoderProxy(decoder.forget(),
                                         do_AddRef(mDecodeTaskQueue.get()));
  }

  *aSuccess = !!mDecoder;
}

MediaResult RemoteAudioDecoderParent::ProcessDecodedData(
    const MediaDataDecoder::DecodedData& aData,
    DecodedOutputIPDL& aDecodedData) {
  MOZ_ASSERT(OnManagerThread());

  nsTArray<RemoteAudioDataIPDL> array;

  for (const auto& data : aData) {
    MOZ_ASSERT(data->mType == MediaData::Type::AUDIO_DATA,
               "Can only decode audio using RemoteAudioDecoderParent!");
    AudioData* audio = static_cast<AudioData*>(data.get());

    MOZ_ASSERT(audio->Data().Elements(),
               "Decoded audio must output an AlignedAudioBuffer "
               "to be used with RemoteAudioDecoderParent");

    ShmemBuffer buffer =
        AllocateBuffer(audio->Data().Length() * sizeof(AudioDataValue));
    if (!buffer.Valid()) {
      return MediaResult(NS_ERROR_OUT_OF_MEMORY,
                         "ShmemBuffer::Get failed in "
                         "RemoteAudioDecoderParent::ProcessDecodedData");
    }

    PodCopy(buffer.Get().get<AudioDataValue>(), audio->Data().Elements(),
            audio->Data().Length());

    RemoteAudioDataIPDL output(
        MediaDataIPDL(data->mOffset, data->mTime, data->mTimecode,
                      data->mDuration, data->mKeyframe),
        audio->mChannels, audio->mRate, audio->mChannelMap,
        audio->Data().Length(), std::move(buffer.Get()));
    array.AppendElement(output);
  }

  // With the new possiblity of batch decodes, we can't always move the
  // results directly into DecodedOutputIPDL.  If there are already
  // elements, we should append the new results.
  if (aDecodedData.type() == DecodedOutputIPDL::TArrayOfRemoteAudioDataIPDL) {
    aDecodedData.get_ArrayOfRemoteAudioDataIPDL().AppendElements(
        std::move(array));
  } else {
    aDecodedData = std::move(array);
  }

  return NS_OK;
}

}  // namespace mozilla
