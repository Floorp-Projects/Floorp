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
#include "mozilla/layers/PCanvasParent.h"
#include "mozilla/ipc/CrossProcessSemaphore.h"
#include "mozilla/Monitor.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
class TaskQueue;

namespace layers {

class TextureData;

class CanvasTranslator final : public gfx::InlineTranslator,
                               public PCanvasParent {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CanvasTranslator)

  friend class PProtocolParent;

  CanvasTranslator();

  /**
   * Dispatches a runnable to the preferred task queue or thread.
   *
   * @param aRunnable the runnable to dispatch
   */
  void DispatchToTaskQueue(already_AddRefed<nsIRunnable> aRunnable);

  /**
   * @returns true if running in the preferred task queue or thread for
   * translation.
   */
  bool IsInTaskQueue() const;

  /**
   * Initialize the canvas translator for a particular TextureType and
   * CanvasEventRingBuffer.
   *
   * @param aTextureType the TextureType the translator will create
   * @param aReadHandle handle to the shared memory for the
   *        CanvasEventRingBuffer
   * @param aReaderSem reading blocked semaphore for the CanvasEventRingBuffer
   * @param aWriterSem writing blocked semaphore for the CanvasEventRingBuffer
   * @param aUseIPDLThread if true, use the IPDL thread instead of the worker
   *        pool for translation requests
   */
  ipc::IPCResult RecvInitTranslator(
      const TextureType& aTextureType,
      ipc::SharedMemoryBasic::Handle&& aReadHandle,
      CrossProcessSemaphoreHandle&& aReaderSem,
      CrossProcessSemaphoreHandle&& aWriterSem, const bool& aUseIPDLThread);

  /**
   * New buffer to resume translation after it has been stopped by writer.
   */
  ipc::IPCResult RecvNewBuffer(ipc::SharedMemoryBasic::Handle&& aReadHandle);

  /**
   * Used to tell the CanvasTranslator to start translating again after it has
   * stopped due to a timeout waiting for events.
   */
  ipc::IPCResult RecvResumeTranslation();

  void ActorDestroy(ActorDestroyReason why) final;

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
   * Set the texture ID that will be used as a lookup for the texture created by
   * the next CreateDrawTarget.
   */
  void SetNextTextureId(int64_t aNextTextureId) {
    mNextTextureId = aNextTextureId;
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

  already_AddRefed<gfx::GradientStops> GetOrCreateGradientStops(
      gfx::GradientStop* aRawStops, uint32_t aNumStops,
      gfx::ExtendMode aExtendMode) final;

  /**
   * Get the TextureData associated with a TextureData from another process.
   *
   * @param aTextureId the key used to find the TextureData
   * @returns the TextureData found
   */
  TextureData* LookupTextureData(int64_t aTextureId);

  /**
   * Removes the texture and other objects associated with a texture ID.
   *
   * @param aTextureId the texture ID to remove
   */
  void RemoveTexture(int64_t aTextureId);

  /**
   * Overriden to remove any DataSourceSurfaces associated with the RefPtr.
   *
   * @param aRefPtr the key to the surface
   * @param aSurface the surface to store
   */
  void AddSourceSurface(gfx::ReferencePtr aRefPtr,
                        gfx::SourceSurface* aSurface) final {
    if (mMappedSurface == aRefPtr) {
      mPreparedMap = nullptr;
      mMappedSurface = nullptr;
    }
    RemoveDataSurface(aRefPtr);
    InlineTranslator::AddSourceSurface(aRefPtr, aSurface);
  }

  /**
   * Removes the SourceSurface and other objects associated with a SourceSurface
   * from another process.
   *
   * @param aRefPtr the key to the objects to remove
   */
  void RemoveSourceSurface(gfx::ReferencePtr aRefPtr) final {
    if (mMappedSurface == aRefPtr) {
      mPreparedMap = nullptr;
      mMappedSurface = nullptr;
    }
    RemoveDataSurface(aRefPtr);
    InlineTranslator::RemoveSourceSurface(aRefPtr);
  }

  already_AddRefed<gfx::SourceSurface> LookupExternalSurface(
      uint64_t aKey) final;

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
  ~CanvasTranslator();

  void Bind(Endpoint<PCanvasParent>&& aEndpoint);

  void StartTranslation();

  void FinishShutdown();

  bool CheckDeactivated();

  void Deactivate();

  TextureData* CreateTextureData(TextureType aTextureType,
                                 const gfx::IntSize& aSize,
                                 gfx::SurfaceFormat aFormat);

  void AddSurfaceDescriptor(int64_t aTextureId, TextureData* atextureData);

  bool HandleExtensionEvent(int32_t aType);

  bool CreateReferenceTexture();
  bool CheckForFreshCanvasDevice(int aLineNumber);
  void NotifyDeviceChanged();

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
  typedef std::unordered_map<int64_t, UniquePtr<TextureData>> TextureMap;
  TextureMap mTextureDatas;
  int64_t mNextTextureId = -1;
  nsRefPtrHashtable<nsPtrHashKey<void>, gfx::DataSourceSurface> mDataSurfaces;
  gfx::ReferencePtr mMappedSurface;
  UniquePtr<gfx::DataSourceSurface::ScopedMap> mPreparedMap;
  Atomic<bool> mDeactivated{false};
  bool mIsInTransaction = false;
  bool mDeviceResetInProgress = false;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_CanvasTranslator_h
