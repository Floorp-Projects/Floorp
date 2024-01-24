/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasChild.h"

#include "MainThreadUtils.h"
#include "mozilla/gfx/CanvasManagerChild.h"
#include "mozilla/gfx/DrawTargetRecording.h"
#include "mozilla/gfx/Tools.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/ProcessChild.h"
#include "mozilla/layers/CanvasDrawEventRecorder.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/SourceSurfaceSharedData.h"
#include "mozilla/Maybe.h"
#include "nsIObserverService.h"
#include "RecordedCanvasEventImpl.h"

namespace mozilla {
namespace layers {

class RecorderHelpers final : public CanvasDrawEventRecorder::Helpers {
 public:
  NS_DECL_OWNINGTHREAD

  explicit RecorderHelpers(const RefPtr<CanvasChild>& aCanvasChild)
      : mCanvasChild(aCanvasChild) {}

  ~RecorderHelpers() override = default;

  bool InitTranslator(TextureType aTextureType, gfx::BackendType aBackendType,
                      Handle&& aReadHandle, nsTArray<Handle>&& aBufferHandles,
                      uint64_t aBufferSize,
                      CrossProcessSemaphoreHandle&& aReaderSem,
                      CrossProcessSemaphoreHandle&& aWriterSem,
                      bool aUseIPDLThread) override {
    NS_ASSERT_OWNINGTHREAD(RecorderHelpers);
    if (NS_WARN_IF(!mCanvasChild)) {
      return false;
    }
    return mCanvasChild->SendInitTranslator(
        aTextureType, aBackendType, std::move(aReadHandle),
        std::move(aBufferHandles), aBufferSize, std::move(aReaderSem),
        std::move(aWriterSem), aUseIPDLThread);
  }

  bool AddBuffer(Handle&& aBufferHandle, uint64_t aBufferSize) override {
    NS_ASSERT_OWNINGTHREAD(RecorderHelpers);
    if (!mCanvasChild) {
      return false;
    }
    return mCanvasChild->SendAddBuffer(std::move(aBufferHandle), aBufferSize);
  }

  bool ReaderClosed() override {
    NS_ASSERT_OWNINGTHREAD(RecorderHelpers);
    if (!mCanvasChild) {
      return false;
    }
    return !mCanvasChild->CanSend() || ipc::ProcessChild::ExpectingShutdown();
  }

  bool RestartReader() override {
    NS_ASSERT_OWNINGTHREAD(RecorderHelpers);
    if (!mCanvasChild) {
      return false;
    }
    return mCanvasChild->SendRestartTranslation();
  }

 private:
  const WeakPtr<CanvasChild> mCanvasChild;
};

class SourceSurfaceCanvasRecording final : public gfx::SourceSurface {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(SourceSurfaceCanvasRecording, final)

  SourceSurfaceCanvasRecording(
      int64_t aTextureId, const RefPtr<gfx::SourceSurface>& aRecordedSuface,
      CanvasChild* aCanvasChild,
      const RefPtr<CanvasDrawEventRecorder>& aRecorder)
      : mTextureId(aTextureId),
        mRecordedSurface(aRecordedSuface),
        mCanvasChild(aCanvasChild),
        mRecorder(aRecorder) {
    // It's important that AddStoredObject is called first because that will
    // run any pending processing required by recorded objects that have been
    // deleted off the main thread.
    mRecorder->AddStoredObject(this);
    mRecorder->RecordEvent(RecordedAddSurfaceAlias(this, aRecordedSuface));
  }

  ~SourceSurfaceCanvasRecording() {
    ReferencePtr surfaceAlias = this;
    if (NS_IsMainThread()) {
      ReleaseOnMainThread(std::move(mRecorder), surfaceAlias,
                          std::move(mRecordedSurface), std::move(mCanvasChild));
      return;
    }

    mRecorder->AddPendingDeletion(
        [recorder = std::move(mRecorder), surfaceAlias,
         aliasedSurface = std::move(mRecordedSurface),
         canvasChild = std::move(mCanvasChild)]() mutable -> void {
          ReleaseOnMainThread(std::move(recorder), surfaceAlias,
                              std::move(aliasedSurface),
                              std::move(canvasChild));
        });
  }

  gfx::SurfaceType GetType() const final { return mRecordedSurface->GetType(); }

  gfx::IntSize GetSize() const final { return mRecordedSurface->GetSize(); }

  gfx::SurfaceFormat GetFormat() const final {
    return mRecordedSurface->GetFormat();
  }

  already_AddRefed<gfx::DataSourceSurface> GetDataSurface() final {
    EnsureDataSurfaceOnMainThread();
    return do_AddRef(mDataSourceSurface);
  }

  void DrawTargetWillChange() { mDetached = true; }

 private:
  void EnsureDataSurfaceOnMainThread() {
    // The data can only be retrieved on the main thread.
    if (!mDataSourceSurface && NS_IsMainThread()) {
      mDataSourceSurface =
          mCanvasChild->GetDataSurface(mTextureId, mRecordedSurface, mDetached);
    }
  }

  // Used to ensure that clean-up that requires it is done on the main thread.
  static void ReleaseOnMainThread(RefPtr<CanvasDrawEventRecorder> aRecorder,
                                  ReferencePtr aSurfaceAlias,
                                  RefPtr<gfx::SourceSurface> aAliasedSurface,
                                  RefPtr<CanvasChild> aCanvasChild) {
    MOZ_ASSERT(NS_IsMainThread());

    aRecorder->RemoveStoredObject(aSurfaceAlias);
    aRecorder->RecordEvent(RecordedRemoveSurfaceAlias(aSurfaceAlias));
    aAliasedSurface = nullptr;
    aCanvasChild = nullptr;
    aRecorder = nullptr;
  }

  int64_t mTextureId;
  RefPtr<gfx::SourceSurface> mRecordedSurface;
  RefPtr<CanvasChild> mCanvasChild;
  RefPtr<CanvasDrawEventRecorder> mRecorder;
  RefPtr<gfx::DataSourceSurface> mDataSourceSurface;
  bool mDetached = false;
};

CanvasChild::CanvasChild() = default;

CanvasChild::~CanvasChild() = default;

static void NotifyCanvasDeviceReset() {
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(nullptr, "canvas-device-reset", nullptr);
  }
}

ipc::IPCResult CanvasChild::RecvNotifyDeviceChanged() {
  NS_ASSERT_OWNINGTHREAD(CanvasChild);

  NotifyCanvasDeviceReset();
  mRecorder->RecordEvent(RecordedDeviceChangeAcknowledged());
  return IPC_OK();
}

/* static */ bool CanvasChild::mDeactivated = false;

ipc::IPCResult CanvasChild::RecvDeactivate() {
  NS_ASSERT_OWNINGTHREAD(CanvasChild);

  RefPtr<CanvasChild> self(this);
  mDeactivated = true;
  if (auto* cm = gfx::CanvasManagerChild::Get()) {
    cm->DeactivateCanvas();
  }
  NotifyCanvasDeviceReset();
  return IPC_OK();
}

ipc::IPCResult CanvasChild::RecvBlockCanvas() {
  mBlocked = true;
  if (auto* cm = gfx::CanvasManagerChild::Get()) {
    cm->BlockCanvas();
  }
  return IPC_OK();
}

void CanvasChild::EnsureRecorder(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                                 TextureType aTextureType) {
  NS_ASSERT_OWNINGTHREAD(CanvasChild);

  if (!mRecorder) {
    gfx::BackendType backendType =
        gfxPlatform::GetPlatform()->GetPreferredCanvasBackend();
    auto recorder = MakeRefPtr<CanvasDrawEventRecorder>();
    if (!recorder->Init(aTextureType, backendType,
                        MakeUnique<RecorderHelpers>(this))) {
      return;
    }

    mRecorder = recorder.forget();
  }

  MOZ_RELEASE_ASSERT(mRecorder->GetTextureType() == aTextureType,
                     "We only support one remote TextureType currently.");

  EnsureDataSurfaceShmem(aSize, aFormat);
}

void CanvasChild::ActorDestroy(ActorDestroyReason aWhy) {
  NS_ASSERT_OWNINGTHREAD(CanvasChild);

  if (mRecorder) {
    mRecorder->DetachResources();
  }
}

void CanvasChild::Destroy() {
  NS_ASSERT_OWNINGTHREAD(CanvasChild);

  if (CanSend()) {
    Send__delete__(this);
  }
}

bool CanvasChild::EnsureBeginTransaction() {
  NS_ASSERT_OWNINGTHREAD(CanvasChild);

  if (!mIsInTransaction) {
    RecordEvent(RecordedCanvasBeginTransaction());
    mIsInTransaction = true;
  }

  return true;
}

void CanvasChild::EndTransaction() {
  NS_ASSERT_OWNINGTHREAD(CanvasChild);

  if (mIsInTransaction) {
    RecordEvent(RecordedCanvasEndTransaction());
    mIsInTransaction = false;
    mDormant = false;
  } else if (mRecorder) {
    // Schedule to drop free buffers if we have no non-empty transactions.
    if (!mDormant) {
      mDormant = true;
      NS_DelayedDispatchToCurrentThread(
          NewRunnableMethod("CanvasChild::DropFreeBuffersWhenDormant", this,
                            &CanvasChild::DropFreeBuffersWhenDormant),
          StaticPrefs::gfx_canvas_remote_drop_buffer_milliseconds());
    }
  }

  ++mTransactionsSinceGetDataSurface;
}

void CanvasChild::DropFreeBuffersWhenDormant() {
  NS_ASSERT_OWNINGTHREAD(CanvasChild);
  // Drop any free buffers if we have not had any non-empty transactions.
  if (mDormant && mRecorder) {
    mRecorder->DropFreeBuffers();
  }
}

void CanvasChild::ClearCachedResources() {
  NS_ASSERT_OWNINGTHREAD(CanvasChild);
  if (mRecorder) {
    mRecorder->DropFreeBuffers();
    // Notify CanvasTranslator it is about to be minimized.
    SendClearCachedResources();
  }
}

bool CanvasChild::ShouldBeCleanedUp() const {
  NS_ASSERT_OWNINGTHREAD(CanvasChild);

  // Always return true if we've been deactivated.
  if (Deactivated()) {
    return true;
  }

  // We can only be cleaned up if nothing else references our recorder.
  return !mRecorder || mRecorder->hasOneRef();
}

already_AddRefed<gfx::DrawTarget> CanvasChild::CreateDrawTarget(
    int64_t aTextureId, const RemoteTextureOwnerId& aTextureOwnerId,
    gfx::IntSize aSize, gfx::SurfaceFormat aFormat) {
  NS_ASSERT_OWNINGTHREAD(CanvasChild);

  if (!mRecorder) {
    return nullptr;
  }

  RefPtr<gfx::DrawTarget> dummyDt = gfx::Factory::CreateDrawTarget(
      gfx::BackendType::SKIA, gfx::IntSize(1, 1), aFormat);
  RefPtr<gfx::DrawTarget> dt = MakeAndAddRef<gfx::DrawTargetRecording>(
      mRecorder, aTextureId, aTextureOwnerId, dummyDt, aSize);

  mTextureInfo.insert({aTextureId, {}});

  return dt.forget();
}

bool CanvasChild::EnsureDataSurfaceShmem(gfx::IntSize aSize,
                                         gfx::SurfaceFormat aFormat) {
  NS_ASSERT_OWNINGTHREAD(CanvasChild);

  if (!mRecorder) {
    return false;
  }

  size_t sizeRequired =
      ImageDataSerializer::ComputeRGBBufferSize(aSize, aFormat);
  if (!sizeRequired) {
    return false;
  }
  sizeRequired = ipc::SharedMemory::PageAlignedSize(sizeRequired);

  if (!mDataSurfaceShmemAvailable || mDataSurfaceShmem->Size() < sizeRequired) {
    RecordEvent(RecordedPauseTranslation());
    auto dataSurfaceShmem = MakeRefPtr<ipc::SharedMemoryBasic>();
    if (!dataSurfaceShmem->Create(sizeRequired) ||
        !dataSurfaceShmem->Map(sizeRequired)) {
      return false;
    }

    auto shmemHandle = dataSurfaceShmem->TakeHandle();
    if (!shmemHandle) {
      return false;
    }

    if (!SendSetDataSurfaceBuffer(std::move(shmemHandle), sizeRequired)) {
      return false;
    }

    mDataSurfaceShmem = dataSurfaceShmem.forget();
    mDataSurfaceShmemAvailable = true;
  }

  MOZ_ASSERT(mDataSurfaceShmemAvailable);
  return true;
}

void CanvasChild::RecordEvent(const gfx::RecordedEvent& aEvent) {
  NS_ASSERT_OWNINGTHREAD(CanvasChild);

  // We drop mRecorder in ActorDestroy to break the reference cycle.
  if (!mRecorder) {
    return;
  }

  mRecorder->RecordEvent(aEvent);
}

int64_t CanvasChild::CreateCheckpoint() {
  NS_ASSERT_OWNINGTHREAD(CanvasChild);
  return mRecorder->CreateCheckpoint();
}

already_AddRefed<gfx::DataSourceSurface> CanvasChild::GetDataSurface(
    int64_t aTextureId, const gfx::SourceSurface* aSurface, bool aDetached) {
  NS_ASSERT_OWNINGTHREAD(CanvasChild);
  MOZ_ASSERT(aSurface);

  // mTransactionsSinceGetDataSurface is used to determine if we want to prepare
  // a DataSourceSurface in the GPU process up front at the end of the
  // transaction, but that only makes sense if the canvas JS is requesting data
  // in between transactions.
  if (!mIsInTransaction) {
    mTransactionsSinceGetDataSurface = 0;
  }

  if (!EnsureBeginTransaction()) {
    return nullptr;
  }

  // Shmem is only valid if the surface is the latest snapshot (not detached).
  if (!aDetached) {
    // If there is a shmem associated with this snapshot id, then we want to try
    // use that directly without having to allocate a new shmem for retrieval.
    auto it = mTextureInfo.find(aTextureId);
    if (it != mTextureInfo.end() && it->second.mSnapshotShmem) {
      const auto shmemPtr =
          reinterpret_cast<uint8_t*>(it->second.mSnapshotShmem->memory());
      MOZ_ASSERT(shmemPtr);
      mRecorder->RecordEvent(RecordedPrepareShmem(aTextureId));
      auto checkpoint = CreateCheckpoint();
      if (NS_WARN_IF(!mRecorder->WaitForCheckpoint(checkpoint))) {
        return nullptr;
      }
      gfx::IntSize size = aSurface->GetSize();
      gfx::SurfaceFormat format = aSurface->GetFormat();
      auto stride = ImageDataSerializer::ComputeRGBStride(format, size.width);
      RefPtr<gfx::DataSourceSurface> dataSurface =
          gfx::Factory::CreateWrappingDataSourceSurface(shmemPtr, stride, size,
                                                        format);
      return dataSurface.forget();
    }
  }

  RecordEvent(RecordedPrepareDataForSurface(aSurface));

  gfx::IntSize ssSize = aSurface->GetSize();
  gfx::SurfaceFormat ssFormat = aSurface->GetFormat();
  if (!EnsureDataSurfaceShmem(ssSize, ssFormat)) {
    return nullptr;
  }

  RecordEvent(RecordedGetDataForSurface(aSurface));
  auto checkpoint = CreateCheckpoint();
  if (NS_WARN_IF(!mRecorder->WaitForCheckpoint(checkpoint))) {
    return nullptr;
  }

  mDataSurfaceShmemAvailable = false;
  struct DataShmemHolder {
    RefPtr<ipc::SharedMemoryBasic> shmem;
    RefPtr<CanvasChild> canvasChild;
  };

  auto* data = static_cast<uint8_t*>(mDataSurfaceShmem->memory());
  auto* closure = new DataShmemHolder{do_AddRef(mDataSurfaceShmem), this};
  auto stride = ImageDataSerializer::ComputeRGBStride(ssFormat, ssSize.width);

  RefPtr<gfx::DataSourceSurface> dataSurface =
      gfx::Factory::CreateWrappingDataSourceSurface(
          data, stride, ssSize, ssFormat, ReleaseDataShmemHolder, closure);
  return dataSurface.forget();
}

/* static */ void CanvasChild::ReleaseDataShmemHolder(void* aClosure) {
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "CanvasChild::ReleaseDataShmemHolder",
        [aClosure]() { ReleaseDataShmemHolder(aClosure); }));
    return;
  }

  auto* shmemHolder = static_cast<DataShmemHolder*>(aClosure);
  shmemHolder->canvasChild->ReturnDataSurfaceShmem(shmemHolder->shmem.forget());
  delete shmemHolder;
}

already_AddRefed<gfx::SourceSurface> CanvasChild::WrapSurface(
    const RefPtr<gfx::SourceSurface>& aSurface, int64_t aTextureId) {
  NS_ASSERT_OWNINGTHREAD(CanvasChild);

  if (!aSurface) {
    return nullptr;
  }

  return MakeAndAddRef<SourceSurfaceCanvasRecording>(aTextureId, aSurface, this,
                                                     mRecorder);
}

void CanvasChild::ReturnDataSurfaceShmem(
    already_AddRefed<ipc::SharedMemoryBasic> aDataSurfaceShmem) {
  RefPtr<ipc::SharedMemoryBasic> data = aDataSurfaceShmem;
  // We can only reuse the latest data surface shmem.
  if (data == mDataSurfaceShmem) {
    MOZ_ASSERT(!mDataSurfaceShmemAvailable);
    mDataSurfaceShmemAvailable = true;
  }
}

void CanvasChild::DetachSurface(const RefPtr<gfx::SourceSurface>& aSurface) {
  if (auto* surface =
          static_cast<SourceSurfaceCanvasRecording*>(aSurface.get())) {
    surface->DrawTargetWillChange();
  }
}

ipc::IPCResult CanvasChild::RecvNotifyRequiresRefresh(int64_t aTextureId) {
  auto it = mTextureInfo.find(aTextureId);
  if (it != mTextureInfo.end()) {
    it->second.mRequiresRefresh = true;
  }
  return IPC_OK();
}

bool CanvasChild::RequiresRefresh(int64_t aTextureId) const {
  if (mBlocked) {
    return true;
  }
  auto it = mTextureInfo.find(aTextureId);
  if (it != mTextureInfo.end()) {
    return it->second.mRequiresRefresh;
  }
  return false;
}

ipc::IPCResult CanvasChild::RecvSnapshotShmem(
    int64_t aTextureId, Handle&& aShmemHandle, uint32_t aShmemSize,
    SnapshotShmemResolver&& aResolve) {
  auto it = mTextureInfo.find(aTextureId);
  if (it != mTextureInfo.end()) {
    auto shmem = MakeRefPtr<ipc::SharedMemoryBasic>();
    if (NS_WARN_IF(!shmem->SetHandle(std::move(aShmemHandle),
                                     ipc::SharedMemory::RightsReadOnly)) ||
        NS_WARN_IF(!shmem->Map(aShmemSize))) {
      shmem = nullptr;
    } else {
      it->second.mSnapshotShmem = std::move(shmem);
    }
    aResolve(true);
  } else {
    aResolve(false);
  }
  return IPC_OK();
}

void CanvasChild::CleanupTexture(int64_t aTextureId) {
  mTextureInfo.erase(aTextureId);
}

}  // namespace layers
}  // namespace mozilla
