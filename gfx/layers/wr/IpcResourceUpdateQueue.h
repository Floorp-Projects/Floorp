/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WR_IPCRESOURCEUPDATEQUEUE_H
#define GFX_WR_IPCRESOURCEUPDATEQUEUE_H

#include "mozilla/layers/WebRenderMessages.h"
#include "mozilla/webrender/WebRenderTypes.h"

namespace mozilla {
namespace ipc {
class IShmemAllocator;
}
namespace wr {

class ShmSegmentsWriter {
public:
  ShmSegmentsWriter(ipc::IShmemAllocator* aAllocator, size_t aChunkSize);
  ~ShmSegmentsWriter();

  layers::OffsetRange Write(Range<uint8_t> aBytes);

  void Flush(nsTArray<ipc::Shmem>& aData);

  void Clear();

protected:
  void AllocChunk();

  nsTArray<ipc::Shmem> mData;
  ipc::IShmemAllocator* mShmAllocator;
  size_t mCursor;
  size_t mChunkSize;
};

class ShmSegmentsReader {
public:
  explicit ShmSegmentsReader(const nsTArray<ipc::Shmem>& aShmems);

  bool Read(layers::OffsetRange aRange, wr::Vec_u8& aInto);

protected:
  void AllocChunk();

  const nsTArray<ipc::Shmem>& mData;
  size_t mChunkSize;
};

class IpcResourceUpdateQueue {
public:
  // TODO: 8192 is completely arbitrary, needs some adjustments.
  // Because we are using shmems, the size should be a multiple of the page size, and keep in mind
  // that each shmem has guard pages, one of which contains meta-data so at least an extra page
  // is mapped under the hood (so having a lot of smaller shmems = overhead).
  explicit IpcResourceUpdateQueue(ipc::IShmemAllocator* aAllocator, size_t aChunkSize = 8192);

  void AddImage(wr::ImageKey aKey,
                const ImageDescriptor& aDescriptor,
                Range<uint8_t> aBytes);

  void AddBlobImage(wr::ImageKey aKey,
                    const ImageDescriptor& aDescriptor,
                    Range<uint8_t> aBytes);

  void AddExternalImage(wr::ExternalImageId aExtId, wr::ImageKey aKey);

  void UpdateImageBuffer(wr::ImageKey aKey,
                         const ImageDescriptor& aDescriptor,
                         Range<uint8_t> aBytes);

  void UpdateBlobImage(wr::ImageKey aKey,
                       const ImageDescriptor& aDescriptor,
                       Range<uint8_t> aBytes);

  void UpdateExternalImage(ImageKey aKey,
                           const ImageDescriptor& aDescriptor,
                           ExternalImageId aExtID,
                           wr::WrExternalImageBufferType aBufferType,
                           uint8_t aChannelIndex = 0);

  void DeleteImage(wr::ImageKey aKey);

  void AddRawFont(wr::FontKey aKey, Range<uint8_t> aBytes, uint32_t aIndex);

  void DeleteFont(wr::FontKey aKey);

  void AddFontInstance(wr::FontInstanceKey aKey,
                       wr::FontKey aFontKey,
                       float aGlyphSize,
                       const wr::FontInstanceOptions* aOptions,
                       const wr::FontInstancePlatformOptions* aPlatformOptions);

  void DeleteFontInstance(wr::FontInstanceKey aKey);

  void Clear();

  void Flush(nsTArray<layers::OpUpdateResource>& aUpdates,
             nsTArray<ipc::Shmem>& aResources);

protected:
  ShmSegmentsWriter mWriter;
  nsTArray<layers::OpUpdateResource> mUpdates;
};

} // namespace
} // namespace

#endif
