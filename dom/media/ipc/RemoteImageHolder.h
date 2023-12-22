/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_media_RemoteImageHolder_h
#define mozilla_dom_media_RemoteImageHolder_h

#include "MediaData.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/layers/VideoBridgeUtils.h"

namespace mozilla {
namespace layers {
class BufferRecycleBin;
class IGPUVideoSurfaceManager;
class SurfaceDescriptor;
}  // namespace layers
class RemoteImageHolder final {
  friend struct ipc::IPDLParamTraits<RemoteImageHolder>;

 public:
  RemoteImageHolder();
  RemoteImageHolder(
      layers::IGPUVideoSurfaceManager* aManager,
      layers::VideoBridgeSource aSource, const gfx::IntSize& aSize,
      const gfx::ColorDepth& aColorDepth, const layers::SurfaceDescriptor& aSD,
      gfx::YUVColorSpace aYUVColorSpace, gfx::ColorSpace2 aColorPrimaries,
      gfx::TransferFunction aTransferFunction, gfx::ColorRange aColorRange);
  RemoteImageHolder(RemoteImageHolder&& aOther);
  // Ensure we never copy this object.
  RemoteImageHolder(const RemoteImageHolder& aOther) = delete;
  RemoteImageHolder& operator=(const RemoteImageHolder& aOther) = delete;
  ~RemoteImageHolder();

  bool IsEmpty() const { return mSD.isNothing(); }
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
  gfx::ColorDepth mColorDepth = gfx::ColorDepth::COLOR_8;
  Maybe<layers::SurfaceDescriptor> mSD;
  RefPtr<layers::IGPUVideoSurfaceManager> mManager;
  gfx::YUVColorSpace mYUVColorSpace = {};
  gfx::ColorSpace2 mColorPrimaries = {};
  gfx::TransferFunction mTransferFunction = {};
  gfx::ColorRange mColorRange = {};
};

  template <>
  struct ipc::IPDLParamTraits<RemoteImageHolder> {
    static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                      RemoteImageHolder&& aParam);
    static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                     RemoteImageHolder* aResult);
};

}  // namespace mozilla

#endif  // mozilla_dom_media_RemoteImageHolder_h
