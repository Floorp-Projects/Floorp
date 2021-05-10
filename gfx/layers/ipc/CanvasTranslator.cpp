/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "CanvasTranslator.h"

#include "gfxGradientCache.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/GPUParent.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/Telemetry.h"
#include "nsTHashSet.h"
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

static const TimeDuration kDescriptorTimeout =
    TimeDuration::FromMilliseconds(10000);

class RingBufferReaderServices final
    : public CanvasEventRingBuffer::ReaderServices {
 public:
  explicit RingBufferReaderServices(RefPtr<CanvasTranslator> aCanvasTranslator)
      : mCanvasTranslator(std::move(aCanvasTranslator)) {}

  ~RingBufferReaderServices() final = default;

  bool WriterClosed() final {
    return !mCanvasTranslator->GetIPCChannel()->CanSend();
  }

 private:
  RefPtr<CanvasTranslator> mCanvasTranslator;
};

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

typedef nsTHashSet<RefPtr<CanvasTranslator>> CanvasTranslatorSet;

static CanvasTranslatorSet& CanvasTranslators() {
  MOZ_ASSERT(CanvasThreadHolder::IsInCanvasThread());
  static CanvasTranslatorSet* sCanvasTranslator = new CanvasTranslatorSet();
  return *sCanvasTranslator;
}

static void EnsureAllClosed() {
  for (const auto& key : CanvasTranslators()) {
    key->Close();
  }
}

/* static */ void CanvasTranslator::Shutdown() {
  // If the dispatch fails there is no canvas thread and so no translators.
  CanvasThreadHolder::MaybeDispatchToCanvasThread(NewRunnableFunction(
      "CanvasTranslator::EnsureAllClosed", &EnsureAllClosed));
}

/* static */ already_AddRefed<CanvasTranslator> CanvasTranslator::Create(
    ipc::Endpoint<PCanvasParent>&& aEndpoint) {
  MOZ_ASSERT(NS_IsInCompositorThread());

  RefPtr<CanvasThreadHolder> threadHolder =
      CanvasThreadHolder::EnsureCanvasThread();
  RefPtr<CanvasTranslator> canvasTranslator =
      new CanvasTranslator(do_AddRef(threadHolder));
  threadHolder->DispatchToCanvasThread(
      NewRunnableMethod<Endpoint<PCanvasParent>&&>(
          "CanvasTranslator::Bind", canvasTranslator, &CanvasTranslator::Bind,
          std::move(aEndpoint)));
  return canvasTranslator.forget();
}

CanvasTranslator::CanvasTranslator(
    already_AddRefed<CanvasThreadHolder> aCanvasThreadHolder)
    : gfx::InlineTranslator(), mCanvasThreadHolder(aCanvasThreadHolder) {
  // Track when remote canvas has been activated.
  Telemetry::ScalarAdd(Telemetry::ScalarID::GFX_CANVAS_REMOTE_ACTIVATED, 1);
}

CanvasTranslator::~CanvasTranslator() {
  // The textures need to be the last thing holding their DrawTargets, so that
  // they can destroy them within a lock.
  mDrawTargets.Clear();
  mBaseDT = nullptr;
}

void CanvasTranslator::Bind(Endpoint<PCanvasParent>&& aEndpoint) {
  if (!aEndpoint.Bind(this)) {
    return;
  }

  CanvasTranslators().Insert(this);
}

mozilla::ipc::IPCResult CanvasTranslator::RecvInitTranslator(
    const TextureType& aTextureType,
    const ipc::SharedMemoryBasic::Handle& aReadHandle,
    const CrossProcessSemaphoreHandle& aReaderSem,
    const CrossProcessSemaphoreHandle& aWriterSem) {
  mTextureType = aTextureType;

  // We need to initialize the stream first, because it might be used to
  // communicate other failures back to the writer.
  mStream = MakeUnique<CanvasEventRingBuffer>();
  if (!mStream->InitReader(aReadHandle, aReaderSem, aWriterSem,
                           MakeUnique<RingBufferReaderServices>(this))) {
    return IPC_FAIL(this, "Failed to initialize ring buffer reader.");
  }

#if defined(XP_WIN)
  if (!CheckForFreshCanvasDevice(__LINE__)) {
    gfxCriticalNote << "GFX: CanvasTranslator failed to get device";
    return IPC_OK();
  }
#endif

  mTranslationTaskQueue = mCanvasThreadHolder->CreateWorkerTaskQueue();
  return RecvResumeTranslation();
}

ipc::IPCResult CanvasTranslator::RecvResumeTranslation() {
  if (mDeactivated) {
    // The other side might have sent a resume message before we deactivated.
    return IPC_OK();
  }

  MOZ_ALWAYS_SUCCEEDS(mTranslationTaskQueue->Dispatch(
      NewRunnableMethod("CanvasTranslator::StartTranslation", this,
                        &CanvasTranslator::StartTranslation)));

  return IPC_OK();
}

void CanvasTranslator::StartTranslation() {
  if (!TranslateRecording() && GetIPCChannel()->CanSend()) {
    MOZ_ALWAYS_SUCCEEDS(mTranslationTaskQueue->Dispatch(
        NewRunnableMethod("CanvasTranslator::StartTranslation", this,
                          &CanvasTranslator::StartTranslation)));
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
  MOZ_ASSERT(CanvasThreadHolder::IsInCanvasThread());

  if (!mTranslationTaskQueue) {
    return FinishShutdown();
  }

  mTranslationTaskQueue->BeginShutdown()->Then(
      GetCurrentSerialEventTarget(), __func__, this,
      &CanvasTranslator::FinishShutdown, &CanvasTranslator::FinishShutdown);
}

void CanvasTranslator::FinishShutdown() {
  MOZ_ASSERT(CanvasThreadHolder::IsInCanvasThread());

  // mTranslationTaskQueue has shutdown we can safely drop the ring buffer to
  // break the cycle caused by RingBufferReaderServices.
  mStream = nullptr;

  // CanvasTranslators has a MOZ_ASSERT(CanvasThreadHolder::IsInCanvasThread())
  // to ensure it is only called on the Canvas Thread. This takes a lock on
  // CanvasThreadHolder::sCanvasThreadHolder, which is also locked in
  // CanvasThreadHolder::StaticRelease on the compositor thread from
  // ReleaseOnCompositorThread below. If that lock wins the race with the one in
  // IsInCanvasThread and it is the last CanvasThreadHolder reference then it
  // shuts down the canvas thread waiting for it to finish. However
  // IsInCanvasThread is waiting for the lock on the canvas thread and we
  // deadlock. So, we need to call CanvasTranslators before
  // ReleaseOnCompositorThread.
  CanvasTranslatorSet& canvasTranslators = CanvasTranslators();
  CanvasThreadHolder::ReleaseOnCompositorThread(mCanvasThreadHolder.forget());
  canvasTranslators.Remove(this);
}

void CanvasTranslator::Deactivate() {
  if (mDeactivated) {
    return;
  }
  mDeactivated = true;

  // We need to tell the other side to deactivate. Make sure the stream is
  // marked as bad so that the writing side won't wait for space to write.
  mStream->SetIsBad();
  mCanvasThreadHolder->DispatchToCanvasThread(
      NewRunnableMethod("CanvasTranslator::SendDeactivate", this,
                        &CanvasTranslator::SendDeactivate));

  // Unlock all of our textures.
  for (auto const& entry : mTextureDatas) {
    entry.second->Unlock();
  }

  // Also notify anyone waiting for a surface descriptor. This must be done
  // after mDeactivated is set to true.
  mSurfaceDescriptorsMonitor.NotifyAll();
}

bool CanvasTranslator::TranslateRecording() {
  MOZ_ASSERT(CanvasThreadHolder::IsInCanvasWorker());

  int32_t eventType = mStream->ReadNextEvent();
  while (mStream->good()) {
    bool success = RecordedEvent::DoWithEventFromStream(
        *mStream, static_cast<RecordedEvent::EventType>(eventType),
        [&](RecordedEvent* recordedEvent) -> bool {
          // Make sure that the whole event was read from the stream.
          if (!mStream->good()) {
            if (!GetIPCChannel()->CanSend()) {
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
      if (!GetIPCChannel()->CanSend()) {                               \
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
  SyncRunnable::DispatchToThread(GetMainThreadEventTarget(), runnable,
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
  mCanvasThreadHolder->DispatchToCanvasThread(
      NewRunnableMethod("CanvasTranslator::SendNotifyDeviceChanged", this,
                        &CanvasTranslator::SendNotifyDeviceChanged));
}

void CanvasTranslator::AddSurfaceDescriptor(int64_t aTextureId,
                                            TextureData* aTextureData) {
  UniquePtr<SurfaceDescriptor> descriptor = MakeUnique<SurfaceDescriptor>();
  if (!aTextureData->Serialize(*descriptor)) {
    MOZ_CRASH("Failed to serialize");
  }

  MonitorAutoLock lock(mSurfaceDescriptorsMonitor);
  mSurfaceDescriptors[aTextureId] = std::move(descriptor);
  mSurfaceDescriptorsMonitor.Notify();
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
      AddSurfaceDescriptor(mNextTextureId, textureData);
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
  // forwarded to the GPU process, so make sure its descriptor is removed.
  MonitorAutoLock lock(mSurfaceDescriptorsMonitor);
  mSurfaceDescriptors.erase(aTextureId);
}

TextureData* CanvasTranslator::LookupTextureData(int64_t aTextureId) {
  TextureMap::const_iterator result = mTextureDatas.find(aTextureId);
  if (result == mTextureDatas.end()) {
    return nullptr;
  }
  return result->second.get();
}

UniquePtr<SurfaceDescriptor> CanvasTranslator::WaitForSurfaceDescriptor(
    int64_t aTextureId) {
  MonitorAutoLock lock(mSurfaceDescriptorsMonitor);
  DescriptorMap::iterator result;
  while ((result = mSurfaceDescriptors.find(aTextureId)) ==
         mSurfaceDescriptors.end()) {
    // If remote canvas has been deactivated just return null.
    if (mDeactivated) {
      return nullptr;
    }

    CVStatus status = mSurfaceDescriptorsMonitor.Wait(kDescriptorTimeout);
    if (status == CVStatus::Timeout) {
      // If something has gone wrong and the texture has already been destroyed,
      // it will have cleaned up its descriptor.
      return nullptr;
    }
  }

  UniquePtr<SurfaceDescriptor> descriptor = std::move(result->second);
  mSurfaceDescriptors.erase(aTextureId);
  return descriptor;
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
