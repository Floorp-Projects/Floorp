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
#include "mozilla/gfx/DrawTargetWebgl.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/GPUParent.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/layers/BufferTexture.h"
#include "mozilla/layers/CanvasTranslator.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/SharedSurfacesParent.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/Telemetry.h"
#include "GLContext.h"
#include "RecordedCanvasEventImpl.h"

#if defined(XP_WIN)
#  include "mozilla/gfx/DeviceManagerDx.h"
#  include "mozilla/layers/TextureD3D11.h"
#endif

namespace mozilla {
namespace layers {

UniquePtr<TextureData> CanvasTranslator::CreateTextureData(
    const gfx::IntSize& aSize, gfx::SurfaceFormat aFormat, bool aClear) {
  TextureData* textureData = nullptr;
  TextureAllocationFlags allocFlags =
      aClear ? ALLOC_CLEAR_BUFFER : ALLOC_DEFAULT;
  switch (mTextureType) {
#ifdef XP_WIN
    case TextureType::D3D11: {
      textureData =
          D3D11TextureData::Create(aSize, aFormat, allocFlags, mDevice);
      break;
    }
#endif
    case TextureType::Unknown:
      textureData = BufferTextureData::Create(
          aSize, aFormat, gfx::BackendType::SKIA, LayersBackend::LAYERS_WR,
          TextureFlags::DEALLOCATE_CLIENT | TextureFlags::REMOTE_TEXTURE,
          allocFlags, nullptr);
      break;
    default:
      textureData = TextureData::Create(mTextureType, aFormat, aSize,
                                        allocFlags, mBackendType);
      break;
  }

  return WrapUnique(textureData);
}

CanvasTranslator::CanvasTranslator(
    layers::SharedSurfacesHolder* aSharedSurfacesHolder,
    const dom::ContentParentId& aContentId, uint32_t aManagerId)
    : mSharedSurfacesHolder(aSharedSurfacesHolder),
      mMaxSpinCount(StaticPrefs::gfx_canvas_remote_max_spin_count()),
      mContentId(aContentId),
      mManagerId(aManagerId) {
  mNextEventTimeout = TimeDuration::FromMilliseconds(
      StaticPrefs::gfx_canvas_remote_event_timeout_ms());

  // Track when remote canvas has been activated.
  Telemetry::ScalarAdd(Telemetry::ScalarID::GFX_CANVAS_REMOTE_ACTIVATED, 1);
}

CanvasTranslator::~CanvasTranslator() = default;

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

bool CanvasTranslator::EnsureSharedContextWebgl() {
  if (!mSharedContext || mSharedContext->IsContextLost()) {
    if (mSharedContext) {
      ForceDrawTargetWebglFallback();
      if (mRemoteTextureOwner) {
        // Ensure any shared surfaces referring to the old context go away.
        mRemoteTextureOwner->ClearRecycledTextures();
      }
    }
    mSharedContext = gfx::SharedContextWebgl::Create();
    if (!mSharedContext || mSharedContext->IsContextLost()) {
      mSharedContext = nullptr;
      BlockCanvas();
      return false;
    }
  }
  return true;
}

mozilla::ipc::IPCResult CanvasTranslator::RecvInitTranslator(
    TextureType aTextureType, gfx::BackendType aBackendType,
    Handle&& aReadHandle, nsTArray<Handle>&& aBufferHandles,
    uint64_t aBufferSize, CrossProcessSemaphoreHandle&& aReaderSem,
    CrossProcessSemaphoreHandle&& aWriterSem, bool aUseIPDLThread) {
  if (mHeaderShmem) {
    return IPC_FAIL(this, "RecvInitTranslator called twice.");
  }

  mTextureType = aTextureType;
  mBackendType = aBackendType;
  mOtherPid = OtherPid();

  mHeaderShmem = MakeAndAddRef<ipc::SharedMemoryBasic>();
  if (!CreateAndMapShmem(mHeaderShmem, std::move(aReadHandle),
                         ipc::SharedMemory::RightsReadWrite, sizeof(Header))) {
    Deactivate();
    return IPC_FAIL(this, "Failed to map canvas header shared memory.");
  }

  mHeader = static_cast<Header*>(mHeaderShmem->memory());

  mWriterSemaphore.reset(CrossProcessSemaphore::Create(std::move(aWriterSem)));
  mWriterSemaphore->CloseHandle();

  mReaderSemaphore.reset(CrossProcessSemaphore::Create(std::move(aReaderSem)));
  mReaderSemaphore->CloseHandle();

  if (!CheckForFreshCanvasDevice(__LINE__)) {
    gfxCriticalNote << "GFX: CanvasTranslator failed to get device";
    return IPC_OK();
  }

  if (gfx::gfxVars::UseAcceleratedCanvas2D() && !EnsureSharedContextWebgl()) {
    gfxCriticalNote
        << "GFX: CanvasTranslator failed creating WebGL shared context";
  }

  if (!aUseIPDLThread) {
    mTranslationTaskQueue = gfx::CanvasRenderThread::CreateWorkerTaskQueue();
  }

  // Use the first buffer as our current buffer.
  mDefaultBufferSize = aBufferSize;
  auto handleIter = aBufferHandles.begin();
  if (!CreateAndMapShmem(mCurrentShmem.shmem, std::move(*handleIter),
                         ipc::SharedMemory::RightsReadOnly, aBufferSize)) {
    Deactivate();
    return IPC_FAIL(this, "Failed to map canvas buffer shared memory.");
  }
  mCurrentMemReader = mCurrentShmem.CreateMemReader();

  // Add all other buffers to our recycled CanvasShmems.
  for (handleIter++; handleIter < aBufferHandles.end(); handleIter++) {
    CanvasShmem newShmem;
    if (!CreateAndMapShmem(newShmem.shmem, std::move(*handleIter),
                           ipc::SharedMemory::RightsReadOnly, aBufferSize)) {
      Deactivate();
      return IPC_FAIL(this, "Failed to map canvas buffer shared memory.");
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
  if (mHeader->readerState == State::Failed) {
    // We failed before we got to the pause event.
    return;
  }

  if (mHeader->readerState != State::Paused) {
    gfxCriticalNote << "CanvasTranslator::AddBuffer bad state "
                    << uint32_t(State(mHeader->readerState));
    MOZ_DIAGNOSTIC_ASSERT(false, "mHeader->readerState == State::Paused");
    Deactivate();
    return;
  }

  MOZ_ASSERT(mDefaultBufferSize != 0);

  // Check and signal the writer when we finish with a buffer, because it
  // might have hit the buffer count limit and be waiting to use our old one.
  CheckAndSignalWriter();

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
  if (mHeader->readerState == State::Failed) {
    // We failed before we got to the pause event.
    return;
  }

  if (mHeader->readerState != State::Paused) {
    gfxCriticalNote << "CanvasTranslator::SetDataSurfaceBuffer bad state "
                    << uint32_t(State(mHeader->readerState));
    MOZ_DIAGNOSTIC_ASSERT(false, "mHeader->readerState == State::Paused");
    Deactivate();
    return;
  }

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
  gfx::SurfaceFormat format = surface->GetFormat();
  int32_t bpp = BytesPerPixel(format);
  int32_t dataFormatWidth = dstSize.width * bpp;
  int32_t srcStride = map->GetStride();
  if (dataFormatWidth > srcStride || srcSize != dstSize) {
    return;
  }

  int32_t dstStride =
      ImageDataSerializer::ComputeRGBStride(format, dstSize.width);
  auto requiredSize =
      ImageDataSerializer::ComputeRGBBufferSize(dstSize, format);
  if (requiredSize <= 0 || size_t(requiredSize) > mDataSurfaceShmem->Size()) {
    return;
  }

  uint8_t* dst = static_cast<uint8_t*>(mDataSurfaceShmem->memory());
  const uint8_t* src = map->GetData();
  const uint8_t* endSrc = src + (srcSize.height * srcStride);
  while (src < endSrc) {
    memcpy(dst, src, dataFormatWidth);
    src += srcStride;
    dst += dstStride;
  }
}

void CanvasTranslator::RecycleBuffer() {
  mCanvasShmems.emplace(std::move(mCurrentShmem));
  NextBuffer();
}

void CanvasTranslator::NextBuffer() {
  // Check and signal the writer when we finish with a buffer, because it
  // might have hit the buffer count limit and be waiting to use our old one.
  CheckAndSignalWriter();

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

  mTranslationTaskQueue->BeginShutdown();
  mTranslationTaskQueue->AwaitShutdownAndIdle();
  FinishShutdown();
}

void CanvasTranslator::FinishShutdown() { ClearTextureInfo(); }

bool CanvasTranslator::CheckDeactivated() {
  if (mDeactivated) {
    return true;
  }

  if (NS_WARN_IF(!gfx::gfxVars::RemoteCanvasEnabled() &&
                 !gfx::gfxVars::UseAcceleratedCanvas2D())) {
    Deactivate();
  }

  return mDeactivated;
}

void CanvasTranslator::Deactivate() {
  if (mDeactivated) {
    return;
  }
  mDeactivated = true;
  if (mHeader) {
    mHeader->readerState = State::Failed;
  }

  // We need to tell the other side to deactivate. Make sure the stream is
  // marked as bad so that the writing side won't wait for space to write.
  gfx::CanvasRenderThread::Dispatch(
      NewRunnableMethod("CanvasTranslator::SendDeactivate", this,
                        &CanvasTranslator::SendDeactivate));

  // Disable remote canvas for all.
  gfx::CanvasManagerParent::DisableRemoteCanvas();
}

inline gfx::DrawTargetWebgl* CanvasTranslator::TextureInfo::GetDrawTargetWebgl(
    bool aCheckForFallback) const {
  if ((!mTextureData || !aCheckForFallback) && mDrawTarget &&
      mDrawTarget->GetBackendType() == gfx::BackendType::WEBGL) {
    return static_cast<gfx::DrawTargetWebgl*>(mDrawTarget.get());
  }
  return nullptr;
}

void CanvasTranslator::ForceDrawTargetWebglFallback() {
  // This looks for any DrawTargetWebgls that have a cached data snapshot that
  // can be used to recover a fallback TextureData in the event of a context
  // loss.
  RemoteTextureOwnerIdSet lost;
  for (const auto& entry : mTextureInfo) {
    const auto& info = entry.second;
    if (gfx::DrawTargetWebgl* webgl = info.GetDrawTargetWebgl()) {
      NotifyRequiresRefresh(entry.first);
      if (webgl->HasDataSnapshot()) {
        if (RefPtr<gfx::DrawTarget> dt = CreateFallbackDrawTarget(
                info.mRefPtr, entry.first, info.mRemoteTextureOwnerId,
                webgl->GetSize(), webgl->GetFormat())) {
          webgl->CopyToFallback(dt);
          AddDrawTarget(info.mRefPtr, dt);
          continue;
        }
      }
      // No fallback could be created, so we need to notify the compositor the
      // texture won't be pushed.
      if (mRemoteTextureOwner &&
          mRemoteTextureOwner->IsRegistered(info.mRemoteTextureOwnerId)) {
        lost.insert(info.mRemoteTextureOwnerId);
      }
    }
  }
  if (!lost.empty()) {
    mRemoteTextureOwner->NotifyContextLost(&lost);
  }
}

void CanvasTranslator::BlockCanvas() {
  if (mDeactivated || mBlocked) {
    return;
  }
  mBlocked = true;
  gfx::CanvasRenderThread::Dispatch(
      NewRunnableMethod("CanvasTranslator::SendBlockCanvas", this,
                        &CanvasTranslator::SendBlockCanvas));
}

void CanvasTranslator::CheckAndSignalWriter() {
  do {
    switch (mHeader->writerState) {
      case State::Processing:
      case State::Failed:
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

  if (mSharedContext && EnsureSharedContextWebgl()) {
    mSharedContext->EnterTlsScope();
  }
  auto exitTlsScope = MakeScopeExit([&] {
    if (mSharedContext) {
      mSharedContext->ExitTlsScope();
    }
  });

  mHeader->readerState = State::Processing;
  EventType eventType = EventType::INVALID;
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
            mHeader->readerState = State::Failed;
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
      mHeader->readerState = State::Failed;
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
  if (mRemoteTextureOwner) {
    mRemoteTextureOwner->NotifyContextRestored();
  }
}

bool CanvasTranslator::CreateReferenceTexture() {
  if (mReferenceTextureData) {
    mReferenceTextureData->Unlock();
  }

  mReferenceTextureData =
      CreateTextureData(gfx::IntSize(1, 1), gfx::SurfaceFormat::B8G8R8A8, true);
  if (!mReferenceTextureData) {
    Deactivate();
    return false;
  }

  if (NS_WARN_IF(!mReferenceTextureData->Lock(OpenMode::OPEN_READ_WRITE))) {
    gfxCriticalNote << "CanvasTranslator::CreateReferenceTexture lock failed";
    mReferenceTextureData.reset();
    Deactivate();
    return false;
  }

  mBaseDT = mReferenceTextureData->BorrowDrawTarget();

  if (!mBaseDT) {
    // We might get a null draw target due to a device failure, deactivate and
    // return false so that we can recover.
    Deactivate();
    return false;
  }

  return true;
}

bool CanvasTranslator::CheckForFreshCanvasDevice(int aLineNumber) {
  // If not on D3D11, we are not dependent on a fresh device for DT creation if
  // one already exists.
  if (mBaseDT && mTextureType != TextureType::D3D11) {
    return false;
  }

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
#endif

  return CreateReferenceTexture();
}

void CanvasTranslator::NotifyDeviceChanged() {
  // Clear out any old recycled texture datas with the wrong device.
  if (mRemoteTextureOwner) {
    mRemoteTextureOwner->NotifyContextLost();
    mRemoteTextureOwner->ClearRecycledTextures();
  }
  mDeviceResetInProgress = true;
  gfx::CanvasRenderThread::Dispatch(
      NewRunnableMethod("CanvasTranslator::SendNotifyDeviceChanged", this,
                        &CanvasTranslator::SendNotifyDeviceChanged));
}

gfx::DrawTargetWebgl* CanvasTranslator::GetDrawTargetWebgl(
    int64_t aTextureId, bool aCheckForFallback) const {
  auto result = mTextureInfo.find(aTextureId);
  if (result != mTextureInfo.end()) {
    return result->second.GetDrawTargetWebgl(aCheckForFallback);
  }
  return nullptr;
}

void CanvasTranslator::NotifyRequiresRefresh(int64_t aTextureId,
                                             bool aDispatch) {
  if (aDispatch) {
    auto& info = mTextureInfo[aTextureId];
    if (!info.mNotifiedRequiresRefresh) {
      info.mNotifiedRequiresRefresh = true;
      DispatchToTaskQueue(NewRunnableMethod<int64_t, bool>(
          "CanvasTranslator::NotifyRequiresRefresh", this,
          &CanvasTranslator::NotifyRequiresRefresh, aTextureId, false));
    }
    return;
  }

  if (mTextureInfo.find(aTextureId) != mTextureInfo.end()) {
    Unused << SendNotifyRequiresRefresh(aTextureId);
  }
}

void CanvasTranslator::CacheSnapshotShmem(int64_t aTextureId, bool aDispatch) {
  if (aDispatch) {
    DispatchToTaskQueue(NewRunnableMethod<int64_t, bool>(
        "CanvasTranslator::CacheSnapshotShmem", this,
        &CanvasTranslator::CacheSnapshotShmem, aTextureId, false));
    return;
  }

  if (gfx::DrawTargetWebgl* webgl = GetDrawTargetWebgl(aTextureId)) {
    if (auto shmemHandle = webgl->TakeShmemHandle()) {
      // Lock the DT so that it doesn't get removed while shmem is in transit.
      mTextureInfo[aTextureId].mLocked++;
      nsCOMPtr<nsIThread> thread =
          gfx::CanvasRenderThread::GetCanvasRenderThread();
      RefPtr<CanvasTranslator> translator = this;
      SendSnapshotShmem(aTextureId, std::move(shmemHandle),
                        webgl->GetShmemSize())
          ->Then(
              thread, __func__,
              [=](bool) { translator->RemoveTexture(aTextureId); },
              [=](ipc::ResponseRejectReason) {
                translator->RemoveTexture(aTextureId);
              });
    }
  }
}

void CanvasTranslator::PrepareShmem(int64_t aTextureId) {
  if (gfx::DrawTargetWebgl* webgl = GetDrawTargetWebgl(aTextureId, false)) {
    if (const auto& fallback = mTextureInfo[aTextureId].mTextureData) {
      // If there was a fallback, copy the fallback to the software framebuffer
      // shmem for reading.
      if (RefPtr<gfx::DrawTarget> dt = fallback->BorrowDrawTarget()) {
        if (RefPtr<gfx::SourceSurface> snapshot = dt->Snapshot()) {
          webgl->CopySurface(snapshot, snapshot->GetRect(),
                             gfx::IntPoint(0, 0));
        }
      }
    } else {
      // Otherwise, just ensure the software framebuffer is up to date.
      webgl->PrepareData();
    }
  }
}

void CanvasTranslator::ClearCachedResources() {
  if (mSharedContext) {
    // If there are any DrawTargetWebgls, then try to cache their framebuffers
    // in software surfaces, just in case the GL context is lost. So long as
    // there is a software copy of the framebuffer, it can be copied into a
    // fallback TextureData later even if the GL context goes away.
    mSharedContext->OnMemoryPressure();
    for (auto const& entry : mTextureInfo) {
      if (gfx::DrawTargetWebgl* webgl = entry.second.GetDrawTargetWebgl()) {
        if (!webgl->HasDataSnapshot()) {
          webgl->PrepareData();
        }
      }
    }
  }
}

ipc::IPCResult CanvasTranslator::RecvClearCachedResources() {
  if (mDeactivated) {
    // The other side might have sent a message before we deactivated.
    return IPC_OK();
  }

  DispatchToTaskQueue(
      NewRunnableMethod("CanvasTranslator::ClearCachedResources", this,
                        &CanvasTranslator::ClearCachedResources));
  return IPC_OK();
}

static const OpenMode kInitMode = OpenMode::OPEN_READ_WRITE;

already_AddRefed<gfx::DrawTarget> CanvasTranslator::CreateFallbackDrawTarget(
    gfx::ReferencePtr aRefPtr, int64_t aTextureId,
    RemoteTextureOwnerId aTextureOwnerId, const gfx::IntSize& aSize,
    gfx::SurfaceFormat aFormat) {
  RefPtr<gfx::DrawTarget> dt;
  do {
    UniquePtr<TextureData> textureData =
        CreateOrRecycleTextureData(aSize, aFormat);
    if (NS_WARN_IF(!textureData)) {
      continue;
    }

    if (NS_WARN_IF(!textureData->Lock(kInitMode))) {
      gfxCriticalNote << "CanvasTranslator::CreateDrawTarget lock failed";
      continue;
    }

    dt = textureData->BorrowDrawTarget();
    if (NS_WARN_IF(!dt)) {
      textureData->Unlock();
      continue;
    }
    // Recycled buffer contents may be uninitialized.
    dt->ClearRect(gfx::Rect(dt->GetRect()));

    TextureInfo& info = mTextureInfo[aTextureId];
    info.mRefPtr = aRefPtr;
    info.mTextureData = std::move(textureData);
    info.mRemoteTextureOwnerId = aTextureOwnerId;
    info.mTextureLockMode = kInitMode;
  } while (!dt && CheckForFreshCanvasDevice(__LINE__));
  return dt.forget();
}

already_AddRefed<gfx::DrawTarget> CanvasTranslator::CreateDrawTarget(
    gfx::ReferencePtr aRefPtr, int64_t aTextureId,
    RemoteTextureOwnerId aTextureOwnerId, const gfx::IntSize& aSize,
    gfx::SurfaceFormat aFormat) {
  if (aTextureId < 0) {
    MOZ_DIAGNOSTIC_ASSERT(false, "No texture ID set");
    return nullptr;
  }

  if (!aTextureOwnerId.IsValid()) {
    MOZ_DIAGNOSTIC_ASSERT(false, "No texture owner set");
    return nullptr;
  }

  RefPtr<gfx::DrawTarget> dt;
  if (gfx::gfxVars::UseAcceleratedCanvas2D()) {
    if (EnsureSharedContextWebgl()) {
      mSharedContext->EnterTlsScope();
    }
    if (RefPtr<gfx::DrawTargetWebgl> webgl =
            gfx::DrawTargetWebgl::Create(aSize, aFormat, mSharedContext)) {
      webgl->BeginFrame(true);
      dt = webgl.forget().downcast<gfx::DrawTarget>();
      if (dt) {
        TextureInfo& info = mTextureInfo[aTextureId];
        info.mRefPtr = aRefPtr;
        info.mDrawTarget = dt;
        info.mRemoteTextureOwnerId = aTextureOwnerId;
        info.mTextureLockMode = kInitMode;
        CacheSnapshotShmem(aTextureId);
      }
    }
    if (!dt) {
      NotifyRequiresRefresh(aTextureId);
    }
  }

  if (!dt) {
    dt = CreateFallbackDrawTarget(aRefPtr, aTextureId, aTextureOwnerId, aSize,
                                  aFormat);
  }

  AddDrawTarget(aRefPtr, dt);
  return dt.forget();
}

already_AddRefed<gfx::DrawTarget> CanvasTranslator::CreateDrawTarget(
    gfx::ReferencePtr aRefPtr, const gfx::IntSize& aSize,
    gfx::SurfaceFormat aFormat) {
  MOZ_DIAGNOSTIC_ASSERT(false, "Unexpected CreateDrawTarget call!");
  return nullptr;
}

void CanvasTranslator::RemoveTexture(int64_t aTextureId,
                                     RemoteTextureTxnType aTxnType,
                                     RemoteTextureTxnId aTxnId) {
  // Don't erase the texture if still in use
  auto result = mTextureInfo.find(aTextureId);
  if (result == mTextureInfo.end()) {
    return;
  }
  auto& info = result->second;
  if (aTxnType && aTxnId) {
    RemoteTextureMap::Get()->WaitForTxn(info.mRemoteTextureOwnerId, mOtherPid,
                                        aTxnType, aTxnId);
  }
  if (--info.mLocked > 0) {
    return;
  }
  if (info.mTextureData) {
    info.mTextureData->Unlock();
  }
  if (mRemoteTextureOwner) {
    // If this texture id was manually registered as a remote texture owner,
    // unregister it so it does not stick around after the texture id goes away.
    RemoteTextureOwnerId owner = info.mRemoteTextureOwnerId;
    if (owner.IsValid()) {
      mRemoteTextureOwner->UnregisterTextureOwner(owner);
    }
  }
  mTextureInfo.erase(result);
}

bool CanvasTranslator::LockTexture(int64_t aTextureId, OpenMode aMode,
                                   bool aInvalidContents) {
  if (aMode == OpenMode::OPEN_NONE) {
    return false;
  }
  auto result = mTextureInfo.find(aTextureId);
  if (result == mTextureInfo.end()) {
    return false;
  }
  auto& info = result->second;
  if (info.mTextureLockMode != OpenMode::OPEN_NONE) {
    return (info.mTextureLockMode & aMode) == aMode;
  }
  if (gfx::DrawTargetWebgl* webgl = info.GetDrawTargetWebgl()) {
    if (aMode & OpenMode::OPEN_WRITE) {
      webgl->BeginFrame(aInvalidContents);
    }
  }
  info.mTextureLockMode = aMode;
  return true;
}

bool CanvasTranslator::UnlockTexture(int64_t aTextureId) {
  auto result = mTextureInfo.find(aTextureId);
  if (result == mTextureInfo.end()) {
    return false;
  }
  auto& info = result->second;
  if (info.mTextureLockMode == OpenMode::OPEN_NONE) {
    return false;
  }

  if (gfx::DrawTargetWebgl* webgl = info.GetDrawTargetWebgl()) {
    if (info.mTextureLockMode & OpenMode::OPEN_WRITE) {
      webgl->EndFrame();
      if (webgl->RequiresRefresh()) {
        NotifyRequiresRefresh(aTextureId);
      }
    }
  }
  info.mTextureLockMode = OpenMode::OPEN_NONE;
  return true;
}

bool CanvasTranslator::PresentTexture(int64_t aTextureId, RemoteTextureId aId) {
  auto result = mTextureInfo.find(aTextureId);
  if (result == mTextureInfo.end()) {
    return false;
  }
  auto& info = result->second;
  RemoteTextureOwnerId ownerId = info.mRemoteTextureOwnerId;
  if (gfx::DrawTargetWebgl* webgl = info.GetDrawTargetWebgl()) {
    EnsureRemoteTextureOwner(ownerId);
    // Check for context loss to avoid CopyToSwapChain becoming a no-op.
    if (webgl->IsValid()) {
      webgl->CopyToSwapChain(aId, ownerId, mRemoteTextureOwner);
      if (webgl->IsValid()) {
        return true;
      }
    }
    // If the context was lost, try to create a fallback to push instead.
    EnsureSharedContextWebgl();
  }
  if (TextureData* data = info.mTextureData.get()) {
    PushRemoteTexture(aTextureId, data, aId, ownerId);
  }
  return true;
}

void CanvasTranslator::EnsureRemoteTextureOwner(RemoteTextureOwnerId aOwnerId) {
  if (!mRemoteTextureOwner) {
    mRemoteTextureOwner = new RemoteTextureOwnerClient(mOtherPid);
  }
  if (aOwnerId.IsValid() && !mRemoteTextureOwner->IsRegistered(aOwnerId)) {
    mRemoteTextureOwner->RegisterTextureOwner(aOwnerId,
                                              /* aSharedRecycling */ true);
  }
}

UniquePtr<TextureData> CanvasTranslator::CreateOrRecycleTextureData(
    const gfx::IntSize& aSize, gfx::SurfaceFormat aFormat) {
  if (mRemoteTextureOwner) {
    if (mTextureType == TextureType::Unknown) {
      return mRemoteTextureOwner->CreateOrRecycleBufferTextureData(aSize,
                                                                   aFormat);
    }
    if (UniquePtr<TextureData> data =
            mRemoteTextureOwner->GetRecycledTextureData(aSize, aFormat,
                                                        mTextureType)) {
      return data;
    }
  }
  return CreateTextureData(aSize, aFormat, false);
}

bool CanvasTranslator::PushRemoteTexture(int64_t aTextureId, TextureData* aData,
                                         RemoteTextureId aId,
                                         RemoteTextureOwnerId aOwnerId) {
  EnsureRemoteTextureOwner(aOwnerId);
  UniquePtr<TextureData> dstData;
  if (!mDeviceResetInProgress) {
    TextureData::Info info;
    aData->FillInfo(info);
    dstData = CreateOrRecycleTextureData(info.size, info.format);
  }
  bool success = false;
  // Source data is already locked.
  if (dstData) {
    if (dstData->Lock(OpenMode::OPEN_WRITE)) {
      if (RefPtr<gfx::DrawTarget> dstDT = dstData->BorrowDrawTarget()) {
        if (RefPtr<gfx::DrawTarget> srcDT = aData->BorrowDrawTarget()) {
          if (RefPtr<gfx::SourceSurface> snapshot = srcDT->Snapshot()) {
            dstDT->CopySurface(snapshot, snapshot->GetRect(),
                               gfx::IntPoint(0, 0));
            dstDT->Flush();
            success = true;
          }
        }
      }
      dstData->Unlock();
    } else {
      gfxCriticalNote << "CanvasTranslator::PushRemoteTexture dst lock failed";
    }
  }
  if (success) {
    mRemoteTextureOwner->PushTexture(aId, aOwnerId, std::move(dstData));
  } else {
    mRemoteTextureOwner->PushDummyTexture(aId, aOwnerId);
  }
  return success;
}

void CanvasTranslator::ClearTextureInfo() {
  for (auto const& entry : mTextureInfo) {
    if (entry.second.mTextureData) {
      entry.second.mTextureData->Unlock();
    }
  }
  mTextureInfo.clear();
  mDrawTargets.Clear();
  mSharedContext = nullptr;
  mBaseDT = nullptr;
  if (mReferenceTextureData) {
    mReferenceTextureData->Unlock();
  }
  if (mRemoteTextureOwner) {
    mRemoteTextureOwner->UnregisterAllTextureOwners();
    mRemoteTextureOwner = nullptr;
  }
}

already_AddRefed<gfx::SourceSurface> CanvasTranslator::LookupExternalSurface(
    uint64_t aKey) {
  return mSharedSurfacesHolder->Get(wr::ToExternalImageId(aKey));
}

void CanvasTranslator::CheckpointReached() { CheckAndSignalWriter(); }

void CanvasTranslator::PauseTranslation() {
  mHeader->readerState = State::Paused;
}

already_AddRefed<gfx::GradientStops> CanvasTranslator::GetOrCreateGradientStops(
    gfx::DrawTarget* aDrawTarget, gfx::GradientStop* aRawStops,
    uint32_t aNumStops, gfx::ExtendMode aExtendMode) {
  MOZ_ASSERT(aDrawTarget);
  nsTArray<gfx::GradientStop> rawStopArray(aRawStops, aNumStops);
  return gfx::gfxGradientCache::GetOrCreateGradientStops(
      aDrawTarget, rawStopArray, aExtendMode);
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
