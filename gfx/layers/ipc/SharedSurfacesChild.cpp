/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurfacesChild.h"
#include "SharedSurfacesParent.h"
#include "CompositorManagerChild.h"
#include "mozilla/layers/SourceSurfaceSharedData.h"
#include "mozilla/SystemGroup.h"        // for SystemGroup

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

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
        SharedSurfacesChild::Unshare(mId);
      } else {
        wr::ExternalImageId id = mId;
        SystemGroup::Dispatch(TaskCategory::Other,
                              NS_NewRunnableFunction("DestroySharedUserData",
                                                     [id]() -> void {
          SharedSurfacesChild::Unshare(id);
        }));
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

private:
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
SharedSurfacesChild::Share(SourceSurfaceSharedData* aSurface,
                           wr::ExternalImageId& aId)
{
  MOZ_ASSERT(NS_IsMainThread());

  CompositorManagerChild* manager = CompositorManagerChild::GetInstance();
  if (NS_WARN_IF(!manager || !manager->CanSend())) {
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
    MOZ_ASSERT(manager->OtherPid() != base::GetCurrentProcId());
    data->SetId(manager->GetNextExternalImageId());
  } else if (data->IsShared()) {
    // It has already been shared with the GPU process, reuse the id.
    aId = data->Id();
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
    aId = data->Id();
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
  aId = data->Id();
  manager->SendAddSharedSurface(aId,
                                SurfaceDescriptorShared(aSurface->GetSize(),
                                                        aSurface->Stride(),
                                                        format, handle));
  return NS_OK;
}

/* static */ nsresult
SharedSurfacesChild::Share(ImageContainer* aContainer,
                           wr::ExternalImageId& aId,
                           uint32_t& aGeneration)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aContainer);

  if (aContainer->IsAsync()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  AutoTArray<ImageContainer::OwningImage,4> images;
  aContainer->GetCurrentImages(&images, &aGeneration);
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
  return Share(sharedSurface, aId);
}

/* static */ void
SharedSurfacesChild::Unshare(const wr::ExternalImageId& aId)
{
  MOZ_ASSERT(NS_IsMainThread());

  CompositorManagerChild* manager = CompositorManagerChild::GetInstance();
  if (MOZ_UNLIKELY(!manager || !manager->CanSend())) {
    return;
  }

  if (manager->OtherPid() == base::GetCurrentProcId()) {
    // We are in the combined UI/GPU process. Call directly to it to remove its
    // wrapper surface to free the underlying buffer.
    MOZ_ASSERT(manager->OwnsExternalImageId(aId));
    SharedSurfacesParent::RemoveSameProcess(aId);
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
