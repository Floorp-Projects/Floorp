/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/SurfacePoolCA.h"

#import <CoreVideo/CVPixelBuffer.h>

#include <algorithm>
#include <unordered_set>
#include <utility>

#include "mozilla/ProfilerLabels.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPrefs_gfx.h"

#include "GLContextCGL.h"
#include "MozFramebuffer.h"
#include "ScopedGLHelpers.h"

namespace mozilla {
namespace layers {

using gfx::IntPoint;
using gfx::IntRect;
using gfx::IntRegion;
using gfx::IntSize;
using gl::GLContext;
using gl::GLContextCGL;

/* static */ RefPtr<SurfacePool> SurfacePool::Create(size_t aPoolSizeLimit) {
  return new SurfacePoolCA(aPoolSizeLimit);
}

// SurfacePoolCA::LockedPool

SurfacePoolCA::LockedPool::LockedPool(size_t aPoolSizeLimit)
    : mPoolSizeLimit(aPoolSizeLimit) {}

SurfacePoolCA::LockedPool::~LockedPool() {
  MOZ_RELEASE_ASSERT(
      mWrappers.empty(),
      "Any outstanding wrappers should have kept the surface pool alive");
  MOZ_RELEASE_ASSERT(mInUseEntries.empty(),
                     "Leak! No more surfaces should be in use at this point.");
  // Remove all entries in mPendingEntries and mAvailableEntries.
  MutateEntryStorage("Clear", {}, [&]() {
    mPendingEntries.Clear();
    mAvailableEntries.Clear();
  });
}

RefPtr<SurfacePoolCAWrapperForGL> SurfacePoolCA::LockedPool::GetWrapperForGL(
    SurfacePoolCA* aPool, GLContext* aGL) {
  auto& wrapper = mWrappers[aGL];
  if (!wrapper) {
    wrapper = new SurfacePoolCAWrapperForGL(aPool, aGL);
  }
  return wrapper;
}

void SurfacePoolCA::LockedPool::DestroyGLResourcesForContext(GLContext* aGL) {
  ForEachEntry([&](SurfacePoolEntry& entry) {
    if (entry.mGLResources && entry.mGLResources->mGLContext == aGL) {
      entry.mGLResources = Nothing();
    }
  });
  mDepthBuffers.RemoveElementsBy(
      [&](const DepthBufferEntry& entry) { return entry.mGLContext == aGL; });
}

template <typename F>
void SurfacePoolCA::LockedPool::MutateEntryStorage(const char* aMutationType,
                                                   const gfx::IntSize& aSize,
                                                   F aFn) {
  [[maybe_unused]] size_t inUseCountBefore = mInUseEntries.size();
  [[maybe_unused]] size_t pendingCountBefore = mPendingEntries.Length();
  [[maybe_unused]] size_t availableCountBefore = mAvailableEntries.Length();
  [[maybe_unused]] TimeStamp before = TimeStamp::Now();

  aFn();

  if (profiler_thread_is_being_profiled_for_markers()) {
    PROFILER_MARKER_TEXT(
        "SurfacePool", GRAPHICS, MarkerTiming::IntervalUntilNowFrom(before),
        nsPrintfCString("%d -> %d in use | %d -> %d waiting for | %d -> %d "
                        "available | %s %dx%d | %dMB total memory",
                        int(inUseCountBefore), int(mInUseEntries.size()),
                        int(pendingCountBefore), int(mPendingEntries.Length()),
                        int(availableCountBefore),
                        int(mAvailableEntries.Length()), aMutationType,
                        aSize.width, aSize.height,
                        int(EstimateTotalMemory() / 1000 / 1000)));
  }
}

template <typename F>
void SurfacePoolCA::LockedPool::ForEachEntry(F aFn) {
  for (auto& iter : mInUseEntries) {
    aFn(iter.second);
  }
  for (auto& entry : mPendingEntries) {
    aFn(entry.mEntry);
  }
  for (auto& entry : mAvailableEntries) {
    aFn(entry);
  }
}

uint64_t SurfacePoolCA::LockedPool::EstimateTotalMemory() {
  std::unordered_set<const gl::DepthAndStencilBuffer*> depthAndStencilBuffers;
  uint64_t memBytes = 0;

  ForEachEntry([&](const SurfacePoolEntry& entry) {
    auto size = entry.mSize;
    memBytes += size.width * 4 * size.height;
    if (entry.mGLResources) {
      const auto& fb = *entry.mGLResources->mFramebuffer;
      if (const auto& buffer = fb.GetDepthAndStencilBuffer()) {
        depthAndStencilBuffers.insert(buffer.get());
      }
    }
  });

  for (const auto& buffer : depthAndStencilBuffers) {
    memBytes += buffer->EstimateMemory();
  }

  return memBytes;
}

bool SurfacePoolCA::LockedPool::CanRecycleSurfaceForRequest(
    const SurfacePoolEntry& aEntry, const IntSize& aSize, GLContext* aGL) {
  if (aEntry.mSize != aSize) {
    return false;
  }
  if (aEntry.mGLResources) {
    return aEntry.mGLResources->mGLContext == aGL;
  }
  return true;
}

CFTypeRefPtr<IOSurfaceRef> SurfacePoolCA::LockedPool::ObtainSurfaceFromPool(
    const IntSize& aSize, GLContext* aGL) {
  // Do a linear scan through mAvailableEntries to find an eligible surface,
  // going from oldest to newest. The size of this array is limited, so the
  // linear scan is fast.
  auto iterToRecycle =
      std::find_if(mAvailableEntries.begin(), mAvailableEntries.end(),
                   [&](const SurfacePoolEntry& aEntry) {
                     return CanRecycleSurfaceForRequest(aEntry, aSize, aGL);
                   });
  if (iterToRecycle != mAvailableEntries.end()) {
    CFTypeRefPtr<IOSurfaceRef> surface = iterToRecycle->mIOSurface;
    MOZ_RELEASE_ASSERT(surface.get(), "Available surfaces should be non-null.");
    // Move the entry from mAvailableEntries to mInUseEntries.
    MutateEntryStorage("Recycle", aSize, [&]() {
      mInUseEntries.insert({surface, std::move(*iterToRecycle)});
      mAvailableEntries.RemoveElementAt(iterToRecycle);
    });
    return surface;
  }

  AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING(
      "IOSurface creation", GRAPHICS_TileAllocation,
      nsPrintfCString("%dx%d", aSize.width, aSize.height));
  CFTypeRefPtr<IOSurfaceRef> surface =
      CFTypeRefPtr<IOSurfaceRef>::WrapUnderCreateRule(
          IOSurfaceCreate((__bridge CFDictionaryRef) @{
            (__bridge NSString*)kIOSurfaceWidth : @(aSize.width),
            (__bridge NSString*)kIOSurfaceHeight : @(aSize.height),
            (__bridge NSString*)
            kIOSurfacePixelFormat : @(kCVPixelFormatType_32BGRA),
            (__bridge NSString*)kIOSurfaceBytesPerElement : @(4),
          }));
  if (surface) {
    if (StaticPrefs::gfx_color_management_native_srgb()) {
      IOSurfaceSetValue(surface.get(), CFSTR("IOSurfaceColorSpace"),
                        kCGColorSpaceSRGB);
    }
    // Create a new entry in mInUseEntries.
    MutateEntryStorage("Create", aSize, [&]() {
      mInUseEntries.insert({surface, SurfacePoolEntry{aSize, surface, {}}});
    });
  }
  return surface;
}

void SurfacePoolCA::LockedPool::ReturnSurfaceToPool(
    CFTypeRefPtr<IOSurfaceRef> aSurface) {
  auto inUseEntryIter = mInUseEntries.find(aSurface);
  MOZ_RELEASE_ASSERT(inUseEntryIter != mInUseEntries.end());
  if (IOSurfaceIsInUse(aSurface.get())) {
    // Move the entry from mInUseEntries to mPendingEntries.
    MutateEntryStorage(
        "Start waiting for", IntSize(inUseEntryIter->second.mSize), [&]() {
          mPendingEntries.AppendElement(PendingSurfaceEntry{
              std::move(inUseEntryIter->second), mCollectionGeneration, 0});
          mInUseEntries.erase(inUseEntryIter);
        });
  } else {
    // Move the entry from mInUseEntries to mAvailableEntries.
    MOZ_RELEASE_ASSERT(inUseEntryIter->second.mIOSurface.get(),
                       "In use surfaces should be non-null.");
    MutateEntryStorage("Retain", IntSize(inUseEntryIter->second.mSize), [&]() {
      mAvailableEntries.AppendElement(std::move(inUseEntryIter->second));
      mInUseEntries.erase(inUseEntryIter);
    });
  }
}

void SurfacePoolCA::LockedPool::EnforcePoolSizeLimit() {
  // Enforce the pool size limit, removing least-recently-used entries as
  // necessary.
  while (mAvailableEntries.Length() > mPoolSizeLimit) {
    MutateEntryStorage("Evict", IntSize(mAvailableEntries[0].mSize),
                       [&]() { mAvailableEntries.RemoveElementAt(0); });
  }
}

uint64_t SurfacePoolCA::LockedPool::CollectPendingSurfaces(
    uint64_t aCheckGenerationsUpTo) {
  mCollectionGeneration++;

  // Loop from back to front, potentially deleting items as we iterate.
  // mPendingEntries is used as a set; the order of its items is not meaningful.
  size_t i = mPendingEntries.Length();
  while (i) {
    i -= 1;
    auto& pendingSurf = mPendingEntries[i];
    if (pendingSurf.mPreviousCheckGeneration > aCheckGenerationsUpTo) {
      continue;
    }
    // Check if the window server is still using the surface. As long as it is
    // doing that, we cannot move the surface to mAvailableSurfaces because
    // anything we draw to it could reach the screen in a place where we don't
    // expect it.
    if (IOSurfaceIsInUse(pendingSurf.mEntry.mIOSurface.get())) {
      // The surface is still in use. Update mPreviousCheckGeneration and
      // mCheckCount.
      pendingSurf.mPreviousCheckGeneration = mCollectionGeneration;
      pendingSurf.mCheckCount++;
      if (pendingSurf.mCheckCount >= 30) {
        // The window server has been holding on to this surface for an
        // unreasonably long time. This is known to happen sometimes, for
        // example in occluded windows or after a GPU switch. In that case,
        // release our references to the surface so that it's Not Our Problem
        // anymore. Remove the entry from mPendingEntries.
        MutateEntryStorage("Eject", IntSize(pendingSurf.mEntry.mSize),
                           [&]() { mPendingEntries.RemoveElementAt(i); });
      }
    } else {
      // The surface has become unused!
      // Move the entry from mPendingEntries to mAvailableEntries.
      MOZ_RELEASE_ASSERT(pendingSurf.mEntry.mIOSurface.get(),
                         "Pending surfaces should be non-null.");
      MutateEntryStorage(
          "Stop waiting for", IntSize(pendingSurf.mEntry.mSize), [&]() {
            mAvailableEntries.AppendElement(std::move(pendingSurf.mEntry));
            mPendingEntries.RemoveElementAt(i);
          });
    }
  }
  return mCollectionGeneration;
}

void SurfacePoolCA::LockedPool::OnWrapperDestroyed(
    gl::GLContext* aGL, SurfacePoolCAWrapperForGL* aWrapper) {
  if (aGL) {
    DestroyGLResourcesForContext(aGL);
  }

  auto iter = mWrappers.find(aGL);
  MOZ_RELEASE_ASSERT(iter != mWrappers.end());
  MOZ_RELEASE_ASSERT(iter->second == aWrapper,
                     "Only one SurfacePoolCAWrapperForGL object should "
                     "exist for each GLContext* at any time");
  mWrappers.erase(iter);
}

Maybe<GLuint> SurfacePoolCA::LockedPool::GetFramebufferForSurface(
    CFTypeRefPtr<IOSurfaceRef> aSurface, GLContext* aGL,
    bool aNeedsDepthBuffer) {
  MOZ_RELEASE_ASSERT(aGL);

  auto inUseEntryIter = mInUseEntries.find(aSurface);
  MOZ_RELEASE_ASSERT(inUseEntryIter != mInUseEntries.end());

  SurfacePoolEntry& entry = inUseEntryIter->second;
  if (entry.mGLResources) {
    // We have an existing framebuffer.
    MOZ_RELEASE_ASSERT(entry.mGLResources->mGLContext == aGL,
                       "Recycled surface that still had GL resources from a "
                       "different GL context. "
                       "This shouldn't happen.");
    if (!aNeedsDepthBuffer || entry.mGLResources->mFramebuffer->HasDepth()) {
      return Some(entry.mGLResources->mFramebuffer->mFB);
    }
  }

  // No usable existing framebuffer, we need to create one.

  AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING(
      "Framebuffer creation", GRAPHICS_TileAllocation,
      nsPrintfCString("%dx%d", entry.mSize.width, entry.mSize.height));

  RefPtr<GLContextCGL> cgl = GLContextCGL::Cast(aGL);
  MOZ_RELEASE_ASSERT(cgl, "Unexpected GLContext type");

  if (!aGL->MakeCurrent()) {
    // Context may have been destroyed.
    return {};
  }

  GLuint tex = aGL->CreateTexture();
  {
    const gl::ScopedBindTexture bindTex(aGL, tex,
                                        LOCAL_GL_TEXTURE_RECTANGLE_ARB);
    CGLTexImageIOSurface2D(cgl->GetCGLContext(), LOCAL_GL_TEXTURE_RECTANGLE_ARB,
                           LOCAL_GL_RGBA, entry.mSize.width, entry.mSize.height,
                           LOCAL_GL_BGRA, LOCAL_GL_UNSIGNED_INT_8_8_8_8_REV,
                           entry.mIOSurface.get(), 0);
  }

  auto fb =
      CreateFramebufferForTexture(aGL, entry.mSize, tex, aNeedsDepthBuffer);
  if (!fb) {
    // Framebuffer completeness check may have failed.
    return {};
  }

  GLuint fbo = fb->mFB;
  entry.mGLResources = Some(GLResourcesForSurface{aGL, std::move(fb)});
  return Some(fbo);
}

RefPtr<gl::DepthAndStencilBuffer>
SurfacePoolCA::LockedPool::GetDepthBufferForSharing(GLContext* aGL,
                                                    const IntSize& aSize) {
  // Clean out entries for which the weak pointer has become null.
  mDepthBuffers.RemoveElementsBy(
      [&](const DepthBufferEntry& entry) { return !entry.mBuffer; });

  for (const auto& entry : mDepthBuffers) {
    if (entry.mGLContext == aGL && entry.mSize == aSize) {
      return entry.mBuffer.get();
    }
  }
  return nullptr;
}

UniquePtr<gl::MozFramebuffer>
SurfacePoolCA::LockedPool::CreateFramebufferForTexture(GLContext* aGL,
                                                       const IntSize& aSize,
                                                       GLuint aTexture,
                                                       bool aNeedsDepthBuffer) {
  if (aNeedsDepthBuffer) {
    // Try to find an existing depth buffer of aSize in aGL and create a
    // framebuffer that shares it.
    if (auto buffer = GetDepthBufferForSharing(aGL, aSize)) {
      return gl::MozFramebuffer::CreateForBackingWithSharedDepthAndStencil(
          aSize, 0, LOCAL_GL_TEXTURE_RECTANGLE_ARB, aTexture, buffer);
    }
  }

  // No depth buffer needed or we didn't find one. Create a framebuffer with a
  // new depth buffer and store a weak pointer to the new depth buffer in
  // mDepthBuffers.
  UniquePtr<gl::MozFramebuffer> fb = gl::MozFramebuffer::CreateForBacking(
      aGL, aSize, 0, aNeedsDepthBuffer, LOCAL_GL_TEXTURE_RECTANGLE_ARB,
      aTexture);
  if (fb && fb->GetDepthAndStencilBuffer()) {
    mDepthBuffers.AppendElement(
        DepthBufferEntry{aGL, aSize, fb->GetDepthAndStencilBuffer().get()});
  }

  return fb;
}

// SurfacePoolHandleCA

SurfacePoolHandleCA::SurfacePoolHandleCA(
    RefPtr<SurfacePoolCAWrapperForGL>&& aPoolWrapper,
    uint64_t aCurrentCollectionGeneration)
    : mPoolWrapper(aPoolWrapper),
      mPreviousFrameCollectionGeneration(
          "SurfacePoolHandleCA::mPreviousFrameCollectionGeneration") {
  auto generation = mPreviousFrameCollectionGeneration.Lock();
  *generation = aCurrentCollectionGeneration;
}

SurfacePoolHandleCA::~SurfacePoolHandleCA() {}

void SurfacePoolHandleCA::OnBeginFrame() {
  auto generation = mPreviousFrameCollectionGeneration.Lock();
  *generation = mPoolWrapper->mPool->CollectPendingSurfaces(*generation);
}

void SurfacePoolHandleCA::OnEndFrame() {
  mPoolWrapper->mPool->EnforcePoolSizeLimit();
}

CFTypeRefPtr<IOSurfaceRef> SurfacePoolHandleCA::ObtainSurfaceFromPool(
    const IntSize& aSize) {
  return mPoolWrapper->mPool->ObtainSurfaceFromPool(aSize, mPoolWrapper->mGL);
}

void SurfacePoolHandleCA::ReturnSurfaceToPool(
    CFTypeRefPtr<IOSurfaceRef> aSurface) {
  mPoolWrapper->mPool->ReturnSurfaceToPool(aSurface);
}

Maybe<GLuint> SurfacePoolHandleCA::GetFramebufferForSurface(
    CFTypeRefPtr<IOSurfaceRef> aSurface, bool aNeedsDepthBuffer) {
  return mPoolWrapper->mPool->GetFramebufferForSurface(
      aSurface, mPoolWrapper->mGL, aNeedsDepthBuffer);
}

// SurfacePoolCA

SurfacePoolCA::SurfacePoolCA(size_t aPoolSizeLimit)
    : mPool(LockedPool(aPoolSizeLimit), "SurfacePoolCA::mPool") {}

SurfacePoolCA::~SurfacePoolCA() {}

RefPtr<SurfacePoolHandle> SurfacePoolCA::GetHandleForGL(GLContext* aGL) {
  RefPtr<SurfacePoolCAWrapperForGL> wrapper;
  uint64_t collectionGeneration = 0;
  {
    auto pool = mPool.Lock();
    wrapper = pool->GetWrapperForGL(this, aGL);
    collectionGeneration = pool->mCollectionGeneration;
  }

  // Run the SurfacePoolHandleCA constructor outside of the lock so that the
  // mPool lock and the handle's lock are always ordered the same way.
  return new SurfacePoolHandleCA(std::move(wrapper), collectionGeneration);
}

void SurfacePoolCA::DestroyGLResourcesForContext(GLContext* aGL) {
  auto pool = mPool.Lock();
  pool->DestroyGLResourcesForContext(aGL);
}

CFTypeRefPtr<IOSurfaceRef> SurfacePoolCA::ObtainSurfaceFromPool(
    const IntSize& aSize, GLContext* aGL) {
  auto pool = mPool.Lock();
  return pool->ObtainSurfaceFromPool(aSize, aGL);
}

void SurfacePoolCA::ReturnSurfaceToPool(CFTypeRefPtr<IOSurfaceRef> aSurface) {
  auto pool = mPool.Lock();
  pool->ReturnSurfaceToPool(aSurface);
}

uint64_t SurfacePoolCA::CollectPendingSurfaces(uint64_t aCheckGenerationsUpTo) {
  auto pool = mPool.Lock();
  return pool->CollectPendingSurfaces(aCheckGenerationsUpTo);
}
void SurfacePoolCA::EnforcePoolSizeLimit() {
  auto pool = mPool.Lock();
  pool->EnforcePoolSizeLimit();
}

Maybe<GLuint> SurfacePoolCA::GetFramebufferForSurface(
    CFTypeRefPtr<IOSurfaceRef> aSurface, GLContext* aGL,
    bool aNeedsDepthBuffer) {
  auto pool = mPool.Lock();
  return pool->GetFramebufferForSurface(aSurface, aGL, aNeedsDepthBuffer);
}

void SurfacePoolCA::OnWrapperDestroyed(gl::GLContext* aGL,
                                       SurfacePoolCAWrapperForGL* aWrapper) {
  auto pool = mPool.Lock();
  return pool->OnWrapperDestroyed(aGL, aWrapper);
}

}  // namespace layers
}  // namespace mozilla
