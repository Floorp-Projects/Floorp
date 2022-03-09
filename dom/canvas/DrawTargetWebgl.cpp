/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DrawTargetWebglInternal.h"
#include "SourceSurfaceWebgl.h"

#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/gfx/Blur.h"
#include "mozilla/gfx/DrawTargetSkia.h"
#include "mozilla/gfx/HelpersSkia.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/PathSkia.h"
#include "mozilla/gfx/Swizzle.h"

#include "ClientWebGLContext.h"

#include "gfxPlatform.h"
#include "gfxUtils.h"

namespace mozilla::gfx {

// Inserts (allocated) a rectangle of the requested size into the tree.
Maybe<IntPoint> TexturePacker::Insert(const IntSize& aSize) {
  // Check if the available space could possibly fit the requested size. If
  // not, there is no reason to continue searching within this sub-tree.
  if (mAvailable < std::min(aSize.width, aSize.height) ||
      mBounds.width < aSize.width || mBounds.height < aSize.height) {
    return Nothing();
  }
  if (mChildren[0]) {
    // If this node has children, then try to insert into each of the children
    // in turn.
    Maybe<IntPoint> inserted = mChildren[0]->Insert(aSize);
    if (!inserted) {
      inserted = mChildren[1]->Insert(aSize);
    }
    // If the insertion succeeded, adjust the available state to reflect the
    // remaining space in the children.
    if (inserted) {
      mAvailable = std::max(mChildren[0]->mAvailable, mChildren[1]->mAvailable);
      if (!mAvailable) {
        DiscardChildren();
      }
    }
    return inserted;
  }
  // If we get here, we've encountered a leaf node. First check if its size is
  // exactly the requested size. If so, mark the node as unavailable and return
  // its offset.
  if (mBounds.Size() == aSize) {
    mAvailable = 0;
    return Some(mBounds.TopLeft());
  }
  // The node is larger than the requested size. Choose the axis which has the
  // most excess space beyond the requested size and split it so that at least
  // one of the children matches the requested size for that axis.
  if (mBounds.width - aSize.width > mBounds.height - aSize.height) {
    mChildren[0].reset(new TexturePacker(
        IntRect(mBounds.x, mBounds.y, aSize.width, mBounds.height)));
    mChildren[1].reset(new TexturePacker(
        IntRect(mBounds.x + aSize.width, mBounds.y, mBounds.width - aSize.width,
                mBounds.height)));
  } else {
    mChildren[0].reset(new TexturePacker(
        IntRect(mBounds.x, mBounds.y, mBounds.width, aSize.height)));
    mChildren[1].reset(new TexturePacker(
        IntRect(mBounds.x, mBounds.y + aSize.height, mBounds.width,
                mBounds.height - aSize.height)));
  }
  // After splitting, try to insert into the first child, which should usually
  // be big enough to accomodate the request. Adjust the available state to the
  // remaining space.
  Maybe<IntPoint> inserted = mChildren[0]->Insert(aSize);
  mAvailable = std::max(mChildren[0]->mAvailable, mChildren[1]->mAvailable);
  return inserted;
}

// Removes (frees) a rectangle with the given bounds from the tree.
bool TexturePacker::Remove(const IntRect& aBounds) {
  if (!mChildren[0]) {
    // If there are no children, we encountered a leaf node. Non-zero available
    // state means that this node was already removed previously. Also, if the
    // bounds don't contain the request, and assuming the tree was previously
    // split during insertion, then this node is not the node we're searching
    // for.
    if (mAvailable > 0 || !mBounds.Contains(aBounds)) {
      return false;
    }
    // The bounds match exactly and it was previously inserted, so in this case
    // we can just remove it.
    if (mBounds == aBounds) {
      mAvailable = std::min(mBounds.width, mBounds.height);
      return true;
    }
    // We need to split this leaf node so that it can exactly match the removed
    // bounds. We know the leaf node at least contains the removed bounds, but
    // needs to be subdivided until it has a child node that exactly matches.
    // Choose the axis to split with the largest amount of excess space. Within
    // that axis, choose the larger of the space before or after the subrect as
    // the split point to the new children.
    if (mBounds.width - aBounds.width > mBounds.height - aBounds.height) {
      int split = aBounds.x - mBounds.x > mBounds.XMost() - aBounds.XMost()
                      ? aBounds.x
                      : aBounds.XMost();
      mChildren[0].reset(new TexturePacker(
          IntRect(mBounds.x, mBounds.y, split - mBounds.x, mBounds.height),
          false));
      mChildren[1].reset(new TexturePacker(
          IntRect(split, mBounds.y, mBounds.XMost() - split, mBounds.height),
          false));
    } else {
      int split = aBounds.y - mBounds.y > mBounds.YMost() - aBounds.YMost()
                      ? aBounds.y
                      : aBounds.YMost();
      mChildren[0].reset(new TexturePacker(
          IntRect(mBounds.x, mBounds.y, mBounds.width, split - mBounds.y),
          false));
      mChildren[1].reset(new TexturePacker(
          IntRect(mBounds.x, split, mBounds.width, mBounds.YMost() - split),
          false));
    }
  }
  // We've encountered a branch node. Determine which of the two child nodes
  // would possibly contain the removed bounds. We first check which axis the
  // children were split on and then whether the removed bounds on that axis
  // are past the start of the second child. Proceed to recurse into that
  // child node for removal.
  bool next = mChildren[0]->mBounds.x < mChildren[1]->mBounds.x
                  ? aBounds.x >= mChildren[1]->mBounds.x
                  : aBounds.y >= mChildren[1]->mBounds.y;
  bool removed = mChildren[next ? 1 : 0]->Remove(aBounds);
  if (removed) {
    if (mChildren[0]->IsFullyAvailable() && mChildren[1]->IsFullyAvailable()) {
      DiscardChildren();
      mAvailable = std::min(mBounds.width, mBounds.height);
    } else {
      mAvailable = std::max(mChildren[0]->mAvailable, mChildren[1]->mAvailable);
    }
  }
  return removed;
}

SharedTexture::SharedTexture(const IntSize& aSize, SurfaceFormat aFormat,
                             const RefPtr<WebGLTextureJS>& aTexture)
    : mPacker(IntRect(IntPoint(0, 0), aSize)),
      mFormat(aFormat),
      mTexture(aTexture) {}

SharedTextureHandle::SharedTextureHandle(const IntRect& aBounds,
                                         SharedTexture* aTexture)
    : mBounds(aBounds), mTexture(aTexture) {}

already_AddRefed<SharedTextureHandle> SharedTexture::Allocate(
    const IntSize& aSize) {
  RefPtr<SharedTextureHandle> handle;
  if (Maybe<IntPoint> origin = mPacker.Insert(aSize)) {
    handle = new SharedTextureHandle(IntRect(*origin, aSize), this);
    ++mAllocatedHandles;
  }
  return handle.forget();
}

bool SharedTexture::Free(const SharedTextureHandle& aHandle) {
  if (aHandle.mTexture != this) {
    return false;
  }
  if (!mPacker.Remove(aHandle.mBounds)) {
    return false;
  }
  --mAllocatedHandles;
  return true;
}

StandaloneTexture::StandaloneTexture(const IntSize& aSize,
                                     SurfaceFormat aFormat,
                                     const RefPtr<WebGLTextureJS>& aTexture)
    : mSize(aSize), mFormat(aFormat), mTexture(aTexture) {}

DrawTargetWebgl::DrawTargetWebgl() = default;

inline void DrawTargetWebgl::SharedContext::ClearLastTexture() {
  mLastTexture = nullptr;
}

// Attempts to clear the snapshot state. If the snapshot is only referenced by
// this target, then it should simply be destroyed. If it is a WebGL surface in
// use by something else, then special cleanup such as reusing the texture or
// copy-on-write may be possible.
void DrawTargetWebgl::ClearSnapshot(bool aCopyOnWrite) {
  if (!mSnapshot) {
    return;
  }
  mSharedContext->ClearLastTexture();
  if (mSnapshot->hasOneRef() || mSnapshot->GetType() != SurfaceType::WEBGL) {
    mSnapshot = nullptr;
    return;
  }
  RefPtr<SourceSurfaceWebgl> snapshot =
      mSnapshot.forget().downcast<SourceSurfaceWebgl>();
  if (aCopyOnWrite) {
    // WebGL snapshots must be notified that the framebuffer contents will be
    // changing so that it can copy the data.
    snapshot->DrawTargetWillChange();
  } else {
    // If not copying, then give the backing texture to the surface for reuse.
    snapshot->GiveTexture(
        mSharedContext->WrapSnapshot(GetSize(), GetFormat(), mTex.forget()));
  }
}

DrawTargetWebgl::~DrawTargetWebgl() {
  ClearSnapshot(false);

  // If this was the last target the shared context used, then get rid of it.
  if (--mSharedContext->mNumTargets <= 0 && sSharedContext == mSharedContext) {
    sSharedContext = nullptr;
  }
}

DrawTargetWebgl::SharedContext::SharedContext() = default;

DrawTargetWebgl::SharedContext::~SharedContext() {
  while (!mTextureHandles.isEmpty()) {
    PruneTextureHandle(mTextureHandles.popLast());
    --mNumTextureHandles;
  }
  UnlinkSurfaceTextures();
  UnlinkGlyphCaches();
}

// Remove any SourceSurface user data associated with this TextureHandle.
inline void DrawTargetWebgl::SharedContext::UnlinkSurfaceTexture(
    const RefPtr<TextureHandle>& aHandle) {
  if (SourceSurface* surface = aHandle->GetSurface()) {
    // Ensure any WebGL snapshot textures get unlinked.
    if (surface->GetType() == SurfaceType::WEBGL) {
      static_cast<SourceSurfaceWebgl*>(surface)->OnUnlinkTexture(this);
    }
    surface->RemoveUserData(aHandle->IsShadow() ? &mShadowTextureKey
                                                : &mTextureHandleKey);
  }
}

// Unlinks TextureHandles from any SourceSurface user data.
void DrawTargetWebgl::SharedContext::UnlinkSurfaceTextures() {
  for (RefPtr<TextureHandle> handle = mTextureHandles.getFirst(); handle;
       handle = handle->getNext()) {
    UnlinkSurfaceTexture(handle);
  }
}

// Unlinks GlyphCaches from any ScaledFont user data.
void DrawTargetWebgl::SharedContext::UnlinkGlyphCaches() {
  GlyphCache* cache = mGlyphCaches.getFirst();
  while (cache) {
    ScaledFont* font = cache->GetFont();
    // Access the next cache before removing the user data, as it might destroy
    // the cache.
    cache = cache->getNext();
    font->RemoveUserData(&mGlyphCacheKey);
  }
}

RefPtr<DrawTargetWebgl::SharedContext> DrawTargetWebgl::sSharedContext;

// Try to initialize a new WebGL context. Verifies that the requested size does
// not exceed the available texture limits and that shader creation succeeded.
bool DrawTargetWebgl::Init(const IntSize& size, const SurfaceFormat format) {
  MOZ_ASSERT(format == SurfaceFormat::B8G8R8A8 ||
             format == SurfaceFormat::B8G8R8X8);

  mSize = size;
  mFormat = format;

  if (!sSharedContext || sSharedContext->IsContextLost()) {
    mSharedContext = new DrawTargetWebgl::SharedContext;
    if (!mSharedContext->Initialize()) {
      mSharedContext = nullptr;
      return false;
    }
  } else {
    mSharedContext = sSharedContext;
  }

  if (size_t(std::max(size.width, size.height)) >
      mSharedContext->mMaxTextureSize) {
    return false;
  }

  if (!CreateFramebuffer()) {
    return false;
  }

  mSkia = new DrawTargetSkia;
  if (!mSkia->Init(size, SurfaceFormat::B8G8R8A8)) {
    return false;
  }
  mSkia->SetPermitSubpixelAA(IsOpaque(format));

  // Remember the last shared context used.
  ++mSharedContext->mNumTargets;
  sSharedContext = mSharedContext;
  return true;
}

bool DrawTargetWebgl::SharedContext::Initialize() {
  WebGLContextOptions options = {};
  options.alpha = true;
  options.depth = false;
  options.stencil = false;
  options.antialias = false;
  options.preserveDrawingBuffer = true;
  options.failIfMajorPerformanceCaveat = true;

  mWebgl = new ClientWebGLContext(true);
  mWebgl->SetContextOptions(options);
  if (mWebgl->SetDimensions(1, 1) != NS_OK) {
    mWebgl = nullptr;
    return false;
  }

  mMaxTextureSize = mWebgl->Limits().maxTex2dSize;

  if (!CreateShaders()) {
    mWebgl = nullptr;
    return false;
  }

  return true;
}

void DrawTargetWebgl::SharedContext::SetBlendState(CompositionOp aOp) {
  if (aOp == mLastCompositionOp) {
    return;
  }
  mLastCompositionOp = aOp;

  // Map the composition op to a WebGL blend mode, if possible.
  mWebgl->Enable(LOCAL_GL_BLEND);
  switch (aOp) {
    case CompositionOp::OP_OVER:
      mWebgl->BlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                                LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA);
      break;
    case CompositionOp::OP_ADD:
      mWebgl->BlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE, LOCAL_GL_ONE,
                                LOCAL_GL_ONE);
      break;
    case CompositionOp::OP_ATOP:
      mWebgl->BlendFuncSeparate(
          LOCAL_GL_DST_ALPHA, LOCAL_GL_ONE_MINUS_SRC_ALPHA, LOCAL_GL_DST_ALPHA,
          LOCAL_GL_ONE_MINUS_SRC_ALPHA);
      break;
    case CompositionOp::OP_SOURCE:
    default:
      mWebgl->Disable(LOCAL_GL_BLEND);
      break;
  }
}

// Ensure the WebGL framebuffer is set to the current target.
bool DrawTargetWebgl::SharedContext::SetTarget(DrawTargetWebgl* aDT) {
  if (mWebgl->IsContextLost()) {
    return false;
  }
  if (aDT != mCurrentTarget) {
    mCurrentTarget = aDT;
    if (aDT) {
      mWebgl->BindFramebuffer(LOCAL_GL_FRAMEBUFFER, aDT->mFramebuffer);
      mViewportSize = aDT->GetSize();
      mWebgl->Viewport(0, 0, mViewportSize.width, mViewportSize.height);
    }
  }
  return true;
}

// Installs the Skia clip rectangle, if applicable, onto the shared WebGL
// context as well as sets the WebGL framebuffer to the current target.
bool DrawTargetWebgl::PrepareContext(bool aClipped) {
  if (!aClipped) {
    // If no clipping requested, just set the clip rect to the viewport.
    mSharedContext->SetClipRect(IntRect(IntPoint(), mSize));
    // Ensure the clip gets reset if clipping is later requested for the target.
    mClipDirty = true;
  } else if (mClipDirty || !mSharedContext->IsCurrentTarget(this)) {
    // Determine whether the clipping rectangle is simple enough to accelerate.
    // Check if there is a device space clip rectangle available from the Skia
    // target.
    Maybe<Rect> clip = mSkia->GetDeviceClipRect();
    if (!clip) {
      return false;
    }
    // If the clip is empty, leave the final integer clip rectangle empty to
    // trivially discard the draw request.
    IntRect intClip;
    if (!clip->IsEmpty()) {
      // Otherwise, check if the clip rectangle is imperceptibly close to pixel
      // boundaries so that rounding won't noticeably change the clipping
      // result. Scissoring only supports integer coordinates.
      intClip = RoundedToInt(*clip);
      if (!clip->WithinEpsilonOf(Rect(intClip), 1.0e-3f)) {
        return false;
      }
      // If the clip rect is larger than the viewport, just set it to the
      // viewport.
      if (intClip.Contains(IntRect(IntPoint(), mSize))) {
        intClip = IntRect(IntPoint(), mSize);
      }
    }
    mSharedContext->SetClipRect(intClip);
    mClipDirty = false;
  }
  return mSharedContext->SetTarget(this);
}

bool DrawTargetWebgl::SharedContext::IsContextLost() const {
  return mWebgl->IsContextLost();
}

// Signal to CanvasRenderingContext2D when the WebGL context is lost.
bool DrawTargetWebgl::IsValid() const {
  return mSharedContext && !mSharedContext->IsContextLost();
}

already_AddRefed<DrawTargetWebgl> DrawTargetWebgl::Create(
    const IntSize& aSize, SurfaceFormat aFormat) {
  if (!StaticPrefs::gfx_canvas_accelerated()) {
    return nullptr;
  }

  // The interpretation of the min-size and max-size follows from the old
  // SkiaGL prefs. First just ensure that the context is not unreasonably
  // small.
  static const int32_t kMinDimension = 16;
  if (std::min(aSize.width, aSize.height) < kMinDimension) {
    return nullptr;
  }

  int32_t minSize = StaticPrefs::gfx_canvas_accelerated_min_size();
  if (aSize.width * aSize.height < minSize * minSize) {
    return nullptr;
  }

  // Maximum pref allows 3 different options:
  //  0 means unlimited size,
  //  > 0 means use value as an absolute threshold,
  //  < 0 means use the number of screen pixels as a threshold.
  int32_t maxSize = StaticPrefs::gfx_canvas_accelerated_max_size();
  if (maxSize > 0) {
    if (std::max(aSize.width, aSize.height) > maxSize) {
      return nullptr;
    }
  } else if (maxSize < 0) {
    // Default to historical mobile screen size of 980x480, like FishIEtank.
    // In addition, allow acceleration up to this size even if the screen is
    // smaller. A lot content expects this size to work well. See Bug 999841
    static const int32_t kScreenPixels = 980 * 480;
    IntSize screenSize = gfxPlatform::GetPlatform()->GetScreenSize();
    if (aSize.width * aSize.height >
        std::max(screenSize.width * screenSize.height, kScreenPixels)) {
      return nullptr;
    }
  }

  RefPtr<DrawTargetWebgl> dt = new DrawTargetWebgl;
  if (!dt->Init(aSize, aFormat) || !dt->IsValid()) {
    return nullptr;
  }

  return dt.forget();
}

void* DrawTargetWebgl::GetNativeSurface(NativeSurfaceType aType) {
  switch (aType) {
    case NativeSurfaceType::WEBGL_CONTEXT:
      // If the context is lost, then don't attempt to access it.
      if (mSharedContext->IsContextLost()) {
        return nullptr;
      }
      if (!mWebglValid) {
        FlushFromSkia();
      }
      return mSharedContext->mWebgl.get();
    default:
      return nullptr;
  }
}

// Wrap a WebGL texture holding a snapshot with a texture handle. Note that
// while the texture is still in use as the backing texture of a framebuffer,
// it's texture memory is not currently tracked with other texture handles.
// Once it is finally orphaned and used as a texture handle, it must be added
// to the resource usage totals.
already_AddRefed<TextureHandle> DrawTargetWebgl::SharedContext::WrapSnapshot(
    const IntSize& aSize, SurfaceFormat aFormat, RefPtr<WebGLTextureJS> aTex) {
  // Ensure there is enough space for the texture.
  size_t usedBytes = TextureHandle::UsedBytes(aFormat, aSize);
  PruneTextureMemory(usedBytes, false);
  // Allocate a handle for the texture
  RefPtr<StandaloneTexture> handle =
      new StandaloneTexture(aSize, aFormat, aTex.forget());
  mStandaloneTextures.push_back(handle);
  mTextureHandles.insertFront(handle);
  mTotalTextureMemory += usedBytes;
  mUsedTextureMemory += usedBytes;
  ++mNumTextureHandles;
  return handle.forget();
}

void DrawTargetWebgl::SharedContext::InitTexParameters(WebGLTextureJS* aTex) {
  mWebgl->TexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S,
                        LOCAL_GL_CLAMP_TO_EDGE);
  mWebgl->TexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T,
                        LOCAL_GL_CLAMP_TO_EDGE);
  mWebgl->TexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER,
                        LOCAL_GL_LINEAR);
  mWebgl->TexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER,
                        LOCAL_GL_LINEAR);
}

// Copy the contents of the WebGL framebuffer into a WebGL texture.
already_AddRefed<TextureHandle> DrawTargetWebgl::SharedContext::CopySnapshot() {
  // If the target is going away, then we can just directly reuse the
  // framebuffer texture since it will never change.
  RefPtr<WebGLTextureJS> tex = mWebgl->CreateTexture();
  if (!tex) {
    return nullptr;
  }

  SurfaceFormat format = mCurrentTarget->GetFormat();
  IntSize size = mCurrentTarget->GetSize();
  // Create a texture to hold the copy
  mWebgl->BindTexture(LOCAL_GL_TEXTURE_2D, tex);
  mWebgl->TexStorage2D(LOCAL_GL_TEXTURE_2D, 1, LOCAL_GL_RGBA8, size.width,
                       size.height);
  InitTexParameters(tex);
  // Copy the framebuffer into the texture
  mWebgl->CopyTexSubImage2D(LOCAL_GL_TEXTURE_2D, 0, 0, 0, 0, 0, size.width,
                            size.height);
  ClearLastTexture();

  return WrapSnapshot(size, format, tex.forget());
}

// Utility method to install the target before copying a snapshot.
already_AddRefed<TextureHandle> DrawTargetWebgl::CopySnapshot() {
  AutoRestoreContext restore(this);
  if (!PrepareContext(false)) {
    return nullptr;
  }
  return mSharedContext->CopySnapshot();
}

already_AddRefed<SourceSurface> DrawTargetWebgl::Snapshot() {
  // If already using the Skia fallback, then just snapshot that.
  if (mSkiaValid) {
    if (mSkiaLayer) {
      FlattenSkia();
    }
    return mSkia->Snapshot(mFormat);
  }

  // There's no valid Skia snapshot, so we need to get one from the WebGL
  // context.
  if (!mSnapshot) {
    // First just try to create a copy-on-write reference to this target.
    RefPtr<SourceSurfaceWebgl> snapshot = new SourceSurfaceWebgl;
    if (snapshot->Init(this)) {
      mSnapshot = snapshot;
    } else {
      // Otherwse, we have to just read back the framebuffer contents. This may
      // fail if the WebGL context is lost.
      mSnapshot = ReadSnapshot();
    }
  }
  return do_AddRef(mSnapshot);
}

// Read from the WebGL context into a buffer. This handles both swizzling BGRA
// to RGBA and flipping the image.
bool DrawTargetWebgl::SharedContext::ReadInto(uint8_t* aDstData,
                                              int32_t aDstStride,
                                              SurfaceFormat aFormat,
                                              const IntRect& aBounds,
                                              TextureHandle* aHandle) {
  MOZ_ASSERT(aFormat == SurfaceFormat::B8G8R8A8 ||
             aFormat == SurfaceFormat::B8G8R8X8);

  // If reading into a new texture, we have to bind it to a scratch framebuffer
  // for reading.
  if (aHandle) {
    if (!mScratchFramebuffer) {
      mScratchFramebuffer = mWebgl->CreateFramebuffer();
    }
    mWebgl->BindFramebuffer(LOCAL_GL_FRAMEBUFFER, mScratchFramebuffer);
    mWebgl->FramebufferTexture2D(
        LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0, LOCAL_GL_TEXTURE_2D,
        aHandle->GetWebGLTexture(), 0);
  }

  webgl::ReadPixelsDesc desc;
  desc.srcOffset = *ivec2::From(aBounds);
  desc.size = *uvec2::FromSize(aBounds);
  desc.packState.rowLength = aDstStride / 4;
  Range<uint8_t> range = {aDstData, size_t(aDstStride) * aBounds.height};
  bool success = mWebgl->DoReadPixels(desc, range);

  // Restore the actual framebuffer after reading is done.
  if (aHandle && mCurrentTarget) {
    mWebgl->BindFramebuffer(LOCAL_GL_FRAMEBUFFER, mCurrentTarget->mFramebuffer);
  }

  return success;
}

already_AddRefed<DataSourceSurface>
DrawTargetWebgl::SharedContext::ReadSnapshot(TextureHandle* aHandle) {
  // Allocate a data surface, map it, and read from the WebGL context into the
  // surface.
  SurfaceFormat format = SurfaceFormat::UNKNOWN;
  IntRect bounds;
  if (aHandle) {
    format = aHandle->GetFormat();
    bounds = aHandle->GetBounds();
  } else {
    format = mCurrentTarget->GetFormat();
    bounds = mCurrentTarget->GetRect();
  }
  RefPtr<DataSourceSurface> surface =
      Factory::CreateDataSourceSurface(bounds.Size(), format);
  if (!surface) {
    return nullptr;
  }
  DataSourceSurface::ScopedMap dstMap(surface, DataSourceSurface::WRITE);
  if (!dstMap.IsMapped() || !ReadInto(dstMap.GetData(), dstMap.GetStride(),
                                      format, bounds, aHandle)) {
    return nullptr;
  }
  return surface.forget();
}

// Utility method to install the target before reading a snapshot.
bool DrawTargetWebgl::ReadInto(uint8_t* aDstData, int32_t aDstStride) {
  if (!PrepareContext(false)) {
    return false;
  }

  return mSharedContext->ReadInto(aDstData, aDstStride, GetFormat(), GetRect());
}

// Utility method to install the target before reading a snapshot.
already_AddRefed<DataSourceSurface> DrawTargetWebgl::ReadSnapshot() {
  AutoRestoreContext restore(this);
  if (!PrepareContext(false)) {
    return nullptr;
  }
  return mSharedContext->ReadSnapshot();
}

already_AddRefed<SourceSurface> DrawTargetWebgl::GetBackingSurface() {
  return Snapshot();
}

bool DrawTargetWebgl::CopySnapshotTo(DrawTarget* aDT) {
  if (mSkiaValid ||
      (mSnapshot &&
       (mSnapshot->GetType() != SurfaceType::WEBGL ||
        static_cast<SourceSurfaceWebgl*>(mSnapshot.get())->HasReadData()))) {
    // There's already a snapshot that is mapped, so just use that.
    return false;
  }
  // Otherwise, attempt to read the data directly into the DT pixels to avoid an
  // intermediate copy.
  if (!PrepareContext(false)) {
    return false;
  }
  uint8_t* data = nullptr;
  IntSize size;
  int32_t stride = 0;
  SurfaceFormat format = SurfaceFormat::UNKNOWN;
  if (!aDT->LockBits(&data, &size, &stride, &format)) {
    return false;
  }
  bool result =
      mSharedContext->ReadInto(data, stride, format,
                               {0, 0, std::min(size.width, mSize.width),
                                std::min(size.height, mSize.height)});
  aDT->ReleaseBits(data);
  return result;
}

void DrawTargetWebgl::DetachAllSnapshots() {
  mSkia->DetachAllSnapshots();
  ClearSnapshot();
}

// Prepare the framebuffer for accelerated drawing. Any cached snapshots will
// be invalidated if not detached and copied here. Ensure the WebGL
// framebuffer's contents are updated if still somehow stored in the Skia
// framebuffer.
bool DrawTargetWebgl::MarkChanged() {
  if (mSnapshot) {
    ClearSnapshot();
  }
  if (!mWebglValid && !FlushFromSkia()) {
    return false;
  }
  mSkiaValid = false;
  return true;
}

bool DrawTargetWebgl::LockBits(uint8_t** aData, IntSize* aSize,
                               int32_t* aStride, SurfaceFormat* aFormat,
                               IntPoint* aOrigin) {
  // Can only access pixels if there is valid, flattened Skia data.
  if (mSkiaValid && !mSkiaLayer) {
    return mSkia->LockBits(aData, aSize, aStride, aFormat, aOrigin);
  }
  return false;
}

void DrawTargetWebgl::ReleaseBits(uint8_t* aData) {
  // Can only access pixels if there is valid, flattened Skia data.
  if (mSkiaValid && !mSkiaLayer) {
    mSkia->ReleaseBits(aData);
  }
}

// Attempts to create all shaders and resources to be used for drawing commands.
// Returns whether or not this succeeded.
bool DrawTargetWebgl::SharedContext::CreateShaders() {
  if (!mVertexArray) {
    mVertexArray = mWebgl->CreateVertexArray();
  }
  if (!mVertexBuffer) {
    mVertexBuffer = mWebgl->CreateBuffer();
    static const float rectData[8] = {0.0f, 0.0f, 1.0f, 0.0f,
                                      1.0f, 1.0f, 0.0f, 1.0f};
    mWebgl->BindVertexArray(mVertexArray.get());
    mWebgl->BindBuffer(LOCAL_GL_ARRAY_BUFFER, mVertexBuffer.get());
    mWebgl->RawBufferData(LOCAL_GL_ARRAY_BUFFER,
                          {(const uint8_t*)rectData, sizeof(rectData)},
                          LOCAL_GL_STATIC_DRAW);
    mWebgl->EnableVertexAttribArray(0);
    mWebgl->VertexAttribPointer(0, 2, LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0, 0);
  }
  if (!mSolidProgram) {
    auto vsSource =
        u"attribute vec2 a_vertex;\n"
        "uniform vec2 u_transform[3];\n"
        "void main() {\n"
        "   vec2 vertex = u_transform[0] * a_vertex.x +\n"
        "                 u_transform[1] * a_vertex.y +\n"
        "                 u_transform[2];\n"
        "   gl_Position = vec4(vertex, 0.0, 1.0);\n"
        "}\n"_ns;
    auto fsSource =
        u"precision mediump float;\n"
        "uniform vec4 u_color;\n"
        "void main() {\n"
        "   gl_FragColor = u_color;\n"
        "}\n"_ns;
    RefPtr<WebGLShaderJS> vsId = mWebgl->CreateShader(LOCAL_GL_VERTEX_SHADER);
    mWebgl->ShaderSource(*vsId, vsSource);
    mWebgl->CompileShader(*vsId);
    if (!mWebgl->GetCompileResult(*vsId).success) {
      return false;
    }
    RefPtr<WebGLShaderJS> fsId = mWebgl->CreateShader(LOCAL_GL_FRAGMENT_SHADER);
    mWebgl->ShaderSource(*fsId, fsSource);
    mWebgl->CompileShader(*fsId);
    if (!mWebgl->GetCompileResult(*fsId).success) {
      return false;
    }
    mSolidProgram = mWebgl->CreateProgram();
    mWebgl->AttachShader(*mSolidProgram, *vsId);
    mWebgl->AttachShader(*mSolidProgram, *fsId);
    mWebgl->BindAttribLocation(*mSolidProgram, 0, u"a_vertex"_ns);
    mWebgl->LinkProgram(*mSolidProgram);
    if (!mWebgl->GetLinkResult(*mSolidProgram).success) {
      return false;
    }
    mSolidProgramTransform =
        mWebgl->GetUniformLocation(*mSolidProgram, u"u_transform"_ns);
    mSolidProgramColor =
        mWebgl->GetUniformLocation(*mSolidProgram, u"u_color"_ns);
    if (!mSolidProgramTransform || !mSolidProgramColor) {
      return false;
    }
  }

  if (!mImageProgram) {
    auto vsSource =
        u"attribute vec2 a_vertex;\n"
        "varying vec2 v_texcoord;\n"
        "uniform vec2 u_transform[3];\n"
        "uniform vec2 u_texmatrix[3];\n"
        "void main() {\n"
        "   vec2 vertex = u_transform[0] * a_vertex.x +\n"
        "                 u_transform[1] * a_vertex.y +\n"
        "                 u_transform[2];\n"
        "   gl_Position = vec4(vertex, 0.0, 1.0);\n"
        "   v_texcoord = u_texmatrix[0] * a_vertex.x +\n"
        "                u_texmatrix[1] * a_vertex.y +\n"
        "                u_texmatrix[2];\n"
        "}\n"_ns;
    auto fsSource =
        u"precision mediump float;\n"
        "varying vec2 v_texcoord;\n"
        "uniform vec4 u_texbounds;\n"
        "uniform vec4 u_color;\n"
        "uniform float u_swizzle;\n"
        "uniform sampler2D u_sampler;\n"
        "void main() {\n"
        "   vec2 tc = clamp(v_texcoord, u_texbounds.xy, u_texbounds.zw);\n"
        "   vec4 image = texture2D(u_sampler, tc);\n"
        "   gl_FragColor = u_color * mix(image, image.rrrr, u_swizzle);\n"
        "}\n"_ns;
    RefPtr<WebGLShaderJS> vsId = mWebgl->CreateShader(LOCAL_GL_VERTEX_SHADER);
    mWebgl->ShaderSource(*vsId, vsSource);
    mWebgl->CompileShader(*vsId);
    if (!mWebgl->GetCompileResult(*vsId).success) {
      return false;
    }
    RefPtr<WebGLShaderJS> fsId = mWebgl->CreateShader(LOCAL_GL_FRAGMENT_SHADER);
    mWebgl->ShaderSource(*fsId, fsSource);
    mWebgl->CompileShader(*fsId);
    if (!mWebgl->GetCompileResult(*fsId).success) {
      return false;
    }
    mImageProgram = mWebgl->CreateProgram();
    mWebgl->AttachShader(*mImageProgram, *vsId);
    mWebgl->AttachShader(*mImageProgram, *fsId);
    mWebgl->BindAttribLocation(*mImageProgram, 0, u"a_vertex"_ns);
    mWebgl->LinkProgram(*mImageProgram);
    if (!mWebgl->GetLinkResult(*mImageProgram).success) {
      return false;
    }
    mImageProgramTransform =
        mWebgl->GetUniformLocation(*mImageProgram, u"u_transform"_ns);
    mImageProgramTexMatrix =
        mWebgl->GetUniformLocation(*mImageProgram, u"u_texmatrix"_ns);
    mImageProgramTexBounds =
        mWebgl->GetUniformLocation(*mImageProgram, u"u_texbounds"_ns);
    mImageProgramSwizzle =
        mWebgl->GetUniformLocation(*mImageProgram, u"u_swizzle"_ns);
    mImageProgramColor =
        mWebgl->GetUniformLocation(*mImageProgram, u"u_color"_ns);
    mImageProgramSampler =
        mWebgl->GetUniformLocation(*mImageProgram, u"u_sampler"_ns);
    if (!mImageProgramTransform || !mImageProgramTexMatrix ||
        !mImageProgramTexBounds || !mImageProgramSwizzle ||
        !mImageProgramColor || !mImageProgramSampler) {
      return false;
    }
    mWebgl->UseProgram(mImageProgram);
    int32_t samplerData = 0;
    mWebgl->UniformData(LOCAL_GL_INT, mImageProgramSampler, false,
                        {(const uint8_t*)&samplerData, sizeof(samplerData)});
  }
  return true;
}

void DrawTargetWebgl::ClearRect(const Rect& aRect) {
  ColorPattern pattern(
      DeviceColor(0.0f, 0.0f, 0.0f, IsOpaque(mFormat) ? 1.0f : 0.0f));
  DrawRect(aRect, pattern, DrawOptions(1.0f, CompositionOp::OP_SOURCE));
}

// Attempts to create the framebuffer used for drawing and also any relevant
// non-shared resources. Returns whether or not this succeeded.
bool DrawTargetWebgl::CreateFramebuffer() {
  RefPtr<ClientWebGLContext> webgl = mSharedContext->mWebgl;
  if (!mFramebuffer) {
    mFramebuffer = webgl->CreateFramebuffer();
  }
  if (!mTex) {
    mTex = webgl->CreateTexture();
    webgl->BindTexture(LOCAL_GL_TEXTURE_2D, mTex);
    webgl->TexStorage2D(LOCAL_GL_TEXTURE_2D, 1, LOCAL_GL_RGBA8, mSize.width,
                        mSize.height);
    mSharedContext->InitTexParameters(mTex);
    webgl->BindFramebuffer(LOCAL_GL_FRAMEBUFFER, mFramebuffer);
    webgl->FramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                                LOCAL_GL_COLOR_ATTACHMENT0, LOCAL_GL_TEXTURE_2D,
                                mTex, 0);
    webgl->Viewport(0, 0, mSize.width, mSize.height);
    webgl->ClearColor(0.0f, 0.0f, 0.0f, IsOpaque(mFormat) ? 1.0f : 0.0f);
    webgl->Clear(LOCAL_GL_COLOR_BUFFER_BIT);
    mSharedContext->ClearTarget();
    mSharedContext->ClearLastTexture();
  }
  return true;
}

void DrawTargetWebgl::CopySurface(SourceSurface* aSurface,
                                  const IntRect& aSourceRect,
                                  const IntPoint& aDestination) {
  if (mSkiaValid) {
    if (mSkiaLayer) {
      if (IntRect(aDestination, aSourceRect.Size()).Contains(GetRect())) {
        // If the the destination would override the entire layer, discard the
        // layer.
        mSkiaLayer = false;
      } else if (!IsOpaque(aSurface->GetFormat())) {
        // If the surface is not opaque, copying it into the layer results in
        // unintended blending rather than a copy to the destination.
        FlattenSkia();
      }
    } else {
      // If there is no layer, copying is safe.
      MarkSkiaChanged();
    }
    mSkia->CopySurface(aSurface, aSourceRect, aDestination);
    return;
  }

  Matrix matrix = Matrix::Translation(aDestination - aSourceRect.TopLeft());
  SurfacePattern pattern(aSurface, ExtendMode::CLAMP, matrix);
  DrawRect(Rect(IntRect(aDestination, aSourceRect.Size())), pattern,
           DrawOptions(1.0f, CompositionOp::OP_SOURCE), Nothing(), nullptr,
           false, false);
}

void DrawTargetWebgl::PushClip(const Path* aPath) {
  mClipDirty = true;
  mSkia->PushClip(aPath);
}

void DrawTargetWebgl::PushClipRect(const Rect& aRect) {
  mClipDirty = true;
  mSkia->PushClipRect(aRect);
}

void DrawTargetWebgl::PushDeviceSpaceClipRects(const IntRect* aRects,
                                               uint32_t aCount) {
  mClipDirty = true;
  mSkia->PushDeviceSpaceClipRects(aRects, aCount);
}

void DrawTargetWebgl::PopClip() {
  mClipDirty = true;
  mSkia->PopClip();
}

// Whether a given composition operator can be mapped to a WebGL blend mode.
static inline bool SupportsDrawOptions(const DrawOptions& aOptions) {
  switch (aOptions.mCompositionOp) {
    case CompositionOp::OP_OVER:
    case CompositionOp::OP_ADD:
    case CompositionOp::OP_ATOP:
    case CompositionOp::OP_SOURCE:
      return true;
    default:
      return false;
  }
}

// Whether a pattern can be mapped to an available WebGL shader.
bool DrawTargetWebgl::SharedContext::SupportsPattern(const Pattern& aPattern) {
  switch (aPattern.GetType()) {
    case PatternType::COLOR:
      return true;
    case PatternType::SURFACE: {
      auto surfacePattern = static_cast<const SurfacePattern&>(aPattern);
      if (surfacePattern.mSurface) {
        IntSize size = surfacePattern.mSurface->GetSize();
        // The maximum size a surface can be before triggering a fallback to
        // software. Bound the maximum surface size by the actual texture size
        // limit.
        int32_t maxSize = int32_t(
            std::min(StaticPrefs::gfx_canvas_accelerated_max_surface_size(),
                     mMaxTextureSize));
        // Check if either of the surface dimensions or the sampling rect,
        // if supplied, exceed the maximum.
        if (std::max(size.width, size.height) > maxSize &&
            (surfacePattern.mSamplingRect.IsEmpty() ||
             std::max(surfacePattern.mSamplingRect.width,
                      surfacePattern.mSamplingRect.height) > maxSize)) {
          return false;
        }
      }
      return true;
    }
    default:
      // Patterns other than colors and surfaces are currently not accelerated.
      return false;
  }
}

// When a texture handle is no longer referenced, it must mark itself unused
// by unlinking its owning surface.
static void ReleaseTextureHandle(void* aPtr) {
  static_cast<TextureHandle*>(aPtr)->SetSurface(nullptr);
}

bool DrawTargetWebgl::DrawRect(const Rect& aRect, const Pattern& aPattern,
                               const DrawOptions& aOptions,
                               Maybe<DeviceColor> aMaskColor,
                               RefPtr<TextureHandle>* aHandle,
                               bool aTransformed, bool aClipped,
                               bool aAccelOnly, bool aForceUpdate,
                               const StrokeOptions* aStrokeOptions) {
  // If there is nothing to draw, then don't draw...
  if (aRect.IsEmpty()) {
    return true;
  }

  // If the shared context couldn't be claimed, then go directly to fallback
  // drawing. If the WebGL context isn't up-to-date and can't be made so by
  // flattening a layer to it, then go to the fallback to avoid expensive
  // uploads.
  if ((!mWebglValid && !mSkiaLayer) || !PrepareContext(aClipped)) {
    if (!aAccelOnly) {
      DrawRectFallback(aRect, aPattern, aOptions, aMaskColor, aTransformed,
                       aClipped, aStrokeOptions);
    }
    return false;
  }

  // The shared context is claimed and the framebuffer is now valid, so try
  // accelerated drawing.
  return mSharedContext->DrawRectAccel(
      aRect, aPattern, aOptions, aMaskColor, aHandle, aTransformed, aClipped,
      aAccelOnly, aForceUpdate, aStrokeOptions);
}

void DrawTargetWebgl::DrawRectFallback(const Rect& aRect,
                                       const Pattern& aPattern,
                                       const DrawOptions& aOptions,
                                       Maybe<DeviceColor> aMaskColor,
                                       bool aTransformed, bool aClipped,
                                       const StrokeOptions* aStrokeOptions) {
  // Invalidate the WebGL target and prepare the Skia target for drawing.
  MarkSkiaChanged(aOptions);

  if (aTransformed) {
    // If transforms are requested, then just translate back to FillRect.
    if (aMaskColor) {
      mSkia->Mask(ColorPattern(*aMaskColor), aPattern, aOptions);
    } else if (aStrokeOptions) {
      mSkia->StrokeRect(aRect, aPattern, *aStrokeOptions, aOptions);
    } else {
      mSkia->FillRect(aRect, aPattern, aOptions);
    }
  } else if (aClipped) {
    // If no transform was requested but clipping is still required, then
    // temporarily reset the transform before translating to FillRect.
    mSkia->SetTransform(Matrix());
    if (aMaskColor) {
      auto surfacePattern = static_cast<const SurfacePattern&>(aPattern);
      if (surfacePattern.mSamplingRect.IsEmpty()) {
        mSkia->MaskSurface(ColorPattern(*aMaskColor), surfacePattern.mSurface,
                           aRect.TopLeft(), aOptions);
      } else {
        mSkia->Mask(ColorPattern(*aMaskColor), aPattern, aOptions);
      }
    } else if (aStrokeOptions) {
      mSkia->StrokeRect(aRect, aPattern, *aStrokeOptions, aOptions);
    } else {
      mSkia->FillRect(aRect, aPattern, aOptions);
    }
    mSkia->SetTransform(mTransform);
  } else if (aPattern.GetType() == PatternType::SURFACE) {
    // No transform nor clipping was requested, so it is essentially just a
    // copy.
    auto surfacePattern = static_cast<const SurfacePattern&>(aPattern);
    mSkia->CopySurface(surfacePattern.mSurface,
                       surfacePattern.mSurface->GetRect(),
                       IntPoint::Round(aRect.TopLeft()));
  } else {
    MOZ_ASSERT(false);
  }
}

inline already_AddRefed<WebGLTextureJS>
DrawTargetWebgl::SharedContext::GetCompatibleSnapshot(SourceSurface* aSurface) {
  if (aSurface->GetType() == SurfaceType::WEBGL) {
    RefPtr<SourceSurfaceWebgl> webglSurf =
        static_cast<SourceSurfaceWebgl*>(aSurface);
    if (this == webglSurf->mSharedContext) {
      // If there is a snapshot copy in a texture handle, use that.
      if (webglSurf->mHandle) {
        return do_AddRef(webglSurf->mHandle->GetWebGLTexture());
      }
      if (RefPtr<DrawTargetWebgl> webglDT = webglSurf->GetTarget()) {
        // If there is a copy-on-write reference to a target, use its backing
        // texture directly. This is only safe if the targets don't match, but
        // MarkChanged should ensure that any snapshots were copied into a
        // texture handle before we ever get here.
        if (!IsCurrentTarget(webglDT)) {
          return do_AddRef(webglDT->mTex);
        }
      }
    }
  }
  return nullptr;
}

bool DrawTargetWebgl::SharedContext::UploadSurface(DataSourceSurface* aData,
                                                   SurfaceFormat aFormat,
                                                   const IntRect& aSrcRect,
                                                   const IntPoint& aDstOffset,
                                                   bool aInit) {
  webgl::TexUnpackBlobDesc texDesc = {
      LOCAL_GL_TEXTURE_2D,
      {uint32_t(aSrcRect.width), uint32_t(aSrcRect.height), 1}};
  if (aData) {
    // The surface needs to be uploaded to its backing texture either to
    // initialize or update the texture handle contents. Map the data
    // contents of the surface so it can be read.
    DataSourceSurface::ScopedMap map(aData, DataSourceSurface::READ);
    if (!map.IsMapped()) {
      return false;
    }
    int32_t stride = map.GetStride();
    int32_t bpp = BytesPerPixel(aFormat);
    // Get the data pointer range considering the sampling rect offset and
    // size.
    Range<const uint8_t> range(
        map.GetData() + aSrcRect.y * size_t(stride) + aSrcRect.x * bpp,
        std::max(aSrcRect.height - 1, 0) * size_t(stride) +
            aSrcRect.width * bpp);
    texDesc.cpuData = Some(RawBuffer(range));
    // If the stride happens to be 4 byte aligned, assume that is the
    // desired alignment regardless of format (even A8). Otherwise, we
    // default to byte alignment.
    texDesc.unpacking.alignmentInTypeElems = stride % 4 ? 1 : 4;
    texDesc.unpacking.rowLength = stride / bpp;
    texDesc.unpacking.imageHeight = aSrcRect.height;
  }
  // Upload as RGBA8 to avoid swizzling during upload. Surfaces provide
  // data as BGRA, but we manually swizzle that in the shader. An A8
  // surface will be stored as an R8 texture that will also be swizzled
  // in the shader.
  GLenum intFormat =
      aFormat == SurfaceFormat::A8 ? LOCAL_GL_R8 : LOCAL_GL_RGBA8;
  GLenum extFormat =
      aFormat == SurfaceFormat::A8 ? LOCAL_GL_RED : LOCAL_GL_RGBA;
  webgl::PackingInfo texPI = {extFormat, LOCAL_GL_UNSIGNED_BYTE};
  // Do the (partial) upload for the shared or standalone texture.
  mWebgl->RawTexImage(0, aInit ? intFormat : 0,
                      {uint32_t(aDstOffset.x), uint32_t(aDstOffset.y), 0},
                      texPI, texDesc);
  return true;
}

// Common rectangle and pattern drawing function shared by many DrawTarget
// commands. If aMaskColor is specified, the provided surface pattern will be
// treated as a mask. If aHandle is specified, then the surface pattern's
// texture will be cached in the supplied handle, as opposed to using the
// surface's user data. If aTransformed or aClipped are false, then transforms
// and/or clipping will be disabled. If aAccelOnly is specified, then this
// function will return before it would have otherwise drawn without
// acceleration. If aForceUpdate is specified, then the provided texture handle
// will be respecified with the provided surface.
bool DrawTargetWebgl::SharedContext::DrawRectAccel(
    const Rect& aRect, const Pattern& aPattern, const DrawOptions& aOptions,
    Maybe<DeviceColor> aMaskColor, RefPtr<TextureHandle>* aHandle,
    bool aTransformed, bool aClipped, bool aAccelOnly, bool aForceUpdate,
    const StrokeOptions* aStrokeOptions) {
  // If the rect or clip rect is empty, then there is nothing to draw.
  if (aRect.IsEmpty() || mClipRect.IsEmpty()) {
    return true;
  }

  // Check if the drawing options and the pattern support acceleration. Also
  // ensure the framebuffer is prepared for drawing. If not, fall back to using
  // the Skia target.
  if (!SupportsDrawOptions(aOptions) || !SupportsPattern(aPattern) ||
      !mCurrentTarget->MarkChanged()) {
    // If only accelerated drawing was requested, bail out without software
    // drawing fallback.
    if (!aAccelOnly) {
      mCurrentTarget->DrawRectFallback(aRect, aPattern, aOptions, aMaskColor,
                                       aTransformed, aClipped, aStrokeOptions);
    }
    return false;
  }

  // Map the composition op to a WebGL blend mode, if possible.
  SetBlendState(aOptions.mCompositionOp);

  // Set up the scissor test to reflect the clipping rectangle, if supplied.
  bool scissor = false;
  if (!mClipRect.Contains(IntRect(IntPoint(), mViewportSize))) {
    scissor = true;
    mWebgl->Enable(LOCAL_GL_SCISSOR_TEST);
    mWebgl->Scissor(mClipRect.x, mClipRect.y, mClipRect.width,
                    mClipRect.height);
  }

  const Matrix& currentTransform = GetTransform();
  bool success = false;

  // Now try to actually draw the pattern...
  switch (aPattern.GetType()) {
    case PatternType::COLOR: {
      auto color = static_cast<const ColorPattern&>(aPattern).mColor;
      if (((color.a * aOptions.mAlpha == 1.0f &&
            aOptions.mCompositionOp == CompositionOp::OP_OVER) ||
           aOptions.mCompositionOp == CompositionOp::OP_SOURCE) &&
          (!aTransformed || currentTransform.HasOnlyIntegerTranslation()) &&
          !aStrokeOptions) {
        // Certain color patterns can be mapped to scissored cleared. The
        // composition op must effectively overwrite the destination, and the
        // transform must map to an axis-aligned integer rectangle.
        auto intRect = RoundedToInt(aRect);
        if (aRect.WithinEpsilonOf(Rect(intRect), 1.0e-3f)) {
          if (aTransformed) {
            intRect += RoundedToInt(currentTransform.GetTranslation());
          }
          if (!intRect.Contains(mClipRect)) {
            scissor = true;
            mWebgl->Enable(LOCAL_GL_SCISSOR_TEST);
            intRect = intRect.Intersect(mClipRect);
            mWebgl->Scissor(intRect.x, intRect.y, intRect.width,
                            intRect.height);
          }
          float a = color.a * aOptions.mAlpha;
          mWebgl->ClearColor(color.b * a, color.g * a, color.r * a, a);
          mWebgl->Clear(LOCAL_GL_COLOR_BUFFER_BIT);
          success = true;
          break;
        }
      }
      // Since it couldn't be mapped to a scissored clear, we need to use the
      // solid color shader with supplied transform.
      if (mLastProgram != mSolidProgram) {
        mWebgl->UseProgram(mSolidProgram);
        mLastProgram = mSolidProgram;
      }
      float a = color.a * aOptions.mAlpha;
      float colorData[4] = {color.b * a, color.g * a, color.r * a, a};
      Matrix xform(aRect.width, 0.0f, 0.0f, aRect.height, aRect.x, aRect.y);
      if (aTransformed) {
        xform *= currentTransform;
      }
      xform *= Matrix(2.0f / float(mViewportSize.width), 0.0f, 0.0f,
                      2.0f / float(mViewportSize.height), -1, -1);
      float xformData[6] = {xform._11, xform._12, xform._21,
                            xform._22, xform._31, xform._32};
      mWebgl->UniformData(LOCAL_GL_FLOAT_VEC2, mSolidProgramTransform, false,
                          {(const uint8_t*)xformData, sizeof(xformData)});
      mWebgl->UniformData(LOCAL_GL_FLOAT_VEC4, mSolidProgramColor, false,
                          {(const uint8_t*)colorData, sizeof(colorData)});
      // Finally draw the colored rectangle.
      mWebgl->DrawArrays(
          aStrokeOptions ? LOCAL_GL_LINE_LOOP : LOCAL_GL_TRIANGLE_FAN, 0, 4);
      success = true;
      break;
    }
    case PatternType::SURFACE: {
      auto surfacePattern = static_cast<const SurfacePattern&>(aPattern);
      // If a texture handle was supplied, or if the surface already has an
      // assigned texture handle stashed in its used data, try to use it.
      RefPtr<TextureHandle> handle =
          aHandle ? aHandle->get()
                  : (surfacePattern.mSurface
                         ? static_cast<TextureHandle*>(
                               surfacePattern.mSurface->GetUserData(
                                   &mTextureHandleKey))
                         : nullptr);
      IntSize texSize;
      IntPoint offset;
      SurfaceFormat format;
      // Check if the found handle is still valid and if its sampling rect
      // matches the requested sampling rect.
      if (handle && handle->IsValid() &&
          (surfacePattern.mSamplingRect.IsEmpty() ||
           handle->GetSamplingRect().IsEqualEdges(
               surfacePattern.mSamplingRect))) {
        texSize = handle->GetSize();
        format = handle->GetFormat();
        offset = handle->GetSamplingOffset();
      } else {
        // Otherwise, there is no handle that can be used yet, so extract
        // information from the surface pattern.
        handle = nullptr;
        if (!surfacePattern.mSurface) {
          // If there was no actual surface supplied, then we tried to draw
          // using a texture handle, but the texture handle wasn't valid.
          break;
        }
        texSize = surfacePattern.mSurface->GetSize();
        format = surfacePattern.mSurface->GetFormat();
        if (!surfacePattern.mSamplingRect.IsEmpty()) {
          texSize = surfacePattern.mSamplingRect.Size();
          offset = surfacePattern.mSamplingRect.TopLeft();
        }
      }

      RefPtr<WebGLTextureJS> tex;
      IntRect bounds;
      IntSize backingSize;
      RefPtr<DataSourceSurface> data;
      bool init = false;
      if (handle) {
        if (aForceUpdate) {
          data = surfacePattern.mSurface->GetDataSurface();
          if (!data) {
            break;
          }
          // The size of the texture may change if we update contents.
          mUsedTextureMemory -= handle->UsedBytes();
          handle->UpdateSize(texSize);
          mUsedTextureMemory += handle->UsedBytes();
          handle->SetSamplingOffset(surfacePattern.mSamplingRect.TopLeft());
        }
        // If using an existing handle, move it to the front of the MRU list.
        handle->remove();
        mTextureHandles.insertFront(handle);
      } else if ((tex = GetCompatibleSnapshot(surfacePattern.mSurface))) {
        backingSize = surfacePattern.mSurface->GetSize();
        bounds = IntRect(offset, texSize);
      } else {
        // If we get here, we need a data surface for a texture upload.
        data = surfacePattern.mSurface->GetDataSurface();
        if (!data) {
          break;
        }
        // If the surface is mapped to an axis-aligned rectangle with no scale,
        // then it's possible to do a texture upload directly to the
        // framebuffer. This is only safe if there is no blending enabled.
        if ((aOptions.mCompositionOp == CompositionOp::OP_SOURCE ||
             (aOptions.mCompositionOp == CompositionOp::OP_OVER &&
              format == SurfaceFormat::B8G8R8X8)) &&
            aOptions.mAlpha == 1.0f &&
            (!aTransformed || currentTransform.HasOnlyIntegerTranslation()) &&
            surfacePattern.mMatrix.HasOnlyIntegerTranslation() &&
            !aStrokeOptions && !aMaskColor) {
          auto intRect = RoundedToInt(aRect);
          if (aRect.WithinEpsilonOf(Rect(intRect), 1.0e-3f)) {
            if (aTransformed) {
              intRect += RoundedToInt(currentTransform.GetTranslation());
            }
            intRect = intRect.Intersect(mClipRect);
            auto destOffset =
                RoundedToInt(surfacePattern.mMatrix.GetTranslation());
            bounds = (IntRect(offset, texSize) + destOffset).Intersect(intRect);
            if (bounds.IsEmpty()) {
              success = true;
              break;
            }
            if (mLastTexture != mCurrentTarget->mTex) {
              mWebgl->BindTexture(LOCAL_GL_TEXTURE_2D, mCurrentTarget->mTex);
              mLastTexture = mCurrentTarget->mTex;
            }
            if (UploadSurface(data, format, bounds - destOffset,
                              bounds.TopLeft(), false)) {
              success = true;
            }
            break;
          }
        }
        // There is no existing handle. Calculate the bytes that would be used
        // by this texture, and prune enough other textures to ensure we have
        // that much usable texture space available to allocate.
        size_t usedBytes = TextureHandle::UsedBytes(format, texSize);
        PruneTextureMemory(usedBytes, false);
        // The requested page size for shared textures.
        int32_t pageSize = int32_t(
            std::min(StaticPrefs::gfx_canvas_accelerated_shared_page_size(),
                     mMaxTextureSize));
        if (!aForceUpdate &&
            std::max(texSize.width, texSize.height) <= pageSize / 2) {
          // Ensure that the surface size won't change via forced update and
          // that the surface is no bigger than a quadrant of a shared texture
          // page. If so, try to allocate it to a shared texture. Look for any
          // existing shared texture page with a matching format and allocate
          // from that if possible.
          for (auto& shared : mSharedTextures) {
            if (shared->GetFormat() == format) {
              handle = shared->Allocate(texSize);
              if (handle) {
                break;
              }
            }
          }
          // If we couldn't find an existing shared texture page with matching
          // format, then allocate a new page to put the request in.
          if (!handle) {
            tex = mWebgl->CreateTexture();
            if (!tex) {
              MOZ_ASSERT(false);
              break;
            }
            RefPtr<SharedTexture> shared =
                new SharedTexture(IntSize(pageSize, pageSize), format, tex);
            mSharedTextures.push_back(shared);
            mTotalTextureMemory += shared->UsedBytes();
            handle = shared->Allocate(texSize);
            if (!handle) {
              MOZ_ASSERT(false);
              break;
            }
            init = true;
          }
        } else {
          // The surface wouldn't fit in a shared texture page, so we need to
          // allocate a standalone texture for it instead.
          tex = mWebgl->CreateTexture();
          if (!tex) {
            MOZ_ASSERT(false);
            break;
          }
          RefPtr<StandaloneTexture> standalone =
              new StandaloneTexture(texSize, format, tex);
          mStandaloneTextures.push_back(standalone);
          mTotalTextureMemory += standalone->UsedBytes();
          handle = standalone;
          init = true;
        }

        // Insert the new texture handle into the front of the MRU list and
        // update used space for it.
        mTextureHandles.insertFront(handle);
        ++mNumTextureHandles;
        mUsedTextureMemory += handle->UsedBytes();
        // Link the handle to the surface's user data.
        handle->SetSamplingOffset(surfacePattern.mSamplingRect.TopLeft());
        if (aHandle) {
          *aHandle = handle;
        } else {
          handle->SetSurface(surfacePattern.mSurface);
          surfacePattern.mSurface->AddUserData(&mTextureHandleKey, handle.get(),
                                               ReleaseTextureHandle);
        }
      }

      // Switch to the image shader and set up relevant transforms.
      if (mLastProgram != mImageProgram) {
        mWebgl->UseProgram(mImageProgram);
        mLastProgram = mImageProgram;
      }
      DeviceColor color = aMaskColor.valueOr(DeviceColor(1, 1, 1, 1));
      float a = color.a * aOptions.mAlpha;
      float colorData[4] = {color.b * a, color.g * a, color.r * a, a};
      float swizzleData = aMaskColor ? 1.0f : 0.0f;
      Matrix xform(aRect.width, 0.0f, 0.0f, aRect.height, aRect.x, aRect.y);
      if (aTransformed) {
        xform *= currentTransform;
      }
      xform *= Matrix(2.0f / float(mViewportSize.width), 0.0f, 0.0f,
                      2.0f / float(mViewportSize.height), -1, -1);
      float xformData[6] = {xform._11, xform._12, xform._21,
                            xform._22, xform._31, xform._32};
      mWebgl->UniformData(LOCAL_GL_FLOAT_VEC2, mImageProgramTransform, false,
                          {(const uint8_t*)xformData, sizeof(xformData)});
      mWebgl->UniformData(LOCAL_GL_FLOAT_VEC4, mImageProgramColor, false,
                          {(const uint8_t*)colorData, sizeof(colorData)});
      mWebgl->UniformData(LOCAL_GL_FLOAT, mImageProgramSwizzle, false,
                          {(const uint8_t*)&swizzleData, sizeof(swizzleData)});

      // Start binding the WebGL state for the texture.
      if (handle) {
        if (!tex) {
          tex = handle->GetWebGLTexture();
        }
        bounds = handle->GetBounds();
        backingSize = handle->GetBackingSize();
      }
      if (mLastTexture != tex) {
        mWebgl->BindTexture(LOCAL_GL_TEXTURE_2D, tex);
        mLastTexture = tex;
      }

      if (init) {
        // If this is the first time the texture is used, we need to initialize
        // the clamping and filtering state.
        InitTexParameters(tex);
        if (texSize != backingSize) {
          // If this is a shared texture handle whose actual backing texture is
          // larger than it, then we need to allocate the texture page to the
          // full backing size before we can do a partial upload of the surface.
          UploadSurface(nullptr, format, IntRect(IntPoint(), backingSize),
                        IntPoint(), true);
        }
      }

      if (data) {
        UploadSurface(data, format, IntRect(offset, texSize), bounds.TopLeft(),
                      texSize == backingSize);
      }

      // Set up the texture coordinate matrix to map from the input rectangle to
      // the backing texture subrect.
      Size backingSizeF(backingSize);
      Matrix uvMatrix(aRect.width, 0.0f, 0.0f, aRect.height, aRect.x, aRect.y);
      uvMatrix *= surfacePattern.mMatrix.Inverse();
      uvMatrix *= Matrix(1.0f / backingSizeF.width, 0.0f, 0.0f,
                         1.0f / backingSizeF.height,
                         float(bounds.x - offset.x) / backingSizeF.width,
                         float(bounds.y - offset.y) / backingSizeF.height);
      float uvData[6] = {uvMatrix._11, uvMatrix._12, uvMatrix._21,
                         uvMatrix._22, uvMatrix._31, uvMatrix._32};
      mWebgl->UniformData(LOCAL_GL_FLOAT_VEC2, mImageProgramTexMatrix, false,
                          {(const uint8_t*)uvData, sizeof(uvData)});

      // Clamp sampling to within the bounds of the backing texture subrect.
      float texBounds[4] = {
          (bounds.x + 0.5f) / backingSizeF.width,
          (bounds.y + 0.5f) / backingSizeF.height,
          (bounds.XMost() - 0.5f) / backingSizeF.width,
          (bounds.YMost() - 0.5f) / backingSizeF.height,
      };
      mWebgl->UniformData(LOCAL_GL_FLOAT_VEC4, mImageProgramTexBounds, false,
                          {(const uint8_t*)texBounds, sizeof(texBounds)});

      // Finally draw the image rectangle.
      mWebgl->DrawArrays(
          aStrokeOptions ? LOCAL_GL_LINE_LOOP : LOCAL_GL_TRIANGLE_FAN, 0, 4);
      success = true;
      break;
    }
    default:
      gfxWarning() << "Unknown DrawTargetWebgl::DrawRect pattern type: "
                   << (int)aPattern.GetType();
      break;
  }
  // mWebgl->Disable(LOCAL_GL_BLEND);

  // Clean up any scissor state if there was clipping.
  if (scissor) {
    mWebgl->Disable(LOCAL_GL_SCISSOR_TEST);
  }
  return success;
}

bool DrawTargetWebgl::SharedContext::RemoveSharedTexture(
    const RefPtr<SharedTexture>& aTexture) {
  auto pos =
      std::find(mSharedTextures.begin(), mSharedTextures.end(), aTexture);
  if (pos == mSharedTextures.end()) {
    return false;
  }
  mTotalTextureMemory -= aTexture->UsedBytes();
  mSharedTextures.erase(pos);
  ClearLastTexture();
  return true;
}

void SharedTextureHandle::Cleanup(DrawTargetWebgl::SharedContext& aContext) {
  mTexture->Free(*this);

  // Check if the shared handle's owning page has no more allocated handles
  // after we freed it. If so, remove the empty shared texture page also.
  if (!mTexture->HasAllocatedHandles()) {
    aContext.RemoveSharedTexture(mTexture);
  }
}

bool DrawTargetWebgl::SharedContext::RemoveStandaloneTexture(
    const RefPtr<StandaloneTexture>& aTexture) {
  auto pos = std::find(mStandaloneTextures.begin(), mStandaloneTextures.end(),
                       aTexture);
  if (pos == mStandaloneTextures.end()) {
    return false;
  }
  mTotalTextureMemory -= aTexture->UsedBytes();
  mStandaloneTextures.erase(pos);
  ClearLastTexture();
  return true;
}

void StandaloneTexture::Cleanup(DrawTargetWebgl::SharedContext& aContext) {
  aContext.RemoveStandaloneTexture(this);
}

// Prune a given texture handle and release its associated resources.
void DrawTargetWebgl::SharedContext::PruneTextureHandle(
    const RefPtr<TextureHandle>& aHandle) {
  // Invalidate the handle so nothing will subsequently use its contents.
  aHandle->Invalidate();
  // If the handle has an associated SourceSurface, unlink it.
  UnlinkSurfaceTexture(aHandle);
  // If the handle has an associated CacheEntry, unlink it.
  if (RefPtr<CacheEntry> entry = aHandle->GetCacheEntry()) {
    entry->Unlink();
  }
  // Deduct the used space from the total.
  mUsedTextureMemory -= aHandle->UsedBytes();
  // Ensure any allocated shared or standalone texture regions get freed.
  aHandle->Cleanup(*this);
}

// Prune any texture memory above the limit (or margin below the limit) or any
// least-recently-used handles that are no longer associated with any usable
// surface.
bool DrawTargetWebgl::SharedContext::PruneTextureMemory(size_t aMargin,
                                                        bool aPruneUnused) {
  // The maximum amount of texture memory that may be used by textures.
  size_t maxBytes = StaticPrefs::gfx_canvas_accelerated_cache_size() << 20;
  maxBytes -= std::min(maxBytes, aMargin);
  size_t maxItems = StaticPrefs::gfx_canvas_accelerated_cache_items();
  size_t oldItems = mNumTextureHandles;
  while (!mTextureHandles.isEmpty() &&
         (mUsedTextureMemory > maxBytes || mNumTextureHandles > maxItems ||
          (aPruneUnused && !mTextureHandles.getLast()->IsUsed()))) {
    PruneTextureHandle(mTextureHandles.popLast());
    --mNumTextureHandles;
  }
  return mNumTextureHandles < oldItems;
}

void DrawTargetWebgl::FillRect(const Rect& aRect, const Pattern& aPattern,
                               const DrawOptions& aOptions) {
  if (SupportsPattern(aPattern)) {
    DrawRect(aRect, aPattern, aOptions);
  } else if (!mWebglValid) {
    MarkSkiaChanged(aOptions);
    mSkia->FillRect(aRect, aPattern, aOptions);
  } else {
    // If the pattern is unsupported, then transform the rect to a path so it
    // can be cached.
    SkPath skiaPath;
    skiaPath.addRect(RectToSkRect(aRect));
    RefPtr<PathSkia> path = new PathSkia(skiaPath, FillRule::FILL_WINDING);
    DrawPath(path, aPattern, aOptions);
  }
}

void CacheEntry::Link(const RefPtr<TextureHandle>& aHandle) {
  mHandle = aHandle;
  mHandle->SetCacheEntry(this);
}

// When the CacheEntry becomes unused, it marks the corresponding
// TextureHandle as unused and unlinks it from the CacheEntry. The
// entry is removed from its containing Cache, if applicable.
void CacheEntry::Unlink() {
  RemoveFromList();

  // The entry may not have a valid handle if rasterization failed.
  if (mHandle) {
    mHandle->SetCacheEntry(nullptr);
    mHandle = nullptr;
  }
}

// Hashes a path and pattern to a single hash value that can be used for quick
// comparisons. This currently avoids to expensive hashing of internal path
// and pattern data for speed, relying instead on later exact comparisons for
// disambiguation.
HashNumber PathCacheEntry::HashPath(const SkPath& aPath,
                                    const Pattern* aPattern,
                                    const Matrix& aTransform,
                                    const IntRect& aBounds) {
  HashNumber hash = 0;
  hash = AddToHash(hash, aPath.countVerbs());
  hash = AddToHash(hash, aPath.countPoints());
  hash = AddToHash(hash, aBounds.width);
  hash = AddToHash(hash, aBounds.height);
  if (aPattern) {
    hash = AddToHash(hash, (int)aPattern->GetType());
  }
  return hash;
}

// When caching rendered geometry, we need to ensure the scale and orientation
// is approximately the same. The offset will be considered separately.
static inline bool HasMatchingScale(const Matrix& aTransform1,
                                    const Matrix& aTransform2) {
  return FuzzyEqual(aTransform1._11, aTransform2._11) &&
         FuzzyEqual(aTransform1._12, aTransform2._12) &&
         FuzzyEqual(aTransform1._21, aTransform2._21) &&
         FuzzyEqual(aTransform1._22, aTransform2._22);
}

// Determines if an existing path cache entry matches an incoming path and
// pattern.
bool PathCacheEntry::MatchesPath(const SkPath& aPath, const Pattern* aPattern,
                                 const StrokeOptions* aStrokeOptions,
                                 const Matrix& aTransform,
                                 const IntRect& aBounds, HashNumber aHash) {
  return aHash == mHash && HasMatchingScale(aTransform, mTransform) &&
         aBounds.Size() == mBounds.Size() && aPath == mPath &&
         (!aPattern ? !mPattern : mPattern && *aPattern == *mPattern) &&
         (!aStrokeOptions
              ? !mStrokeOptions
              : mStrokeOptions && *aStrokeOptions == *mStrokeOptions);
}

PathCacheEntry::PathCacheEntry(const SkPath& aPath, Pattern* aPattern,
                               StoredStrokeOptions* aStrokeOptions,
                               const Matrix& aTransform, const IntRect& aBounds,
                               const Point& aOrigin, HashNumber aHash)
    : CacheEntryImpl<PathCacheEntry>(aTransform, aBounds, aHash),
      mPath(aPath),
      mOrigin(aOrigin),
      mPattern(aPattern),
      mStrokeOptions(aStrokeOptions) {}

// Attempt to find a matching entry in the path cache. If one isn't found,
// a new entry will be created. The caller should check whether the contained
// texture handle is valid to determine if it will need to render the text run
// or just reuse the cached texture.
already_AddRefed<PathCacheEntry> PathCache::FindOrInsertEntry(
    const SkPath& aPath, const Pattern* aPattern,
    const StrokeOptions* aStrokeOptions, const Matrix& aTransform,
    const IntRect& aBounds, const Point& aOrigin) {
  HashNumber hash =
      PathCacheEntry::HashPath(aPath, aPattern, aTransform, aBounds);
  for (const RefPtr<PathCacheEntry>& entry : mEntries) {
    if (entry->MatchesPath(aPath, aPattern, aStrokeOptions, aTransform, aBounds,
                           hash)) {
      return do_AddRef(entry);
    }
  }
  Pattern* pattern = nullptr;
  if (aPattern) {
    pattern = aPattern->Clone();
    if (!pattern) {
      return nullptr;
    }
  }
  StoredStrokeOptions* strokeOptions = nullptr;
  if (aStrokeOptions) {
    strokeOptions = aStrokeOptions->Clone();
    if (!strokeOptions) {
      return nullptr;
    }
  }
  RefPtr<PathCacheEntry> entry = new PathCacheEntry(
      aPath, pattern, strokeOptions, aTransform, aBounds, aOrigin, hash);
  mEntries.insertFront(entry);
  return entry.forget();
}

void DrawTargetWebgl::Fill(const Path* aPath, const Pattern& aPattern,
                           const DrawOptions& aOptions) {
  if (!aPath || aPath->GetBackendType() != BackendType::SKIA) {
    return;
  }
  const SkPath& skiaPath = static_cast<const PathSkia*>(aPath)->GetPath();
  SkRect rect;
  // Draw the path as a simple rectangle with a supported pattern when possible.
  if (skiaPath.isRect(&rect) && SupportsPattern(aPattern)) {
    DrawRect(SkRectToRect(rect), aPattern, aOptions);
  } else {
    DrawPath(aPath, aPattern, aOptions);
  }
}

bool DrawTargetWebgl::SharedContext::DrawPathAccel(
    const Path* aPath, const Pattern& aPattern, const DrawOptions& aOptions,
    const StrokeOptions* aStrokeOptions) {
  // Get the transformed bounds for the path and conservatively check if the
  // bounds overlap the canvas.
  const PathSkia* pathSkia = static_cast<const PathSkia*>(aPath);
  const Matrix& currentTransform = GetTransform();
  Rect bounds = pathSkia->GetFastBounds(currentTransform, aStrokeOptions)
                    .Intersect(Rect(IntRect(IntPoint(), mViewportSize)));
  // If the path doesn't intersect the viewport, then there is nothing to draw.
  if (bounds.IsEmpty()) {
    return true;
  }
  IntRect intBounds = RoundedOut(bounds);
  // If the pattern is a solid color, then this will be used along with a path
  // mask to render the path, as opposed to baking the pattern into the cached
  // path texture.
  Maybe<DeviceColor> color =
      aPattern.GetType() == PatternType::COLOR
          ? Some(static_cast<const ColorPattern&>(aPattern).mColor)
          : Nothing();
  if (!mPathCache) {
    mPathCache = MakeUnique<PathCache>();
  }
  // Look for an existing path cache entry, if possible, or otherwise create
  // one.
  RefPtr<PathCacheEntry> entry = mPathCache->FindOrInsertEntry(
      pathSkia->GetPath(), color ? nullptr : &aPattern, aStrokeOptions,
      currentTransform, intBounds, bounds.TopLeft());
  if (!entry) {
    return false;
  }

  RefPtr<TextureHandle> handle = entry->GetHandle();
  if (handle && handle->IsValid()) {
    // If the entry has a valid texture handle still, use it. However, only
    // the rounded integer bounds of the path are considered, so we need to
    // be careful to subtract off the old subpixel offset from the cached
    // patch and add in the new subpixel offset. The integer bounds origin
    // is subtracted from the path offset when it is cached, so we rather
    // need to consider the offset as the difference between the path's real
    // bounds origin and the rounded integer origin.
    Point oldOffset = entry->GetOrigin() - entry->GetBounds().TopLeft();
    Point newOffset = bounds.TopLeft() - intBounds.TopLeft();
    Rect offsetRect = Rect(intBounds) + (newOffset - oldOffset);
    SurfacePattern pathPattern(nullptr, ExtendMode::CLAMP,
                               Matrix::Translation(offsetRect.TopLeft()));
    if (DrawRectAccel(offsetRect, pathPattern, aOptions, color, &handle, false,
                      true, true)) {
      return true;
    }
  } else {
    // If there isn't a valid texture handle, then we need to rasterize the
    // path in a software canvas and upload this to a texture. Solid color
    // patterns will be rendered as a path mask that can then be modulated
    // with any color. Other pattern types have to rasterize the pattern
    // directly into the cached texture.
    handle = nullptr;
    RefPtr<DrawTargetSkia> pathDT = new DrawTargetSkia;
    if (pathDT->Init(intBounds.Size(),
                     color ? SurfaceFormat::A8 : SurfaceFormat::B8G8R8A8)) {
      pathDT->SetTransform(currentTransform *
                           Matrix::Translation(-intBounds.TopLeft()));
      DrawOptions drawOptions(1.0f, CompositionOp::OP_OVER,
                              aOptions.mAntialiasMode);
      static const ColorPattern maskPattern(
          DeviceColor(1.0f, 1.0f, 1.0f, 1.0f));
      const Pattern& cachePattern = color ? maskPattern : aPattern;
      if (aStrokeOptions) {
        pathDT->Stroke(aPath, cachePattern, *aStrokeOptions, drawOptions);
      } else {
        pathDT->Fill(aPath, cachePattern, drawOptions);
      }
      RefPtr<SourceSurface> pathSurface = pathDT->Snapshot();
      if (pathSurface) {
        SurfacePattern pathPattern(pathSurface, ExtendMode::CLAMP,
                                   Matrix::Translation(intBounds.TopLeft()));
        // Try and upload the rasterized path to a texture. If there is a
        // valid texture handle after this, then link it to the entry.
        // Otherwise, we might have to fall back to software drawing the
        // path, so unlink it from the entry.
        if (DrawRectAccel(Rect(intBounds), pathPattern, aOptions, color,
                          &handle, false, true) &&
            handle) {
          entry->Link(handle);
        } else {
          entry->Unlink();
        }
        return true;
      }
    }
  }

  return false;
}

void DrawTargetWebgl::DrawPath(const Path* aPath, const Pattern& aPattern,
                               const DrawOptions& aOptions,
                               const StrokeOptions* aStrokeOptions) {
  // If there is a WebGL context, then try to cache the path to avoid slow
  // fallbacks.
  if (mWebglValid && SupportsDrawOptions(aOptions) && PrepareContext() &&
      mSharedContext->DrawPathAccel(aPath, aPattern, aOptions,
                                    aStrokeOptions)) {
    return;
  }

  // There was no path cache entry available to use, so fall back to drawing the
  // path with Skia.
  MarkSkiaChanged(aOptions);
  if (aStrokeOptions) {
    mSkia->Stroke(aPath, aPattern, *aStrokeOptions, aOptions);
  } else {
    mSkia->Fill(aPath, aPattern, aOptions);
  }
}

void DrawTargetWebgl::DrawSurface(SourceSurface* aSurface, const Rect& aDest,
                                  const Rect& aSource,
                                  const DrawSurfaceOptions& aSurfOptions,
                                  const DrawOptions& aOptions) {
  Matrix matrix = Matrix::Scaling(aDest.width / aSource.width,
                                  aDest.height / aSource.height);
  matrix.PreTranslate(-aSource.x, -aSource.y);
  matrix.PostTranslate(aDest.x, aDest.y);
  SurfacePattern pattern(aSurface, ExtendMode::CLAMP, matrix);
  DrawRect(aDest, pattern, aOptions);
}

void DrawTargetWebgl::Mask(const Pattern& aSource, const Pattern& aMask,
                           const DrawOptions& aOptions) {
  if (!SupportsDrawOptions(aOptions) ||
      aMask.GetType() != PatternType::SURFACE ||
      aSource.GetType() != PatternType::COLOR) {
    MarkSkiaChanged(aOptions);
    mSkia->Mask(aSource, aMask, aOptions);
    return;
  }
  auto sourceColor = static_cast<const ColorPattern&>(aSource).mColor;
  auto maskPattern = static_cast<const SurfacePattern&>(aMask);
  DrawRect(Rect(IntRect(IntPoint(), maskPattern.mSurface->GetSize())),
           maskPattern, aOptions, Some(sourceColor));
}

void DrawTargetWebgl::MaskSurface(const Pattern& aSource, SourceSurface* aMask,
                                  Point aOffset, const DrawOptions& aOptions) {
  if (!SupportsDrawOptions(aOptions) ||
      aSource.GetType() != PatternType::COLOR) {
    MarkSkiaChanged(aOptions);
    mSkia->MaskSurface(aSource, aMask, aOffset, aOptions);
  } else {
    auto sourceColor = static_cast<const ColorPattern&>(aSource).mColor;
    SurfacePattern pattern(aMask, ExtendMode::CLAMP,
                           Matrix::Translation(aOffset));
    DrawRect(Rect(aOffset, Size(aMask->GetSize())), pattern, aOptions,
             Some(sourceColor));
  }
}

// Extract the surface's alpha values into an A8 surface.
static already_AddRefed<DataSourceSurface> ExtractAlpha(
    SourceSurface* aSurface) {
  RefPtr<DataSourceSurface> surfaceData = aSurface->GetDataSurface();
  if (!surfaceData) {
    return nullptr;
  }
  DataSourceSurface::ScopedMap srcMap(surfaceData, DataSourceSurface::READ);
  if (!srcMap.IsMapped()) {
    return nullptr;
  }
  IntSize size = surfaceData->GetSize();
  RefPtr<DataSourceSurface> alpha =
      Factory::CreateDataSourceSurface(size, SurfaceFormat::A8, false);
  if (!alpha) {
    return nullptr;
  }
  DataSourceSurface::ScopedMap dstMap(alpha, DataSourceSurface::WRITE);
  if (!dstMap.IsMapped()) {
    return nullptr;
  }
  SwizzleData(srcMap.GetData(), srcMap.GetStride(), surfaceData->GetFormat(),
              dstMap.GetData(), dstMap.GetStride(), SurfaceFormat::A8, size);
  return alpha.forget();
}

bool DrawTargetWebgl::SharedContext::DrawSurfaceWithShadowAccel(
    SourceSurface* aSurface, const Point& aDest, const DeviceColor& aColor,
    const Point& aOffset, Float aSigma, CompositionOp aOperator) {
  // Attempts to generate a software blur of the surface which will then be
  // cached in a texture handle as well as the original surface. Both will
  // then be subsequently blended to the WebGL context. First, look for an
  // existing cached shadow texture assigned to the surface.
  IntSize size = aSurface->GetSize();
  RefPtr<TextureHandle> handle =
      static_cast<TextureHandle*>(aSurface->GetUserData(&mShadowTextureKey));
  if (!handle || !handle->IsValid() || handle->GetSigma() != aSigma) {
    // If there is no existing shadow texture, we need to generate one. Extract
    // the surface's alpha values and blur them with the provided sigma.
    RefPtr<DataSourceSurface> alpha;
    if (RefPtr<DataSourceSurface> alphaData = ExtractAlpha(aSurface)) {
      DataSourceSurface::ScopedMap dstMap(alphaData,
                                          DataSourceSurface::READ_WRITE);
      if (dstMap.IsMapped()) {
        AlphaBoxBlur blur(Rect(0, 0, size.width, size.height),
                          dstMap.GetStride(), aSigma, aSigma);
        blur.Blur(dstMap.GetData());
        alpha = alphaData;
      }
    }
    if (alpha) {
      // Once we successfully generate the blurred shadow surface, we now need
      // to generate a cached texture for it.
      IntPoint shadowOffset = IntPoint::Round(aDest + aOffset);
      SurfacePattern shadowMask(alpha, ExtendMode::CLAMP,
                                Matrix::Translation(Point(shadowOffset)));
      handle = nullptr;
      if (DrawRectAccel(Rect(IntRect(shadowOffset, size)), shadowMask,
                        DrawOptions(1.0f, aOperator), Some(aColor), &handle,
                        false, true) &&
          handle) {
        // If drawing succeeded, then we now have a cached texture for the
        // shadow that can be assigned to the surface for reuse.
        handle->SetSurface(aSurface);
        handle->SetSigma(aSigma);
        aSurface->AddUserData(&mShadowTextureKey, handle.get(),
                              ReleaseTextureHandle);
      }

      // Finally draw the original surface.
      IntPoint surfaceOffset = IntPoint::Round(aDest);
      SurfacePattern surfacePattern(aSurface, ExtendMode::CLAMP,
                                    Matrix::Translation(Point(surfaceOffset)));
      DrawRectAccel(Rect(IntRect(surfaceOffset, size)), surfacePattern,
                    DrawOptions(1.0f, aOperator), Nothing(), nullptr, false,
                    true);
      return true;
    }
  } else {
    // We found a cached shadow texture. Try to draw with it.
    IntPoint shadowOffset = IntPoint::Round(aDest + aOffset);
    SurfacePattern shadowMask(nullptr, ExtendMode::CLAMP,
                              Matrix::Translation(Point(shadowOffset)));
    if (DrawRectAccel(Rect(IntRect(shadowOffset, size)), shadowMask,
                      DrawOptions(1.0f, aOperator), Some(aColor), &handle,
                      false, true, true)) {
      // If drawing the cached shadow texture succeeded, then proceed to draw
      // the original surface as well.
      IntPoint surfaceOffset = IntPoint::Round(aDest);
      SurfacePattern surfacePattern(aSurface, ExtendMode::CLAMP,
                                    Matrix::Translation(Point(surfaceOffset)));
      DrawRectAccel(Rect(IntRect(surfaceOffset, size)), surfacePattern,
                    DrawOptions(1.0f, aOperator), Nothing(), nullptr, false,
                    true);
      return true;
    }
  }
  return false;
}

void DrawTargetWebgl::DrawSurfaceWithShadow(SourceSurface* aSurface,
                                            const Point& aDest,
                                            const DeviceColor& aColor,
                                            const Point& aOffset, Float aSigma,
                                            CompositionOp aOperator) {
  if (mWebglValid && PrepareContext() &&
      mSharedContext->DrawSurfaceWithShadowAccel(aSurface, aDest, aColor,
                                                 aOffset, aSigma, aOperator)) {
    return;
  }
  // If it wasn't possible to draw the shadow and surface to the WebGL context,
  // just fall back to drawing it to the Skia target.
  MarkSkiaChanged();
  mSkia->DrawSurfaceWithShadow(aSurface, aDest, aColor, aOffset, aSigma,
                               aOperator);
}

already_AddRefed<PathBuilder> DrawTargetWebgl::CreatePathBuilder(
    FillRule aFillRule) const {
  return mSkia->CreatePathBuilder(aFillRule);
}

void DrawTargetWebgl::SetTransform(const Matrix& aTransform) {
  DrawTarget::SetTransform(aTransform);
  mSkia->SetTransform(aTransform);
}

void DrawTargetWebgl::StrokeRect(const Rect& aRect, const Pattern& aPattern,
                                 const StrokeOptions& aStrokeOptions,
                                 const DrawOptions& aOptions) {
  // TODO: Support other stroke options. Ensure that we only stroke with the
  // default settings for now.
  if (aStrokeOptions == StrokeOptions() && mTransform.PreservesDistance()) {
    DrawRect(aRect, aPattern, aOptions, Nothing(), nullptr, true, true, false,
             false, &aStrokeOptions);
  } else if (!mWebglValid || !SupportsPattern(aPattern)) {
    MarkSkiaChanged(aOptions);
    mSkia->StrokeRect(aRect, aPattern, aStrokeOptions, aOptions);
  } else {
    // If the stroke options are unsupported, then transform the rect to a path
    // so it can be cached.
    SkPath skiaPath;
    skiaPath.addRect(RectToSkRect(aRect));
    RefPtr<PathSkia> path = new PathSkia(skiaPath, FillRule::FILL_WINDING);
    DrawPath(path, aPattern, aOptions, &aStrokeOptions);
  }
}

void DrawTargetWebgl::StrokeLine(const Point& aStart, const Point& aEnd,
                                 const Pattern& aPattern,
                                 const StrokeOptions& aStrokeOptions,
                                 const DrawOptions& aOptions) {
  MarkSkiaChanged(aOptions);
  mSkia->StrokeLine(aStart, aEnd, aPattern, aStrokeOptions, aOptions);
}

void DrawTargetWebgl::Stroke(const Path* aPath, const Pattern& aPattern,
                             const StrokeOptions& aStrokeOptions,
                             const DrawOptions& aOptions) {
  if (!aPath || aPath->GetBackendType() != BackendType::SKIA) {
    return;
  }
  const auto& skiaPath = static_cast<const PathSkia*>(aPath)->GetPath();
  SkRect rect;
  SkPoint line[2];
  if (!mWebglValid) {
    MarkSkiaChanged(aOptions);
    mSkia->Stroke(aPath, aPattern, aStrokeOptions, aOptions);
  } else if (skiaPath.isRect(&rect)) {
    StrokeRect(SkRectToRect(rect), aPattern, aStrokeOptions, aOptions);
  } else if (skiaPath.isLine(line)) {
    StrokeLine(SkPointToPoint(line[0]), SkPointToPoint(line[1]), aPattern,
               aStrokeOptions, aOptions);
  } else {
    DrawPath(aPath, aPattern, aOptions, &aStrokeOptions);
  }
}

void DrawTargetWebgl::StrokeGlyphs(ScaledFont* aFont,
                                   const GlyphBuffer& aBuffer,
                                   const Pattern& aPattern,
                                   const StrokeOptions& aStrokeOptions,
                                   const DrawOptions& aOptions) {
  MarkSkiaChanged(aOptions);
  mSkia->StrokeGlyphs(aFont, aBuffer, aPattern, aStrokeOptions, aOptions);
}

// Skia only supports subpixel positioning to the nearest 1/4 fraction. It
// would be wasteful to attempt to cache text runs with positioning that is
// anymore precise than this. To prevent this cache bloat, we quantize the
// transformed glyph positions to the nearest 1/4.
static inline IntPoint QuantizePosition(const Matrix& aTransform,
                                        const IntPoint& aOffset,
                                        const Point& aPosition) {
  IntPoint pos =
      RoundedToInt(aTransform.TransformPoint(aPosition) * 4.0f) - aOffset * 4;
  return IntPoint(pos.x & 3, pos.y & 3);
}

// Hashes a glyph buffer to a single hash value that can be used for quick
// comparisons. Each glyph position is transformed and quantized before
// hashing.
HashNumber GlyphCacheEntry::HashGlyphs(const GlyphBuffer& aBuffer,
                                       const Matrix& aTransform) {
  HashNumber hash = 0;
  IntPoint offset =
      TruncatedToInt(aTransform.TransformPoint(aBuffer.mGlyphs[0].mPosition));
  for (size_t i = 0; i < aBuffer.mNumGlyphs; i++) {
    const Glyph& glyph = aBuffer.mGlyphs[i];
    hash = AddToHash(hash, glyph.mIndex);
    IntPoint pos = QuantizePosition(aTransform, offset, glyph.mPosition);
    hash = AddToHash(hash, pos.x);
    hash = AddToHash(hash, pos.y);
  }
  return hash;
}

// Determines if an existing glyph cache entry matches an incoming text run.
bool GlyphCacheEntry::MatchesGlyphs(const GlyphBuffer& aBuffer,
                                    const DeviceColor& aColor,
                                    const Matrix& aTransform,
                                    const IntRect& aBounds, HashNumber aHash) {
  // First check if the hash matches to quickly reject the text run before any
  // more expensive checking. If it matches, then check if the color, transform,
  // and bounds are the same.
  if (aHash != mHash || aBuffer.mNumGlyphs != mBuffer.mNumGlyphs ||
      aColor != mColor || !HasMatchingScale(aTransform, mTransform) ||
      aBounds.Size() != mBounds.Size()) {
    return false;
  }
  IntPoint offset =
      TruncatedToInt(aTransform.TransformPoint(aBuffer.mGlyphs[0].mPosition));
  if (aBounds.TopLeft() - offset != mBounds.TopLeft()) {
    return false;
  }
  // Finally check if all glyphs and their quantized positions match.
  for (size_t i = 0; i < aBuffer.mNumGlyphs; i++) {
    const Glyph& dst = mBuffer.mGlyphs[i];
    const Glyph& src = aBuffer.mGlyphs[i];
    if (dst.mIndex != src.mIndex ||
        dst.mPosition !=
            Point(QuantizePosition(aTransform, offset, src.mPosition))) {
      return false;
    }
  }
  return true;
}

GlyphCacheEntry::GlyphCacheEntry(const GlyphBuffer& aBuffer,
                                 const DeviceColor& aColor,
                                 const Matrix& aTransform,
                                 const IntRect& aBounds, HashNumber aHash)
    : CacheEntryImpl<GlyphCacheEntry>(aTransform, aBounds, aHash),
      mColor(aColor) {
  // Store a copy of the glyph buffer with positions already quantized for fast
  // comparison later.
  Glyph* glyphs = new Glyph[aBuffer.mNumGlyphs];
  IntPoint offset =
      TruncatedToInt(aTransform.TransformPoint(aBuffer.mGlyphs[0].mPosition));
  mBounds -= offset;
  for (size_t i = 0; i < aBuffer.mNumGlyphs; i++) {
    Glyph& dst = glyphs[i];
    const Glyph& src = aBuffer.mGlyphs[i];
    dst.mIndex = src.mIndex;
    dst.mPosition = Point(QuantizePosition(aTransform, offset, src.mPosition));
  }
  mBuffer.mGlyphs = glyphs;
  mBuffer.mNumGlyphs = aBuffer.mNumGlyphs;
}

// Attempt to find a matching entry in the glyph cache. If one isn't found,
// a new entry will be created. The caller should check whether the contained
// texture handle is valid to determine if it will need to render the text run
// or just reuse the cached texture.
already_AddRefed<GlyphCacheEntry> GlyphCache::FindOrInsertEntry(
    const GlyphBuffer& aBuffer, const DeviceColor& aColor,
    const Matrix& aTransform, const IntRect& aBounds) {
  HashNumber hash = GlyphCacheEntry::HashGlyphs(aBuffer, aTransform);
  for (const RefPtr<GlyphCacheEntry>& entry : mEntries) {
    if (entry->MatchesGlyphs(aBuffer, aColor, aTransform, aBounds, hash)) {
      return do_AddRef(entry);
    }
  }
  RefPtr<GlyphCacheEntry> entry =
      new GlyphCacheEntry(aBuffer, aColor, aTransform, aBounds, hash);
  mEntries.insertFront(entry);
  return entry.forget();
}

GlyphCache::GlyphCache(ScaledFont* aFont) : mFont(aFont) {}

static void ReleaseGlyphCache(void* aPtr) {
  delete static_cast<GlyphCache*>(aPtr);
}

void DrawTargetWebgl::SetPermitSubpixelAA(bool aPermitSubpixelAA) {
  DrawTarget::SetPermitSubpixelAA(aPermitSubpixelAA);
  mSkia->SetPermitSubpixelAA(aPermitSubpixelAA);
}

// Check for any color glyphs contained within a rasterized BGRA8 text result.
static bool CheckForColorGlyphs(const RefPtr<SourceSurface>& aSurface) {
  if (aSurface->GetFormat() != SurfaceFormat::B8G8R8A8) {
    return false;
  }
  RefPtr<DataSourceSurface> dataSurf = aSurface->GetDataSurface();
  if (!dataSurf) {
    return true;
  }
  DataSourceSurface::ScopedMap map(dataSurf, DataSourceSurface::READ);
  if (!map.IsMapped()) {
    return true;
  }
  IntSize size = dataSurf->GetSize();
  const uint8_t* data = map.GetData();
  int32_t stride = map.GetStride();
  for (int y = 0; y < size.height; y++) {
    const uint32_t* x = (const uint32_t*)data;
    const uint32_t* end = x + size.width;
    for (; x < end; x++) {
      // Verify if all components are the same as for premultiplied grayscale.
      uint32_t color = *x;
      uint32_t gray = color & 0xFF;
      gray |= gray << 8;
      gray |= gray << 16;
      if (color != gray) return true;
    }
    data += stride;
  }
  return false;
}

bool DrawTargetWebgl::SharedContext::FillGlyphsAccel(
    ScaledFont* aFont, const GlyphBuffer& aBuffer, const Pattern& aPattern,
    const DrawOptions& aOptions, bool aUseSubpixelAA) {
  // Draws glyphs to the WebGL target by trying to generate a cached texture for
  // the text run that can be subsequently reused to quickly render the text run
  // without using any software surfaces.
  // Get the local bounds of the text run.
  Maybe<Rect> bounds = mCurrentTarget->mSkia->GetGlyphLocalBounds(
      aFont, aBuffer, aPattern, nullptr, aOptions);
  if (!bounds) {
    return false;
  }

  // Transform the local bounds into device space so that we know how big
  // the cached texture will be.
  const Matrix& currentTransform = GetTransform();
  Rect xformBounds = currentTransform.TransformBounds(*bounds).Intersect(
      Rect(IntRect(IntPoint(), mViewportSize)));
  if (xformBounds.IsEmpty()) {
    return true;
  }
  // Ensure there is a clear border around the text.
  xformBounds.Inflate(1);
  IntRect intBounds = RoundedOut(xformBounds);

  // Whether to render the text as a full color result as opposed to as a
  // grayscale mask. Subpixel AA or fonts with color glyphs require this.
  // We currently have to check to check the rasterized result to see if
  // there are any color glyphs as there is not yet a way to a priori know
  // this.
  bool useColor = aUseSubpixelAA;

  // Look for an existing glyph cache on the font. If not there, create it.
  GlyphCache* cache =
      static_cast<GlyphCache*>(aFont->GetUserData(&mGlyphCacheKey));
  if (!cache) {
    cache = new GlyphCache(aFont);
    aFont->AddUserData(&mGlyphCacheKey, cache, ReleaseGlyphCache);
    mGlyphCaches.insertFront(cache);
  }
  // Hash the incoming text run and looking for a matching entry.
  DeviceColor color = static_cast<const ColorPattern&>(aPattern).mColor;
  DeviceColor aaColor = useColor ? color : DeviceColor(1.0f, 1.0f, 1.0f, 1.0f);
  RefPtr<GlyphCacheEntry> entry =
      cache->FindOrInsertEntry(aBuffer, aaColor, currentTransform, intBounds);
  if (!entry) {
    return false;
  }

  RefPtr<TextureHandle> handle = entry->GetHandle();
  if (handle && handle->IsValid()) {
    // If there is an entry with a valid cached texture handle, then try
    // to draw with that. If that for some reason failed, then fall back
    // to using the Skia target as that means we were preventing from
    // drawing to the WebGL context based on something other than the
    // texture.
    SurfacePattern pattern(nullptr, ExtendMode::CLAMP,
                           Matrix::Translation(intBounds.TopLeft()));
    if (DrawRectAccel(
            Rect(intBounds), pattern, aOptions,
            handle->GetFormat() == SurfaceFormat::A8 ? Some(color) : Nothing(),
            &handle, false, true, true)) {
      return true;
    }
  } else {
    handle = nullptr;

    // If we get here, either there wasn't a cached texture handle or it
    // wasn't valid. Render the text run into a temporary target.
    RefPtr<DrawTargetSkia> textDT = new DrawTargetSkia;
    if (textDT->Init(intBounds.Size(), SurfaceFormat::B8G8R8A8)) {
      textDT->SetTransform(currentTransform *
                           Matrix::Translation(-intBounds.TopLeft()));
      textDT->SetPermitSubpixelAA(aUseSubpixelAA);
      DrawOptions drawOptions(1.0f, CompositionOp::OP_OVER,
                              aOptions.mAntialiasMode);
      textDT->FillGlyphs(aFont, aBuffer, ColorPattern(aaColor), drawOptions);
      RefPtr<SourceSurface> textSurface = textDT->Snapshot();
      if (textSurface) {
        if (!useColor) {
          // If we don't expect the text surface to contain color glyphs
          // such as from subpixel AA, then do one final check to see if
          // any ended up in the result. If not, extract the alpha values
          // from the surface so we can render it as a mask.
          if (CheckForColorGlyphs(textSurface)) {
            useColor = true;
          } else {
            textSurface = ExtractAlpha(textSurface);
            if (!textSurface) {
              // Failed extracting alpha for the text surface...
              return false;
            }
          }
        }
        // Attempt to upload the rendered text surface into a texture
        // handle and draw it.
        SurfacePattern pattern(textSurface, ExtendMode::CLAMP,
                               Matrix::Translation(intBounds.TopLeft()));
        if (DrawRectAccel(Rect(intBounds), pattern, aOptions,
                          useColor ? Nothing() : Some(color), &handle, false,
                          true) &&
            handle) {
          // If drawing succeeded, then the text surface was uploaded to
          // a texture handle. Assign it to the glyph cache entry.
          entry->Link(handle);
        } else {
          // If drawing failed, remove the entry from the cache.
          entry->Unlink();
        }
        return true;
      }
    }
  }
  return false;
}

void DrawTargetWebgl::FillGlyphs(ScaledFont* aFont, const GlyphBuffer& aBuffer,
                                 const Pattern& aPattern,
                                 const DrawOptions& aOptions) {
  if (!aFont || !aBuffer.mNumGlyphs) {
    return;
  }

  AntialiasMode aaMode = aFont->GetDefaultAAMode();
  if (aOptions.mAntialiasMode != AntialiasMode::DEFAULT) {
    aaMode = aOptions.mAntialiasMode;
  }
  bool useSubpixelAA =
      GetPermitSubpixelAA() &&
      (aaMode == AntialiasMode::DEFAULT || aaMode == AntialiasMode::SUBPIXEL);

  if (mWebglValid && SupportsDrawOptions(aOptions) &&
      aPattern.GetType() == PatternType::COLOR && PrepareContext() &&
      mSharedContext->FillGlyphsAccel(aFont, aBuffer, aPattern, aOptions,
                                      useSubpixelAA)) {
    return;
  }

  // If not able to cache the text run to a texture, then just fall back to
  // drawing with the Skia target.
  if (useSubpixelAA) {
    // Subpixel AA does not support layering because the subpixel masks can't
    // blend with the over op.
    MarkSkiaChanged();
  } else {
    MarkSkiaChanged(aOptions);
  }
  mSkia->FillGlyphs(aFont, aBuffer, aPattern, aOptions);
}

void DrawTargetWebgl::MarkSkiaChanged(const DrawOptions& aOptions) {
  if (aOptions.mCompositionOp == CompositionOp::OP_OVER) {
    // Layering is only supporting for the default source-over composition op.
    if (!mSkiaValid) {
      // If the Skia context needs initialization, clear it and enable layering.
      mSkiaValid = true;
      if (mWebglValid) {
        mSkiaLayer = true;
        mSkia->Clear();
      }
    }
    // The WebGL context is no longer up-to-date.
    mWebglValid = false;
  } else {
    // For other composition ops, just overwrite the Skia data.
    MarkSkiaChanged();
  }
}

// Attempts to read the contents of the WebGL context into the Skia target.
void DrawTargetWebgl::ReadIntoSkia() {
  if (mSkiaValid) {
    return;
  }
  if (mWebglValid) {
    uint8_t* data = nullptr;
    IntSize size;
    int32_t stride;
    SurfaceFormat format;
    // If there's no existing snapshot and we can successfully map the Skia
    // target for reading, then try to read into that.
    if (!mSnapshot && mSkia->LockBits(&data, &size, &stride, &format)) {
      (void)ReadInto(data, stride);
      mSkia->ReleaseBits(data);
    } else if (RefPtr<SourceSurface> snapshot = Snapshot()) {
      // Otherwise, fall back to getting a snapshot from WebGL if available
      // and then copying that to Skia.
      mSkia->CopySurface(snapshot, GetRect(), IntPoint(0, 0));
    }
  }
  mSkiaValid = true;
  // The Skia data is flat after reading, so disable any layering.
  mSkiaLayer = false;
}

// Reads data from the WebGL context and blends it with the current Skia layer.
void DrawTargetWebgl::FlattenSkia() {
  if (!mSkiaValid || !mSkiaLayer) {
    return;
  }
  if (RefPtr<DataSourceSurface> base = ReadSnapshot()) {
    mSkia->BlendSurface(base, GetRect(), IntPoint(),
                        CompositionOp::OP_DEST_OVER);
  }
  mSkiaLayer = false;
}

// Attempts to draw the contents of the Skia target into the WebGL context.
bool DrawTargetWebgl::FlushFromSkia() {
  // If the WebGL context has been lost, then mark it as invalid and fail.
  if (mSharedContext->IsContextLost()) {
    mWebglValid = false;
    return false;
  }
  // The WebGL target is already valid, so there is nothing to do.
  if (mWebglValid) {
    return true;
  }
  // Ensure that DrawRect doesn't recursively call into FlushFromSkia. If
  // the Skia target isn't valid, then it doesn't matter what is in the the
  // WebGL target either, so only try to blend if there is a valid Skia target.
  mWebglValid = true;
  if (mSkiaValid) {
    RefPtr<SourceSurface> skiaSnapshot = mSkia->Snapshot();
    if (!skiaSnapshot) {
      // There's a valid Skia target to draw to, but for some reason there is
      // no available snapshot, so just keep using the Skia target.
      mWebglValid = false;
      return false;
    }
    AutoRestoreContext restore(this);
    SurfacePattern pattern(skiaSnapshot, ExtendMode::CLAMP);
    // If there is a layer, blend the snapshot with the WebGL context,
    // otherwise copy it.
    if (!DrawRect(Rect(GetRect()), pattern,
                  DrawOptions(1.0f, mSkiaLayer ? CompositionOp::OP_OVER
                                               : CompositionOp::OP_SOURCE),
                  Nothing(), mSkiaLayer ? &mSnapshotTexture : nullptr, false,
                  false, true, true)) {
      // If accelerated drawing failed for some reason, then leave the Skia
      // target unchanged.
      mWebglValid = false;
      return false;
    }
  }
  return true;
}

// For use within CanvasRenderingContext2D, called on BorrowDrawTarget.
void DrawTargetWebgl::BeginFrame(const IntRect& aPersistedRect) {
  if (mNeedsPresent) {
    mNeedsPresent = false;
    // If still rendering into the Skia target, switch back to the WebGL
    // context.
    if (!mWebglValid) {
      if (aPersistedRect.IsEmpty()) {
        // If nothing needs to persist, just mark the WebGL context valid.
        mWebglValid = true;
      } else {
        FlushFromSkia();
      }
    }
  }
}

// For use within CanvasRenderingContext2D, called on ReturnDrawTarget.
void DrawTargetWebgl::EndFrame() {
  //  Ensure we're not somehow using more than the allowed texture memory.
  mSharedContext->PruneTextureMemory();
  // Signal that we're done rendering the frame in case no present occurs.
  mSharedContext->mWebgl->EndOfFrame();
  // The framebuffer is dirty, so it needs to be copied to the swapchain.
  mNeedsPresent = true;
}

Maybe<layers::SurfaceDescriptor> DrawTargetWebgl::GetFrontBuffer() {
  // Only try to present and retrieve the front buffer if there is a valid
  // WebGL framebuffer that can be sent to the compositor. Otherwise, return
  // nothing to try to reuse the Skia snapshot.
  if (mSharedContext->mWebgl->GetTexTypeForSwapChain() ==
      layers::TextureType::Unknown) {
    return Nothing();
  }
  if (mNeedsPresent) {
    mNeedsPresent = false;
    if (mWebglValid || FlushFromSkia()) {
      // Copy and swizzle the WebGL framebuffer to the swap chain front buffer.
      webgl::SwapChainOptions options;
      options.bgra = true;
      mSharedContext->mWebgl->CopyToSwapChain(mFramebuffer, options);
    }
  }
  if (mWebglValid) {
    return mSharedContext->mWebgl->GetFrontBuffer(mFramebuffer);
  }
  return Nothing();
}

already_AddRefed<DrawTarget> DrawTargetWebgl::CreateSimilarDrawTarget(
    const IntSize& aSize, SurfaceFormat aFormat) const {
  return mSkia->CreateSimilarDrawTarget(aSize, aFormat);
}

bool DrawTargetWebgl::CanCreateSimilarDrawTarget(const IntSize& aSize,
                                                 SurfaceFormat aFormat) const {
  return mSkia->CanCreateSimilarDrawTarget(aSize, aFormat);
}

RefPtr<DrawTarget> DrawTargetWebgl::CreateClippedDrawTarget(
    const Rect& aBounds, SurfaceFormat aFormat) {
  return mSkia->CreateClippedDrawTarget(aBounds, aFormat);
}

already_AddRefed<SourceSurface> DrawTargetWebgl::CreateSourceSurfaceFromData(
    unsigned char* aData, const IntSize& aSize, int32_t aStride,
    SurfaceFormat aFormat) const {
  return mSkia->CreateSourceSurfaceFromData(aData, aSize, aStride, aFormat);
}

already_AddRefed<SourceSurface>
DrawTargetWebgl::CreateSourceSurfaceFromNativeSurface(
    const NativeSurface& aSurface) const {
  return mSkia->CreateSourceSurfaceFromNativeSurface(aSurface);
}

already_AddRefed<SourceSurface> DrawTargetWebgl::OptimizeSourceSurface(
    SourceSurface* aSurface) const {
  if (aSurface->GetType() == SurfaceType::WEBGL) {
    return do_AddRef(aSurface);
  }
  return mSkia->OptimizeSourceSurface(aSurface);
}

already_AddRefed<SourceSurface>
DrawTargetWebgl::OptimizeSourceSurfaceForUnknownAlpha(
    SourceSurface* aSurface) const {
  return mSkia->OptimizeSourceSurfaceForUnknownAlpha(aSurface);
}

already_AddRefed<GradientStops> DrawTargetWebgl::CreateGradientStops(
    GradientStop* aStops, uint32_t aNumStops, ExtendMode aExtendMode) const {
  return mSkia->CreateGradientStops(aStops, aNumStops, aExtendMode);
}

already_AddRefed<FilterNode> DrawTargetWebgl::CreateFilter(FilterType aType) {
  return mSkia->CreateFilter(aType);
}

void DrawTargetWebgl::DrawFilter(FilterNode* aNode, const Rect& aSourceRect,
                                 const Point& aDestPoint,
                                 const DrawOptions& aOptions) {
  MarkSkiaChanged(aOptions);
  mSkia->DrawFilter(aNode, aSourceRect, aDestPoint, aOptions);
}

bool DrawTargetWebgl::Draw3DTransformedSurface(SourceSurface* aSurface,
                                               const Matrix4x4& aMatrix) {
  MarkSkiaChanged();
  return mSkia->Draw3DTransformedSurface(aSurface, aMatrix);
}

}  // namespace mozilla::gfx
