/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SHAREDSURFACESPARENT_H
#define MOZILLA_GFX_SHAREDSURFACESPARENT_H

#include <stdint.h>                         // for uint32_t
#include "mozilla/Attributes.h"             // for override
#include "mozilla/StaticMutex.h"            // for StaticMutex
#include "mozilla/StaticPtr.h"              // for StaticAutoPtr
#include "mozilla/RefPtr.h"                 // for already_AddRefed
#include "mozilla/ipc/SharedMemory.h"       // for SharedMemory, etc
#include "mozilla/gfx/2D.h"                 // for SurfaceFormat
#include "mozilla/gfx/Point.h"              // for IntSize
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptorShared
#include "mozilla/layers/SourceSurfaceSharedData.h"
#include "mozilla/webrender/WebRenderTypes.h"  // for wr::ExternalImageId
#include "nsExpirationTracker.h"
#include "nsRefPtrHashtable.h"

namespace mozilla {
namespace gfx {
class DataSourceSurface;
}  // namespace gfx

namespace layers {

class CompositorManagerParent;
class SharedSurfacesMemoryReport;

class SharedSurfacesParent final {
 public:
  static void Initialize();
  static void ShutdownRenderThread();
  static void Shutdown();

  // Get without increasing the consumer count.
  static already_AddRefed<gfx::DataSourceSurface> Get(
      const wr::ExternalImageId& aId);

  // Get but also increase the consumer count. Must call Release after finished.
  static already_AddRefed<gfx::DataSourceSurface> Acquire(
      const wr::ExternalImageId& aId);

  static bool Release(const wr::ExternalImageId& aId, bool aForCreator = false);

  static void Add(const wr::ExternalImageId& aId,
                  SurfaceDescriptorShared&& aDesc, base::ProcessId aPid);

  static void Remove(const wr::ExternalImageId& aId);

  static void RemoveAll(uint32_t aNamespace);

  static void AccumulateMemoryReport(uint32_t aNamespace,
                                     SharedSurfacesMemoryReport& aReport);

  static bool AccumulateMemoryReport(SharedSurfacesMemoryReport& aReport);

  static void AddTracking(gfx::SourceSurfaceSharedDataWrapper* aSurface);

  static void RemoveTracking(gfx::SourceSurfaceSharedDataWrapper* aSurface);

  static bool AgeOneGeneration(
      nsTArray<RefPtr<gfx::SourceSurfaceSharedDataWrapper>>& aExpired);

  static bool AgeAndExpireOneGeneration();

 private:
  friend class CompositorManagerParent;
  friend class gfx::SourceSurfaceSharedDataWrapper;

  SharedSurfacesParent();

  static void AddSameProcess(const wr::ExternalImageId& aId,
                             gfx::SourceSurfaceSharedData* aSurface);

  static void AddTrackingLocked(gfx::SourceSurfaceSharedDataWrapper* aSurface,
                                const StaticMutexAutoLock& aAutoLock);

  static void RemoveTrackingLocked(
      gfx::SourceSurfaceSharedDataWrapper* aSurface,
      const StaticMutexAutoLock& aAutoLock);

  static bool AgeOneGenerationLocked(
      nsTArray<RefPtr<gfx::SourceSurfaceSharedDataWrapper>>& aExpired,
      const StaticMutexAutoLock& aAutoLock);

  static void ExpireMap(
      nsTArray<RefPtr<gfx::SourceSurfaceSharedDataWrapper>>& aExpired);

  static StaticMutex sMutex MOZ_UNANNOTATED;

  static StaticAutoPtr<SharedSurfacesParent> sInstance;

  nsRefPtrHashtable<nsUint64HashKey, gfx::SourceSurfaceSharedDataWrapper>
      mSurfaces;

  class MappingTracker final
      : public ExpirationTrackerImpl<gfx::SourceSurfaceSharedDataWrapper, 4,
                                     StaticMutex, StaticMutexAutoLock> {
   public:
    explicit MappingTracker(uint32_t aExpirationTimeoutMS,
                            nsIEventTarget* aEventTarget)
        : ExpirationTrackerImpl<gfx::SourceSurfaceSharedDataWrapper, 4,
                                StaticMutex, StaticMutexAutoLock>(
              aExpirationTimeoutMS, "SharedMappingTracker", aEventTarget) {}

    void TakeExpired(
        nsTArray<RefPtr<gfx::SourceSurfaceSharedDataWrapper>>& aExpired,
        const StaticMutexAutoLock& aAutoLock);

   protected:
    void NotifyExpiredLocked(gfx::SourceSurfaceSharedDataWrapper* aSurface,
                             const StaticMutexAutoLock& aAutoLock) override;

    void NotifyHandlerEndLocked(const StaticMutexAutoLock& aAutoLock) override {
    }

    void NotifyHandlerEnd() override;

    StaticMutex& GetMutex() override { return sMutex; }

    nsTArray<RefPtr<gfx::SourceSurfaceSharedDataWrapper>> mExpired;
  };

  MappingTracker mTracker;
};

/**
 * Helper class that is used to keep SourceSurfaceSharedDataWrapper objects
 * around as long as one of the dependent IPDL actors is still alive and may
 * reference them for a given PCompositorManager namespace.
 */
class SharedSurfacesHolder final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SharedSurfacesHolder)

 public:
  explicit SharedSurfacesHolder(uint32_t aNamespace) : mNamespace(aNamespace) {}

  already_AddRefed<gfx::DataSourceSurface> Get(const wr::ExternalImageId& aId) {
    uint32_t extNamespace = static_cast<uint32_t>(wr::AsUint64(aId) >> 32);
    if (NS_WARN_IF(extNamespace != mNamespace)) {
      MOZ_ASSERT_UNREACHABLE("Wrong namespace?");
      return nullptr;
    }

    return SharedSurfacesParent::Get(aId);
  }

 private:
  ~SharedSurfacesHolder() { SharedSurfacesParent::RemoveAll(mNamespace); }

  uint32_t mNamespace;
};

}  // namespace layers
}  // namespace mozilla

#endif
