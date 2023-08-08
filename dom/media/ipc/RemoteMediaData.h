/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_media_ipc_RemoteMediaData_h
#define mozilla_dom_media_ipc_RemoteMediaData_h

#include <functional>

#include "MediaData.h"
#include "PlatformDecoderModule.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/GfxMessageUtils.h"
#include "mozilla/PMediaDecoderParams.h"
#include "mozilla/RemoteImageHolder.h"
#include "mozilla/ShmemPool.h"
#include "mozilla/gfx/Rect.h"

namespace mozilla {

class ShmemPool;

namespace ipc {
class IProtocol;
class Shmem;
}  // namespace ipc

//-----------------------------------------------------------------------------
// Declaration of the IPDL type |struct RemoteVideoData|
//
// We can't use the generated binding in order to use move semantics properly
// (see bug 1664362)
class RemoteVideoData final {
 private:
  typedef mozilla::gfx::IntSize IntSize;

 public:
  RemoteVideoData() = default;

  RemoteVideoData(const MediaDataIPDL& aBase, const IntSize& aDisplay,
                  RemoteImageHolder&& aImage, int32_t aFrameID)
      : mBase(aBase),
        mDisplay(aDisplay),
        mImage(std::move(aImage)),
        mFrameID(aFrameID) {}

  // This is equivalent to the old RemoteVideoDataIPDL object and is similar to
  // the RemoteAudioDataIPDL object. To ensure style consistency we use the IPDL
  // naming convention here.
  MediaDataIPDL& base() { return mBase; }
  const MediaDataIPDL& base() const { return mBase; }

  IntSize& display() { return mDisplay; }
  const IntSize& display() const { return mDisplay; }

  RemoteImageHolder& image() { return mImage; }
  const RemoteImageHolder& image() const { return mImage; }

  int32_t& frameID() { return mFrameID; }
  const int32_t& frameID() const { return mFrameID; }

 private:
  friend struct ipc::IPDLParamTraits<RemoteVideoData>;
  MediaDataIPDL mBase;
  IntSize mDisplay;
  RemoteImageHolder mImage;
  int32_t mFrameID;
};

// Until bug 1572054 is resolved, we can't move our objects when using IPDL's
// union or array. They are always copied. So we make the class refcounted to
// and always pass it by pointed to bypass the problem for now.
class ArrayOfRemoteVideoData final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ArrayOfRemoteVideoData)
 public:
  ArrayOfRemoteVideoData() = default;
  ArrayOfRemoteVideoData(ArrayOfRemoteVideoData&& aOther)
      : mArray(std::move(aOther.mArray)) {}
  explicit ArrayOfRemoteVideoData(nsTArray<RemoteVideoData>&& aOther)
      : mArray(std::move(aOther)) {}
  ArrayOfRemoteVideoData(const ArrayOfRemoteVideoData& aOther) {
    MOZ_CRASH("Should never be used but declared by generated IPDL binding");
  }
  ArrayOfRemoteVideoData& operator=(ArrayOfRemoteVideoData&& aOther) noexcept {
    if (this != &aOther) {
      mArray = std::move(aOther.mArray);
    }
    return *this;
  }
  ArrayOfRemoteVideoData& operator=(nsTArray<RemoteVideoData>&& aOther) {
    mArray = std::move(aOther);
    return *this;
  }

  void AppendElements(nsTArray<RemoteVideoData>&& aOther) {
    mArray.AppendElements(std::move(aOther));
  }
  void Append(RemoteVideoData&& aVideo) {
    mArray.AppendElement(std::move(aVideo));
  }
  const nsTArray<RemoteVideoData>& Array() const { return mArray; }
  nsTArray<RemoteVideoData>& Array() { return mArray; }

 private:
  ~ArrayOfRemoteVideoData() = default;
  friend struct ipc::IPDLParamTraits<mozilla::ArrayOfRemoteVideoData*>;
  nsTArray<RemoteVideoData> mArray;
};

/* The class will pack either an array of AlignedBuffer or MediaByteBuffer
 * into a single Shmem objects. */
class RemoteArrayOfByteBuffer {
 public:
  RemoteArrayOfByteBuffer();
  template <typename Type>
  RemoteArrayOfByteBuffer(const nsTArray<AlignedBuffer<Type>>& aArray,
                          std::function<ShmemBuffer(size_t)>& aAllocator) {
    // Determine the total size we will need for this object.
    size_t totalSize = 0;
    for (auto& buffer : aArray) {
      totalSize += buffer.Size();
    }
    if (totalSize) {
      if (!AllocateShmem(totalSize, aAllocator)) {
        return;
      }
    }
    size_t offset = 0;
    for (auto& buffer : aArray) {
      if (totalSize && buffer && buffer.Size()) {
        Write(offset, buffer.Data(), buffer.Size());
      }
      mOffsets.AppendElement(OffsetEntry{offset, buffer.Size()});
      offset += buffer.Size();
    }
    mIsValid = true;
  }

  RemoteArrayOfByteBuffer(const nsTArray<RefPtr<MediaByteBuffer>>& aArray,
                          std::function<ShmemBuffer(size_t)>& aAllocator);
  RemoteArrayOfByteBuffer& operator=(RemoteArrayOfByteBuffer&& aOther) noexcept;

  // Return the packed aIndexth buffer as an AlignedByteBuffer.
  // The operation is fallible should an out of memory be encountered. The
  // result should be tested accordingly.
  template <typename Type>
  AlignedBuffer<Type> AlignedBufferAt(size_t aIndex) const {
    MOZ_ASSERT(aIndex < Count());
    const OffsetEntry& entry = mOffsets[aIndex];
    size_t entrySize = std::get<1>(entry);
    if (!mBuffers || !entrySize) {
      // It's an empty one.
      return AlignedBuffer<Type>();
    }
    if (!Check(std::get<0>(entry), entrySize)) {
      // This Shmem is corrupted and can't contain the data we are about to
      // retrieve. We return an empty array instead of asserting to allow for
      // recovery.
      return AlignedBuffer<Type>();
    }
    if (0 != entrySize % sizeof(Type)) {
      // There's an error, that entry can't represent this data.
      return AlignedBuffer<Type>();
    }
    return AlignedBuffer<Type>(
        reinterpret_cast<Type*>(BuffersStartAddress() + std::get<0>(entry)),
        entrySize / sizeof(Type));
  }

  // Return the packed aIndexth buffer as aMediaByteBuffer.
  // Will return nullptr if the packed buffer was originally empty.
  already_AddRefed<MediaByteBuffer> MediaByteBufferAt(size_t aIndex) const;
  // Return the size of the aIndexth buffer.
  size_t SizeAt(size_t aIndex) const { return std::get<1>(mOffsets[aIndex]); }
  // Return false if an out of memory error was encountered during construction.
  bool IsValid() const { return mIsValid; };
  // Return the number of buffers packed into this entity.
  size_t Count() const { return mOffsets.Length(); }
  virtual ~RemoteArrayOfByteBuffer();

 private:
  friend struct ipc::IPDLParamTraits<RemoteArrayOfByteBuffer>;
  // Allocate shmem, false if an error occurred.
  bool AllocateShmem(size_t aSize,
                     std::function<ShmemBuffer(size_t)>& aAllocator);
  // The starting address of the Shmem
  uint8_t* BuffersStartAddress() const;
  // Check that the allocated Shmem can contain such range.
  bool Check(size_t aOffset, size_t aSizeInBytes) const;
  void Write(size_t aOffset, const void* aSourceAddr, size_t aSizeInBytes);
  // Set to false is the buffer isn't initialized yet or a memory error occurred
  // during construction.
  bool mIsValid = false;
  // The packed data. The Maybe will be empty if all buffers packed were
  // orignally empty.
  Maybe<ipc::Shmem> mBuffers;
  // The offset to the start of the individual buffer and its size (all in
  // bytes)
  typedef std::tuple<size_t, size_t> OffsetEntry;
  nsTArray<OffsetEntry> mOffsets;
};

/* The class will pack an array of MediaRawData using at most three Shmem
 * objects. Under the most common scenaria, only two Shmems will be used as
 * there are few videos with an alpha channel in the wild.
 * We unfortunately can't populate the array at construction nor present an
 * interface similar to an actual nsTArray or the ArrayOfRemoteVideoData above
 * as currently IPC serialization is always non-fallible. So we must create the
 * object first, fill it to determine if we ran out of memory and then send the
 * object over IPC.
 */
class ArrayOfRemoteMediaRawData {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ArrayOfRemoteMediaRawData)
 public:
  // Fill the content, return false if an OOM occurred.
  bool Fill(const nsTArray<RefPtr<MediaRawData>>& aData,
            std::function<ShmemBuffer(size_t)>&& aAllocator);

  // Return the aIndexth MediaRawData or nullptr if a memory error occurred.
  already_AddRefed<MediaRawData> ElementAt(size_t aIndex) const;

  // Return the number of MediaRawData stored in this container.
  size_t Count() const { return mSamples.Length(); }
  bool IsEmpty() const { return Count() == 0; }
  bool IsValid() const {
    return mBuffers.IsValid() && mAlphaBuffers.IsValid() &&
           mExtraDatas.IsValid();
  }

  struct RemoteMediaRawData {
    MediaDataIPDL mBase;
    bool mEOS;
    // This will be zero for audio.
    int32_t mHeight;
    Maybe<media::TimeInterval> mOriginalPresentationWindow;
    Maybe<CryptoInfo> mCryptoConfig;
  };

 private:
  friend struct ipc::IPDLParamTraits<ArrayOfRemoteMediaRawData*>;
  virtual ~ArrayOfRemoteMediaRawData() = default;

  nsTArray<RemoteMediaRawData> mSamples;
  RemoteArrayOfByteBuffer mBuffers;
  RemoteArrayOfByteBuffer mAlphaBuffers;
  RemoteArrayOfByteBuffer mExtraDatas;
};

/* The class will pack an array of MediaAudioData using at most a single Shmem
 * objects.
 * We unfortunately can't populate the array at construction nor present an
 * interface similar to an actual nsTArray or the ArrayOfRemoteVideoData above
 * as currently IPC serialization is always non-fallible. So we must create the
 * object first, fill it to determine if we ran out of memory and then send the
 * object over IPC.
 */
class ArrayOfRemoteAudioData final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ArrayOfRemoteAudioData)
 public:
  // Fill the content, return false if an OOM occurred.
  bool Fill(const nsTArray<RefPtr<AudioData>>& aData,
            std::function<ShmemBuffer(size_t)>&& aAllocator);

  // Return the aIndexth MediaRawData or nullptr if a memory error occurred.
  already_AddRefed<AudioData> ElementAt(size_t aIndex) const;

  // Return the number of MediaRawData stored in this container.
  size_t Count() const { return mSamples.Length(); }
  bool IsEmpty() const { return Count() == 0; }
  bool IsValid() const { return mBuffers.IsValid(); }

  struct RemoteAudioData {
    friend struct ipc::IPDLParamTraits<RemoteVideoData>;
    MediaDataIPDL mBase;
    uint32_t mChannels;
    uint32_t mRate;
    uint32_t mChannelMap;
    media::TimeUnit mOriginalTime;
    Maybe<media::TimeInterval> mTrimWindow;
    uint32_t mFrames;
    size_t mDataOffset;
  };

 private:
  friend struct ipc::IPDLParamTraits<ArrayOfRemoteAudioData*>;
  ~ArrayOfRemoteAudioData() = default;

  nsTArray<RemoteAudioData> mSamples;
  RemoteArrayOfByteBuffer mBuffers;
};

namespace ipc {

template <>
struct IPDLParamTraits<RemoteVideoData> {
  typedef RemoteVideoData paramType;
  static void Write(IPC::MessageWriter* aWriter, ipc::IProtocol* aActor,
                    paramType&& aVar) {
    WriteIPDLParam(aWriter, aActor, std::move(aVar.mBase));
    WriteIPDLParam(aWriter, aActor, std::move(aVar.mDisplay));
    WriteIPDLParam(aWriter, aActor, std::move(aVar.mImage));
    aWriter->WriteBytes(&aVar.mFrameID, 4);
  }

  static bool Read(IPC::MessageReader* aReader, mozilla::ipc::IProtocol* aActor,
                   paramType* aVar) {
    if (!ReadIPDLParam(aReader, aActor, &aVar->mBase) ||
        !ReadIPDLParam(aReader, aActor, &aVar->mDisplay) ||
        !ReadIPDLParam(aReader, aActor, &aVar->mImage) ||
        !aReader->ReadBytesInto(&aVar->mFrameID, 4)) {
      return false;
    }
    return true;
  }
};

template <>
struct IPDLParamTraits<ArrayOfRemoteVideoData*> {
  typedef ArrayOfRemoteVideoData paramType;
  static void Write(IPC::MessageWriter* aWriter,
                    mozilla::ipc::IProtocol* aActor, paramType* aVar) {
    WriteIPDLParam(aWriter, aActor, std::move(aVar->mArray));
  }

  static bool Read(IPC::MessageReader* aReader, ipc::IProtocol* aActor,
                   RefPtr<paramType>* aVar) {
    nsTArray<RemoteVideoData> array;
    if (!ReadIPDLParam(aReader, aActor, &array)) {
      return false;
    }
    auto results = MakeRefPtr<ArrayOfRemoteVideoData>(std::move(array));
    *aVar = std::move(results);
    return true;
  }
};

template <>
struct IPDLParamTraits<RemoteArrayOfByteBuffer> {
  typedef RemoteArrayOfByteBuffer paramType;
  // We do not want to move the RemoteArrayOfByteBuffer as we want to recycle
  // the shmem it contains for another time.
  static void Write(IPC::MessageWriter* aWriter, ipc::IProtocol* aActor,
                    const paramType& aVar);

  static bool Read(IPC::MessageReader* aReader, ipc::IProtocol* aActor,
                   paramType* aVar);
};

template <>
struct IPDLParamTraits<ArrayOfRemoteMediaRawData::RemoteMediaRawData> {
  typedef ArrayOfRemoteMediaRawData::RemoteMediaRawData paramType;
  static void Write(IPC::MessageWriter* aWriter, ipc::IProtocol* aActor,
                    const paramType& aVar);

  static bool Read(IPC::MessageReader* aReader, ipc::IProtocol* aActor,
                   paramType* aVar);
};

template <>
struct IPDLParamTraits<ArrayOfRemoteMediaRawData*> {
  typedef ArrayOfRemoteMediaRawData paramType;
  static void Write(IPC::MessageWriter* aWriter, ipc::IProtocol* aActor,
                    paramType* aVar);

  static bool Read(IPC::MessageReader* aReader, ipc::IProtocol* aActor,
                   RefPtr<paramType>* aVar);
};

template <>
struct IPDLParamTraits<ArrayOfRemoteAudioData::RemoteAudioData> {
  typedef ArrayOfRemoteAudioData::RemoteAudioData paramType;
  static void Write(IPC::MessageWriter* aWriter, ipc::IProtocol* aActor,
                    const paramType& aVar);

  static bool Read(IPC::MessageReader* aReader, ipc::IProtocol* aActor,
                   paramType* aVar);
};

template <>
struct IPDLParamTraits<ArrayOfRemoteAudioData*> {
  typedef ArrayOfRemoteAudioData paramType;
  static void Write(IPC::MessageWriter* aWriter, ipc::IProtocol* aActor,
                    paramType* aVar);

  static bool Read(IPC::MessageReader* aReader, ipc::IProtocol* aActor,
                   RefPtr<paramType>* aVar);
};
}  // namespace ipc

}  // namespace mozilla

#endif  // mozilla_dom_media_ipc_RemoteMediaData_h
