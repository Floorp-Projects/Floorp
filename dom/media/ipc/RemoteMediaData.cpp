/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteMediaData.h"

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
  if (!mBuffers || !Get<1>(entry)) {
    // It's an empty one.
    return nullptr;
  }
  size_t entrySize = Get<1>(entry);
  if (!Check(Get<0>(entry), entrySize)) {
    // This Shmem is corrupted and can't contain the data we are about to
    // retrieve. We return an empty array instead of asserting to allow for
    // recovery.
    return nullptr;
  }
  RefPtr<MediaByteBuffer> buffer = new MediaByteBuffer(entrySize);
  buffer->SetLength(entrySize);
  memcpy(buffer->Elements(), mBuffers->get<uint8_t>() + Get<0>(entry),
         entrySize);
  return buffer.forget();
}

/*static */ void ipc::IPDLParamTraits<RemoteArrayOfByteBuffer>::Write(
    IPC::Message* aMsg, ipc::IProtocol* aActor,
    const RemoteArrayOfByteBuffer& aVar) {
  WriteIPDLParam(aMsg, aActor, aVar.mIsValid);
  // We need the following gymnastic as the Shmem transfered over IPC will be
  // revoked. We must create a temporary one instead so that it can be recycled
  // later back into the original ShmemPool.
  if (aVar.mBuffers) {
    WriteIPDLParam(aMsg, aActor, Some(ipc::Shmem(*aVar.mBuffers)));
  } else {
    WriteIPDLParam(aMsg, aActor, Maybe<ipc::Shmem>());
  }
  WriteIPDLParam(aMsg, aActor, aVar.mOffsets);
}

/* static */ bool ipc::IPDLParamTraits<RemoteArrayOfByteBuffer>::Read(
    const IPC::Message* aMsg, PickleIterator* aIter,
    mozilla::ipc::IProtocol* aActor, RemoteArrayOfByteBuffer* aVar) {
  return ReadIPDLParam(aMsg, aIter, aActor, &aVar->mIsValid) &&
         ReadIPDLParam(aMsg, aIter, aActor, &aVar->mBuffers) &&
         ReadIPDLParam(aMsg, aIter, aActor, &aVar->mOffsets);
}

bool ArrayOfRemoteMediaRawData::Fill(
    const nsTArray<RefPtr<MediaRawData>>& aData,
    std::function<ShmemBuffer(size_t)>&& aAllocator) {
  nsTArray<AlignedByteBuffer> dataBuffers(aData.Length());
  nsTArray<AlignedByteBuffer> alphaBuffers(aData.Length());
  nsTArray<RefPtr<MediaByteBuffer>> extraDataBuffers(aData.Length());
  for (auto&& entry : aData) {
    dataBuffers.AppendElement(std::move(entry->mBuffer));
    alphaBuffers.AppendElement(std::move(entry->mAlphaBuffer));
    extraDataBuffers.AppendElement(std::move(entry->mExtraData));
    mSamples.AppendElement(RemoteMediaRawData{
        MediaDataIPDL(entry->mOffset, entry->mTime, entry->mTimecode,
                      entry->mDuration, entry->mKeyframe),
        entry->mEOS, entry->mDiscardPadding,
        entry->mOriginalPresentationWindow});
  }
  mBuffers = RemoteArrayOfByteBuffer(dataBuffers, aAllocator);
  if (!mBuffers.IsValid()) {
    return false;
  }
  mAlphaBuffers = RemoteArrayOfByteBuffer(alphaBuffers, aAllocator);
  if (!mAlphaBuffers.IsValid()) {
    return false;
  }
  mExtraDatas = RemoteArrayOfByteBuffer(extraDataBuffers, aAllocator);
  return mExtraDatas.IsValid();
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
  rawData->mDiscardPadding = sample.mDiscardPadding;
  rawData->mExtraData = mExtraDatas.MediaByteBufferAt(aIndex);
  return rawData.forget();
}

/*static */ void ipc::IPDLParamTraits<ArrayOfRemoteMediaRawData*>::Write(
    IPC::Message* aMsg, ipc::IProtocol* aActor,
    ArrayOfRemoteMediaRawData* aVar) {
  WriteIPDLParam(aMsg, aActor, std::move(aVar->mSamples));
  WriteIPDLParam(aMsg, aActor, std::move(aVar->mBuffers));
  WriteIPDLParam(aMsg, aActor, std::move(aVar->mAlphaBuffers));
  WriteIPDLParam(aMsg, aActor, std::move(aVar->mExtraDatas));
}

/* static */ bool ipc::IPDLParamTraits<ArrayOfRemoteMediaRawData*>::Read(
    const IPC::Message* aMsg, PickleIterator* aIter,
    mozilla::ipc::IProtocol* aActor, RefPtr<ArrayOfRemoteMediaRawData>* aVar) {
  auto array = MakeRefPtr<ArrayOfRemoteMediaRawData>();
  if (!ReadIPDLParam(aMsg, aIter, aActor, &array->mSamples) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &array->mBuffers) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &array->mAlphaBuffers) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &array->mExtraDatas)) {
    return false;
  }
  *aVar = std::move(array);
  return true;
}

/* static */ void
ipc::IPDLParamTraits<ArrayOfRemoteMediaRawData::RemoteMediaRawData>::Write(
    IPC::Message* aMsg, ipc::IProtocol* aActor, const paramType& aVar) {
  WriteIPDLParam(aMsg, aActor, aVar.mBase);
  WriteIPDLParam(aMsg, aActor, aVar.mEOS);
  WriteIPDLParam(aMsg, aActor, aVar.mDiscardPadding);
  WriteIPDLParam(aMsg, aActor, aVar.mOriginalPresentationWindow);
}

/* static */ bool
ipc::IPDLParamTraits<ArrayOfRemoteMediaRawData::RemoteMediaRawData>::Read(
    const IPC::Message* aMsg, PickleIterator* aIter, ipc::IProtocol* aActor,
    paramType* aVar) {
  MediaDataIPDL mBase;
  return ReadIPDLParam(aMsg, aIter, aActor, &aVar->mBase) &&
         ReadIPDLParam(aMsg, aIter, aActor, &aVar->mEOS) &&
         ReadIPDLParam(aMsg, aIter, aActor, &aVar->mDiscardPadding) &&
         ReadIPDLParam(aMsg, aIter, aActor, &aVar->mOriginalPresentationWindow);
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
    IPC::Message* aMsg, ipc::IProtocol* aActor, ArrayOfRemoteAudioData* aVar) {
  WriteIPDLParam(aMsg, aActor, std::move(aVar->mSamples));
  WriteIPDLParam(aMsg, aActor, std::move(aVar->mBuffers));
}

/* static */ bool ipc::IPDLParamTraits<ArrayOfRemoteAudioData*>::Read(
    const IPC::Message* aMsg, PickleIterator* aIter,
    mozilla::ipc::IProtocol* aActor, RefPtr<ArrayOfRemoteAudioData>* aVar) {
  auto array = MakeRefPtr<ArrayOfRemoteAudioData>();
  if (!ReadIPDLParam(aMsg, aIter, aActor, &array->mSamples) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &array->mBuffers)) {
    return false;
  }
  *aVar = std::move(array);
  return true;
}

/* static */ void
ipc::IPDLParamTraits<ArrayOfRemoteAudioData::RemoteAudioData>::Write(
    IPC::Message* aMsg, ipc::IProtocol* aActor, const paramType& aVar) {
  WriteIPDLParam(aMsg, aActor, aVar.mBase);
  WriteIPDLParam(aMsg, aActor, aVar.mChannels);
  WriteIPDLParam(aMsg, aActor, aVar.mRate);
  WriteIPDLParam(aMsg, aActor, aVar.mChannelMap);
  WriteIPDLParam(aMsg, aActor, aVar.mOriginalTime);
  WriteIPDLParam(aMsg, aActor, aVar.mTrimWindow);
  WriteIPDLParam(aMsg, aActor, aVar.mFrames);
  WriteIPDLParam(aMsg, aActor, aVar.mDataOffset);
}

/* static */ bool
ipc::IPDLParamTraits<ArrayOfRemoteAudioData::RemoteAudioData>::Read(
    const IPC::Message* aMsg, PickleIterator* aIter, ipc::IProtocol* aActor,
    paramType* aVar) {
  MediaDataIPDL mBase;
  if (!ReadIPDLParam(aMsg, aIter, aActor, &aVar->mBase) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aVar->mChannels) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aVar->mRate) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aVar->mChannelMap) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aVar->mOriginalTime) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aVar->mTrimWindow) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aVar->mFrames) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aVar->mDataOffset)) {
    return false;
  }
  return true;
};

}  // namespace mozilla
