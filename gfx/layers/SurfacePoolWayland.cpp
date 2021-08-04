/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nullptr; c-basic-offset: 2
 * -*- This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/SurfacePoolWayland.h"

#include "GLBlitHelper.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"

namespace mozilla::layers {

using gfx::IntSize;
using gl::DepthAndStencilBuffer;
using gl::GLContext;
using gl::MozFramebuffer;
using widget::WaylandBuffer;

/* static */ RefPtr<SurfacePool> SurfacePool::Create(size_t aPoolSizeLimit) {
  return new SurfacePoolWayland(aPoolSizeLimit);
}

SurfacePoolWayland::SurfacePoolWayland(size_t aPoolSizeLimit)
    : mMutex("SurfacePoolWayland"), mPoolSizeLimit(aPoolSizeLimit) {}

RefPtr<SurfacePoolHandle> SurfacePoolWayland::GetHandleForGL(GLContext* aGL) {
  return new SurfacePoolHandleWayland(this, aGL);
}

template <typename F>
void SurfacePoolWayland::ForEachEntry(F aFn) {
  for (auto& iter : mInUseEntries) {
    aFn(iter.second);
  }
  for (auto& entry : mPendingEntries) {
    aFn(entry);
  }
  for (auto& entry : mAvailableEntries) {
    aFn(entry);
  }
}

void SurfacePoolWayland::DestroyGLResourcesForContext(GLContext* aGL) {
  MutexAutoLock lock(mMutex);

  ForEachEntry([&](SurfacePoolEntry& entry) {
    if (entry.mGLResources && entry.mGLResources->mGL == aGL) {
      entry.mGLResources = Nothing();
      entry.mWaylandBuffer->DestroyGLResources();
    }
  });
  mDepthBuffers.RemoveElementsBy(
      [&](const DepthBufferEntry& entry) { return entry.mGL == aGL; });
}

bool SurfacePoolWayland::CanRecycleSurfaceForRequest(
    const MutexAutoLock& aProofOfLock, const SurfacePoolEntry& aEntry,
    const IntSize& aSize, GLContext* aGL) {
  if (aEntry.mSize != aSize) {
    return false;
  }
  if (aEntry.mGLResources) {
    return aEntry.mGLResources->mGL == aGL;
  }
  return aGL == nullptr;
}

RefPtr<WaylandBuffer> SurfacePoolWayland::ObtainBufferFromPool(
    const IntSize& aSize, GLContext* aGL) {
  MutexAutoLock lock(mMutex);

  auto iterToRecycle = std::find_if(
      mAvailableEntries.begin(), mAvailableEntries.end(),
      [&](const SurfacePoolEntry& aEntry) {
        return CanRecycleSurfaceForRequest(lock, aEntry, aSize, aGL);
      });
  if (iterToRecycle != mAvailableEntries.end()) {
    RefPtr<WaylandBuffer> buffer = iterToRecycle->mWaylandBuffer;
    mInUseEntries.insert({buffer.get(), std::move(*iterToRecycle)});
    mAvailableEntries.RemoveElementAt(iterToRecycle);
    return buffer;
  }

  RefPtr<WaylandBuffer> buffer;
  if (aGL) {
    buffer = widget::WaylandBufferDMABUF::Create(
        LayoutDeviceIntSize::FromUnknownSize(aSize), aGL);
  } else {
    buffer = widget::WaylandBufferSHM::Create(
        LayoutDeviceIntSize::FromUnknownSize(aSize));
  }
  if (buffer) {
    mInUseEntries.insert({buffer.get(), SurfacePoolEntry{aSize, buffer, {}}});
  }

  return buffer;
}

void SurfacePoolWayland::ReturnBufferToPool(
    const RefPtr<WaylandBuffer>& aBuffer) {
  MutexAutoLock lock(mMutex);

  auto inUseEntryIter = mInUseEntries.find(aBuffer);
  MOZ_RELEASE_ASSERT(inUseEntryIter != mInUseEntries.end());

  if (aBuffer->IsAttached()) {
    mPendingEntries.AppendElement(std::move(inUseEntryIter->second));
    mInUseEntries.erase(inUseEntryIter);
  } else {
    mAvailableEntries.AppendElement(std::move(inUseEntryIter->second));
    mInUseEntries.erase(inUseEntryIter);
  }
}

void SurfacePoolWayland::EnforcePoolSizeLimit() {
  MutexAutoLock lock(mMutex);

  // Enforce the pool size limit, removing least-recently-used entries as
  // necessary.
  while (mAvailableEntries.Length() > mPoolSizeLimit) {
    mAvailableEntries.RemoveElementAt(0);
  }

  NS_WARNING_ASSERTION(mPendingEntries.Length() < mPoolSizeLimit * 2,
                       "Are we leaking pending entries?");
  NS_WARNING_ASSERTION(mInUseEntries.size() < mPoolSizeLimit * 2,
                       "Are we leaking in-use entries?");
}

void SurfacePoolWayland::CollectPendingSurfaces() {
  MutexAutoLock lock(mMutex);
  mPendingEntries.RemoveElementsBy([&](auto& entry) {
    if (!entry.mWaylandBuffer->IsAttached()) {
      mAvailableEntries.AppendElement(std::move(entry));
      return true;
    }
    return false;
  });
}

Maybe<GLuint> SurfacePoolWayland::GetFramebufferForBuffer(
    const RefPtr<WaylandBuffer>& aBuffer, GLContext* aGL,
    bool aNeedsDepthBuffer) {
  MutexAutoLock lock(mMutex);
  MOZ_RELEASE_ASSERT(aGL);

  auto inUseEntryIter = mInUseEntries.find(aBuffer);
  MOZ_RELEASE_ASSERT(inUseEntryIter != mInUseEntries.end());

  SurfacePoolEntry& entry = inUseEntryIter->second;
  if (entry.mGLResources) {
    // We have an existing framebuffer.
    MOZ_RELEASE_ASSERT(entry.mGLResources->mGL == aGL,
                       "Recycled surface that still had GL resources from a "
                       "different GL context. "
                       "This shouldn't happen.");
    if (!aNeedsDepthBuffer || entry.mGLResources->mFramebuffer->HasDepth()) {
      return Some(entry.mGLResources->mFramebuffer->mFB);
    }
  }

  // No usable existing framebuffer, we need to create one.

  if (!aGL->MakeCurrent()) {
    // Context may have been destroyed.
    return {};
  }

  const GLuint tex = aBuffer->GetTexture();
  auto fb = CreateFramebufferForTexture(lock, aGL, entry.mSize, tex,
                                        aNeedsDepthBuffer);
  if (!fb) {
    // Framebuffer completeness check may have failed.
    return {};
  }

  GLuint fbo = fb->mFB;
  entry.mGLResources = Some(GLResourcesForBuffer{aGL, std::move(fb)});
  return Some(fbo);
}

RefPtr<gl::DepthAndStencilBuffer> SurfacePoolWayland::GetDepthBufferForSharing(
    const MutexAutoLock& aProofOfLock, GLContext* aGL, const IntSize& aSize) {
  // Clean out entries for which the weak pointer has become null.
  mDepthBuffers.RemoveElementsBy(
      [&](const DepthBufferEntry& entry) { return !entry.mBuffer; });

  for (const auto& entry : mDepthBuffers) {
    if (entry.mGL == aGL && entry.mSize == aSize) {
      return entry.mBuffer.get();
    }
  }
  return nullptr;
}

UniquePtr<MozFramebuffer> SurfacePoolWayland::CreateFramebufferForTexture(
    const MutexAutoLock& aProofOfLock, GLContext* aGL, const IntSize& aSize,
    GLuint aTexture, bool aNeedsDepthBuffer) {
  if (aNeedsDepthBuffer) {
    // Try to find an existing depth buffer of aSize in aGL and create a
    // framebuffer that shares it.
    if (auto buffer = GetDepthBufferForSharing(aProofOfLock, aGL, aSize)) {
      return MozFramebuffer::CreateForBackingWithSharedDepthAndStencil(
          aSize, 0, LOCAL_GL_TEXTURE_2D, aTexture, buffer);
    }
  }

  // No depth buffer needed or we didn't find one. Create a framebuffer with a
  // new depth buffer and store a weak pointer to the new depth buffer in
  // mDepthBuffers.
  UniquePtr<MozFramebuffer> fb = MozFramebuffer::CreateForBacking(
      aGL, aSize, 0, aNeedsDepthBuffer, LOCAL_GL_TEXTURE_2D, aTexture);
  if (fb && fb->GetDepthAndStencilBuffer()) {
    mDepthBuffers.AppendElement(
        DepthBufferEntry{aGL, aSize, fb->GetDepthAndStencilBuffer().get()});
  }

  return fb;
}

SurfacePoolHandleWayland::SurfacePoolHandleWayland(
    RefPtr<SurfacePoolWayland> aPool, GLContext* aGL)
    : mPool(std::move(aPool)), mGL(aGL) {}

void SurfacePoolHandleWayland::OnBeginFrame() {
  mPool->CollectPendingSurfaces();
}

void SurfacePoolHandleWayland::OnEndFrame() { mPool->EnforcePoolSizeLimit(); }

RefPtr<WaylandBuffer> SurfacePoolHandleWayland::ObtainBufferFromPool(
    const IntSize& aSize) {
  return mPool->ObtainBufferFromPool(aSize, mGL);
}

void SurfacePoolHandleWayland::ReturnBufferToPool(
    const RefPtr<WaylandBuffer>& aBuffer) {
  mPool->ReturnBufferToPool(aBuffer);
}

Maybe<GLuint> SurfacePoolHandleWayland::GetFramebufferForBuffer(
    const RefPtr<WaylandBuffer>& aBuffer, bool aNeedsDepthBuffer) {
  return mPool->GetFramebufferForBuffer(aBuffer, mGL, aNeedsDepthBuffer);
}

}  // namespace mozilla::layers
