/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "CanvasTranslator.h"

#include "gfxGradientCache.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/CanvasManagerParent.h"
#include "mozilla/gfx/CanvasRenderThread.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/GPUParent.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/layers/CanvasTranslator.h"
#include "mozilla/layers/SharedSurfacesParent.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/Telemetry.h"
#include "RecordedCanvasEventImpl.h"

#if defined(XP_WIN)
#  include "mozilla/gfx/DeviceManagerDx.h"
#  include "mozilla/layers/TextureD3D11.h"
#endif

namespace mozilla {
namespace layers {

TextureData* CanvasTranslator::CreateTextureData(TextureType aTextureType,
                                                 const gfx::IntSize& aSize,
                                                 gfx::SurfaceFormat aFormat) {
  TextureData* textureData = nullptr;
  switch (aTextureType) {
#ifdef XP_WIN
    case TextureType::D3D11: {
      textureData =
          D3D11TextureData::Create(aSize, aFormat, ALLOC_CLEAR_BUFFER, mDevice);
      break;
    }
#endif
    default:
      MOZ_CRASH("Unsupported TextureType for CanvasTranslator.");
  }

  return textureData;
}

CanvasTranslator::CanvasTranslator() {
  mMaxSpinCount = StaticPrefs::gfx_canvas_remote_max_spin_count();
  mNextEventTimeout = TimeDuration::FromMilliseconds(
      StaticPrefs::gfx_canvas_remote_event_timeout_ms());

  // Track when remote canvas has been activated.
  Telemetry::ScalarAdd(Telemetry::ScalarID::GFX_CANVAS_REMOTE_ACTIVATED, 1);
}

CanvasTranslator::~CanvasTranslator() {
  // The textures need to be the last thing holding their DrawTargets, so that
  // they can destroy them within a lock.
  mDrawTargets.Clear();
  mBaseDT = nullptr;
}

void CanvasTranslator::DispatchToTaskQueue(
    already_AddRefed<nsIRunnable> aRunnable) {
  if (mTranslationTaskQueue) {
    MOZ_ALWAYS_SUCCEEDS(mTranslationTaskQueue->Dispatch(std::move(aRunnable)));
  } else {
    gfx::CanvasRenderThread::Dispatch(std::move(aRunnable));
  }
}

bool CanvasTranslator::IsInTaskQueue() const {
  if (mTranslationTaskQueue) {
    return mTranslationTaskQueue->IsCurrentThreadIn();
  }
  return gfx::CanvasRenderThread::IsInCanvasRenderThread();
}

static bool CreateAndMapShmem(RefPtr<ipc::SharedMemoryBasic>& aShmem,
                              Handle&& aHandle,
                              ipc::SharedMemory::OpenRights aOpenRights,
                              size_t aSize) {
  auto shmem = MakeRefPtr<ipc::SharedMemoryBasic>();
  if (!shmem->SetHandle(std::move(aHandle), aOpenRights) ||
      !shmem->Map(aSize)) {
    return false;
  }

  shmem->CloseHandle();
  aShmem = shmem.forget();
  return true;
}

mozilla::ipc::IPCResult CanvasTranslator::RecvInitTranslator(
    const TextureType& aTextureType, Handle&& aReadHandle,
    nsTArray<Handle>&& aBufferHandles, uint64_t aBufferSize,
    CrossProcessSemaphoreHandle&& aReaderSem,
    CrossProcessSemaphoreHandle&& aWriterSem, bool aUseIPDLThread) {
  if (mHeaderShmem) {
    return IPC_FAIL(this, "RecvInitTranslator called twice.");
  }

  mTextureType = aTextureType;

  mHeaderShmem = MakeAndAddRef<ipc::SharedMemoryBasic>();
  if (!CreateAndMapShmem(mHeaderShmem, std::move(aReadHandle),
                         ipc::SharedMemory::RightsReadWrite, sizeof(Header))) {
    return IPC_FAIL(this, "Failed.");
  }

  mHeader = static_cast<Header*>(mHeaderShmem->memory());

  mWriterSemaphore.reset(CrossProcessSemaphore::Create(std::move(aWriterSem)));
  mWriterSemaphore->CloseHandle();

  mReaderSemaphore.reset(CrossProcessSemaphore::Create(std::move(aReaderSem)));
  mReaderSemaphore->CloseHandle();

#if defined(XP_WIN)
  if (!CheckForFreshCanvasDevice(__LINE__)) {
    gfxCriticalNote << "GFX: CanvasTranslator failed to get device";
    return IPC_OK();
  }
#endif

  if (!aUseIPDLThread) {
    mTranslationTaskQueue = gfx::CanvasRenderThread::CreateWorkerTaskQueue();
  }

  // Use the first buffer as our current buffer.
  mDefaultBufferSize = aBufferSize;
  auto handleIter = aBufferHandles.begin();
  if (!CreateAndMapShmem(mCurrentShmem.shmem, std::move(*handleIter),
                         ipc::SharedMemory::RightsReadOnly, aBufferSize)) {
    return IPC_FAIL(this, "Failed.");
  }
  mCurrentMemReader = mCurrentShmem.CreateMemReader();

  // Add all other buffers to our recycled CanvasShmems.
  for (handleIter++; handleIter < aBufferHandles.end(); handleIter++) {
    CanvasShmem newShmem;
    if (!CreateAndMapShmem(newShmem.shmem, std::move(*handleIter),
                           ipc::SharedMemory::RightsReadOnly, aBufferSize)) {
      return IPC_FAIL(this, "Failed.");
    }
    mCanvasShmems.emplace(std::move(newShmem));
  }

  DispatchToTaskQueue(NewRunnableMethod("CanvasTranslator::TranslateRecording",
                                        this,
                                        &CanvasTranslator::TranslateRecording));
  return IPC_OK();
}

ipc::IPCResult CanvasTranslator::RecvRestartTranslation() {
  if (mDeactivated) {
    // The other side might have sent a message before we deactivated.
    return IPC_OK();
  }

  DispatchToTaskQueue(NewRunnableMethod("CanvasTranslator::TranslateRecording",
                                        this,
                                        &CanvasTranslator::TranslateRecording));

  return IPC_OK();
}

ipc::IPCResult CanvasTranslator::RecvAddBuffer(
    ipc::SharedMemoryBasic::Handle&& aBufferHandle, uint64_t aBufferSize) {
  if (mDeactivated) {
    // The other side might have sent a resume message before we deactivated.
    return IPC_OK();
  }

  DispatchToTaskQueue(
      NewRunnableMethod<ipc::SharedMemoryBasic::Handle&&, size_t>(
          "CanvasTranslator::AddBuffer", this, &CanvasTranslator::AddBuffer,
          std::move(aBufferHandle), aBufferSize));

  return IPC_OK();
}

void CanvasTranslator::AddBuffer(ipc::SharedMemoryBasic::Handle&& aBufferHandle,
                                 size_t aBufferSize) {
  MOZ_ASSERT(IsInTaskQueue());
  MOZ_RELEASE_ASSERT(mHeader->readerState == State::Paused);

  // Default sized buffers will have been queued for recycling.
  if (mCurrentShmem.Size() == mDefaultBufferSize) {
    mCanvasShmems.emplace(std::move(mCurrentShmem));
  }

  CanvasShmem newShmem;
  if (!CreateAndMapShmem(newShmem.shmem, std::move(aBufferHandle),
                         ipc::SharedMemory::RightsReadOnly, aBufferSize)) {
    return;
  }

  mCurrentShmem = std::move(newShmem);
  mCurrentMemReader = mCurrentShmem.CreateMemReader();

  TranslateRecording();
}

ipc::IPCResult CanvasTranslator::RecvSetDataSurfaceBuffer(
    ipc::SharedMemoryBasic::Handle&& aBufferHandle, uint64_t aBufferSize) {
  if (mDeactivated) {
    // The other side might have sent a resume message before we deactivated.
    return IPC_OK();
  }

  DispatchToTaskQueue(
      NewRunnableMethod<ipc::SharedMemoryBasic::Handle&&, size_t>(
          "CanvasTranslator::SetDataSurfaceBuffer", this,
          &CanvasTranslator::SetDataSurfaceBuffer, std::move(aBufferHandle),
          aBufferSize));

  return IPC_OK();
}

void CanvasTranslator::SetDataSurfaceBuffer(
    ipc::SharedMemoryBasic::Handle&& aBufferHandle, size_t aBufferSize) {
  MOZ_ASSERT(IsInTaskQueue());
  MOZ_RELEASE_ASSERT(mHeader->readerState == State::Paused);

  if (!CreateAndMapShmem(mDataSurfaceShmem, std::move(aBufferHandle),
                         ipc::SharedMemory::RightsReadWrite, aBufferSize)) {
    return;
  }

  TranslateRecording();
}

void CanvasTranslator::GetDataSurface(uint64_t aSurfaceRef) {
  MOZ_ASSERT(IsInTaskQueue());

  ReferencePtr surfaceRef = reinterpret_cast<void*>(aSurfaceRef);
  gfx::SourceSurface* surface = LookupSourceSurface(surfaceRef);
  if (!surface) {
    return;
  }

  UniquePtr<gfx::DataSourceSurface::ScopedMap> map = GetPreparedMap(surfaceRef);
  if (!map) {
    return;
  }

  auto dstSize = surface->GetSize();
  auto srcSize = map->GetSurface()->GetSize();
  int32_t dataFormatWidth = dstSize.width * BytesPerPixel(surface->GetFormat());
  int32_t srcStride = map->GetStride();
  if (dataFormatWidth > srcStride || srcSize != dstSize) {
    return;
  }

  auto requiredSize = dataFormatWidth * dstSize.height;
  if (requiredSize <= 0 || size_t(requiredSize) > mDataSurfaceShmem->Size()) {
    return;
  }

  char* dst = static_cast<char*>(mDataSurfaceShmem->memory());
  const char* src = reinterpret_cast<char*>(map->GetData());
  const char* endSrc = src + (srcSize.height * srcStride);
  while (src < endSrc) {
    memcpy(dst, src, dataFormatWidth);
    src += srcStride;
    dst += dataFormatWidth;
  }
}

void CanvasTranslator::RecycleBuffer() {
  mCanvasShmems.emplace(std::move(mCurrentShmem));
  NextBuffer();
}

void CanvasTranslator::NextBuffer() {
  mCurrentShmem = std::move(mCanvasShmems.front());
  mCanvasShmems.pop();
  mCurrentMemReader = mCurrentShmem.CreateMemReader();
}

void CanvasTranslator::ActorDestroy(ActorDestroyReason why) {
  MOZ_ASSERT(gfx::CanvasRenderThread::IsInCanvasRenderThread());

  if (!mTranslationTaskQueue) {
    gfx::CanvasRenderThread::Dispatch(
        NewRunnableMethod("CanvasTranslator::FinishShutdown", this,
                          &CanvasTranslator::FinishShutdown));
    return;
  }

  mTranslationTaskQueue->BeginShutdown()->Then(
      GetCurrentSerialEventTarget(), __func__, this,
      &CanvasTranslator::FinishShutdown, &CanvasTranslator::FinishShutdown);
}

void CanvasTranslator::FinishShutdown() {
  MOZ_ASSERT(gfx::CanvasRenderThread::IsInCanvasRenderThread());
}

bool CanvasTranslator::CheckDeactivated() {
  if (mDeactivated) {
    return true;
  }

  if (NS_WARN_IF(!gfx::gfxVars::RemoteCanvasEnabled())) {
    Deactivate();
  }

  return mDeactivated;
}

void CanvasTranslator::Deactivate() {
  if (mDeactivated) {
    return;
  }
  mDeactivated = true;
  mHeader->readerState = State::Failed;

  // We need to tell the other side to deactivate. Make sure the stream is
  // marked as bad so that the writing side won't wait for space to write.
  gfx::CanvasRenderThread::Dispatch(
      NewRunnableMethod("CanvasTranslator::SendDeactivate", this,
                        &CanvasTranslator::SendDeactivate));

  // Unlock all of our textures.
  for (auto const& entry : mTextureDatas) {
    entry.second->Unlock();
  }

  // Disable remote canvas for all.
  gfx::CanvasManagerParent::DisableRemoteCanvas();
}

void CanvasTranslator::CheckAndSignalWriter() {
  do {
    switch (mHeader->writerState) {
      case State::Processing:
        return;
      case State::AboutToWait:
        // The writer is making a decision about whether to wait. So, we must
        // wait until it has decided to avoid races. Check if the writer is
        // closed to avoid hangs.
        if (!CanSend()) {
          return;
        }
        continue;
      case State::Waiting:
        if (mHeader->processedCount >= mHeader->writerWaitCount) {
          mHeader->writerState = State::Processing;
          mWriterSemaphore->Signal();
        }
        return;
      default:
        MOZ_ASSERT_UNREACHABLE("Invalid waiting state.");
        return;
    }
  } while (true);
}

bool CanvasTranslator::HasPendingEvent() {
  return mHeader->processedCount < mHeader->eventCount;
}

bool CanvasTranslator::ReadPendingEvent(EventType& aEventType) {
  ReadElementConstrained(mCurrentMemReader, aEventType,
                         EventType::DRAWTARGETCREATION, LAST_CANVAS_EVENT_TYPE);
  return mCurrentMemReader.good();
}

bool CanvasTranslator::ReadNextEvent(EventType& aEventType) {
  if (mHeader->readerState == State::Paused) {
    Flush();
    return false;
  }

  uint32_t spinCount = mMaxSpinCount;
  do {
    if (HasPendingEvent()) {
      return ReadPendingEvent(aEventType);
    }
  } while (--spinCount != 0);

  Flush();
  mHeader->readerState = State::AboutToWait;
  if (HasPendingEvent()) {
    mHeader->readerState = State::Processing;
    return ReadPendingEvent(aEventType);
  }

  if (!mIsInTransaction) {
    mHeader->readerState = State::Stopped;
    return false;
  }

  // When in a transaction we wait for a short time because we're expecting more
  // events from the content process. We don't want to wait for too long in case
  // other content processes are waiting for events to process.
  mHeader->readerState = State::Waiting;
  if (mReaderSemaphore->Wait(Some(mNextEventTimeout))) {
    MOZ_RELEASE_ASSERT(HasPendingEvent());
    MOZ_RELEASE_ASSERT(mHeader->readerState == State::Processing);
    return ReadPendingEvent(aEventType);
  }

  // We have to use compareExchange here because the writer can change our
  // state if we are waiting.
  if (!mHeader->readerState.compareExchange(State::Waiting, State::Stopped)) {
    MOZ_RELEASE_ASSERT(HasPendingEvent());
    MOZ_RELEASE_ASSERT(mHeader->readerState == State::Processing);
    // The writer has just signaled us, so consume it before returning
    MOZ_ALWAYS_TRUE(mReaderSemaphore->Wait());
    return ReadPendingEvent(aEventType);
  }

  return false;
}

void CanvasTranslator::TranslateRecording() {
  MOZ_ASSERT(IsInTaskQueue());

  mHeader->readerState = State::Processing;
  EventType eventType;
  while (ReadNextEvent(eventType)) {
    bool success = RecordedEvent::DoWithEventFromReader(
        mCurrentMemReader, eventType,
        [&](RecordedEvent* recordedEvent) -> bool {
          // Make sure that the whole event was read from the stream.
          if (!mCurrentMemReader.good()) {
            if (!CanSend()) {
              // The other side has closed only warn about read failure.
              gfxWarning() << "Failed to read event type: "
                           << recordedEvent->GetType();
            } else {
              gfxCriticalNote << "Failed to read event type: "
                              << recordedEvent->GetType();
            }
            return false;
          }

          return recordedEvent->PlayEvent(this);
        });

    // Check the stream is good here or we will log the issue twice.
    if (!mCurrentMemReader.good()) {
      return;
    }

    if (!success && !HandleExtensionEvent(eventType)) {
      if (mDeviceResetInProgress) {
        // We've notified the recorder of a device change, so we are expecting
        // failures. Log as a warning to prevent crash reporting being flooded.
        gfxWarning() << "Failed to play canvas event type: " << eventType;
      } else {
        gfxCriticalNote << "Failed to play canvas event type: " << eventType;
      }
    }

    mHeader->processedCount++;
  }
}

#define READ_AND_PLAY_CANVAS_EVENT_TYPE(_typeenum, _class)             \
  case _typeenum: {                                                    \
    auto e = _class(mCurrentMemReader);                                \
    if (!mCurrentMemReader.good()) {                                   \
      if (!CanSend()) {                                                \
        /* The other side has closed only warn about read failure. */  \
        gfxWarning() << "Failed to read event type: " << _typeenum;    \
      } else {                                                         \
        gfxCriticalNote << "Failed to read event type: " << _typeenum; \
      }                                                                \
      return false;                                                    \
    }                                                                  \
    return e.PlayCanvasEvent(this);                                    \
  }

bool CanvasTranslator::HandleExtensionEvent(int32_t aType) {
  // This is where we handle extensions to the Moz2D Recording events to handle
  // canvas specific things.
  switch (aType) {
    FOR_EACH_CANVAS_EVENT(READ_AND_PLAY_CANVAS_EVENT_TYPE)
    default:
      return false;
  }
}

void CanvasTranslator::BeginTransaction() { mIsInTransaction = true; }

void CanvasTranslator::Flush() {
#if defined(XP_WIN)
  // We can end up without a device, due to a reset and failure to re-create.
  if (!mDevice) {
    return;
  }

  gfx::AutoSerializeWithMoz2D serializeWithMoz2D(mBackendType);
  RefPtr<ID3D11DeviceContext> deviceContext;
  mDevice->GetImmediateContext(getter_AddRefs(deviceContext));
  deviceContext->Flush();
#endif
}

void CanvasTranslator::EndTransaction() {
  Flush();
  // At the end of a transaction is a good time to check if a new canvas device
  // has been created, even if a reset did not occur.
  Unused << CheckForFreshCanvasDevice(__LINE__);
  mIsInTransaction = false;
}

void CanvasTranslator::DeviceChangeAcknowledged() {
  mDeviceResetInProgress = false;
}

bool CanvasTranslator::CreateReferenceTexture() {
  if (mReferenceTextureData) {
    mReferenceTextureData->Unlock();
  }

  mReferenceTextureData.reset(CreateTextureData(
      mTextureType, gfx::IntSize(1, 1), gfx::SurfaceFormat::B8G8R8A8));
  if (!mReferenceTextureData) {
    return false;
  }

  mReferenceTextureData->Lock(OpenMode::OPEN_READ_WRITE);
  mBaseDT = mReferenceTextureData->BorrowDrawTarget();
  if (!mBaseDT) {
    // We might get a null draw target due to a device failure, just return
    // false so that we can recover.
    return false;
  }

  mBackendType = mBaseDT->GetBackendType();
  return true;
}

bool CanvasTranslator::CheckForFreshCanvasDevice(int aLineNumber) {
#if defined(XP_WIN)
  // If a new device has already been created, use that one.
  RefPtr<ID3D11Device> device = gfx::DeviceManagerDx::Get()->GetCanvasDevice();
  if (device && device != mDevice) {
    if (mDevice) {
      // We already had a device, notify child of change.
      NotifyDeviceChanged();
    }
    mDevice = device.forget();
    return CreateReferenceTexture();
  }

  if (mDevice) {
    if (mDevice->GetDeviceRemovedReason() == S_OK) {
      return false;
    }

    gfxCriticalNote << "GFX: CanvasTranslator detected a device reset at "
                    << aLineNumber;
    NotifyDeviceChanged();
  }

  RefPtr<Runnable> runnable = NS_NewRunnableFunction(
      "CanvasTranslator NotifyDeviceReset",
      []() { gfx::GPUParent::GetSingleton()->NotifyDeviceReset(); });

  // It is safe to wait here because only the Compositor thread waits on us and
  // the main thread doesn't wait on the compositor thread in the GPU process.
  SyncRunnable::DispatchToThread(GetMainThreadSerialEventTarget(), runnable,
                                 /*aForceDispatch*/ true);

  mDevice = gfx::DeviceManagerDx::Get()->GetCanvasDevice();
  if (!mDevice) {
    // We don't have a canvas device, we need to deactivate.
    Telemetry::ScalarAdd(
        Telemetry::ScalarID::GFX_CANVAS_REMOTE_DEACTIVATED_NO_DEVICE, 1);
    Deactivate();
    return false;
  }

  return CreateReferenceTexture();
#else
  return false;
#endif
}

void CanvasTranslator::NotifyDeviceChanged() {
  mDeviceResetInProgress = true;
  gfx::CanvasRenderThread::Dispatch(
      NewRunnableMethod("CanvasTranslator::SendNotifyDeviceChanged", this,
                        &CanvasTranslator::SendNotifyDeviceChanged));
}

already_AddRefed<gfx::DrawTarget> CanvasTranslator::CreateDrawTarget(
    gfx::ReferencePtr aRefPtr, const gfx::IntSize& aSize,
    gfx::SurfaceFormat aFormat) {
  RefPtr<gfx::DrawTarget> dt;
  do {
    TextureData* textureData = CreateTextureData(mTextureType, aSize, aFormat);
    if (textureData) {
      MOZ_DIAGNOSTIC_ASSERT(mNextTextureId >= 0, "No texture ID set");
      textureData->Lock(OpenMode::OPEN_READ_WRITE);
      mTextureDatas[mNextTextureId] = UniquePtr<TextureData>(textureData);
      gfx::CanvasManagerParent::AddReplayTexture(this, mNextTextureId,
                                                 textureData);
      dt = textureData->BorrowDrawTarget();
    }
  } while (!dt && CheckForFreshCanvasDevice(__LINE__));
  AddDrawTarget(aRefPtr, dt);
  mNextTextureId = -1;

  return dt.forget();
}

void CanvasTranslator::RemoveTexture(int64_t aTextureId) {
  mTextureDatas.erase(aTextureId);

  // It is possible that the texture from the content process has never been
  // forwarded from the GPU process, so make sure its descriptor is removed.
  gfx::CanvasManagerParent::RemoveReplayTexture(this, aTextureId);
}

TextureData* CanvasTranslator::LookupTextureData(int64_t aTextureId) {
  TextureMap::const_iterator result = mTextureDatas.find(aTextureId);
  if (result == mTextureDatas.end()) {
    return nullptr;
  }
  return result->second.get();
}

already_AddRefed<gfx::SourceSurface> CanvasTranslator::LookupExternalSurface(
    uint64_t aKey) {
  return SharedSurfacesParent::Get(wr::ToExternalImageId(aKey));
}

void CanvasTranslator::CheckpointReached() { CheckAndSignalWriter(); }

void CanvasTranslator::PauseTranslation() {
  mHeader->readerState = State::Paused;
}

already_AddRefed<gfx::GradientStops> CanvasTranslator::GetOrCreateGradientStops(
    gfx::GradientStop* aRawStops, uint32_t aNumStops,
    gfx::ExtendMode aExtendMode) {
  nsTArray<gfx::GradientStop> rawStopArray(aRawStops, aNumStops);
  RefPtr<DrawTarget> drawTarget = GetReferenceDrawTarget();
  if (!drawTarget) {
    // We might end up with a null reference draw target due to a device
    // failure, just return false so that we can recover.
    return nullptr;
  }

  return gfx::gfxGradientCache::GetOrCreateGradientStops(
      drawTarget, rawStopArray, aExtendMode);
}

gfx::DataSourceSurface* CanvasTranslator::LookupDataSurface(
    gfx::ReferencePtr aRefPtr) {
  return mDataSurfaces.GetWeak(aRefPtr);
}

void CanvasTranslator::AddDataSurface(
    gfx::ReferencePtr aRefPtr, RefPtr<gfx::DataSourceSurface>&& aSurface) {
  mDataSurfaces.InsertOrUpdate(aRefPtr, std::move(aSurface));
}

void CanvasTranslator::RemoveDataSurface(gfx::ReferencePtr aRefPtr) {
  mDataSurfaces.Remove(aRefPtr);
}

void CanvasTranslator::SetPreparedMap(
    gfx::ReferencePtr aSurface,
    UniquePtr<gfx::DataSourceSurface::ScopedMap> aMap) {
  mMappedSurface = aSurface;
  mPreparedMap = std::move(aMap);
}

UniquePtr<gfx::DataSourceSurface::ScopedMap> CanvasTranslator::GetPreparedMap(
    gfx::ReferencePtr aSurface) {
  if (!mPreparedMap) {
    // We might fail to set the map during, for example, device resets.
    return nullptr;
  }

  MOZ_RELEASE_ASSERT(mMappedSurface == aSurface,
                     "aSurface must match previously stored surface.");

  mMappedSurface = nullptr;
  return std::move(mPreparedMap);
}

}  // namespace layers
}  // namespace mozilla
