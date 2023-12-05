/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "CanvasTranslator.h"

#include "gfxGradientCache.h"
#include "gfxPlatform.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/CanvasManagerParent.h"
#include "mozilla/gfx/CanvasRenderThread.h"
#include "mozilla/gfx/DrawTargetWebgl.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/GPUParent.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/layers/BufferTexture.h"
#include "mozilla/layers/SharedSurfacesParent.h"
#include "mozilla/layers/TextureClient.h"
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

// When in a transaction we wait for a short time because we're expecting more
// events from the content process. We don't want to wait for too long in case
// other content processes are waiting for events to process.
static const TimeDuration kReadEventTimeout = TimeDuration::FromMilliseconds(5);

class RingBufferReaderServices final
    : public CanvasEventRingBuffer::ReaderServices {
 public:
  explicit RingBufferReaderServices(RefPtr<CanvasTranslator> aCanvasTranslator)
      : mCanvasTranslator(std::move(aCanvasTranslator)) {}

  ~RingBufferReaderServices() final = default;

  bool WriterClosed() final { return !mCanvasTranslator->CanSend(); }

 private:
  RefPtr<CanvasTranslator> mCanvasTranslator;
};

TextureData* CanvasTranslator::CreateTextureData(TextureType aTextureType,
                                                 gfx::BackendType aBackendType,
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
    case TextureType::Unknown:
      textureData = BufferTextureData::Create(
          aSize, aFormat, gfx::BackendType::SKIA, LayersBackend::LAYERS_WR,
          TextureFlags::DEALLOCATE_CLIENT | TextureFlags::REMOTE_TEXTURE,
          ALLOC_CLEAR_BUFFER, nullptr);
      break;

    default:
      textureData = TextureData::Create(aTextureType, aFormat, aSize,
                                        ALLOC_CLEAR_BUFFER, aBackendType);
      break;
  }

  return textureData;
}

CanvasTranslator::CanvasTranslator() {
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

bool CanvasTranslator::EnsureSharedContextWebgl() {
  if (!mSharedContext || mSharedContext->IsContextLost()) {
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
    const TextureType& aTextureType, const gfx::BackendType& aBackendType,
    ipc::SharedMemoryBasic::Handle&& aReadHandle,
    CrossProcessSemaphoreHandle&& aReaderSem,
    CrossProcessSemaphoreHandle&& aWriterSem, const bool& aUseIPDLThread) {
  if (mStream) {
    return IPC_FAIL(this, "RecvInitTranslator called twice.");
  }

  mTextureType = aTextureType;
  mBackendType = aBackendType;

  // We need to initialize the stream first, because it might be used to
  // communicate other failures back to the writer.
  mStream = MakeUnique<CanvasEventRingBuffer>();
  if (!mStream->InitReader(std::move(aReadHandle), std::move(aReaderSem),
                           std::move(aWriterSem),
                           MakeUnique<RingBufferReaderServices>(this))) {
    mStream = nullptr;
    return IPC_FAIL(this, "Failed to initialize ring buffer reader.");
  }

  if (!CheckForFreshCanvasDevice(__LINE__)) {
    gfxCriticalNote << "GFX: CanvasTranslator failed to get device";
    mStream = nullptr;
    return IPC_OK();
  }

  if (gfx::gfxVars::UseAcceleratedCanvas2D() && !EnsureSharedContextWebgl()) {
    gfxCriticalNote
        << "GFX: CanvasTranslator failed creating WebGL shared context";
  }

  if (!aUseIPDLThread) {
    mTranslationTaskQueue = gfx::CanvasRenderThread::CreateWorkerTaskQueue();
  }
  return RecvResumeTranslation();
}

ipc::IPCResult CanvasTranslator::RecvNewBuffer(
    ipc::SharedMemoryBasic::Handle&& aReadHandle) {
  if (!mStream) {
    return IPC_FAIL(this, "RecvNewBuffer before RecvInitTranslator.");
  }
  // We need to set the new buffer on the translation queue to be sure that the
  // drop buffer event has been processed.
  DispatchToTaskQueue(NS_NewRunnableFunction(
      "CanvasTranslator SetNewBuffer",
      [self = RefPtr(this), readHandle = std::move(aReadHandle)]() mutable {
        self->mStream->SetNewBuffer(std::move(readHandle));
      }));
  return RecvResumeTranslation();
}

ipc::IPCResult CanvasTranslator::RecvResumeTranslation() {
  if (!mStream) {
    return IPC_FAIL(this, "RecvResumeTranslation before RecvInitTranslator.");
  }
  if (CheckDeactivated()) {
    // The other side might have sent a resume message before we deactivated.
    return IPC_OK();
  }

  DispatchToTaskQueue(NewRunnableMethod("CanvasTranslator::StartTranslation",
                                        this,
                                        &CanvasTranslator::StartTranslation));
  return IPC_OK();
}

void CanvasTranslator::StartTranslation() {
  MOZ_RELEASE_ASSERT(mStream->IsValid(),
                     "StartTranslation called before buffer has been set.");

  if (!TranslateRecording() && CanSend()) {
    DispatchToTaskQueue(NewRunnableMethod("CanvasTranslator::StartTranslation",
                                          this,
                                          &CanvasTranslator::StartTranslation));
  }

  // If the stream has been marked as bad and the Writer hasn't failed,
  // deactivate remote canvas.
  if (!mStream->good() && !mStream->WriterFailed()) {
    Telemetry::ScalarAdd(
        Telemetry::ScalarID::GFX_CANVAS_REMOTE_DEACTIVATED_BAD_STREAM, 1);
    Deactivate();
  }
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

  // mTranslationTaskQueue has shutdown we can safely drop the ring buffer to
  // break the cycle caused by RingBufferReaderServices.
  mStream = nullptr;

  ClearTextureInfo();

  gfx::CanvasManagerParent::RemoveReplayTextures(this);
}

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

  // We need to tell the other side to deactivate. Make sure the stream is
  // marked as bad so that the writing side won't wait for space to write.
  mStream->SetIsBad();
  gfx::CanvasRenderThread::Dispatch(
      NewRunnableMethod("CanvasTranslator::SendDeactivate", this,
                        &CanvasTranslator::SendDeactivate));

  // Unlock all of our textures.
  for (auto const& entry : mTextureInfo) {
    if (entry.second.mTextureData) {
      entry.second.mTextureData->Unlock();
    }
  }

  // Disable remote canvas for all.
  gfx::CanvasManagerParent::DisableRemoteCanvas();
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

bool CanvasTranslator::TranslateRecording() {
  MOZ_ASSERT(IsInTaskQueue());

  if (!mStream) {
    return false;
  }

  if (mSharedContext && EnsureSharedContextWebgl()) {
    mSharedContext->EnterTlsScope();
  }
  auto exitTlsScope = MakeScopeExit([&] {
    if (mSharedContext) {
      mSharedContext->ExitTlsScope();
    }
  });

  uint8_t eventType = mStream->ReadNextEvent();
  while (mStream->good() && eventType != kDropBufferEventType) {
    bool success = RecordedEvent::DoWithEventFromStream(
        *mStream, static_cast<RecordedEvent::EventType>(eventType),
        [&](RecordedEvent* recordedEvent) -> bool {
          // Make sure that the whole event was read from the stream.
          if (!mStream->good()) {
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
    if (!mStream->good()) {
      return true;
    }

    if (!success && !HandleExtensionEvent(eventType)) {
      if (mDeviceResetInProgress) {
        // We've notified the recorder of a device change, so we are expecting
        // failures. Log as a warning to prevent crash reporting being flooded.
        gfxWarning() << "Failed to play canvas event type: " << eventType;
      } else {
        gfxCriticalNote << "Failed to play canvas event type: " << eventType;
      }
      if (!mStream->good()) {
        return true;
      }
    }

    if (!mIsInTransaction) {
      return mStream->StopIfEmpty();
    }

    if (!mStream->HasDataToRead()) {
      // We're going to wait for the next event, so take the opportunity to
      // flush the rendering.
      Flush();
      if (!mStream->WaitForDataToRead(kReadEventTimeout, 0)) {
        return true;
      }
    }

    eventType = mStream->ReadNextEvent();
  }

  return true;
}

#define READ_AND_PLAY_CANVAS_EVENT_TYPE(_typeenum, _class)             \
  case _typeenum: {                                                    \
    auto e = _class(*mStream);                                         \
    if (!mStream->good()) {                                            \
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

  mReferenceTextureData.reset(CreateTextureData(mTextureType, mBackendType,
                                                gfx::IntSize(1, 1),
                                                gfx::SurfaceFormat::B8G8R8A8));
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
  mDeviceResetInProgress = true;
  gfx::CanvasRenderThread::Dispatch(
      NewRunnableMethod("CanvasTranslator::SendNotifyDeviceChanged", this,
                        &CanvasTranslator::SendNotifyDeviceChanged));
}

gfx::DrawTargetWebgl* CanvasTranslator::GetDrawTargetWebgl(
    int64_t aTextureId) const {
  auto result = mTextureInfo.find(aTextureId);
  if (result != mTextureInfo.end() && result->second.mDrawTarget &&
      result->second.mDrawTarget->GetBackendType() == gfx::BackendType::WEBGL) {
    return static_cast<gfx::DrawTargetWebgl*>(result->second.mDrawTarget.get());
  }
  return nullptr;
}

void CanvasTranslator::NotifyRequiresRefresh(int64_t aTextureId,
                                             bool aDispatch) {
  if (aDispatch) {
    DispatchToTaskQueue(NewRunnableMethod<int64_t, bool>(
        "CanvasTranslator::NotifyRequiresRefresh", this,
        &CanvasTranslator::NotifyRequiresRefresh, aTextureId, false));
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
    if (Maybe<Shmem> shmem = webgl->GetShmem()) {
      // Lock the DT so that it doesn't get removed while shmem is in transit.
      mTextureInfo[aTextureId].mLocked++;
      nsCOMPtr<nsIThread> thread =
          gfx::CanvasRenderThread::GetCanvasRenderThread();
      RefPtr<CanvasTranslator> translator = this;
      SendSnapshotShmem(aTextureId, std::move(*shmem))
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
  if (gfx::DrawTargetWebgl* webgl = GetDrawTargetWebgl(aTextureId)) {
    webgl->PrepareData();
  }
}

already_AddRefed<gfx::DrawTarget> CanvasTranslator::CreateDrawTarget(
    gfx::ReferencePtr aRefPtr, const gfx::IntSize& aSize,
    gfx::SurfaceFormat aFormat) {
  MOZ_DIAGNOSTIC_ASSERT(mNextTextureId >= 0, "No texture ID set");
  RefPtr<gfx::DrawTarget> dt;
  if (mNextRemoteTextureOwnerId.IsValid()) {
    if (EnsureSharedContextWebgl()) {
      mSharedContext->EnterTlsScope();
    }
    if (RefPtr<gfx::DrawTargetWebgl> webgl = gfx::DrawTargetWebgl::Create(
            aSize, aFormat, this, mSharedContext)) {
      webgl->BeginFrame(gfx::IntRect());
      dt = webgl.forget().downcast<gfx::DrawTarget>();
      if (dt) {
        TextureInfo& info = mTextureInfo[mNextTextureId];
        info.mDrawTarget = dt;
        info.mRemoteTextureOwnerId = mNextRemoteTextureOwnerId;
        CacheSnapshotShmem(mNextTextureId);
      }
    }
    if (!dt) {
      NotifyRequiresRefresh(mNextTextureId);
    }
  }

  if (!dt) {
    do {
      TextureData* textureData =
          CreateTextureData(mTextureType, mBackendType, aSize, aFormat);
      if (textureData) {
        TextureInfo& info = mTextureInfo[mNextTextureId];
        info.mTextureData = UniquePtr<TextureData>(textureData);
        info.mRemoteTextureOwnerId = mNextRemoteTextureOwnerId;
        if (textureData->Lock(OpenMode::OPEN_READ_WRITE)) {
          dt = textureData->BorrowDrawTarget();
        }
      }
    } while (!dt && CheckForFreshCanvasDevice(__LINE__));
  }
  AddDrawTarget(aRefPtr, dt);
  mNextTextureId = -1;
  mNextRemoteTextureOwnerId = RemoteTextureOwnerId();

  return dt.forget();
}

void CanvasTranslator::RemoveTexture(int64_t aTextureId) {
  {
    // Don't erase the texture if still in use
    auto result = mTextureInfo.find(aTextureId);
    if (result == mTextureInfo.end() || --result->second.mLocked > 0) {
      return;
    }
    mTextureInfo.erase(result);
  }

  // It is possible that the texture from the content process has never been
  // forwarded from the GPU process, so make sure its descriptor is removed.
  gfx::CanvasManagerParent::RemoveReplayTexture(this, aTextureId);
}

TextureData* CanvasTranslator::LookupTextureData(int64_t aTextureId) {
  auto result = mTextureInfo.find(aTextureId);
  if (result == mTextureInfo.end()) {
    return nullptr;
  }
  return result->second.mTextureData.get();
}

bool CanvasTranslator::LockTexture(int64_t aTextureId, OpenMode aMode,
                                   RemoteTextureId aId) {
  auto result = mTextureInfo.find(aTextureId);
  if (result == mTextureInfo.end()) {
    return false;
  }
  if (result->second.mDrawTarget &&
      result->second.mDrawTarget->GetBackendType() == gfx::BackendType::WEBGL) {
    gfx::DrawTargetWebgl* webgl =
        static_cast<gfx::DrawTargetWebgl*>(result->second.mDrawTarget.get());
    webgl->BeginFrame(webgl->GetRect());
  } else if (TextureData* data = result->second.mTextureData.get()) {
    if (!data->Lock(aMode)) {
      return false;
    }
  }
  return true;
}

bool CanvasTranslator::UnlockTexture(int64_t aTextureId, RemoteTextureId aId) {
  auto result = mTextureInfo.find(aTextureId);
  if (result == mTextureInfo.end()) {
    return false;
  }
  RemoteTextureOwnerId ownerId = result->second.mRemoteTextureOwnerId;
  if (result->second.mDrawTarget &&
      result->second.mDrawTarget->GetBackendType() == gfx::BackendType::WEBGL) {
    gfx::DrawTargetWebgl* webgl =
        static_cast<gfx::DrawTargetWebgl*>(result->second.mDrawTarget.get());
    webgl->EndFrame();
    webgl->CopyToSwapChain(aId, ownerId, OtherPid());
    if (!result->second.mNotifiedRequiresRefresh && webgl->RequiresRefresh()) {
      result->second.mNotifiedRequiresRefresh = true;
      NotifyRequiresRefresh(aTextureId);
    }
  } else if (TextureData* data = result->second.mTextureData.get()) {
    if (aId.IsValid()) {
      PushRemoteTexture(data, aId, ownerId);
      data->Unlock();
    } else {
      data->Unlock();
      gfx::CanvasManagerParent::AddReplayTexture(this, aTextureId, data);
    }
  }
  return true;
}

bool CanvasTranslator::PushRemoteTexture(TextureData* aData,
                                         RemoteTextureId aId,
                                         RemoteTextureOwnerId aOwnerId) {
  if (!mRemoteTextureOwner) {
    mRemoteTextureOwner = new RemoteTextureOwnerClient(OtherPid());
  }
  if (!mRemoteTextureOwner->IsRegistered(aOwnerId)) {
    mRemoteTextureOwner->RegisterTextureOwner(
        aOwnerId,
        /* aIsSyncMode */ gfx::gfxVars::WebglOopAsyncPresentForceSync());
  }
  TextureData::Info info;
  aData->FillInfo(info);
  UniquePtr<TextureData> dstData;
  if (mTextureType == TextureType::Unknown) {
    dstData = mRemoteTextureOwner->CreateOrRecycleBufferTextureData(
        aOwnerId, info.size, info.format);
  } else {
    dstData.reset(
        CreateTextureData(mTextureType, mBackendType, info.size, info.format));
  }
  bool success = false;
  // Source data is already locked.
  if (dstData && dstData->Lock(OpenMode::OPEN_WRITE)) {
    if (RefPtr<gfx::DrawTarget> dstDT = dstData->BorrowDrawTarget()) {
      if (RefPtr<gfx::DrawTarget> srcDT = aData->BorrowDrawTarget()) {
        if (RefPtr<gfx::SourceSurface> snapshot = srcDT->Snapshot()) {
          dstDT->CopySurface(snapshot, snapshot->GetRect(),
                             gfx::IntPoint(0, 0));
          success = true;
        }
      }
    }
    dstData->Unlock();
  }
  if (success) {
    mRemoteTextureOwner->PushTexture(aId, aOwnerId, std::move(dstData));
  } else {
    mRemoteTextureOwner->PushDummyTexture(aId, aOwnerId);
  }
  return success;
}

void CanvasTranslator::ClearTextureInfo() {
  mTextureInfo.clear();
  mDrawTargets.Clear();
  mSharedContext = nullptr;
  mBaseDT = nullptr;
  if (mRemoteTextureOwner) {
    mRemoteTextureOwner->UnregisterAllTextureOwners();
    mRemoteTextureOwner = nullptr;
  }
}

already_AddRefed<gfx::SourceSurface> CanvasTranslator::LookupExternalSurface(
    uint64_t aKey) {
  return SharedSurfacesParent::Get(wr::ToExternalImageId(aKey), true);
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
