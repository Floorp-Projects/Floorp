/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_CanvasTranslator_h
#define mozilla_layers_CanvasTranslator_h

#include <unordered_map>
#include <vector>

#include "mozilla/gfx/InlineTranslator.h"
#include "mozilla/layers/CanvasDrawEventRecorder.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/ipc/CrossProcessSemaphore.h"
#include "mozilla/Monitor.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace layers {

class TextureData;

class CanvasTranslator final : public gfx::InlineTranslator {
 public:
  /**
   * Create a canvas translator for a particular TextureType, which translates
   * events from a CanvasEventRingBuffer.
   *
   * @param aTextureType the TextureType the translator will create
   * @param aReadHandle handle to the shared memory for the
   * CanvasEventRingBuffer
   * @param aReaderSem reading blocked semaphore for the CanvasEventRingBuffer
   * @param aWriterSem writing blocked semaphore for the CanvasEventRingBuffer
   * @param aReaderServices provides functions required by the reader
   * @returns the new CanvasTranslator
   */
  static UniquePtr<CanvasTranslator> Create(
      const TextureType& aTextureType,
      const ipc::SharedMemoryBasic::Handle& aReadHandle,
      const CrossProcessSemaphoreHandle& aReaderSem,
      const CrossProcessSemaphoreHandle& aWriterSem,
      UniquePtr<CanvasEventRingBuffer::ReaderServices> aReaderServices);

  ~CanvasTranslator();

  bool IsValid() { return mIsValid; }

  /**
   * Translates events until no more are available or the end of a transaction
   * If this returns false the caller of this is responsible for re-calling
   * this function.
   *
   * @returns true if all events are processed and false otherwise.
   */
  bool TranslateRecording();

  /**
   * Marks the beginning of rendering for a transaction. While in a transaction
   * the translator will wait for a short time for events before returning.
   * When not in a transaction the translator will only translate one event at a
   * time.
   */
  void BeginTransaction();

  /**
   * Marks the end of a transaction.
   */
  void EndTransaction();

  /**
   * Flushes canvas drawing, for example to a device.
   */
  void Flush();

  /**
   * Used to send data back to the writer. This is done through the same shared
   * memory so the writer must wait and read the response after it has submitted
   * the event that uses this.
   *
   * @param aData the data to be written back to the writer
   * @param aSize the number of chars to write
   */
  void ReturnWrite(const char* aData, size_t aSize) {
    mStream.ReturnWrite(aData, aSize);
  }

  /**
   * Used during playback of events to create DrawTargets. For the
   * CanvasTranslator this means creating TextureDatas and getting the
   * DrawTargets from those.
   *
   * @param aRefPtr the key to store the created DrawTarget against
   * @param aSize the size of the DrawTarget
   * @param aFormat the surface format for the DrawTarget
   * @returns the new DrawTarget
   */
  already_AddRefed<gfx::DrawTarget> CreateDrawTarget(
      gfx::ReferencePtr aRefPtr, const gfx::IntSize& aSize,
      gfx::SurfaceFormat aFormat) final;

  /**
   * Get the TextureData associated with a DrawTarget from another process.
   *
   * @param aDrawTarget the key used to find the TextureData
   * @returns the TextureData found
   */
  TextureData* LookupTextureData(gfx::ReferencePtr aDrawTarget);

  /**
   * Waits for the SurfaceDescriptor associated with a DrawTarget from another
   * process to be created and then returns it.
   *
   * @param aDrawTarget the key used to find the TextureData
   * @returns the SurfaceDescriptor found
   */
  UniquePtr<SurfaceDescriptor> WaitForSurfaceDescriptor(
      gfx::ReferencePtr aDrawTarget);

  /**
   * Removes the DrawTarget and other objects associated with a DrawTarget from
   * another process.
   *
   * @param aDrawTarget the key to the objects to remove
   */
  void RemoveDrawTarget(gfx::ReferencePtr aDrawTarget) final;

  /**
   * Removes the SourceSurface and other objects associated with a SourceSurface
   * from another process.
   *
   * @param aRefPtr the key to the objects to remove
   */
  void RemoveSourceSurface(gfx::ReferencePtr aRefPtr) final {
    RemoveDataSurface(aRefPtr);
    InlineTranslator::RemoveSourceSurface(aRefPtr);
  }

  /**
   * Gets the cached DataSourceSurface, if it exists, associated with a
   * SourceSurface from another process.
   *
   * @param aRefPtr the key used to find the DataSourceSurface
   * @returns the DataSourceSurface or nullptr if not found
   */
  gfx::DataSourceSurface* LookupDataSurface(gfx::ReferencePtr aRefPtr);

  /**
   * Used to cache the DataSourceSurface from a SourceSurface associated with a
   * SourceSurface from another process. This is to improve performance if we
   * require the data for that SourceSurface.
   *
   * @param aRefPtr the key used to store the DataSourceSurface
   * @param aSurface the DataSourceSurface to store
   */
  void AddDataSurface(gfx::ReferencePtr aRefPtr,
                      RefPtr<gfx::DataSourceSurface>&& aSurface);

  /**
   * Gets the cached DataSourceSurface, if it exists, associated with a
   * SourceSurface from another process.
   *
   * @param aRefPtr the key used to find the DataSourceSurface
   * @returns the DataSourceSurface or nullptr if not found
   */
  void RemoveDataSurface(gfx::ReferencePtr aRefPtr);

  /**
   * Sets a ScopedMap, to be used in a later event.
   *
   * @param aSurface the associated surface in the other process
   * @param aMap the ScopedMap to store
   */
  void SetPreparedMap(gfx::ReferencePtr aSurface,
                      UniquePtr<gfx::DataSourceSurface::ScopedMap> aMap);

  /**
   * Gets the ScopedMap stored using SetPreparedMap.
   *
   * @param aSurface must match the surface from the SetPreparedMap call
   * @returns the ScopedMap if aSurface matches otherwise nullptr
   */
  UniquePtr<gfx::DataSourceSurface::ScopedMap> GetPreparedMap(
      gfx::ReferencePtr aSurface);

 private:
  CanvasTranslator(
      const TextureType& aTextureType, TextureData* aTextureData,
      gfx::DrawTarget* aDrawTarget,
      const ipc::SharedMemoryBasic::Handle& aReadHandle,
      const CrossProcessSemaphoreHandle& aReaderSem,
      const CrossProcessSemaphoreHandle& aWriterSem,
      UniquePtr<CanvasEventRingBuffer::ReaderServices> aReaderServices);

  void AddSurfaceDescriptor(gfx::ReferencePtr aRefPtr,
                            TextureData* atextureData);

  bool HandleExtensionEvent(int32_t aType);

  CanvasEventRingBuffer mStream;
  TextureType mTextureType = TextureType::Unknown;
  UniquePtr<TextureData> mReferenceTextureData;
  typedef std::unordered_map<void*, UniquePtr<TextureData>> TextureMap;
  TextureMap mTextureDatas;
  nsRefPtrHashtable<nsPtrHashKey<void>, gfx::DataSourceSurface> mDataSurfaces;
  gfx::ReferencePtr mMappedSurface;
  UniquePtr<gfx::DataSourceSurface::ScopedMap> mPreparedMap;
  typedef std::unordered_map<void*, UniquePtr<SurfaceDescriptor>> DescriptorMap;
  DescriptorMap mSurfaceDescriptors;
  Monitor mSurfaceDescriptorsMonitor{
      "CanvasTranslator::mSurfaceDescriptorsMonitor"};
  bool mIsValid = true;
  bool mIsInTransaction = false;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_CanvasTranslator_h
