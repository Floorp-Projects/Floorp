/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedBufferMLGPU.h"
#include "BufferCache.h"
#include "MLGDevice.h"

using namespace std;

namespace mozilla {
namespace layers {

SharedBufferMLGPU::SharedBufferMLGPU(MLGDevice* aDevice, MLGBufferType aType, size_t aDefaultSize)
 : mDevice(aDevice),
   mType(aType),
   mDefaultSize(aDefaultSize),
   mCanUseOffsetAllocation(true),
   mCurrentPosition(0),
   mMaxSize(0),
   mMapped(false),
   mBytesUsedThisFrame(0),
   mNumSmallFrames(0)
{
  MOZ_COUNT_CTOR(SharedBufferMLGPU);
}

SharedBufferMLGPU::~SharedBufferMLGPU()
{
  MOZ_COUNT_DTOR(SharedBufferMLGPU);
  Unmap();
}

bool
SharedBufferMLGPU::Init()
{
  // If we can't use buffer offset binding, we never allocated shared buffers.
  if (!mCanUseOffsetAllocation) {
    return true;
  }

  // If we can use offset binding, allocate an initial shared buffer now.
  if (!GrowBuffer(mDefaultSize)) {
    return false;
  }
  return true;
}

void
SharedBufferMLGPU::Reset()
{
  // We shouldn't be mapped here, but just in case, unmap now.
  Unmap();
  mBytesUsedThisFrame = 0;

  // If we allocated a large buffer for a particularly heavy layer tree,
  // but have not used most of the buffer again for many frames, we
  // discard the buffer. This is to prevent having to perform large
  // pointless uploads after visiting a single havy page - it also
  // lessens ping-ponging between large and small buffers.
  if (mBuffer &&
      (mBuffer->GetSize() > mDefaultSize * 4) &&
      mNumSmallFrames >= 10)
  {
    mBuffer = nullptr;
  }

  // Note that we do not aggressively map a new buffer. There's no reason to,
  // and it'd cause unnecessary uploads when painting empty frames.
}

bool
SharedBufferMLGPU::EnsureMappedBuffer(size_t aBytes)
{
  if (!mBuffer || (mMaxSize - mCurrentPosition < aBytes)) {
    if (!GrowBuffer(aBytes)) {
      return false;
    }
  }
  if (!mMapped && !Map()) {
    return false;
  }
  return true;
}

// We don't want to cache large buffers, since it results in larger uploads
// that might not be needed.
static const size_t kMaxCachedBufferSize = 128 * 1024;

bool
SharedBufferMLGPU::GrowBuffer(size_t aBytes)
{
  // We only pre-allocate buffers if we can use offset allocation.
  MOZ_ASSERT(mCanUseOffsetAllocation);

  // Unmap the previous buffer. This will retain mBuffer, but free up the
  // address space used by its mapping.
  Unmap();

  size_t maybeSize = mDefaultSize;
  if (mBuffer) {
    // Try to first grow the previous allocation size.
    maybeSize = std::min(kMaxCachedBufferSize, mBuffer->GetSize() * 2);
  }

  size_t bytes = std::max(aBytes, maybeSize);
  mBuffer = mDevice->CreateBuffer(mType, bytes, MLGUsage::Dynamic);
  if (!mBuffer) {
    return false;
  }

  mCurrentPosition = 0;
  mMaxSize = mBuffer->GetSize();
  return true;
}

void
SharedBufferMLGPU::PrepareForUsage()
{
  Unmap();

  if (mBytesUsedThisFrame <= mDefaultSize) {
    mNumSmallFrames++;
  } else {
    mNumSmallFrames = 0;
  }
}

bool
SharedBufferMLGPU::Map()
{
  MOZ_ASSERT(mBuffer);
  MOZ_ASSERT(!mMapped);

  if (!mDevice->Map(mBuffer, MLGMapType::WRITE_DISCARD, &mMap)) {
    // Don't retain the buffer, it's useless if we can't map it.
    mBuffer = nullptr;
    return false;
  }

  mCurrentPosition = 0;
  mMapped = true;
  return true;
}

void
SharedBufferMLGPU::Unmap()
{
  if (!mMapped) {
    return;
  }

  mBytesUsedThisFrame += mCurrentPosition;

  mDevice->Unmap(mBuffer);
  mMap = MLGMappedResource();
  mMapped = false;
}

uint8_t*
SharedBufferMLGPU::GetBufferPointer(size_t aBytes, ptrdiff_t* aOutOffset, RefPtr<MLGBuffer>* aOutBuffer)
{
  if (!EnsureMappedBuffer(aBytes)) {
    return nullptr;
  }

  ptrdiff_t newPos = mCurrentPosition + aBytes;
  MOZ_ASSERT(size_t(newPos) <= mMaxSize);

  *aOutOffset = mCurrentPosition;
  *aOutBuffer = mBuffer;

  uint8_t* ptr = reinterpret_cast<uint8_t*>(mMap.mData) + mCurrentPosition;
  mCurrentPosition = newPos;
  return ptr;
}

VertexBufferSection::VertexBufferSection()
 : mOffset(-1),
   mNumVertices(0),
   mStride(0)
{}

void
VertexBufferSection::Init(MLGBuffer* aBuffer, ptrdiff_t aOffset, size_t aNumVertices, size_t aStride)
{
  mBuffer = aBuffer;
  mOffset = aOffset;
  mNumVertices = aNumVertices;
  mStride = aStride;
}

ConstantBufferSection::ConstantBufferSection()
 : mOffset(-1)
{}

void
ConstantBufferSection::Init(MLGBuffer* aBuffer, ptrdiff_t aOffset, size_t aBytes, size_t aNumItems)
{
  mBuffer = aBuffer;
  mOffset = aOffset;
  mNumBytes = aBytes;
  mNumItems = aNumItems;
}

SharedVertexBuffer::SharedVertexBuffer(MLGDevice* aDevice, size_t aDefaultSize)
 : SharedBufferMLGPU(aDevice, MLGBufferType::Vertex, aDefaultSize)
{
}

bool
SharedVertexBuffer::Allocate(VertexBufferSection* aHolder,
                             size_t aNumItems,
                             size_t aSizeOfItem,
                             const void* aData)
{
  RefPtr<MLGBuffer> buffer;
  ptrdiff_t offset;
  size_t bytes = aSizeOfItem * aNumItems;
  uint8_t* ptr = GetBufferPointer(bytes, &offset, &buffer);
  if (!ptr) {
    return false;
  }

  memcpy(ptr, aData, bytes);
  aHolder->Init(buffer, offset, aNumItems, aSizeOfItem);
  return true;
}

AutoBufferUploadBase::AutoBufferUploadBase()
  : mPtr(nullptr)
{
}

AutoBufferUploadBase::~AutoBufferUploadBase()
{
  if (mBuffer) {
    UnmapBuffer();
  }
}

void
AutoBufferUploadBase::Init(void* aPtr, MLGDevice* aDevice, MLGBuffer* aBuffer)
{
  MOZ_ASSERT(!mPtr && aPtr);
  mPtr = aPtr;
  mDevice = aDevice;
  mBuffer = aBuffer;
}

SharedConstantBuffer::SharedConstantBuffer(MLGDevice* aDevice, size_t aDefaultSize)
 : SharedBufferMLGPU(aDevice, MLGBufferType::Constant, aDefaultSize)
{
  mMaxConstantBufferBindSize = aDevice->GetMaxConstantBufferBindSize();
  mCanUseOffsetAllocation = aDevice->CanUseConstantBufferOffsetBinding();
}

bool
SharedConstantBuffer::Allocate(ConstantBufferSection* aHolder,
                               AutoBufferUploadBase* aPtr,
                               size_t aNumItems,
                               size_t aSizeOfItem)
{
  MOZ_ASSERT(aSizeOfItem % 16 == 0, "Items must be padded to 16 bytes");

  size_t bytes = aNumItems * aSizeOfItem;
  if (bytes > mMaxConstantBufferBindSize) {
    gfxWarning() << "Attempted to allocate too many bytes into a constant buffer";
    return false;
  }

  RefPtr<MLGBuffer> buffer;
  ptrdiff_t offset;
  if (!GetBufferPointer(aPtr, bytes, &offset, &buffer)) {
    return false;
  }

  aHolder->Init(buffer, offset, bytes, aNumItems);
  return true;
}

uint8_t*
SharedConstantBuffer::AllocateNewBuffer(size_t aBytes, ptrdiff_t* aOutOffset, RefPtr<MLGBuffer>* aOutBuffer)
{
  RefPtr<MLGBuffer> buffer;
  if (BufferCache* cache = mDevice->GetConstantBufferCache()) {
    buffer = cache->GetOrCreateBuffer(aBytes);
  } else {
    buffer = mDevice->CreateBuffer(MLGBufferType::Constant, aBytes, MLGUsage::Dynamic);
  }
  if (!buffer) {
    return nullptr;
  }

  MLGMappedResource map;
  if (!mDevice->Map(buffer, MLGMapType::WRITE_DISCARD, &map)) {
    return nullptr;
  }

  // Signal that offsetting is not supported.
  *aOutOffset = -1;
  *aOutBuffer = buffer;
  return reinterpret_cast<uint8_t*>(map.mData);
}

void
AutoBufferUploadBase::UnmapBuffer()
{
  mDevice->Unmap(mBuffer);
}

} // namespace layers
} // namespace mozilla
