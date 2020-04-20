/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WR_IPCRESOURCEUPDATEQUEUE_H
#define GFX_WR_IPCRESOURCEUPDATEQUEUE_H

#include "mozilla/layers/WebRenderMessages.h"
#include "mozilla/layers/RefCountedShmem.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/webrender/WebRenderTypes.h"

namespace mozilla {
namespace ipc {
class IShmemAllocator;
}
namespace layers {
class TextureClient;
}

namespace wr {

/// ShmSegmentsWriter pushes bytes in a sequence of fixed size shmems for small
/// allocations and creates dedicated shmems for large allocations.
class ShmSegmentsWriter {
 public:
  ShmSegmentsWriter(layers::WebRenderBridgeChild* aAllocator,
                    size_t aChunkSize);
  ~ShmSegmentsWriter();

  ShmSegmentsWriter(ShmSegmentsWriter&& aOther) noexcept;
  ShmSegmentsWriter& operator=(ShmSegmentsWriter&& aOther) noexcept;

  ShmSegmentsWriter(const ShmSegmentsWriter& aOther) = delete;
  ShmSegmentsWriter& operator=(const ShmSegmentsWriter& aOther) = delete;

  layers::OffsetRange Write(Range<uint8_t> aBytes);

  template <typename T>
  layers::OffsetRange WriteAsBytes(Range<T> aValues) {
    return Write(Range<uint8_t>((uint8_t*)aValues.begin().get(),
                                aValues.length() * sizeof(T)));
  }

  void Flush(nsTArray<layers::RefCountedShmem>& aSmallAllocs,
             nsTArray<mozilla::ipc::Shmem>& aLargeAllocs);

  void Clear();
  bool IsEmpty() const;

  layers::WebRenderBridgeChild* WrBridge() const { return mShmAllocator; }
  size_t ChunkSize() const { return mChunkSize; }

 protected:
  bool AllocChunk();
  layers::OffsetRange AllocLargeChunk(size_t aSize);

  nsTArray<layers::RefCountedShmem> mSmallAllocs;
  nsTArray<mozilla::ipc::Shmem> mLargeAllocs;
  layers::WebRenderBridgeChild* mShmAllocator;
  size_t mCursor;
  size_t mChunkSize;
};

class ShmSegmentsReader {
 public:
  ShmSegmentsReader(const nsTArray<layers::RefCountedShmem>& aSmallShmems,
                    const nsTArray<mozilla::ipc::Shmem>& aLargeShmems);

  bool Read(const layers::OffsetRange& aRange, wr::Vec<uint8_t>& aInto);

 protected:
  bool ReadLarge(const layers::OffsetRange& aRange, wr::Vec<uint8_t>& aInto);

  const nsTArray<layers::RefCountedShmem>& mSmallAllocs;
  const nsTArray<mozilla::ipc::Shmem>& mLargeAllocs;
  size_t mChunkSize;
};

class IpcResourceUpdateQueue {
 public:
  // Because we are using shmems, the size should be a multiple of the page
  // size. Each shmem has two guard pages, and the minimum shmem size (at least
  // one Windows) is 64k which is already quite large for a lot of the resources
  // we use here. The RefCountedShmem type used to allocate the chunks keeps a
  // 16 bytes header in the buffer which we account for here as well. So we pick
  // 64k - 2 * 4k - 16 = 57328 bytes as the default alloc size.
  explicit IpcResourceUpdateQueue(
      layers::WebRenderBridgeChild* aAllocator,
      wr::RenderRoot aRenderRoot = wr::RenderRoot::Default,
      size_t aChunkSize = 57328);

  // Although resource updates don't belong to a particular document/render root
  // in any concrete way, they still end up being tied to a render root because
  // we need to know which WR document to generate a frame for when they change.
  IpcResourceUpdateQueue& SubQueue(wr::RenderRoot aRenderRoot) {
    MOZ_ASSERT(mRenderRoot == wr::RenderRoot::Default);
    MOZ_ASSERT(aRenderRoot == wr::RenderRoot::Default);
    return *this;
  }

  bool HasAnySubQueue() { return false; }

  bool HasSubQueue(wr::RenderRoot aRenderRoot) {
    return aRenderRoot == wr::RenderRoot::Default;
  }

  wr::RenderRoot GetRenderRoot() { return mRenderRoot; }

  IpcResourceUpdateQueue(IpcResourceUpdateQueue&& aOther) noexcept;
  IpcResourceUpdateQueue& operator=(IpcResourceUpdateQueue&& aOther) noexcept;

  IpcResourceUpdateQueue(const IpcResourceUpdateQueue& aOther) = delete;
  IpcResourceUpdateQueue& operator=(const IpcResourceUpdateQueue& aOther) =
      delete;

  // Moves over everything but the subqueues
  void ReplaceResources(IpcResourceUpdateQueue&& aOther);

  bool AddImage(wr::ImageKey aKey, const ImageDescriptor& aDescriptor,
                Range<uint8_t> aBytes);

  bool AddBlobImage(wr::BlobImageKey aKey, const ImageDescriptor& aDescriptor,
                    Range<uint8_t> aBytes, ImageIntRect aVisibleRect);

  void AddPrivateExternalImage(wr::ExternalImageId aExtId, wr::ImageKey aKey,
                               wr::ImageDescriptor aDesc);

  void AddSharedExternalImage(wr::ExternalImageId aExtId, wr::ImageKey aKey);

  void PushExternalImageForTexture(wr::ExternalImageId aExtId,
                                   wr::ImageKey aKey,
                                   layers::TextureClient* aTexture,
                                   bool aIsUpdate);

  bool UpdateImageBuffer(wr::ImageKey aKey, const ImageDescriptor& aDescriptor,
                         Range<uint8_t> aBytes);

  bool UpdateBlobImage(wr::BlobImageKey aKey,
                       const ImageDescriptor& aDescriptor,
                       Range<uint8_t> aBytes, ImageIntRect aVisibleRect,
                       ImageIntRect aDirtyRect);

  void UpdatePrivateExternalImage(wr::ExternalImageId aExtId, wr::ImageKey aKey,
                                  const wr::ImageDescriptor& aDesc,
                                  ImageIntRect aDirtyRect);
  void UpdateSharedExternalImage(ExternalImageId aExtID, ImageKey aKey,
                                 ImageIntRect aDirtyRect);

  void SetBlobImageVisibleArea(BlobImageKey aKey, const ImageIntRect& aArea);

  void DeleteImage(wr::ImageKey aKey);

  void DeleteBlobImage(wr::BlobImageKey aKey);

  bool AddRawFont(wr::FontKey aKey, Range<uint8_t> aBytes, uint32_t aIndex);

  bool AddFontDescriptor(wr::FontKey aKey, Range<uint8_t> aBytes,
                         uint32_t aIndex);

  void DeleteFont(wr::FontKey aKey);

  void AddFontInstance(wr::FontInstanceKey aKey, wr::FontKey aFontKey,
                       float aGlyphSize,
                       const wr::FontInstanceOptions* aOptions,
                       const wr::FontInstancePlatformOptions* aPlatformOptions,
                       Range<const gfx::FontVariation> aVariations);

  void DeleteFontInstance(wr::FontInstanceKey aKey);

  void Clear();

  void Flush(nsTArray<layers::OpUpdateResource>& aUpdates,
             nsTArray<layers::RefCountedShmem>& aSmallAllocs,
             nsTArray<mozilla::ipc::Shmem>& aLargeAllocs);

  bool IsEmpty() const;

  static void ReleaseShmems(mozilla::ipc::IProtocol*,
                            nsTArray<layers::RefCountedShmem>& aShms);
  static void ReleaseShmems(mozilla::ipc::IProtocol*,
                            nsTArray<mozilla::ipc::Shmem>& aShms);

 protected:
  ShmSegmentsWriter mWriter;
  nsTArray<layers::OpUpdateResource> mUpdates;
  wr::RenderRoot mRenderRoot;
};

}  // namespace wr
}  // namespace mozilla

#endif
