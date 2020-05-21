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
#include "mozilla/layers/CanvasThread.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/layers/PCanvasParent.h"
#include "mozilla/ipc/CrossProcessSemaphore.h"
#include "mozilla/Monitor.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace layers {

class TextureData;

class CanvasTranslator final : public gfx::InlineTranslator,
                               public PCanvasParent {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CanvasTranslator)

  friend class PProtocolParent;

  /**
   * Create an uninitialized CanvasTranslator and bind it to the given endpoint
   * on the CanvasPlaybackLoop.
   *
   * @param aEndpoint the endpoint to bind to
   * @return the new CanvasTranslator
   */
  static already_AddRefed<CanvasTranslator> Create(
      Endpoint<PCanvasParent>&& aEndpoint);

  /**
   * Shutdown all of the CanvasTranslators.
   */
  static void Shutdown();

  /**
   * Initialize the canvas translator for a particular TextureType and
   * CanvasEventRingBuffer.
   *
   * @param aTextureType the TextureType the translator will create
   * @param aReadHandle handle to the shared memory for the
   *        CanvasEventRingBuffer
   * @param aReaderSem reading blocked semaphore for the CanvasEventRingBuffer
   * @param aWriterSem writing blocked semaphore for the CanvasEventRingBuffer
   */
  ipc::IPCResult RecvInitTranslator(
      const TextureType& aTextureType,
      const ipc::SharedMemoryBasic::Handle& aReadHandle,
      const CrossProcessSemaphoreHandle& aReaderSem,
      const CrossProcessSemaphoreHandle& aWriterSem);

  /**
   * Used to tell the CanvasTranslator to start translating again after it has
   * stopped due to a timeout waiting for events.
   */
  ipc::IPCResult RecvResumeTranslation();

  void ActorDestroy(ActorDestroyReason why) final;

  /**
   * Used by the compositor thread to get the SurfaceDescriptor associated with
   * the DrawTarget from another process.
   *
   * @param aDrawTarget the key to find the TextureData
   * @returns the SurfaceDescriptor associated with the key
   */
  UniquePtr<SurfaceDescriptor> LookupSurfaceDescriptorForClientDrawTarget(
      const uintptr_t aDrawTarget);

  /**
   * Initializes a canvas translator for a particular TextureType, which
   * translates events from a CanvasEventRingBuffer.
   *
   * @param aTextureType the TextureType the translator will create
   * @param aReadHandle handle to the shared memory for the
   * CanvasEventRingBuffer
   * @param aReaderSem reading blocked semaphore for the CanvasEventRingBuffer
   * @param aWriterSem writing blocked semaphore for the CanvasEventRingBuffer
   * @param aReaderServices provides functions required by the reader
   * @returns true if the initialization works, false otherwise
   */
  bool Init(const TextureType& aTextureType,
            const ipc::SharedMemoryBasic::Handle& aReadHandle,
            const CrossProcessSemaphoreHandle& aReaderSem,
            const CrossProcessSemaphoreHandle& aWriterSem,
            UniquePtr<CanvasEventRingBuffer::ReaderServices> aReaderServices);

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
   * Marks that device change processing in the writing process has finished.
   */
  void DeviceChangeAcknowledged();

  /**
   * Used to send data back to the writer. This is done through the same shared
   * memory so the writer must wait and read the response after it has submitted
   * the event that uses this.
   *
   * @param aData the data to be written back to the writer
   * @param aSize the number of chars to write
   */
  void ReturnWrite(const char* aData, size_t aSize) {
    mStream->ReturnWrite(aData, aSize);
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

  /**
   * @return the BackendType being used for translation
   */
  gfx::BackendType GetBackendType() { return mBackendType; }

 private:
  explicit CanvasTranslator(
      already_AddRefed<CanvasThreadHolder> aCanvasThreadHolder);

  ~CanvasTranslator();

  void Bind(Endpoint<PCanvasParent>&& aEndpoint);

  void StartTranslation();

  void FinishShutdown();

  TextureData* CreateTextureData(TextureType aTextureType,
                                 const gfx::IntSize& aSize,
                                 gfx::SurfaceFormat aFormat);

  void AddSurfaceDescriptor(gfx::ReferencePtr aRefPtr,
                            TextureData* atextureData);

  bool HandleExtensionEvent(int32_t aType);

  bool CreateReferenceTexture();
  bool CheckForFreshCanvasDevice(int aLineNumber);
  void NotifyDeviceChanged();

  RefPtr<CanvasThreadHolder> mCanvasThreadHolder;
  RefPtr<TaskQueue> mTranslationTaskQueue;
#if defined(XP_WIN)
  RefPtr<ID3D11Device> mDevice;
#endif
  // We hold the ring buffer as a UniquePtr so we can drop it once
  // mTranslationTaskQueue has shutdown to break a RefPtr cycle.
  UniquePtr<CanvasEventRingBuffer> mStream;
  TextureType mTextureType = TextureType::Unknown;
  UniquePtr<TextureData> mReferenceTextureData;
  // Sometimes during device reset our reference DrawTarget can be null, so we
  // hold the BackendType separately.
  gfx::BackendType mBackendType = gfx::BackendType::NONE;
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
  bool mDeviceResetInProgress = false;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_CanvasTranslator_h
