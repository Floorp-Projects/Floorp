/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteImageHolder.h"

#include "GPUVideoImage.h"
#include "mozilla/PRemoteDecoderChild.h"
#include "mozilla/RemoteDecoderManagerChild.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/VideoBridgeUtils.h"

namespace mozilla {

using namespace gfx;
using namespace layers;

RemoteImageHolder::RemoteImageHolder() = default;
RemoteImageHolder::RemoteImageHolder(layers::IGPUVideoSurfaceManager* aManager,
                                     layers::VideoBridgeSource aSource,
                                     const gfx::IntSize& aSize,
                                     const layers::SurfaceDescriptor& aSD)
    : mSource(aSource), mSize(aSize), mSD(Some(aSD)), mManager(aManager) {}
RemoteImageHolder::RemoteImageHolder(RemoteImageHolder&& aOther)
    : mSource(aOther.mSource),
      mSize(aOther.mSize),
      mSD(std::move(aOther.mSD)),
      mManager(aOther.mManager) {
  aOther.mSD = Nothing();
}

already_AddRefed<Image> RemoteImageHolder::DeserializeImage(
    layers::BufferRecycleBin* aBufferRecycleBin) {
  MOZ_ASSERT(mSD && mSD->type() == SurfaceDescriptor::TSurfaceDescriptorBuffer);
  const SurfaceDescriptorBuffer& sdBuffer = mSD->get_SurfaceDescriptorBuffer();
  MOZ_ASSERT(sdBuffer.desc().type() == BufferDescriptor::TYCbCrDescriptor);
  if (sdBuffer.desc().type() != BufferDescriptor::TYCbCrDescriptor ||
      !aBufferRecycleBin) {
    return nullptr;
  }
  const YCbCrDescriptor& descriptor = sdBuffer.desc().get_YCbCrDescriptor();

  uint8_t* buffer = nullptr;
  const MemoryOrShmem& memOrShmem = sdBuffer.data();
  switch (memOrShmem.type()) {
    case MemoryOrShmem::Tuintptr_t:
      buffer = reinterpret_cast<uint8_t*>(memOrShmem.get_uintptr_t());
      break;
    case MemoryOrShmem::TShmem:
      buffer = memOrShmem.get_Shmem().get<uint8_t>();
      break;
    default:
      MOZ_ASSERT(false, "Unknown MemoryOrShmem type");
  }
  if (!buffer) {
    return nullptr;
  }

  PlanarYCbCrData pData;
  pData.mYSize = descriptor.ySize();
  pData.mYStride = descriptor.yStride();
  pData.mCbCrSize = descriptor.cbCrSize();
  pData.mCbCrStride = descriptor.cbCrStride();
  // default mYSkip, mCbSkip, mCrSkip because not held in YCbCrDescriptor
  pData.mYSkip = pData.mCbSkip = pData.mCrSkip = 0;
  gfx::IntRect display = descriptor.display();
  pData.mPicX = display.X();
  pData.mPicY = display.Y();
  pData.mPicSize = display.Size();
  pData.mStereoMode = descriptor.stereoMode();
  pData.mColorDepth = descriptor.colorDepth();
  pData.mYUVColorSpace = descriptor.yUVColorSpace();
  pData.mColorRange = descriptor.colorRange();
  pData.mYChannel = ImageDataSerializer::GetYChannel(buffer, descriptor);
  pData.mCbChannel = ImageDataSerializer::GetCbChannel(buffer, descriptor);
  pData.mCrChannel = ImageDataSerializer::GetCrChannel(buffer, descriptor);

  // images coming from AOMDecoder are RecyclingPlanarYCbCrImages.
  RefPtr<RecyclingPlanarYCbCrImage> image =
      new RecyclingPlanarYCbCrImage(aBufferRecycleBin);
  bool setData = image->CopyData(pData);
  MOZ_ASSERT(setData);

  switch (memOrShmem.type()) {
    case MemoryOrShmem::Tuintptr_t:
      delete[] reinterpret_cast<uint8_t*>(memOrShmem.get_uintptr_t());
      break;
    case MemoryOrShmem::TShmem:
      // Memory buffer will be recycled by the parent automatically.
      break;
    default:
      MOZ_ASSERT(false, "Unknown MemoryOrShmem type");
  }

  if (!setData) {
    return nullptr;
  }

  return image.forget();
}

already_AddRefed<layers::Image> RemoteImageHolder::TransferToImage(
    layers::BufferRecycleBin* aBufferRecycleBin) {
  if (IsEmpty()) {
    return nullptr;
  }
  RefPtr<Image> image;
  if (mSD->type() == SurfaceDescriptor::TSurfaceDescriptorBuffer) {
    image = DeserializeImage(aBufferRecycleBin);
  } else {
    // The Image here creates a TextureData object that takes ownership
    // of the SurfaceDescriptor, and is responsible for making sure that
    // it gets deallocated.
    SurfaceDescriptorRemoteDecoder remoteSD =
        static_cast<const SurfaceDescriptorGPUVideo&>(*mSD);
    remoteSD.source() = Some(mSource);
    image = new GPUVideoImage(mManager, remoteSD, mSize);
  }
  mSD = Nothing();
  mManager = nullptr;

  return image.forget();
}

RemoteImageHolder::~RemoteImageHolder() {
  // GPU Images are held by the RemoteDecoderManagerParent, we didn't get to use
  // this image holder (the decoder could have been flushed). We don't need to
  // worry about Shmem based image as the Shmem will be automatically re-used
  // once the decoder is used again.
  if (!IsEmpty() && mManager &&
      mSD->type() != SurfaceDescriptor::TSurfaceDescriptorBuffer) {
    SurfaceDescriptorRemoteDecoder remoteSD =
        static_cast<const SurfaceDescriptorGPUVideo&>(*mSD);
    mManager->DeallocateSurfaceDescriptor(remoteSD);
  }
}

/* static */ void ipc::IPDLParamTraits<RemoteImageHolder>::Write(
    IPC::Message* aMsg, ipc::IProtocol* aActor, RemoteImageHolder&& aParam) {
  WriteIPDLParam(aMsg, aActor, aParam.mSource);
  WriteIPDLParam(aMsg, aActor, aParam.mSize);
  WriteIPDLParam(aMsg, aActor, aParam.mSD);
  // Empty this holder.
  aParam.mSD = Nothing();
  aParam.mManager = nullptr;
}

/* static */ bool ipc::IPDLParamTraits<RemoteImageHolder>::Read(
    const IPC::Message* aMsg, PickleIterator* aIter, ipc::IProtocol* aActor,
    RemoteImageHolder* aResult) {
  if (!ReadIPDLParam(aMsg, aIter, aActor, &aResult->mSource) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mSize) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &aResult->mSD)) {
    return false;
  }
  if (!aResult->IsEmpty()) {
    aResult->mManager = RemoteDecoderManagerChild::GetSingleton(
        aResult->mSource == VideoBridgeSource::GpuProcess
            ? RemoteDecodeIn::GpuProcess
            : RemoteDecodeIn::RddProcess);
  }
  return true;
}

}  // namespace mozilla
