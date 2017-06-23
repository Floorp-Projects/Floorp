/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_mlgpu_SharedBufferMLGPU_h
#define mozilla_gfx_layers_mlgpu_SharedBufferMLGPU_h

#include "ShaderDefinitionsMLGPU.h"
#include "MLGDeviceTypes.h"
#include "StagingBuffer.h"
#include "mozilla/gfx/Logging.h"

namespace mozilla {
namespace layers {

class MLGBuffer;

class SharedBufferMLGPU
{
public:
  virtual ~SharedBufferMLGPU();

  bool Init();

  // Call before starting a new frame.
  void Reset();

  // Call to finish any pending uploads.
  void PrepareForUsage();

protected:
  SharedBufferMLGPU(MLGDevice* aDevice, MLGBufferType aType, size_t aDefaultSize);

  bool EnsureMappedBuffer(size_t aBytes);
  bool GrowBuffer(size_t aBytes);
  void ForgetBuffer();
  bool Map();
  void Unmap();

  uint8_t* GetBufferPointer(size_t aBytes, ptrdiff_t* aOutOffset, RefPtr<MLGBuffer>* aOutBuffer) {
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

protected:
  // Note: RefPtr here would cause a cycle. Only MLGDevice should own
  // SharedBufferMLGPU objects for now.
  MLGDevice* mDevice;
  MLGBufferType mType;
  size_t mDefaultSize;
  bool mCanUseOffsetAllocation;

  // When |mBuffer| is non-null, mMaxSize is the buffer size. If mapped, the
  // position is between 0 and mMaxSize, otherwise it is always 0.
  RefPtr<MLGBuffer> mBuffer;
  ptrdiff_t mCurrentPosition;
  size_t mMaxSize;

  MLGMappedResource mMap;
  bool mMapped;

  // These are used to track how many frames come in under the default
  // buffer size in a row.
  size_t mBytesUsedThisFrame;
  size_t mNumSmallFrames;
};

class VertexBufferSection final
{
  friend class SharedVertexBuffer;
public:
  VertexBufferSection()
   : mOffset(-1),
     mNumVertices(0),
     mStride(0)
  {}

  uint32_t Stride() const {
    return mStride;
  }
  MLGBuffer* GetBuffer() const {
    return mBuffer;
  }
  ptrdiff_t Offset() const {
    MOZ_ASSERT(IsValid());
    return mOffset;
  }
  size_t NumVertices() const {
    return mNumVertices;
  }
  bool IsValid() const {
    return !!mBuffer;
  }

protected:
  void Init(MLGBuffer* aBuffer, ptrdiff_t aOffset, size_t aNumVertices, size_t aStride) {
    mBuffer = aBuffer;
    mOffset = aOffset;
    mNumVertices = aNumVertices;
    mStride = aStride;
  }

protected:
  RefPtr<MLGBuffer> mBuffer;
  ptrdiff_t mOffset;
  size_t mNumVertices;
  size_t mStride;
};

class ConstantBufferSection final
{
  friend class SharedConstantBuffer;

public:
  ConstantBufferSection()
   : mOffset(-1)
  {}

  uint32_t NumConstants() const {
    return NumConstantsForBytes(mNumBytes);
  }
  size_t NumItems() const {
    return mNumItems;
  }
  uint32_t Offset() const {
    MOZ_ASSERT(IsValid());
    return mOffset / 16;
  }
  MLGBuffer* GetBuffer() const {
    return mBuffer;
  }
  bool IsValid() const {
    return !!mBuffer;
  }
  bool HasOffset() const {
    return mOffset != -1;
  }

protected:
  static constexpr size_t NumConstantsForBytes(size_t aBytes) {
    return (aBytes + ((256 - (aBytes % 256)) % 256)) / 16;
  }

  void Init(MLGBuffer* aBuffer, ptrdiff_t aOffset, size_t aBytes, size_t aNumItems) {
    mBuffer = aBuffer;
    mOffset = aOffset;
    mNumBytes = aBytes;
    mNumItems = aNumItems;
  }

protected:
  RefPtr<MLGBuffer> mBuffer;
  ptrdiff_t mOffset;
  size_t mNumBytes;
  size_t mNumItems;
};

// Vertex buffers don't need special alignment.
typedef StagingBuffer<0> VertexStagingBuffer;

class SharedVertexBuffer final : public SharedBufferMLGPU
{
public:
  SharedVertexBuffer(MLGDevice* aDevice, size_t aDefaultSize);

  // Allocate a buffer that can be uploaded immediately.
  bool Allocate(VertexBufferSection* aHolder, const VertexStagingBuffer& aStaging) {
    return Allocate(aHolder,
                    aStaging.NumItems(),
                    aStaging.SizeOfItem(),
                    aStaging.GetBufferStart());
  }

  // Allocate a buffer that can be uploaded immediately. This is the
  // direct access version, for cases where a StagingBuffer is not
  // needed.
  bool Allocate(VertexBufferSection* aHolder,
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

  template <typename T>
  bool Allocate(VertexBufferSection* aHolder, const T& aItem) {
    return Allocate(aHolder, 1, sizeof(T), &aItem);
  }
};

// To support older Direct3D versions, we need to support one-off MLGBuffers,
// where data is uploaded immediately rather than at the end of all batch
// preparation. We achieve this through a small helper class.
//
// Note: the unmap is not inline sincce we don't include MLGDevice.h.
class MOZ_STACK_CLASS AutoBufferUploadBase
{
public:
  AutoBufferUploadBase() : mPtr(nullptr) {}
  ~AutoBufferUploadBase() {
    if (mBuffer) {
      UnmapBuffer();
    }
  }

  void Init(void* aPtr) {
    MOZ_ASSERT(!mPtr && aPtr);
    mPtr = aPtr;
  }
  void Init(void* aPtr, MLGDevice* aDevice, MLGBuffer* aBuffer) {
    MOZ_ASSERT(!mPtr && aPtr);
    mPtr = aPtr;
    mDevice = aDevice;
    mBuffer = aBuffer;
  }
  void* get() {
    return const_cast<void*>(mPtr);
  }

private:
  void UnmapBuffer();

protected:
  RefPtr<MLGDevice> mDevice;
  RefPtr<MLGBuffer> mBuffer;
  void* mPtr;
};

// This is a typed helper for AutoBufferUploadBase.
template <typename T>
class AutoBufferUpload : public AutoBufferUploadBase
{
public:
  AutoBufferUpload()
  {}

  T* operator ->() const {
    return reinterpret_cast<T*>(mPtr);
  }
};

class SharedConstantBuffer final : public SharedBufferMLGPU
{
public:
  SharedConstantBuffer(MLGDevice* aDevice, size_t aDefaultSize);

  // Allocate a buffer that can be immediately uploaded.
  bool Allocate(ConstantBufferSection* aHolder, const ConstantStagingBuffer& aStaging) {
    MOZ_ASSERT(aStaging.NumItems() * aStaging.SizeOfItem() == aStaging.NumBytes());
    return Allocate(aHolder, aStaging.NumItems(), aStaging.SizeOfItem(), aStaging.GetBufferStart());
  }

  // Allocate a buffer of one item that can be immediately uploaded.
  template <typename T>
  bool Allocate(ConstantBufferSection* aHolder, const T& aItem) {
    return Allocate(aHolder, 1, sizeof(aItem), &aItem);
  }

  // Allocate a buffer of N items that can be immediately uploaded.
  template <typename T>
  bool Allocate(ConstantBufferSection* aHolder, const T* aItems, size_t aNumItems) {
    return Allocate(aHolder, aNumItems, sizeof(T), aItems);
  }

  // Allocate a buffer that is uploaded after the caller has finished writing
  // to it. This should method should generally not be used unless copying T
  // is expensive, since the default immediate-upload version has an implicit
  // extra copy to the GPU. This version exposes the mapped memory directly.
  template <typename T>
  bool Allocate(ConstantBufferSection* aHolder, AutoBufferUpload<T>* aPtr) {
    MOZ_ASSERT(sizeof(T) % 16 == 0, "Items must be padded to 16 bytes");

    return Allocate(aHolder, aPtr, 1, sizeof(T));
  }

private:
  bool Allocate(ConstantBufferSection* aHolder,
                size_t aNumItems,
                size_t aSizeOfItem,
                const void* aData)
  {
    AutoBufferUploadBase ptr;
    if (!Allocate(aHolder, &ptr, aNumItems, aSizeOfItem)) {
      return false;
    }
    memcpy(ptr.get(), aData, aNumItems * aSizeOfItem);
    return true;
  }

  bool Allocate(ConstantBufferSection* aHolder,
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

  bool GetBufferPointer(AutoBufferUploadBase* aPtr,
                        size_t aBytes,
                        ptrdiff_t* aOutOffset,
                        RefPtr<MLGBuffer>* aOutBuffer)
  {
    if (!mCanUseOffsetAllocation) {
      uint8_t* ptr = AllocateNewBuffer(aBytes, aOutOffset, aOutBuffer);
      if (!ptr) {
        return false;
      }
      aPtr->Init(ptr, mDevice, *aOutBuffer);
      return true;
    }

    // Align up the allocation to 256 bytes, since D3D11 requires that
    // constant buffers start at multiples of 16 elements.
    size_t alignedBytes = AlignUp<256>::calc(aBytes);

    uint8_t* ptr = SharedBufferMLGPU::GetBufferPointer(alignedBytes, aOutOffset, aOutBuffer);
    if (!ptr) {
      return false;
    }

    aPtr->Init(ptr);
    return true;
  }

  uint8_t* AllocateNewBuffer(size_t aBytes, ptrdiff_t* aOutOffset, RefPtr<MLGBuffer>* aOutBuffer);

private:
  size_t mMaxConstantBufferBindSize;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_gfx_layers_mlgpu_SharedBufferMLGPU_h
