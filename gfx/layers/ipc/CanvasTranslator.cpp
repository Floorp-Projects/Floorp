/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "CanvasTranslator.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/GPUParent.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/SyncRunnable.h"
#include "nsTHashtable.h"
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

typedef nsTHashtable<nsRefPtrHashKey<CanvasTranslator>> CanvasTranslatorSet;

static CanvasTranslatorSet& CanvasTranslators() {
  MOZ_ASSERT(CanvasThreadHolder::IsInCanvasThread());
  static CanvasTranslatorSet* sCanvasTranslator = new CanvasTranslatorSet();
  return *sCanvasTranslator;
}

static void EnsureAllClosed() {
  for (auto iter = CanvasTranslators().Iter(); !iter.Done(); iter.Next()) {
    iter.Get()->GetKey()->Close();
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
    : gfx::InlineTranslator(), mCanvasThreadHolder(aCanvasThreadHolder) {}

CanvasTranslator::~CanvasTranslator() {
  if (mReferenceTextureData) {
    mReferenceTextureData->Unlock();
  }
}

void CanvasTranslator::Bind(Endpoint<PCanvasParent>&& aEndpoint) {
  if (!aEndpoint.Bind(this)) {
    return;
  }

  CanvasTranslators().PutEntry(this);
}

mozilla::ipc::IPCResult CanvasTranslator::RecvInitTranslator(
    const TextureType& aTextureType,
    const ipc::SharedMemoryBasic::Handle& aReadHandle,
    const CrossProcessSemaphoreHandle& aReaderSem,
    const CrossProcessSemaphoreHandle& aWriterSem) {
  mTextureType = aTextureType;
#if defined(XP_WIN)
  if (!CheckForFreshCanvasDevice(__LINE__)) {
    gfxCriticalNote << "GFX: CanvasTranslator failed to get device";
    return IPC_FAIL(this, "Failed to get canvas device.");
  }
#endif

  mStream = MakeUnique<CanvasEventRingBuffer>();
  if (!mStream->InitReader(aReadHandle, aReaderSem, aWriterSem,
                           MakeUnique<RingBufferReaderServices>(this))) {
    return IPC_FAIL(this, "Failed to initialize ring buffer reader.");
  }

  mTranslationTaskQueue = mCanvasThreadHolder->CreateWorkerTaskQueue();
  return RecvResumeTranslation();
}

ipc::IPCResult CanvasTranslator::RecvResumeTranslation() {
  if (!IsValid()) {
    return IPC_FAIL(this, "Canvas Translation failed.");
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
}

void CanvasTranslator::ActorDestroy(ActorDestroyReason why) {
  mTranslationTaskQueue->BeginShutdown()->Then(
      MessageLoop::current()->SerialEventTarget(), __func__, this,
      &CanvasTranslator::FinishShutdown, &CanvasTranslator::FinishShutdown);
}

void CanvasTranslator::FinishShutdown() {
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
  canvasTranslators.RemoveEntry(this);
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

  mIsValid = false;
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
  gfx::AutoSerializeWithMoz2D serializeWithMoz2D(
      GetReferenceDrawTarget()->GetBackendType());
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
  return mDevice && CreateReferenceTexture();
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

void CanvasTranslator::AddSurfaceDescriptor(gfx::ReferencePtr aRefPtr,
                                            TextureData* aTextureData) {
  UniquePtr<SurfaceDescriptor> descriptor = MakeUnique<SurfaceDescriptor>();
  if (!aTextureData->Serialize(*descriptor)) {
    MOZ_CRASH("Failed to serialize");
  }

  MonitorAutoLock lock(mSurfaceDescriptorsMonitor);
  mSurfaceDescriptors[aRefPtr] = std::move(descriptor);
  mSurfaceDescriptorsMonitor.Notify();
}

already_AddRefed<gfx::DrawTarget> CanvasTranslator::CreateDrawTarget(
    gfx::ReferencePtr aRefPtr, const gfx::IntSize& aSize,
    gfx::SurfaceFormat aFormat) {
  RefPtr<gfx::DrawTarget> dt;
  do {
    // It is important that AutoSerializeWithMoz2D is called within the loop
    // and doesn't hold during calls to CheckForFreshCanvasDevice, because that
    // might cause a deadlock with device reset code on the main thread.
    gfx::AutoSerializeWithMoz2D serializeWithMoz2D(
        GetReferenceDrawTarget()->GetBackendType());
    TextureData* textureData = CreateTextureData(mTextureType, aSize, aFormat);
    if (textureData) {
      textureData->Lock(OpenMode::OPEN_READ_WRITE);
      mTextureDatas[aRefPtr] = UniquePtr<TextureData>(textureData);
      AddSurfaceDescriptor(aRefPtr, textureData);
      dt = textureData->BorrowDrawTarget();
    }
  } while (!dt && CheckForFreshCanvasDevice(__LINE__));
  AddDrawTarget(aRefPtr, dt);

  return dt.forget();
}

void CanvasTranslator::RemoveDrawTarget(gfx::ReferencePtr aDrawTarget) {
  InlineTranslator::RemoveDrawTarget(aDrawTarget);
  gfx::AutoSerializeWithMoz2D serializeWithMoz2D(
      GetReferenceDrawTarget()->GetBackendType());
  mTextureDatas.erase(aDrawTarget);

  // It is possible that the texture from the content process has never been
  // forwarded to the GPU process, so we have to make sure it is removed here
  // otherwise if the same pointer gets used for a DrawTarget again in the
  // content process then it could pick up the old (now invalid) descriptor.
  MonitorAutoLock lock(mSurfaceDescriptorsMonitor);
  mSurfaceDescriptors.erase(aDrawTarget);
}

TextureData* CanvasTranslator::LookupTextureData(
    gfx::ReferencePtr aDrawTarget) {
  TextureMap::const_iterator result = mTextureDatas.find(aDrawTarget);
  if (result == mTextureDatas.end()) {
    return nullptr;
  }
  return result->second.get();
}

UniquePtr<SurfaceDescriptor> CanvasTranslator::WaitForSurfaceDescriptor(
    gfx::ReferencePtr aDrawTarget) {
  MonitorAutoLock lock(mSurfaceDescriptorsMonitor);
  DescriptorMap::iterator result;
  while ((result = mSurfaceDescriptors.find(aDrawTarget)) ==
         mSurfaceDescriptors.end()) {
    mSurfaceDescriptorsMonitor.Wait();
  }

  UniquePtr<SurfaceDescriptor> descriptor = std::move(result->second);
  mSurfaceDescriptors.erase(aDrawTarget);
  return descriptor;
}

gfx::DataSourceSurface* CanvasTranslator::LookupDataSurface(
    gfx::ReferencePtr aRefPtr) {
  return mDataSurfaces.GetWeak(aRefPtr);
}

void CanvasTranslator::AddDataSurface(
    gfx::ReferencePtr aRefPtr, RefPtr<gfx::DataSourceSurface>&& aSurface) {
  mDataSurfaces.Put(aRefPtr, std::move(aSurface));
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
