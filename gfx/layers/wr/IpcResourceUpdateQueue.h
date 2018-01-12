/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WR_IPCRESOURCEUPDATEQUEUE_H
#define GFX_WR_IPCRESOURCEUPDATEQUEUE_H

#include "mozilla/layers/WebRenderMessages.h"
#include "mozilla/layers/RefCountedShmem.h"
#include "mozilla/webrender/WebRenderTypes.h"

namespace mozilla {
namespace ipc {
class IShmemAllocator;
}
namespace wr {

/// ShmSegmentsWriter pushes bytes in a sequence of fixed size shmems for small
/// allocations and creates dedicated shmems for large allocations.
class ShmSegmentsWriter {
public:
  ShmSegmentsWriter(layers::WebRenderBridgeChild* aAllocator, size_t aChunkSize);
  ~ShmSegmentsWriter();

  layers::OffsetRange Write(Range<uint8_t> aBytes);

  template<typename T>
  layers::OffsetRange WriteAsBytes(Range<T> aValues)
  {
    return Write(Range<uint8_t>((uint8_t*)aValues.begin().get(), aValues.length() * sizeof(T)));
  }

  void Flush(nsTArray<layers::RefCountedShmem>& aSmallAllocs, nsTArray<ipc::Shmem>& aLargeAllocs);

  void Clear();

protected:
  bool AllocChunk();
  layers::OffsetRange AllocLargeChunk(size_t aSize);

  nsTArray<layers::RefCountedShmem> mSmallAllocs;
  nsTArray<ipc::Shmem> mLargeAllocs;
  layers::WebRenderBridgeChild* mShmAllocator;
  size_t mCursor;
  size_t mChunkSize;
};

class ShmSegmentsReader {
public:
  ShmSegmentsReader(const nsTArray<layers::RefCountedShmem>& aSmallShmems,
                    const nsTArray<ipc::Shmem>& aLargeShmems);

  bool Read(const layers::OffsetRange& aRange, wr::Vec<uint8_t>& aInto);

protected:
  bool ReadLarge(const layers::OffsetRange& aRange, wr::Vec<uint8_t>& aInto);

  const nsTArray<layers::RefCountedShmem>& mSmallAllocs;
  const nsTArray<ipc::Shmem>& mLargeAllocs;
  size_t mChunkSize;
};

class IpcResourceUpdateQueue {
public:
  // Because we are using shmems, the size should be a multiple of the page size.
  // Each shmem has two guard pages, and the minimum shmem size (at least one Windows)
  // is 64k which is already quite large for a lot of the resources we use here.
  // So we pick 64k - 2 * 4k = 57344 bytes as the defautl alloc
  explicit IpcResourceUpdateQueue(layers::WebRenderBridgeChild* aAllocator, size_t aChunkSize = 57344);

  bool AddImage(wr::ImageKey aKey,
                const ImageDescriptor& aDescriptor,
                Range<uint8_t> aBytes);

  bool AddBlobImage(wr::ImageKey aKey,
                    const ImageDescriptor& aDescriptor,
                    Range<uint8_t> aBytes);

  void AddExternalImage(wr::ExternalImageId aExtId, wr::ImageKey aKey);

  bool UpdateImageBuffer(wr::ImageKey aKey,
                         const ImageDescriptor& aDescriptor,
                         Range<uint8_t> aBytes);

  bool UpdateBlobImage(wr::ImageKey aKey,
                       const ImageDescriptor& aDescriptor,
                       Range<uint8_t> aBytes,
                       ImageIntRect aDirtyRect);

  void UpdateExternalImage(ImageKey aKey,
                           const ImageDescriptor& aDescriptor,
                           ExternalImageId aExtID,
                           wr::WrExternalImageBufferType aBufferType,
                           uint8_t aChannelIndex = 0);

  void DeleteImage(wr::ImageKey aKey);

  bool AddRawFont(wr::FontKey aKey, Range<uint8_t> aBytes, uint32_t aIndex);

  bool AddFontDescriptor(wr::FontKey aKey, Range<uint8_t> aBytes, uint32_t aIndex);

  void DeleteFont(wr::FontKey aKey);

  void AddFontInstance(wr::FontInstanceKey aKey,
                       wr::FontKey aFontKey,
                       float aGlyphSize,
                       const wr::FontInstanceOptions* aOptions,
                       const wr::FontInstancePlatformOptions* aPlatformOptions,
                       Range<const gfx::FontVariation> aVariations);

  void DeleteFontInstance(wr::FontInstanceKey aKey);

  void Clear();

  void Flush(nsTArray<layers::OpUpdateResource>& aUpdates,
             nsTArray<layers::RefCountedShmem>& aSmallAllocs,
             nsTArray<ipc::Shmem>& aLargeAllocs);

protected:
  ShmSegmentsWriter mWriter;
  nsTArray<layers::OpUpdateResource> mUpdates;
};

} // namespace
} // namespace

#endif
