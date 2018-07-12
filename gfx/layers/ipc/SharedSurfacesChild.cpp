/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurfacesChild.h"
#include "SharedSurfacesParent.h"
#include "CompositorManagerChild.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/IpcResourceUpdateQueue.h"
#include "mozilla/layers/SourceSurfaceSharedData.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/SystemGroup.h"        // for SystemGroup

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

class SharedSurfacesChild::ImageKeyData final
{
public:
  ImageKeyData(WebRenderLayerManager* aManager,
               const wr::ImageKey& aImageKey)
    : mManager(aManager)
    , mImageKey(aImageKey)
  { }

  ImageKeyData(ImageKeyData&& aOther)
    : mManager(std::move(aOther.mManager))
    , mImageKey(aOther.mImageKey)
  { }

  ImageKeyData& operator=(ImageKeyData&& aOther)
  {
    mManager = std::move(aOther.mManager);
    mImageKey = aOther.mImageKey;
    return *this;
  }

  ImageKeyData(const ImageKeyData&) = delete;
  ImageKeyData& operator=(const ImageKeyData&) = delete;

  RefPtr<WebRenderLayerManager> mManager;
  wr::ImageKey mImageKey;
};

class SharedSurfacesChild::SharedUserData final
{
public:
  explicit SharedUserData(const wr::ExternalImageId& aId)
    : mId(aId)
    , mShared(false)
  { }

  ~SharedUserData()
  {
    if (mShared) {
      mShared = false;
      if (NS_IsMainThread()) {
        SharedSurfacesChild::Unshare(mId, mKeys);
      } else {
        class DestroyRunnable final : public Runnable
        {
        public:
          DestroyRunnable(const wr::ExternalImageId& aId,
                          nsTArray<ImageKeyData>&& aKeys)
            : Runnable("SharedSurfacesChild::SharedUserData::DestroyRunnable")
            , mId(aId)
            , mKeys(std::move(aKeys))
          { }

          NS_IMETHOD Run() override
          {
            SharedSurfacesChild::Unshare(mId, mKeys);
            return NS_OK;
          }

        private:
          wr::ExternalImageId mId;
          AutoTArray<ImageKeyData, 1> mKeys;
        };

        nsCOMPtr<nsIRunnable> task = new DestroyRunnable(mId, std::move(mKeys));
        SystemGroup::Dispatch(TaskCategory::Other, task.forget());
      }
    }
  }

  const wr::ExternalImageId& Id() const
  {
    return mId;
  }

  void SetId(const wr::ExternalImageId& aId)
  {
    mId = aId;
    mKeys.Clear();
    mShared = false;
  }

  bool IsShared() const
  {
    return mShared;
  }

  void MarkShared()
  {
    MOZ_ASSERT(!mShared);
    mShared = true;
  }

  wr::ImageKey UpdateKey(WebRenderLayerManager* aManager,
                         wr::IpcResourceUpdateQueue& aResources,
                         const Maybe<IntRect>& aDirtyRect)
  {
    MOZ_ASSERT(aManager);
    MOZ_ASSERT(!aManager->IsDestroyed());

    // We iterate through all of the items to ensure we clean up the old
    // WebRenderLayerManager references. Most of the time there will be few
    // entries and this should not be particularly expensive compared to the
    // cost of duplicating image keys. In an ideal world, we would generate a
    // single key for the surface, and it would be usable on all of the
    // renderer instances. For now, we must allocate a key for each WR bridge.
    wr::ImageKey key;
    bool found = false;
    auto i = mKeys.Length();
    while (i > 0) {
      --i;
      ImageKeyData& entry = mKeys[i];
      if (entry.mManager->IsDestroyed()) {
        mKeys.RemoveElementAt(i);
      } else if (entry.mManager == aManager) {
        WebRenderBridgeChild* wrBridge = aManager->WrBridge();
        MOZ_ASSERT(wrBridge);

        // Even if the manager is the same, its underlying WebRenderBridgeChild
        // can change state. If our namespace differs, then our old key has
        // already been discarded.
        bool ownsKey = wrBridge->GetNamespace() == entry.mImageKey.mNamespace;
        if (!ownsKey) {
          entry.mImageKey = wrBridge->GetNextImageKey();
          aResources.AddExternalImage(mId, entry.mImageKey);
        } else if (aDirtyRect) {
          aResources.UpdateExternalImage(mId, entry.mImageKey,
                                         ViewAs<ImagePixel>(aDirtyRect.ref()));
        }

        key = entry.mImageKey;
        found = true;
      }
    }

    if (!found) {
      key = aManager->WrBridge()->GetNextImageKey();
      ImageKeyData data(aManager, key);
      mKeys.AppendElement(std::move(data));
      aResources.AddExternalImage(mId, key);
    }

    return key;
  }

private:
  AutoTArray<ImageKeyData, 1> mKeys;
  wr::ExternalImageId mId;
  bool mShared : 1;
};

/* static */ void
SharedSurfacesChild::DestroySharedUserData(void* aClosure)
{
  MOZ_ASSERT(aClosure);
  auto data = static_cast<SharedUserData*>(aClosure);
  delete data;
}

/* static */ nsresult
SharedSurfacesChild::ShareInternal(SourceSurfaceSharedData* aSurface,
                                   SharedUserData** aUserData)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aSurface);
  MOZ_ASSERT(aUserData);

  CompositorManagerChild* manager = CompositorManagerChild::GetInstance();
  if (NS_WARN_IF(!manager || !manager->CanSend() || !gfxVars::UseWebRender())) {
    // We cannot try to share the surface, most likely because the GPU process
    // crashed. Ideally, we would retry when it is ready, but the handles may be
    // a scarce resource, which can cause much more serious problems if we run
    // out. Better to copy into a fresh buffer later.
    aSurface->FinishedSharing();
    return NS_ERROR_NOT_INITIALIZED;
  }

  static UserDataKey sSharedKey;
  SharedUserData* data =
    static_cast<SharedUserData*>(aSurface->GetUserData(&sSharedKey));
  if (!data) {
    data = new SharedUserData(manager->GetNextExternalImageId());
    aSurface->AddUserData(&sSharedKey, data, DestroySharedUserData);
  } else if (!manager->OwnsExternalImageId(data->Id())) {
    // If the id isn't owned by us, that means the bridge was reinitialized, due
    // to the GPU process crashing. All previous mappings have been released.
    data->SetId(manager->GetNextExternalImageId());
  } else if (data->IsShared()) {
    // It has already been shared with the GPU process.
    *aUserData = data;
    return NS_OK;
  }

  // Ensure that the handle doesn't get released until after we have finished
  // sending the buffer to the GPU process and/or reallocating it.
  // FinishedSharing is not a sufficient condition because another thread may
  // decide we are done while we are in the processing of sharing our newly
  // reallocated handle. Once it goes out of scope, it may release the handle.
  SourceSurfaceSharedData::HandleLock lock(aSurface);

  // If we live in the same process, then it is a simple matter of directly
  // asking the parent instance to store a pointer to the same data, no need
  // to map the data into our memory space twice.
  auto pid = manager->OtherPid();
  if (pid == base::GetCurrentProcId()) {
    SharedSurfacesParent::AddSameProcess(data->Id(), aSurface);
    data->MarkShared();
    *aUserData = data;
    return NS_OK;
  }

  // Attempt to share a handle with the GPU process. The handle may or may not
  // be available -- it will only be available if it is either not yet finalized
  // and/or if it has been finalized but never used for drawing in process.
  ipc::SharedMemoryBasic::Handle handle = ipc::SharedMemoryBasic::NULLHandle();
  nsresult rv = aSurface->ShareToProcess(pid, handle);
  if (rv == NS_ERROR_NOT_AVAILABLE) {
    // It is at least as expensive to copy the image to the GPU process if we
    // have already closed the handle necessary to share, but if we reallocate
    // the shared buffer to get a new handle, we can save some memory.
    if (NS_WARN_IF(!aSurface->ReallocHandle())) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // Reattempt the sharing of the handle to the GPU process.
    rv = aSurface->ShareToProcess(pid, handle);
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_ASSERT(rv != NS_ERROR_NOT_AVAILABLE);
    return rv;
  }

  SurfaceFormat format = aSurface->GetFormat();
  MOZ_RELEASE_ASSERT(format == SurfaceFormat::B8G8R8X8 ||
                     format == SurfaceFormat::B8G8R8A8, "bad format");

  data->MarkShared();
  manager->SendAddSharedSurface(data->Id(),
                                SurfaceDescriptorShared(aSurface->GetSize(),
                                                        aSurface->Stride(),
                                                        format, handle));
  *aUserData = data;
  return NS_OK;
}

/* static */ void
SharedSurfacesChild::Share(SourceSurfaceSharedData* aSurface)
{
  MOZ_ASSERT(aSurface);

  // The IPDL actor to do sharing can only be accessed on the main thread so we
  // need to dispatch if off the main thread. However there is no real danger if
  // we end up racing because if it is already shared, this method will do
  // nothing.
  if (!NS_IsMainThread()) {
    class ShareRunnable final : public Runnable
    {
    public:
      explicit ShareRunnable(SourceSurfaceSharedData* aSurface)
        : Runnable("SharedSurfacesChild::Share")
        , mSurface(aSurface)
      { }

      NS_IMETHOD Run() override
      {
        SharedUserData* unused = nullptr;
        SharedSurfacesChild::ShareInternal(mSurface, &unused);
        return NS_OK;
      }

    private:
      RefPtr<SourceSurfaceSharedData> mSurface;
    };

    SystemGroup::Dispatch(TaskCategory::Other,
                          MakeAndAddRef<ShareRunnable>(aSurface));
    return;
  }

  SharedUserData* unused = nullptr;
  SharedSurfacesChild::ShareInternal(aSurface, &unused);
}

/* static */ nsresult
SharedSurfacesChild::Share(SourceSurfaceSharedData* aSurface,
                           WebRenderLayerManager* aManager,
                           wr::IpcResourceUpdateQueue& aResources,
                           wr::ImageKey& aKey)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aSurface);
  MOZ_ASSERT(aManager);

  // Each time the surface changes, the producers of SourceSurfaceSharedData
  // surfaces promise to increment the invalidation counter each time the
  // surface has changed. We can use this counter to determine whether or not
  // we should upate our paired ImageKey.
  Maybe<IntRect> dirtyRect = aSurface->TakeDirtyRect();
  SharedUserData* data = nullptr;
  nsresult rv = SharedSurfacesChild::ShareInternal(aSurface, &data);
  if (NS_SUCCEEDED(rv)) {
    MOZ_ASSERT(data);
    aKey = data->UpdateKey(aManager, aResources, dirtyRect);
  }

  return rv;
}

/* static */ nsresult
SharedSurfacesChild::Share(ImageContainer* aContainer,
                           WebRenderLayerManager* aManager,
                           wr::IpcResourceUpdateQueue& aResources,
                           wr::ImageKey& aKey)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aContainer);
  MOZ_ASSERT(aManager);

  if (aContainer->IsAsync()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  AutoTArray<ImageContainer::OwningImage,4> images;
  aContainer->GetCurrentImages(&images);
  if (images.IsEmpty()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  RefPtr<gfx::SourceSurface> surface = images[0].mImage->GetAsSourceSurface();
  if (!surface) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (surface->GetType() != SurfaceType::DATA_SHARED) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  auto sharedSurface = static_cast<SourceSurfaceSharedData*>(surface.get());
  return Share(sharedSurface, aManager, aResources, aKey);
}

/* static */ nsresult
SharedSurfacesChild::Share(SourceSurface* aSurface,
                           wr::ExternalImageId& aId)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aSurface);

  if (aSurface->GetType() != SurfaceType::DATA_SHARED) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // The external image ID does not change with the invalidation counter. The
  // caller of this should be aware of the invalidations of the surface through
  // another mechanism (e.g. imgRequestProxy listener notifications).
  auto sharedSurface = static_cast<SourceSurfaceSharedData*>(aSurface);
  SharedUserData* data = nullptr;
  nsresult rv = ShareInternal(sharedSurface, &data);
  if (NS_SUCCEEDED(rv)) {
    MOZ_ASSERT(data);
    aId = data->Id();
  }

  return rv;
}

/* static */ void
SharedSurfacesChild::Unshare(const wr::ExternalImageId& aId,
                             nsTArray<ImageKeyData>& aKeys)
{
  MOZ_ASSERT(NS_IsMainThread());

  for (const auto& entry : aKeys) {
    if (entry.mManager->IsDestroyed()) {
      continue;
    }

    entry.mManager->AddImageKeyForDiscard(entry.mImageKey);
    WebRenderBridgeChild* wrBridge = entry.mManager->WrBridge();
    if (wrBridge) {
      wrBridge->DeallocExternalImageId(aId);
    }
  }

  CompositorManagerChild* manager = CompositorManagerChild::GetInstance();
  if (MOZ_UNLIKELY(!manager || !manager->CanSend())) {
    return;
  }

  if (manager->OtherPid() == base::GetCurrentProcId()) {
    // We are in the combined UI/GPU process. Call directly to it to remove its
    // wrapper surface to free the underlying buffer, but only if the external
    // image ID is owned by the manager. It can be different if the surface was
    // last shared with the GPU process, which crashed several times, and its
    // job was moved into the parent process.
    if (manager->OwnsExternalImageId(aId)) {
      SharedSurfacesParent::RemoveSameProcess(aId);
    }
  } else if (manager->OwnsExternalImageId(aId)) {
    // Only attempt to release current mappings in the GPU process. It is
    // possible we had a surface that was previously shared, the GPU process
    // crashed / was restarted, and then we freed the surface. In that case
    // we know the mapping has already been freed.
    manager->SendRemoveSharedSurface(aId);
  }
}

} // namespace layers
} // namespace mozilla
