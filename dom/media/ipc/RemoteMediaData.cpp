/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteMediaData.h"

#include "PerformanceRecorder.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/dom/MediaIPCUtils.h"
#include "mozilla/ipc/Shmem.h"

namespace mozilla {

bool RemoteArrayOfByteBuffer::AllocateShmem(
    size_t aSize, std::function<ShmemBuffer(size_t)>& aAllocator) {
  ShmemBuffer buffer = aAllocator(aSize);
  if (!buffer.Valid()) {
    return false;
  }
  mBuffers.emplace(std::move(buffer.Get()));
  return true;
}

uint8_t* RemoteArrayOfByteBuffer::BuffersStartAddress() const {
  MOZ_ASSERT(mBuffers);
  return mBuffers->get<uint8_t>();
}

bool RemoteArrayOfByteBuffer::Check(size_t aOffset, size_t aSizeInBytes) const {
  return mBuffers && mBuffers->IsReadable() &&
         detail::IsAddValid(aOffset, aSizeInBytes) &&
         aOffset + aSizeInBytes <= mBuffers->Size<uint8_t>();
}

void RemoteArrayOfByteBuffer::Write(size_t aOffset, const void* aSourceAddr,
                                    size_t aSizeInBytes) {
  if (!aSizeInBytes) {
    return;
  }
  MOZ_DIAGNOSTIC_ASSERT(Check(aOffset, aSizeInBytes),
                        "Allocated Shmem is too small");
  memcpy(BuffersStartAddress() + aOffset, aSourceAddr, aSizeInBytes);
}

RemoteArrayOfByteBuffer::RemoteArrayOfByteBuffer() = default;

RemoteArrayOfByteBuffer::RemoteArrayOfByteBuffer(
    const nsTArray<RefPtr<MediaByteBuffer>>& aArray,
    std::function<ShmemBuffer(size_t)>& aAllocator) {
  // Determine the total size we will need for this object.
  size_t totalSize = 0;
  for (const auto& buffer : aArray) {
    if (buffer) {
      totalSize += buffer->Length();
    }
  }
  if (totalSize) {
    if (!AllocateShmem(totalSize, aAllocator)) {
      return;
    }
  }
  size_t offset = 0;
  for (const auto& buffer : aArray) {
    size_t sizeBuffer = buffer ? buffer->Length() : 0;
    if (totalSize && sizeBuffer) {
      Write(offset, buffer->Elements(), sizeBuffer);
    }
    mOffsets.AppendElement(OffsetEntry{offset, sizeBuffer});
    offset += sizeBuffer;
  }
  mIsValid = true;
}

RemoteArrayOfByteBuffer& RemoteArrayOfByteBuffer::operator=(
    RemoteArrayOfByteBuffer&& aOther) noexcept {
  mIsValid = aOther.mIsValid;
  mBuffers = std::move(aOther.mBuffers);
  mOffsets = std::move(aOther.mOffsets);
  aOther.mIsValid = false;
  return *this;
}

RemoteArrayOfByteBuffer::~RemoteArrayOfByteBuffer() = default;

already_AddRefed<MediaByteBuffer> RemoteArrayOfByteBuffer::MediaByteBufferAt(
    size_t aIndex) const {
  MOZ_ASSERT(aIndex < Count());
  const OffsetEntry& entry = mOffsets[aIndex];
  if (!mBuffers || !std::get<1>(entry)) {
    // It's an empty one.
    return nullptr;
  }
  size_t entrySize = std::get<1>(entry);
  if (!Check(std::get<0>(entry), entrySize)) {
    // This Shmem is corrupted and can't contain the data we are about to
    // retrieve. We return an empty array instead of asserting to allow for
    // recovery.
    return nullptr;
  }
  RefPtr<MediaByteBuffer> buffer = new MediaByteBuffer(entrySize);
  buffer->SetLength(entrySize);
  memcpy(buffer->Elements(), mBuffers->get<uint8_t>() + std::get<0>(entry),
         entrySize);
  return buffer.forget();
}

/*static */ void ipc::IPDLParamTraits<RemoteArrayOfByteBuffer>::Write(
    IPC::MessageWriter* aWriter, ipc::IProtocol* aActor,
    const RemoteArrayOfByteBuffer& aVar) {
  WriteIPDLParam(aWriter, aActor, aVar.mIsValid);
  // We need the following gymnastic as the Shmem transfered over IPC will be
  // revoked. We must create a temporary one instead so that it can be recycled
  // later back into the original ShmemPool.
  if (aVar.mBuffers) {
    WriteIPDLParam(aWriter, aActor, Some(ipc::Shmem(*aVar.mBuffers)));
  } else {
    WriteIPDLParam(aWriter, aActor, Maybe<ipc::Shmem>());
  }
  WriteIPDLParam(aWriter, aActor, aVar.mOffsets);
}

/* static */ bool ipc::IPDLParamTraits<RemoteArrayOfByteBuffer>::Read(
    IPC::MessageReader* aReader, mozilla::ipc::IProtocol* aActor,
    RemoteArrayOfByteBuffer* aVar) {
  return ReadIPDLParam(aReader, aActor, &aVar->mIsValid) &&
         ReadIPDLParam(aReader, aActor, &aVar->mBuffers) &&
         ReadIPDLParam(aReader, aActor, &aVar->mOffsets);
}

bool ArrayOfRemoteMediaRawData::Fill(
    const nsTArray<RefPtr<MediaRawData>>& aData,
    std::function<ShmemBuffer(size_t)>&& aAllocator) {
  nsTArray<AlignedByteBuffer> dataBuffers(aData.Length());
  nsTArray<AlignedByteBuffer> alphaBuffers(aData.Length());
  nsTArray<RefPtr<MediaByteBuffer>> extraDataBuffers(aData.Length());
  int32_t height = 0;
  for (auto&& entry : aData) {
    dataBuffers.AppendElement(std::move(entry->mBuffer));
    alphaBuffers.AppendElement(std::move(entry->mAlphaBuffer));
    extraDataBuffers.AppendElement(std::move(entry->mExtraData));
    if (auto&& info = entry->mTrackInfo; info && info->GetAsVideoInfo()) {
      height = info->GetAsVideoInfo()->mImage.height;
    }
    mSamples.AppendElement(RemoteMediaRawData{
        MediaDataIPDL(entry->mOffset, entry->mTime, entry->mTimecode,
                      entry->mDuration, entry->mKeyframe),
        entry->mEOS, height, entry->mOriginalPresentationWindow,
        entry->mCrypto.IsEncrypted() && entry->mShouldCopyCryptoToRemoteRawData
            ? Some(CryptoInfo{
                  entry->mCrypto.mCryptoScheme,
                  entry->mCrypto.mIV,
                  entry->mCrypto.mKeyId,
                  entry->mCrypto.mPlainSizes,
                  entry->mCrypto.mEncryptedSizes,
              })
            : Nothing()});
  }
  PerformanceRecorder<PlaybackStage> perfRecorder(MediaStage::CopyDemuxedData,
                                                  height);
  mBuffers = RemoteArrayOfByteBuffer(dataBuffers, aAllocator);
  if (!mBuffers.IsValid()) {
    return false;
  }
  mAlphaBuffers = RemoteArrayOfByteBuffer(alphaBuffers, aAllocator);
  if (!mAlphaBuffers.IsValid()) {
    return false;
  }
  mExtraDatas = RemoteArrayOfByteBuffer(extraDataBuffers, aAllocator);
  if (!mExtraDatas.IsValid()) {
    return false;
  }
  perfRecorder.Record();
  return true;
}

already_AddRefed<MediaRawData> ArrayOfRemoteMediaRawData::ElementAt(
    size_t aIndex) const {
  if (!IsValid()) {
    return nullptr;
  }
  MOZ_ASSERT(aIndex < Count());
  MOZ_DIAGNOSTIC_ASSERT(mBuffers.Count() == Count() &&
                            mAlphaBuffers.Count() == Count() &&
                            mExtraDatas.Count() == Count(),
                        "Something ain't right here");
  const auto& sample = mSamples[aIndex];
  PerformanceRecorder<PlaybackStage> perfRecorder(MediaStage::CopyDemuxedData,
                                                  sample.mHeight);
  AlignedByteBuffer data = mBuffers.AlignedBufferAt<uint8_t>(aIndex);
  if (mBuffers.SizeAt(aIndex) && !data) {
    // OOM
    return nullptr;
  }
  AlignedByteBuffer alphaData = mAlphaBuffers.AlignedBufferAt<uint8_t>(aIndex);
  if (mAlphaBuffers.SizeAt(aIndex) && !alphaData) {
    // OOM
    return nullptr;
  }
  RefPtr<MediaRawData> rawData;
  if (mAlphaBuffers.SizeAt(aIndex)) {
    rawData = new MediaRawData(std::move(data), std::move(alphaData));
  } else {
    rawData = new MediaRawData(std::move(data));
  }
  rawData->mOffset = sample.mBase.offset();
  rawData->mTime = sample.mBase.time();
  rawData->mTimecode = sample.mBase.timecode();
  rawData->mDuration = sample.mBase.duration();
  rawData->mKeyframe = sample.mBase.keyframe();
  rawData->mEOS = sample.mEOS;
  rawData->mExtraData = mExtraDatas.MediaByteBufferAt(aIndex);
  if (sample.mCryptoConfig) {
    CryptoSample& cypto = rawData->GetWritableCrypto();
    cypto.mCryptoScheme = sample.mCryptoConfig->mEncryptionScheme();
    cypto.mIV = std::move(sample.mCryptoConfig->mIV());
    cypto.mIVSize = cypto.mIV.Length();
    cypto.mKeyId = std::move(sample.mCryptoConfig->mKeyId());
    cypto.mPlainSizes = std::move(sample.mCryptoConfig->mClearBytes());
    cypto.mEncryptedSizes = std::move(sample.mCryptoConfig->mCipherBytes());
  }
  perfRecorder.Record();
  return rawData.forget();
}

/*static */ void ipc::IPDLParamTraits<ArrayOfRemoteMediaRawData*>::Write(
    IPC::MessageWriter* aWriter, ipc::IProtocol* aActor,
    ArrayOfRemoteMediaRawData* aVar) {
  WriteIPDLParam(aWriter, aActor, std::move(aVar->mSamples));
  WriteIPDLParam(aWriter, aActor, std::move(aVar->mBuffers));
  WriteIPDLParam(aWriter, aActor, std::move(aVar->mAlphaBuffers));
  WriteIPDLParam(aWriter, aActor, std::move(aVar->mExtraDatas));
}

/* static */ bool ipc::IPDLParamTraits<ArrayOfRemoteMediaRawData*>::Read(
    IPC::MessageReader* aReader, mozilla::ipc::IProtocol* aActor,
    RefPtr<ArrayOfRemoteMediaRawData>* aVar) {
  auto array = MakeRefPtr<ArrayOfRemoteMediaRawData>();
  if (!ReadIPDLParam(aReader, aActor, &array->mSamples) ||
      !ReadIPDLParam(aReader, aActor, &array->mBuffers) ||
      !ReadIPDLParam(aReader, aActor, &array->mAlphaBuffers) ||
      !ReadIPDLParam(aReader, aActor, &array->mExtraDatas)) {
    return false;
  }
  *aVar = std::move(array);
  return true;
}

/* static */ void
ipc::IPDLParamTraits<ArrayOfRemoteMediaRawData::RemoteMediaRawData>::Write(
    IPC::MessageWriter* aWriter, ipc::IProtocol* aActor,
    const paramType& aVar) {
  WriteIPDLParam(aWriter, aActor, aVar.mBase);
  WriteIPDLParam(aWriter, aActor, aVar.mEOS);
  WriteIPDLParam(aWriter, aActor, aVar.mHeight);
  WriteIPDLParam(aWriter, aActor, aVar.mOriginalPresentationWindow);
  WriteIPDLParam(aWriter, aActor, aVar.mCryptoConfig);
}

/* static */ bool
ipc::IPDLParamTraits<ArrayOfRemoteMediaRawData::RemoteMediaRawData>::Read(
    IPC::MessageReader* aReader, ipc::IProtocol* aActor, paramType* aVar) {
  MediaDataIPDL mBase;
  return ReadIPDLParam(aReader, aActor, &aVar->mBase) &&
         ReadIPDLParam(aReader, aActor, &aVar->mEOS) &&
         ReadIPDLParam(aReader, aActor, &aVar->mHeight) &&
         ReadIPDLParam(aReader, aActor, &aVar->mOriginalPresentationWindow) &&
         ReadIPDLParam(aReader, aActor, &aVar->mCryptoConfig);
};

bool ArrayOfRemoteAudioData::Fill(
    const nsTArray<RefPtr<AudioData>>& aData,
    std::function<ShmemBuffer(size_t)>&& aAllocator) {
  nsTArray<AlignedAudioBuffer> dataBuffers(aData.Length());
  for (auto&& entry : aData) {
    dataBuffers.AppendElement(std::move(entry->mAudioData));
    mSamples.AppendElement(RemoteAudioData{
        MediaDataIPDL(entry->mOffset, entry->mTime, entry->mTimecode,
                      entry->mDuration, entry->mKeyframe),
        entry->mChannels, entry->mRate, uint32_t(entry->mChannelMap),
        entry->mOriginalTime, entry->mTrimWindow, entry->mFrames,
        entry->mDataOffset});
  }
  mBuffers = RemoteArrayOfByteBuffer(dataBuffers, aAllocator);
  if (!mBuffers.IsValid()) {
    return false;
  }
  return true;
}

already_AddRefed<AudioData> ArrayOfRemoteAudioData::ElementAt(
    size_t aIndex) const {
  if (!IsValid()) {
    return nullptr;
  }
  MOZ_ASSERT(aIndex < Count());
  MOZ_DIAGNOSTIC_ASSERT(mBuffers.Count() == Count(),
                        "Something ain't right here");
  const auto& sample = mSamples[aIndex];
  AlignedAudioBuffer data = mBuffers.AlignedBufferAt<AudioDataValue>(aIndex);
  if (mBuffers.SizeAt(aIndex) && !data) {
    // OOM
    return nullptr;
  }
  auto audioData = MakeRefPtr<AudioData>(
      sample.mBase.offset(), sample.mBase.time(), std::move(data),
      sample.mChannels, sample.mRate, sample.mChannelMap);
  // An AudioData's duration is set at construction time based on the size of
  // the provided buffer. However, if a trim window is set, this value will be
  // incorrect. We have to re-set it to what it actually was.
  audioData->mDuration = sample.mBase.duration();
  audioData->mOriginalTime = sample.mOriginalTime;
  audioData->mTrimWindow = sample.mTrimWindow;
  audioData->mFrames = sample.mFrames;
  audioData->mDataOffset = sample.mDataOffset;
  return audioData.forget();
}

/*static */ void ipc::IPDLParamTraits<ArrayOfRemoteAudioData*>::Write(
    IPC::MessageWriter* aWriter, ipc::IProtocol* aActor,
    ArrayOfRemoteAudioData* aVar) {
  WriteIPDLParam(aWriter, aActor, std::move(aVar->mSamples));
  WriteIPDLParam(aWriter, aActor, std::move(aVar->mBuffers));
}

/* static */ bool ipc::IPDLParamTraits<ArrayOfRemoteAudioData*>::Read(
    IPC::MessageReader* aReader, mozilla::ipc::IProtocol* aActor,
    RefPtr<ArrayOfRemoteAudioData>* aVar) {
  auto array = MakeRefPtr<ArrayOfRemoteAudioData>();
  if (!ReadIPDLParam(aReader, aActor, &array->mSamples) ||
      !ReadIPDLParam(aReader, aActor, &array->mBuffers)) {
    return false;
  }
  *aVar = std::move(array);
  return true;
}

/* static */ void
ipc::IPDLParamTraits<ArrayOfRemoteAudioData::RemoteAudioData>::Write(
    IPC::MessageWriter* aWriter, ipc::IProtocol* aActor,
    const paramType& aVar) {
  WriteIPDLParam(aWriter, aActor, aVar.mBase);
  WriteIPDLParam(aWriter, aActor, aVar.mChannels);
  WriteIPDLParam(aWriter, aActor, aVar.mRate);
  WriteIPDLParam(aWriter, aActor, aVar.mChannelMap);
  WriteIPDLParam(aWriter, aActor, aVar.mOriginalTime);
  WriteIPDLParam(aWriter, aActor, aVar.mTrimWindow);
  WriteIPDLParam(aWriter, aActor, aVar.mFrames);
  WriteIPDLParam(aWriter, aActor, aVar.mDataOffset);
}

/* static */ bool
ipc::IPDLParamTraits<ArrayOfRemoteAudioData::RemoteAudioData>::Read(
    IPC::MessageReader* aReader, ipc::IProtocol* aActor, paramType* aVar) {
  MediaDataIPDL mBase;
  if (!ReadIPDLParam(aReader, aActor, &aVar->mBase) ||
      !ReadIPDLParam(aReader, aActor, &aVar->mChannels) ||
      !ReadIPDLParam(aReader, aActor, &aVar->mRate) ||
      !ReadIPDLParam(aReader, aActor, &aVar->mChannelMap) ||
      !ReadIPDLParam(aReader, aActor, &aVar->mOriginalTime) ||
      !ReadIPDLParam(aReader, aActor, &aVar->mTrimWindow) ||
      !ReadIPDLParam(aReader, aActor, &aVar->mFrames) ||
      !ReadIPDLParam(aReader, aActor, &aVar->mDataOffset)) {
    return false;
  }
  return true;
};

}  // namespace mozilla
