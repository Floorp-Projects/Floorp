/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurfacesChild.h"
#include "SharedSurfacesParent.h"
#include "CompositorManagerChild.h"
#include "mozilla/layers/IpcResourceUpdateQueue.h"
#include "mozilla/layers/SourceSurfaceSharedData.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/layers/RenderRootStateManager.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/StaticPrefs_image.h"
#include "mozilla/PresShell.h"
#include "nsRefreshDriver.h"
#include "nsView.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

/* static */
UserDataKey SharedSurfacesChild::sSharedKey;

SharedSurfacesChild::ImageKeyData::ImageKeyData(
    RenderRootStateManager* aManager, const wr::ImageKey& aImageKey)
    : mManager(aManager), mImageKey(aImageKey) {}

SharedSurfacesChild::ImageKeyData::ImageKeyData(
    SharedSurfacesChild::ImageKeyData&& aOther)
    : mManager(std::move(aOther.mManager)),
      mDirtyRect(std::move(aOther.mDirtyRect)),
      mImageKey(aOther.mImageKey) {}

SharedSurfacesChild::ImageKeyData& SharedSurfacesChild::ImageKeyData::operator=(
    SharedSurfacesChild::ImageKeyData&& aOther) {
  mManager = std::move(aOther.mManager);
  mDirtyRect = std::move(aOther.mDirtyRect);
  mImageKey = aOther.mImageKey;
  return *this;
}

SharedSurfacesChild::ImageKeyData::~ImageKeyData() = default;

void SharedSurfacesChild::ImageKeyData::MergeDirtyRect(
    const Maybe<IntRect>& aDirtyRect) {
  if (mDirtyRect) {
    if (aDirtyRect) {
      mDirtyRect->UnionRect(mDirtyRect.ref(), aDirtyRect.ref());
    }
  } else {
    mDirtyRect = aDirtyRect;
  }
}

SharedSurfacesChild::SharedUserData::SharedUserData(
    const wr::ExternalImageId& aId)
    : Runnable("SharedSurfacesChild::SharedUserData"),
      mId(aId),
      mShared(false) {}

SharedSurfacesChild::SharedUserData::~SharedUserData() {
  // We may fail to dispatch during shutdown, and since it would be issued on
  // the main thread, it releases the runnable instead of leaking it.
  if (mShared || !mKeys.IsEmpty()) {
    if (NS_IsMainThread()) {
      SharedSurfacesChild::Unshare(mId, mShared, mKeys);
    } else {
      MOZ_ASSERT_UNREACHABLE("Shared resources not released!");
    }
  }
}

/* static */
void SharedSurfacesChild::SharedUserData::Destroy(void* aClosure) {
  MOZ_ASSERT(aClosure);
  RefPtr<SharedUserData> data =
      dont_AddRef(static_cast<SharedUserData*>(aClosure));
  if (data->mShared || !data->mKeys.IsEmpty()) {
    SchedulerGroup::Dispatch(data.forget());
  }
}

NS_IMETHODIMP SharedSurfacesChild::SharedUserData::Run() {
  SharedSurfacesChild::Unshare(mId, mShared, mKeys);
  mShared = false;
  mKeys.Clear();
  return NS_OK;
}

wr::ImageKey SharedSurfacesChild::SharedUserData::UpdateKey(
    RenderRootStateManager* aManager, wr::IpcResourceUpdateQueue& aResources,
    const Maybe<IntRect>& aDirtyRect) {
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(!aManager->IsDestroyed());

  // We iterate through all of the items to ensure we clean up the old
  // RenderRootStateManager references. Most of the time there will be few
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
        entry.TakeDirtyRect();
        aResources.AddSharedExternalImage(mId, entry.mImageKey);
      } else {
        entry.MergeDirtyRect(aDirtyRect);
        Maybe<IntRect> dirtyRect = entry.TakeDirtyRect();
        if (dirtyRect) {
          MOZ_ASSERT(mShared);
          aResources.UpdateSharedExternalImage(
              mId, entry.mImageKey, ViewAs<ImagePixel>(dirtyRect.ref()));
        }
      }

      key = entry.mImageKey;
      found = true;
    } else {
      // We don't have the resource update queue for this manager, so just
      // accumulate the dirty rects until it is requested.
      entry.MergeDirtyRect(aDirtyRect);
    }
  }

  if (!found) {
    key = aManager->WrBridge()->GetNextImageKey();
    ImageKeyData data(aManager, key);
    mKeys.AppendElement(std::move(data));
    aResources.AddSharedExternalImage(mId, key);
  }

  return key;
}

/* static */
SourceSurfaceSharedData* SharedSurfacesChild::AsSourceSurfaceSharedData(
    SourceSurface* aSurface) {
  MOZ_ASSERT(aSurface);
  switch (aSurface->GetType()) {
    case SurfaceType::DATA_SHARED:
    case SurfaceType::DATA_RECYCLING_SHARED:
      return static_cast<SourceSurfaceSharedData*>(aSurface);
    default:
      return nullptr;
  }
}

/* static */
nsresult SharedSurfacesChild::ShareInternal(SourceSurfaceSharedData* aSurface,
                                            SharedUserData** aUserData) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aSurface);
  MOZ_ASSERT(aUserData);

  CompositorManagerChild* manager = CompositorManagerChild::GetInstance();
  if (NS_WARN_IF(!manager || !manager->CanSend())) {
    // We cannot try to share the surface, most likely because the GPU process
    // crashed. Ideally, we would retry when it is ready, but the handles may be
    // a scarce resource, which can cause much more serious problems if we run
    // out. Better to copy into a fresh buffer later.
    aSurface->FinishedSharing();
    return NS_ERROR_NOT_INITIALIZED;
  }

  SharedUserData* data =
      static_cast<SharedUserData*>(aSurface->GetUserData(&sSharedKey));
  if (!data) {
    data =
        MakeAndAddRef<SharedUserData>(manager->GetNextExternalImageId()).take();
    aSurface->AddUserData(&sSharedKey, data, SharedUserData::Destroy);
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
  if (manager->SameProcess()) {
    SharedSurfacesParent::AddSameProcess(data->Id(), aSurface);
    data->MarkShared();
    *aUserData = data;
    return NS_OK;
  }

  // Attempt to share a handle with the GPU process. The handle may or may not
  // be available -- it will only be available if it is either not yet finalized
  // and/or if it has been finalized but never used for drawing in process.
  ipc::SharedMemoryBasic::Handle handle = ipc::SharedMemoryBasic::NULLHandle();
  nsresult rv = aSurface->CloneHandle(handle);
  if (rv == NS_ERROR_NOT_AVAILABLE) {
    // It is at least as expensive to copy the image to the GPU process if we
    // have already closed the handle necessary to share, but if we reallocate
    // the shared buffer to get a new handle, we can save some memory.
    if (NS_WARN_IF(!aSurface->ReallocHandle())) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // Reattempt the sharing of the handle to the GPU process.
    rv = aSurface->CloneHandle(handle);
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_ASSERT(rv != NS_ERROR_NOT_AVAILABLE);
    return rv;
  }

  SurfaceFormat format = aSurface->GetFormat();
  MOZ_RELEASE_ASSERT(
      format == SurfaceFormat::B8G8R8X8 || format == SurfaceFormat::B8G8R8A8,
      "bad format");

  data->MarkShared();
  manager->SendAddSharedSurface(
      data->Id(),
      SurfaceDescriptorShared(aSurface->GetSize(), aSurface->Stride(), format,
                              std::move(handle)));
  *aUserData = data;
  return NS_OK;
}

/* static */
void SharedSurfacesChild::Share(SourceSurfaceSharedData* aSurface) {
  MOZ_ASSERT(aSurface);

  // The IPDL actor to do sharing can only be accessed on the main thread so we
  // need to dispatch if off the main thread. However there is no real danger if
  // we end up racing because if it is already shared, this method will do
  // nothing.
  if (!NS_IsMainThread()) {
    class ShareRunnable final : public Runnable {
     public:
      explicit ShareRunnable(SourceSurfaceSharedData* aSurface)
          : Runnable("SharedSurfacesChild::Share"), mSurface(aSurface) {}

      NS_IMETHOD Run() override {
        SharedUserData* unused = nullptr;
        SharedSurfacesChild::ShareInternal(mSurface, &unused);
        return NS_OK;
      }

     private:
      RefPtr<SourceSurfaceSharedData> mSurface;
    };

    SchedulerGroup::Dispatch(MakeAndAddRef<ShareRunnable>(aSurface));
    return;
  }

  SharedUserData* unused = nullptr;
  SharedSurfacesChild::ShareInternal(aSurface, &unused);
}

/* static */
nsresult SharedSurfacesChild::Share(SourceSurfaceSharedData* aSurface,
                                    RenderRootStateManager* aManager,
                                    wr::IpcResourceUpdateQueue& aResources,
                                    wr::ImageKey& aKey) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aSurface);
  MOZ_ASSERT(aManager);

  // Each time the surface changes, the producers of SourceSurfaceSharedData
  // surfaces promise to increment the invalidation counter each time the
  // surface has changed. We can use this counter to determine whether or not
  // we should update our paired ImageKey.
  Maybe<IntRect> dirtyRect = aSurface->TakeDirtyRect();
  SharedUserData* data = nullptr;
  nsresult rv = SharedSurfacesChild::ShareInternal(aSurface, &data);
  if (NS_SUCCEEDED(rv)) {
    MOZ_ASSERT(data);
    aKey = data->UpdateKey(aManager, aResources, dirtyRect);
  }

  return rv;
}

/* static */
nsresult SharedSurfacesChild::Share(SourceSurface* aSurface,
                                    RenderRootStateManager* aManager,
                                    wr::IpcResourceUpdateQueue& aResources,
                                    wr::ImageKey& aKey) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aSurface);
  MOZ_ASSERT(aManager);

  auto sharedSurface = AsSourceSurfaceSharedData(aSurface);
  if (!sharedSurface) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  return Share(sharedSurface, aManager, aResources, aKey);
}

/* static */
nsresult SharedSurfacesChild::Share(SourceSurface* aSurface,
                                    wr::ExternalImageId& aId) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aSurface);

  auto sharedSurface = AsSourceSurfaceSharedData(aSurface);
  if (!sharedSurface) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // The external image ID does not change with the invalidation counter. The
  // caller of this should be aware of the invalidations of the surface through
  // another mechanism (e.g. imgRequestProxy listener notifications).
  SharedUserData* data = nullptr;
  nsresult rv = ShareInternal(sharedSurface, &data);
  if (NS_SUCCEEDED(rv)) {
    MOZ_ASSERT(data);
    aId = data->Id();
  }

  return rv;
}

/* static */
void SharedSurfacesChild::Unshare(const wr::ExternalImageId& aId,
                                  bool aReleaseId,
                                  nsTArray<ImageKeyData>& aKeys) {
  MOZ_ASSERT(NS_IsMainThread());

  for (const auto& entry : aKeys) {
    if (!entry.mManager->IsDestroyed()) {
      entry.mManager->AddImageKeyForDiscard(entry.mImageKey);
    }
  }

  if (!aReleaseId) {
    // We don't own the external image ID itself.
    return;
  }

  CompositorManagerChild* manager = CompositorManagerChild::GetInstance();
  if (MOZ_UNLIKELY(!manager || !manager->CanSend())) {
    return;
  }

  if (manager->OwnsExternalImageId(aId)) {
    // Only attempt to release current mappings in the compositor process. It is
    // possible we had a surface that was previously shared, the compositor
    // process crashed / was restarted, and then we freed the surface. In that
    // case we know the mapping has already been freed.
    manager->SendRemoveSharedSurface(aId);
  }
}

/* static */ Maybe<wr::ExternalImageId> SharedSurfacesChild::GetExternalId(
    const SourceSurfaceSharedData* aSurface) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aSurface);

  SharedUserData* data =
      static_cast<SharedUserData*>(aSurface->GetUserData(&sSharedKey));
  if (!data || !data->IsShared()) {
    return Nothing();
  }

  return Some(data->Id());
}

AnimationImageKeyData::AnimationImageKeyData(RenderRootStateManager* aManager,
                                             const wr::ImageKey& aImageKey)
    : SharedSurfacesChild::ImageKeyData(aManager, aImageKey) {}

AnimationImageKeyData::AnimationImageKeyData(AnimationImageKeyData&& aOther)
    : SharedSurfacesChild::ImageKeyData(std::move(aOther)),
      mPendingRelease(std::move(aOther.mPendingRelease)) {}

AnimationImageKeyData& AnimationImageKeyData::operator=(
    AnimationImageKeyData&& aOther) {
  mPendingRelease = std::move(aOther.mPendingRelease);
  SharedSurfacesChild::ImageKeyData::operator=(std::move(aOther));
  return *this;
}

AnimationImageKeyData::~AnimationImageKeyData() = default;

SharedSurfacesAnimation::~SharedSurfacesAnimation() {
  MOZ_ASSERT(mKeys.IsEmpty());
}

void SharedSurfacesAnimation::Destroy() {
  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIRunnable> task =
        NewRunnableMethod("SharedSurfacesAnimation::Destroy", this,
                          &SharedSurfacesAnimation::Destroy);
    NS_DispatchToMainThread(task.forget());
    return;
  }

  if (mKeys.IsEmpty()) {
    return;
  }

  for (const auto& entry : mKeys) {
    MOZ_ASSERT(!entry.mManager->IsDestroyed());
    if (StaticPrefs::image_animated_decode_on_demand_recycle_AtStartup()) {
      entry.mManager->DeregisterAsyncAnimation(entry.mImageKey);
    }
    entry.mManager->AddImageKeyForDiscard(entry.mImageKey);
  }

  mKeys.Clear();
}

void SharedSurfacesAnimation::HoldSurfaceForRecycling(
    AnimationImageKeyData& aEntry, SourceSurfaceSharedData* aSurface) {
  if (aSurface->GetType() != SurfaceType::DATA_RECYCLING_SHARED) {
    return;
  }

  MOZ_ASSERT(StaticPrefs::image_animated_decode_on_demand_recycle_AtStartup());
  aEntry.mPendingRelease.AppendElement(aSurface);
}

// This will get the widget listener that handles painting. Generally, this is
// the attached widget listener (or previously attached if the attached is paint
// suppressed). Otherwise it is the widget listener. There should be a function
// in nsIWidget that does this for us but there isn't yet.
static nsIWidgetListener* GetPaintWidgetListener(nsIWidget* aWidget) {
  if (auto* attached = aWidget->GetAttachedWidgetListener()) {
    if (attached->GetView() &&
        attached->GetView()->IsPrimaryFramePaintSuppressed()) {
      if (auto* previouslyAttached =
              aWidget->GetPreviouslyAttachedWidgetListener()) {
        return previouslyAttached;
      }
    }
    return attached;
  }

  return aWidget->GetWidgetListener();
}

nsresult SharedSurfacesAnimation::SetCurrentFrame(
    SourceSurfaceSharedData* aSurface, const gfx::IntRect& aDirtyRect) {
  MOZ_ASSERT(aSurface);

  SharedSurfacesChild::SharedUserData* data = nullptr;
  nsresult rv = SharedSurfacesChild::ShareInternal(aSurface, &data);
  if (NS_FAILED(rv)) {
    return rv;
  }

  MOZ_ASSERT(data);
  mId = data->Id();

  auto i = mKeys.Length();
  while (i > 0) {
    --i;
    AnimationImageKeyData& entry = mKeys[i];
    MOZ_ASSERT(!entry.mManager->IsDestroyed());

    if (auto* cbc =
            entry.mManager->LayerManager()->GetCompositorBridgeChild()) {
      if (cbc->IsPaused()) {
        continue;
      }
    }

    // Only root compositor bridge childs record if they are paused, so check
    // the refresh driver.
    if (auto* widget = entry.mManager->LayerManager()->GetWidget()) {
      nsIWidgetListener* wl = GetPaintWidgetListener(widget);
      // Note call to wl->GetView() to make sure this is view type widget
      // listener even though we don't use the view in this code.
      if (wl && wl->GetView() && wl->GetPresShell()) {
        if (auto* rd = wl->GetPresShell()->GetRefreshDriver()) {
          if (rd->IsThrottled()) {
            continue;
          }
        }
      }
    }

    entry.MergeDirtyRect(Some(aDirtyRect));
    Maybe<IntRect> dirtyRect = entry.TakeDirtyRect();
    if (dirtyRect) {
      HoldSurfaceForRecycling(entry, aSurface);
      auto& resourceUpdates = entry.mManager->AsyncResourceUpdates();
      resourceUpdates.UpdateSharedExternalImage(
          mId, entry.mImageKey, ViewAs<ImagePixel>(dirtyRect.ref()));
    }
  }

  return NS_OK;
}

nsresult SharedSurfacesAnimation::UpdateKey(
    SourceSurfaceSharedData* aSurface, RenderRootStateManager* aManager,
    wr::IpcResourceUpdateQueue& aResources, wr::ImageKey& aKey) {
  SharedSurfacesChild::SharedUserData* data = nullptr;
  nsresult rv = SharedSurfacesChild::ShareInternal(aSurface, &data);
  if (NS_FAILED(rv)) {
    return rv;
  }

  MOZ_ASSERT(data);
  if (wr::AsUint64(mId) != wr::AsUint64(data->Id())) {
    mKeys.Clear();
    mId = data->Id();
  }

  // We iterate through all of the items to ensure we clean up the old
  // RenderRootStateManager references. Most of the time there will be few
  // entries and this should not be particularly expensive compared to the
  // cost of duplicating image keys. In an ideal world, we would generate a
  // single key for the surface, and it would be usable on all of the
  // renderer instances. For now, we must allocate a key for each WR bridge.
  bool found = false;
  auto i = mKeys.Length();
  while (i > 0) {
    --i;
    AnimationImageKeyData& entry = mKeys[i];
    MOZ_ASSERT(!entry.mManager->IsDestroyed());
    if (entry.mManager == aManager) {
      WebRenderBridgeChild* wrBridge = aManager->WrBridge();
      MOZ_ASSERT(wrBridge);

      // Even if the manager is the same, its underlying WebRenderBridgeChild
      // can change state. If our namespace differs, then our old key has
      // already been discarded.
      bool ownsKey = wrBridge->GetNamespace() == entry.mImageKey.mNamespace;
      if (!ownsKey) {
        entry.mImageKey = wrBridge->GetNextImageKey();
        HoldSurfaceForRecycling(entry, aSurface);
        aResources.AddSharedExternalImage(mId, entry.mImageKey);
      } else {
        MOZ_ASSERT(entry.mDirtyRect.isNothing());
      }

      aKey = entry.mImageKey;
      found = true;
      break;
    }
  }

  if (!found) {
    aKey = aManager->WrBridge()->GetNextImageKey();
    if (StaticPrefs::image_animated_decode_on_demand_recycle_AtStartup()) {
      aManager->RegisterAsyncAnimation(aKey, this);
    }

    AnimationImageKeyData data(aManager, aKey);
    HoldSurfaceForRecycling(data, aSurface);
    mKeys.AppendElement(std::move(data));
    aResources.AddSharedExternalImage(mId, aKey);
  }

  return NS_OK;
}

void SharedSurfacesAnimation::ReleasePreviousFrame(
    RenderRootStateManager* aManager, const wr::ExternalImageId& aId) {
  MOZ_ASSERT(aManager);

  auto i = mKeys.Length();
  while (i > 0) {
    --i;
    AnimationImageKeyData& entry = mKeys[i];
    MOZ_ASSERT(!entry.mManager->IsDestroyed());
    if (entry.mManager == aManager) {
      size_t k;
      for (k = 0; k < entry.mPendingRelease.Length(); ++k) {
        Maybe<wr::ExternalImageId> extId =
            SharedSurfacesChild::GetExternalId(entry.mPendingRelease[k]);
        if (extId && extId.ref() == aId) {
          break;
        }
      }

      if (k == entry.mPendingRelease.Length()) {
        continue;
      }

      entry.mPendingRelease.RemoveElementsAt(0, k + 1);
      break;
    }
  }
}

void SharedSurfacesAnimation::Invalidate(RenderRootStateManager* aManager) {
  auto i = mKeys.Length();
  while (i > 0) {
    --i;
    AnimationImageKeyData& entry = mKeys[i];
    if (entry.mManager == aManager) {
      mKeys.RemoveElementAt(i);
      break;
    }
  }
}

}  // namespace layers
}  // namespace mozilla
