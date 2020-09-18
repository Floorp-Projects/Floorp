/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_media_ipc_RemoteMediaData_h
#define mozilla_dom_media_ipc_RemoteMediaData_h

#include "PlatformDecoderModule.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/GfxMessageUtils.h"
#include "mozilla/PMediaDecoderParams.h"
#include "mozilla/RemoteImageHolder.h"
#include "mozilla/gfx/Rect.h"

namespace mozilla {

//-----------------------------------------------------------------------------
// Declaration of the IPDL type |struct RemoteVideoData|
//
// We can't use the generated binding in order to use move semantics properly
// (see bug 1664362)
class RemoteVideoData final {
 private:
  typedef mozilla::MediaDataIPDL MediaDataIPDL;
  typedef mozilla::gfx::IntSize IntSize;
  typedef mozilla::RemoteImageHolder RemoteImageHolder;

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
  ArrayOfRemoteVideoData& operator=(ArrayOfRemoteVideoData&& aOther) {
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

namespace ipc {

template <>
struct IPDLParamTraits<RemoteVideoData> {
  typedef RemoteVideoData paramType;
  static void Write(IPC::Message* aMsg, ipc::IProtocol* aActor,
                    paramType&& aVar) {
    WriteIPDLParam(aMsg, aActor, std::move(aVar.mBase));
    WriteIPDLParam(aMsg, aActor, std::move(aVar.mDisplay));
    WriteIPDLParam(aMsg, aActor, std::move(aVar.mImage));
    aMsg->WriteBytes(&aVar.mFrameID, 4);
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   mozilla::ipc::IProtocol* aActor, paramType* aVar) {
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aVar->mBase) ||
        !ReadIPDLParam(aMsg, aIter, aActor, &aVar->mDisplay) ||
        !ReadIPDLParam(aMsg, aIter, aActor, &aVar->mImage) ||
        !aMsg->ReadBytesInto(aIter, &aVar->mFrameID, 4)) {
      return false;
    }
    return true;
  }
};

template <>
struct IPDLParamTraits<mozilla::ArrayOfRemoteVideoData*> {
  typedef mozilla::ArrayOfRemoteVideoData paramType;
  static void Write(IPC::Message* aMsg, mozilla::ipc::IProtocol* aActor,
                    paramType* aVar) {
    WriteIPDLParam(aMsg, aActor, std::move(aVar->mArray));
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   mozilla::ipc::IProtocol* aActor, RefPtr<paramType>* aVar) {
    nsTArray<RemoteVideoData> array;
    if (!ReadIPDLParam(aMsg, aIter, aActor, &array)) {
      return false;
    }
    auto results = MakeRefPtr<ArrayOfRemoteVideoData>(std::move(array));
    *aVar = std::move(results);
    return true;
  }
};

}  // namespace ipc

}  // namespace mozilla

#endif  // mozilla_dom_media_ipc_RemoteMediaData_h