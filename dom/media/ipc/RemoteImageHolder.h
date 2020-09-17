/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_media_RemoteImageHolder_h
#define mozilla_dom_media_RemoteImageHolder_h

#include "MediaData.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/RefPtr.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/layers/VideoBridgeUtils.h"

namespace mozilla {
namespace layers {
class BufferRecycleBin;
}
class RemoteImageHolder final {
  friend struct ipc::IPDLParamTraits<RemoteImageHolder>;

 public:
  RemoteImageHolder() = default;
  RemoteImageHolder(layers::VideoBridgeSource aSource,
                    const gfx::IntSize& aSize,
                    const layers::SurfaceDescriptor& aSD)
      : mSource(aSource), mSize(aSize), mSD(aSD), mEmpty(false) {}
  RemoteImageHolder(RemoteImageHolder&& aOther)
      : mSource(aOther.mSource),
        mSize(aOther.mSize),
        mSD(aOther.mSD),
        mEmpty(aOther.mEmpty) {
    aOther.mEmpty = true;
  }
  // Ensure we never copy this object.
  RemoteImageHolder(const RemoteImageHolder& aOther) = delete;
  RemoteImageHolder& operator=(const RemoteImageHolder& aOther) = delete;
  ~RemoteImageHolder();
  // Move content of RemoteImageHolder into a usable Image. Ownership is
  // transfered to that Image.
  already_AddRefed<layers::Image> TransferToImage(
      layers::BufferRecycleBin* aBufferRecycleBin = nullptr);

 private:
  already_AddRefed<layers::Image> DeserializeImage(
      layers::BufferRecycleBin* aBufferRecycleBin);
  // We need a default for the default constructor, never used in practice.
  layers::VideoBridgeSource mSource = layers::VideoBridgeSource::GpuProcess;
  gfx::IntSize mSize;
  layers::SurfaceDescriptor mSD;
  bool mEmpty = true;
};

namespace ipc {
template <>
struct IPDLParamTraits<mozilla::RemoteImageHolder> {
  typedef mozilla::RemoteImageHolder paramType;

  static void Write(IPC::Message* aMsg, IProtocol* aActor, paramType&& aParam) {
    WriteIPDLParam(aMsg, aActor, aParam.mSource);
    WriteIPDLParam(aMsg, aActor, aParam.mSize);
    WriteIPDLParam(aMsg, aActor, aParam.mSD);
    WriteIPDLParam(aMsg, aActor, aParam.mEmpty);
    aParam.mEmpty = true;
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, paramType* aResult) {
    if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mSource) ||
        !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mSize) ||
        !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mSD) ||
        !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mEmpty)) {
      return false;
    }
    return true;
  }
};

}  // namespace ipc

}  // namespace mozilla

#endif  // mozilla_dom_media_RemoteImageHolder_h
