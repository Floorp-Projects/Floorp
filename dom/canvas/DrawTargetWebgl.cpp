/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DrawTargetWebglInternal.h"

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

DrawTargetWebgl::~DrawTargetWebgl() {
  while (!mTextureHandles.isEmpty()) {
    PruneTextureHandle(mTextureHandles.popLast());
    --mNumTextureHandles;
  }
  UnlinkSurfaceTextures();
  UnlinkGlyphCaches();
}

// Remove any SourceSurface user data associated with this TextureHandle.
inline void DrawTargetWebgl::UnlinkSurfaceTexture(
    const RefPtr<TextureHandle>& aHandle) {
  if (SourceSurface* surface = aHandle->GetSurface()) {
    surface->RemoveUserData(aHandle->IsShadow() ? &mShadowTextureKey
                                                : &mTextureHandleKey);
  }
}

// Unlinks TextureHandles from any SourceSurface user data.
void DrawTargetWebgl::UnlinkSurfaceTextures() {
  for (RefPtr<TextureHandle> handle = mTextureHandles.getFirst(); handle;
       handle = handle->getNext()) {
    UnlinkSurfaceTexture(handle);
  }
}

// Unlinks GlyphCaches from any ScaledFont user data.
void DrawTargetWebgl::UnlinkGlyphCaches() {
  GlyphCache* cache = mGlyphCaches.getFirst();
  while (cache) {
    ScaledFont* font = cache->GetFont();
    // Access the next cache before removing the user data, as it might destroy
    // the cache.
    cache = cache->getNext();
    font->RemoveUserData(&mGlyphCacheKey);
  }
}

// Try to initialize a new WebGL context. Verifies that the requested size does
// not exceed the available texture limits and that shader creation succeeded.
bool DrawTargetWebgl::Init(const IntSize& size, const SurfaceFormat format) {
  WebGLContextOptions options = {};
  options.alpha = !IsOpaque(format);
  options.depth = false;
  options.stencil = false;
  options.antialias = true;
  options.preserveDrawingBuffer = true;
  options.failIfMajorPerformanceCaveat = true;

  mWebgl = new ClientWebGLContext(true);
  mWebgl->SetContextOptions(options);
  if (mWebgl->SetDimensions(size.width, size.height) != NS_OK) {
    mWebgl = nullptr;
    return false;
  }

  // Cache the max texture size in case the WebGL context is lost.
  mMaxTextureSize = mWebgl->Limits().maxTex2dSize;
  if (size_t(std::max(size.width, size.height)) > mMaxTextureSize) {
    mWebgl = nullptr;
    return false;
  }

  if (!CreateShaders()) {
    mWebgl = nullptr;
    return false;
  }

  mSkia = new DrawTargetSkia;
  if (!mSkia->Init(size, SurfaceFormat::B8G8R8A8)) {
    return false;
  }
  mSize = size;
  mFormat = format;
  return true;
}

// Signal to CanvasRenderingContext2D when the WebGL context is lost.
bool DrawTargetWebgl::IsValid() const {
  return mWebgl && !mWebgl->IsContextLost();
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
      if (mWebgl->IsContextLost()) {
        return nullptr;
      }
      if (!mWebglValid) {
        FlushFromSkia();
      }
      return mWebgl.get();
    default:
      return nullptr;
  }
}

already_AddRefed<SourceSurface> DrawTargetWebgl::Snapshot() {
  // If already using the Skia fallback, then just snapshot that.
  if (mSkiaValid) {
    return mSkia->Snapshot();
  }

  // Check if there's already a snapshot.
  RefPtr<SourceSurface> snapshot = mSnapshot;
  if (snapshot) {
    return snapshot.forget();
  }

  // If there's no valid Skia and the WebGL context is lost, there is nothing to
  // snapshot.
  if (mWebgl->IsContextLost()) {
    return nullptr;
  }

  // If no snapshot, then allocate one, map it, and read from the WebGL context
  // into the surface.
  mSnapshot = Factory::CreateDataSourceSurface(mSize, mFormat);
  if (!mSnapshot) {
    return nullptr;
  }
  DataSourceSurface::ScopedMap dstMap(mSnapshot, DataSourceSurface::WRITE);
  if (!dstMap.IsMapped() || !ReadInto(dstMap.GetData(), dstMap.GetStride())) {
    mSnapshot = nullptr;
    return nullptr;
  }

  snapshot = mSnapshot;
  return snapshot.forget();
}

// Read from the WebGL context into a buffer. This handles both swizzling BGRA
// to RGBA and flipping the image.
bool DrawTargetWebgl::ReadInto(uint8_t* aDstData, int32_t aDstStride) {
  RefPtr<DataSourceSurface> temp =
      Factory::CreateDataSourceSurface(mSize, mFormat);
  if (!temp) {
    return false;
  }
  DataSourceSurface::ScopedMap srcMap(temp, DataSourceSurface::READ_WRITE);
  if (!srcMap.IsMapped()) {
    return false;
  }

  int32_t srcStride = srcMap.GetStride();
  webgl::ReadPixelsDesc desc;
  desc.srcOffset = {0, 0};
  desc.size = *uvec2::FromSize(mSize);
  desc.packState.rowLength = srcStride / 4;
  Range<uint8_t> range = {srcMap.GetData(), size_t(srcStride) * mSize.height};
  mWebgl->DoReadPixels(desc, range);
  const uint8_t* srcRow = srcMap.GetData();
  uint8_t* dstRow = aDstData + size_t(aDstStride) * mSize.height;
  for (int y = 0; y < mSize.height; y++) {
    dstRow -= aDstStride;
    SwizzleData(srcRow, srcMap.GetStride(), SurfaceFormat::B8G8R8A8, dstRow,
                aDstStride, SurfaceFormat::R8G8B8A8, IntSize(mSize.width, 1));
    srcRow += srcStride;
  }
  return true;
}

already_AddRefed<SourceSurface> DrawTargetWebgl::GetBackingSurface() {
  return Snapshot();
}

void DrawTargetWebgl::MarkChanged() {
  mSkiaValid = false;
  mSnapshot = nullptr;
}

bool DrawTargetWebgl::LockBits(uint8_t** aData, IntSize* aSize,
                               int32_t* aStride, SurfaceFormat* aFormat,
                               IntPoint* aOrigin) {
  if (mSkiaValid) {
    return mSkia->LockBits(aData, aSize, aStride, aFormat, aOrigin);
  }
  return false;
}

void DrawTargetWebgl::ReleaseBits(uint8_t* aData) {
  if (mSkiaValid) {
    mSkia->ReleaseBits(aData);
  }
}

// Attempts to create all shaders and resources to be used for drawing commands.
// Returns whether or not this succeeded.
bool DrawTargetWebgl::CreateShaders() {
  if (!mVertexArray) {
    mVertexArray = mWebgl->CreateVertexArray();
  }
  if (!mVertexBuffer) {
    mVertexBuffer = mWebgl->CreateBuffer();
    static const float rectData[8] = {0.0f, 0.0f, 1.0f, 0.0f,
                                      0.0f, 1.0f, 1.0f, 1.0f};
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
        "   gl_FragColor = u_color * mix(image.bgra, image.rrrr, u_swizzle);\n"
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
  }
  return true;
}

void DrawTargetWebgl::ClearRect(const Rect& aRect) {
  ColorPattern pattern(
      DeviceColor(0.0f, 0.0f, 0.0f, IsOpaque(mFormat) ? 1.0f : 0.0f));
  DrawRect(aRect, pattern, DrawOptions(1.0f, CompositionOp::OP_SOURCE));
}

void DrawTargetWebgl::CopySurface(SourceSurface* aSurface,
                                  const IntRect& aSourceRect,
                                  const IntPoint& aDestination) {
  if (mSkiaValid) {
    MarkSkiaChanged();
    mSkia->CopySurface(aSurface, aSourceRect, aDestination);
    return;
  }

  Matrix matrix = Matrix::Translation(aDestination - aSourceRect.TopLeft());
  SurfacePattern pattern(aSurface, ExtendMode::CLAMP, matrix);
  DrawRect(Rect(IntRect(aDestination, aSourceRect.Size())), pattern,
           DrawOptions(1.0f, CompositionOp::OP_SOURCE), Nothing(), nullptr,
           false, false);
}

void DrawTargetWebgl::PushClip(const Path* aPath) { mSkia->PushClip(aPath); }

void DrawTargetWebgl::PushClipRect(const Rect& aRect) {
  mSkia->PushClipRect(aRect);
}

void DrawTargetWebgl::PushDeviceSpaceClipRects(const IntRect* aRects,
                                               uint32_t aCount) {
  mSkia->PushDeviceSpaceClipRects(aRects, aCount);
}

void DrawTargetWebgl::PopClip() { mSkia->PopClip(); }

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
bool DrawTargetWebgl::SupportsPattern(const Pattern& aPattern) {
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

// Common rectangle and pattern drawing function shared my many DrawTarget
// commands. If aMaskColor is specified, the provided surface pattern will be
// treated as a mask. If aHandle is specified, then the surface pattern's
// texture will be cached in the supplied handle, as opposed to using the
// surface's user data. If aTransformed or aClipped are false, then transforms
// and/or clipping will be disabled. If aAccelOnly is specified, then this
// function will return before it would have otherwise drawn without
// acceleration. If aForceUpdate is specified, then the provided texture handle
// will be respecified with the provided surface.
bool DrawTargetWebgl::DrawRect(const Rect& aRect, const Pattern& aPattern,
                               const DrawOptions& aOptions,
                               Maybe<DeviceColor> aMaskColor,
                               RefPtr<TextureHandle>* aHandle,
                               bool aTransformed, bool aClipped,
                               bool aAccelOnly, bool aForceUpdate) {
  if (aRect.IsEmpty()) {
    return true;
  }
  // Determine whether the clipping rectangle is simple enough to accelerate.
  Maybe<IntRect> intClip;
  if (!aClipped) {
    // If no clipping requested, just set it to the size of the target.
    intClip = Some(IntRect(IntPoint(), mSize));
  } else if (Maybe<Rect> clip = mSkia->GetDeviceClipRect()) {
    // Check if there is a device space clip rectangle available from the Skia
    // target. If the clip is empty, trivially discard the draw request.
    if (clip->IsEmpty()) {
      return true;
    }
    // Check if the clip rectangle is imperceptibly close to pixel boundaries
    // so that rounding won't noticeably change the clipping result. Scissoring
    // only supports integer coordinates.
    IntRect intRect = RoundedToInt(*clip);
    if (clip->WithinEpsilonOf(Rect(intRect), 1.0e-3f)) {
      intClip = Some(intRect);
    }
  }
  // Check if the drawing options, the pattern, and the clip rectangle support
  // acceleration. If not, fall back to using the Skia target.
  if (!SupportsDrawOptions(aOptions) || !SupportsPattern(aPattern) ||
      !intClip || mWebgl->IsContextLost()) {
    // If only accelerated drawing was requested, bail out.
    if (aAccelOnly) {
      return false;
    }
    // Invalidate the WebGL target and prepare the Skia target for drawing.
    MarkSkiaChanged();
    if (aTransformed) {
      // If transforms are requested, then just translate back to FillRect.
      if (aMaskColor) {
        mSkia->Mask(ColorPattern(*aMaskColor), aPattern, aOptions);
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
    return false;
  }

  // The drawing command can be accelerated. Ensure the Skia target contents if
  // flushed to the WebGL context if necessary.
  if (!mWebglValid) {
    FlushFromSkia();
  }

  MarkChanged();

  // Map the composition op to a WebGL blend mode, if possible.
  if (aOptions.mCompositionOp != mLastCompositionOp) {
    mLastCompositionOp = aOptions.mCompositionOp;
    mWebgl->Enable(LOCAL_GL_BLEND);
    switch (aOptions.mCompositionOp) {
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
            LOCAL_GL_DST_ALPHA, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
            LOCAL_GL_DST_ALPHA, LOCAL_GL_ONE_MINUS_SRC_ALPHA);
        break;
      case CompositionOp::OP_SOURCE:
      default:
        mWebgl->Disable(LOCAL_GL_BLEND);
        break;
    }
  }

  // Set up the scissor test to reflect the clipping rectangle, if supplied.
  if (intClip->Contains(IntRect(IntPoint(), mSize))) {
    intClip = Nothing();
  } else {
    mWebgl->Enable(LOCAL_GL_SCISSOR_TEST);
    mWebgl->Scissor(intClip->x, mSize.height - (intClip->y + intClip->height),
                    intClip->width, intClip->height);
  }

  // Now try to actually draw the pattern...
  switch (aPattern.GetType()) {
    case PatternType::COLOR: {
      auto color = static_cast<const ColorPattern&>(aPattern).mColor;
      if (((color.a * aOptions.mAlpha == 1.0f &&
            aOptions.mCompositionOp == CompositionOp::OP_OVER) ||
           aOptions.mCompositionOp == CompositionOp::OP_SOURCE) &&
          mTransform.HasOnlyIntegerTranslation()) {
        // Certain color patterns can be mapped to scissored cleared. The
        // composition op must effectively overwrite the destination, and the
        // transform must map to an axis-aligned integer rectangle.
        auto intRect = RoundedToInt(aRect);
        if (aRect.WithinEpsilonOf(Rect(intRect), 1.0e-3f)) {
          intRect += RoundedToInt(mTransform.GetTranslation());
          IntRect bounds = intClip.refOr(IntRect(IntPoint(), mSize));
          bool scissor = !intRect.Contains(bounds);
          if (scissor) {
            mWebgl->Enable(LOCAL_GL_SCISSOR_TEST);
            intRect =
                intRect.Intersect(intClip.refOr(IntRect(IntPoint(), mSize)));
            mWebgl->Scissor(intRect.x,
                            mSize.height - (intRect.y + intRect.height),
                            intRect.width, intRect.height);
          }
          float a = color.a * aOptions.mAlpha;
          mWebgl->ClearColor(color.r * a, color.g * a, color.b * a, a);
          mWebgl->Clear(LOCAL_GL_COLOR_BUFFER_BIT);
          if (scissor) {
            mWebgl->Disable(LOCAL_GL_SCISSOR_TEST);
          }
          break;
        }
      }
      // Since it couldn't be mapped to a scissored clear, we need to use the
      // solid color shader with supplied transform.
      mWebgl->UseProgram(mSolidProgram.get());
      float a = color.a * aOptions.mAlpha;
      float colorData[4] = {color.r * a, color.g * a, color.b * a, a};
      Matrix xform(aRect.width, 0.0f, 0.0f, aRect.height, aRect.x, aRect.y);
      if (aTransformed) {
        xform *= mTransform;
      }
      xform *= Matrix(2.0f / float(mSize.width), 0.0f, 0.0f,
                      -2.0f / float(mSize.height), -1, 1);
      float xformData[6] = {xform._11, xform._12, xform._21,
                            xform._22, xform._31, xform._32};
      mWebgl->UniformData(LOCAL_GL_FLOAT_VEC2, mSolidProgramTransform, false,
                          {(const uint8_t*)xformData, sizeof(xformData)});
      mWebgl->UniformData(LOCAL_GL_FLOAT_VEC4, mSolidProgramColor, false,
                          {(const uint8_t*)colorData, sizeof(colorData)});
      // Finally draw the colored rectangle.
      mWebgl->DrawArrays(LOCAL_GL_TRIANGLE_STRIP, 0, 4);
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

      // Switch to the image shader and set up relevant transforms.
      mWebgl->UseProgram(mImageProgram.get());
      DeviceColor color = aMaskColor.valueOr(DeviceColor(1, 1, 1, 1));
      float a = color.a * aOptions.mAlpha;
      float colorData[4] = {color.r * a, color.g * a, color.b * a, a};
      float swizzleData = aMaskColor ? 1.0f : 0.0f;
      Matrix xform(aRect.width, 0.0f, 0.0f, aRect.height, aRect.x, aRect.y);
      if (aTransformed) {
        xform *= mTransform;
      }
      xform *= Matrix(2.0f / float(mSize.width), 0.0f, 0.0f,
                      -2.0f / float(mSize.height), -1, 1);
      float xformData[6] = {xform._11, xform._12, xform._21,
                            xform._22, xform._31, xform._32};
      mWebgl->UniformData(LOCAL_GL_FLOAT_VEC2, mImageProgramTransform, false,
                          {(const uint8_t*)xformData, sizeof(xformData)});
      mWebgl->UniformData(LOCAL_GL_FLOAT_VEC4, mImageProgramColor, false,
                          {(const uint8_t*)colorData, sizeof(colorData)});
      mWebgl->UniformData(LOCAL_GL_FLOAT, mImageProgramSwizzle, false,
                          {(const uint8_t*)&swizzleData, sizeof(swizzleData)});
      int32_t samplerData = 0;
      mWebgl->UniformData(LOCAL_GL_INT, mImageProgramSampler, false,
                          {(const uint8_t*)&samplerData, sizeof(samplerData)});

      RefPtr<WebGLTextureJS> tex;
      IntRect bounds;
      IntSize backingSize;
      bool update = aForceUpdate;
      bool init = false;
      if (handle) {
        // If using an existing handle, move it to the front of the MRU list.
        handle->remove();
        mTextureHandles.insertFront(handle);
        if (update) {
          // The size of the texture may change if we update contents.
          mUsedTextureMemory -= handle->UsedBytes();
          handle->UpdateSize(texSize);
          mUsedTextureMemory += handle->UsedBytes();
          handle->SetSamplingOffset(surfacePattern.mSamplingRect.TopLeft());
        }
      } else {
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
          update = true;
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
          update = true;
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

      // Start binding the WebGL state for the texture.
      if (!tex) {
        tex = handle->GetWebGLTexture();
      }
      bounds = handle->GetBounds();
      backingSize = handle->GetBackingSize();
      mWebgl->BindTexture(LOCAL_GL_TEXTURE_2D, tex.get());

      if (init) {
        // If this is the first time the texture is used, we need to initialize
        // the clamping and filtering state.
        mWebgl->TexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S,
                              LOCAL_GL_CLAMP_TO_EDGE);
        mWebgl->TexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T,
                              LOCAL_GL_CLAMP_TO_EDGE);
        mWebgl->TexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER,
                              LOCAL_GL_LINEAR);
        mWebgl->TexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER,
                              LOCAL_GL_LINEAR);
        if (texSize != backingSize) {
          // If this is a shared texture handle whose actual backing texture is
          // larger than it, then we need to allocate the texture page to the
          // full backing size before we can do a partial upload of the surface.
          GLenum intFormat =
              format == SurfaceFormat::A8 ? LOCAL_GL_R8 : LOCAL_GL_RGBA8;
          GLenum extFormat =
              format == SurfaceFormat::A8 ? LOCAL_GL_RED : LOCAL_GL_RGBA;
          webgl::PackingInfo texPI = {extFormat, LOCAL_GL_UNSIGNED_BYTE};
          webgl::TexUnpackBlobDesc texDesc = {
              LOCAL_GL_TEXTURE_2D,
              {uint32_t(backingSize.width), uint32_t(backingSize.height), 1},
          };
          mWebgl->RawTexImage(0, intFormat, {0, 0, 0}, texPI, texDesc);
        }
      }

      if (update) {
        // The surface needs to be uploaded to its backing texture either to
        // initialize or update the texture handle contents. Map the data
        // contents of the surface so it can be read.
        RefPtr<DataSourceSurface> data =
            surfacePattern.mSurface->GetDataSurface();
        DataSourceSurface::ScopedMap map(data, DataSourceSurface::READ);
        int32_t stride = map.GetStride();
        int32_t bpp = BytesPerPixel(format);
        // Get the data pointer range considering the sampling rect offset and
        // size.
        Range<const uint8_t> range(
            map.GetData() + offset.y * size_t(stride) + offset.x * bpp,
            texSize.height * size_t(stride));
        webgl::TexUnpackBlobDesc texDesc = {
            LOCAL_GL_TEXTURE_2D,
            {uint32_t(texSize.width), uint32_t(texSize.height), 1},
            gfxAlphaType::NonPremult,
            Some(RawBuffer(range)),
        };
        // If the stride happens to be 4 byte aligned, assume that is the
        // desired alignment regardless of format (even A8). Otherwise, we
        // default to byte alignment.
        texDesc.unpacking.mUnpackAlignment = stride % 4 ? 1 : 4;
        texDesc.unpacking.mUnpackRowLength = stride / bpp;
        texDesc.unpacking.mUnpackImageHeight = texSize.height;
        // Upload as RGBA8 to avoid swizzling during upload. Surfaces provide
        // data as BGRA, but we manually swizzle that in the shader. An A8
        // surface will be stored as an R8 texture that will also be swizzled
        // in the shader.
        GLenum intFormat =
            format == SurfaceFormat::A8 ? LOCAL_GL_R8 : LOCAL_GL_RGBA8;
        GLenum extFormat =
            format == SurfaceFormat::A8 ? LOCAL_GL_RED : LOCAL_GL_RGBA;
        webgl::PackingInfo texPI = {extFormat, LOCAL_GL_UNSIGNED_BYTE};
        // Do the (partial) upload for the shared or standalone texture.
        mWebgl->RawTexImage(0, texSize == backingSize ? intFormat : 0,
                            {uint32_t(bounds.x), uint32_t(bounds.y), 0}, texPI,
                            texDesc);
      }

      // Set up the texture coordinate matrix to map from the input rectangle to
      // the backing texture subrect.
      Size backingSizeF(backingSize);
      Matrix uvMatrix(aRect.width, 0.0f, 0.0f, aRect.height, aRect.x, aRect.y);
      uvMatrix *= surfacePattern.mMatrix.Inverse();
      uvMatrix *=
          Matrix(1.0f / backingSizeF.width, 0, 0, 1.0f / backingSizeF.height,
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
      mWebgl->DrawArrays(LOCAL_GL_TRIANGLE_STRIP, 0, 4);
      break;
    }
    default:
      gfxWarning() << "Unknown DrawTargetWebgl::DrawRect pattern type: "
                   << (int)aPattern.GetType();
      break;
  }
  // mWebgl->Disable(LOCAL_GL_BLEND);

  // Clean up any scissor state if there was clipping.
  if (intClip) {
    mWebgl->Disable(LOCAL_GL_SCISSOR_TEST);
  }

  return true;
}

bool DrawTargetWebgl::RemoveSharedTexture(
    const RefPtr<SharedTexture>& aTexture) {
  auto pos =
      std::find(mSharedTextures.begin(), mSharedTextures.end(), aTexture);
  if (pos == mSharedTextures.end()) {
    return false;
  }
  mTotalTextureMemory -= aTexture->UsedBytes();
  mSharedTextures.erase(pos);
  return true;
}

void SharedTextureHandle::Cleanup(DrawTargetWebgl& aDT) {
  mTexture->Free(*this);

  // Check if the shared handle's owning page has no more allocated handles
  // after we freed it. If so, remove the empty shared texture page also.
  if (!mTexture->HasAllocatedHandles()) {
    aDT.RemoveSharedTexture(mTexture);
  }
}

bool DrawTargetWebgl::RemoveStandaloneTexture(
    const RefPtr<StandaloneTexture>& aTexture) {
  auto pos = std::find(mStandaloneTextures.begin(), mStandaloneTextures.end(),
                       aTexture);
  if (pos == mStandaloneTextures.end()) {
    return false;
  }
  mTotalTextureMemory -= aTexture->UsedBytes();
  mStandaloneTextures.erase(pos);
  return true;
}

void StandaloneTexture::Cleanup(DrawTargetWebgl& aDT) {
  aDT.RemoveStandaloneTexture(this);
}

// Prune a given texture handle and release its associated resources.
void DrawTargetWebgl::PruneTextureHandle(RefPtr<TextureHandle> aHandle) {
  // Invalidate the handle so nothing will subsequently use its contents.
  aHandle->Invalidate();
  // If the handle has an associated SourceSurface, unlink it.
  UnlinkSurfaceTexture(aHandle);
  // If the handle has an associated GlyphCacheEntry, unlink it.
  if (RefPtr<GlyphCacheEntry> entry = aHandle->GetGlyphCacheEntry()) {
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
bool DrawTargetWebgl::PruneTextureMemory(size_t aMargin, bool aPruneUnused) {
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
  DrawRect(aRect, aPattern, aOptions);
}

void DrawTargetWebgl::Fill(const Path* aPath, const Pattern& aPattern,
                           const DrawOptions& aOptions) {
  if (!aPath || aPath->GetBackendType() != BackendType::SKIA) {
    return;
  }
  const auto& skiaPath = static_cast<const PathSkia*>(aPath)->GetPath();
  SkRect rect;
  if (skiaPath.isRect(&rect)) {
    DrawRect(SkRectToRect(rect), aPattern, aOptions);
  } else {
    MarkSkiaChanged();
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
    MarkSkiaChanged();
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
    MarkSkiaChanged();
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

void DrawTargetWebgl::DrawSurfaceWithShadow(SourceSurface* aSurface,
                                            const Point& aDest,
                                            const DeviceColor& aColor,
                                            const Point& aOffset, Float aSigma,
                                            CompositionOp aOperator) {
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
      if (DrawRect(Rect(IntRect(shadowOffset, size)), shadowMask,
                   DrawOptions(1.0f, aOperator), Some(aColor), &handle, false,
                   true) &&
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
      DrawRect(Rect(IntRect(surfaceOffset, size)), surfacePattern,
               DrawOptions(1.0f, aOperator), Nothing(), nullptr, false, true);
      return;
    }
  } else {
    // We found a cached shadow texture. Try to draw with it.
    IntPoint shadowOffset = IntPoint::Round(aDest + aOffset);
    SurfacePattern shadowMask(nullptr, ExtendMode::CLAMP,
                              Matrix::Translation(Point(shadowOffset)));
    if (DrawRect(Rect(IntRect(shadowOffset, size)), shadowMask,
                 DrawOptions(1.0f, aOperator), Some(aColor), &handle, false,
                 true, true)) {
      // If drawing the cached shadow texture succeeded, then proceed to draw
      // the original surface as well.
      IntPoint surfaceOffset = IntPoint::Round(aDest);
      SurfacePattern surfacePattern(aSurface, ExtendMode::CLAMP,
                                    Matrix::Translation(Point(surfaceOffset)));
      DrawRect(Rect(IntRect(surfaceOffset, size)), surfacePattern,
               DrawOptions(1.0f, aOperator), Nothing(), nullptr, false, true);
      return;
    }
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
  MarkSkiaChanged();
  mSkia->StrokeRect(aRect, aPattern, aStrokeOptions, aOptions);
}

void DrawTargetWebgl::StrokeLine(const Point& aStart, const Point& aEnd,
                                 const Pattern& aPattern,
                                 const StrokeOptions& aStrokeOptions,
                                 const DrawOptions& aOptions) {
  MarkSkiaChanged();
  mSkia->StrokeLine(aStart, aEnd, aPattern, aStrokeOptions, aOptions);
}

void DrawTargetWebgl::Stroke(const Path* aPath, const Pattern& aPattern,
                             const StrokeOptions& aStrokeOptions,
                             const DrawOptions& aOptions) {
  MarkSkiaChanged();
  mSkia->Stroke(aPath, aPattern, aStrokeOptions, aOptions);
}

void DrawTargetWebgl::StrokeGlyphs(ScaledFont* aFont,
                                   const GlyphBuffer& aBuffer,
                                   const Pattern& aPattern,
                                   const StrokeOptions& aStrokeOptions,
                                   const DrawOptions& aOptions) {
  MarkSkiaChanged();
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

// When caching text runs, we need to ensure the scale and orientation of the
// text is approximately the same. The offset will be considered separately.
static inline bool HasMatchingScale(const Matrix& aTransform1,
                                    const Matrix& aTransform2) {
  return FuzzyEqual(aTransform1._11, aTransform2._11) &&
         FuzzyEqual(aTransform1._12, aTransform2._12) &&
         FuzzyEqual(aTransform1._21, aTransform2._21) &&
         FuzzyEqual(aTransform1._22, aTransform2._22);
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
    : mColor(aColor),
      mTransform(aTransform),
      mBounds(aBounds),
      mHash(aHash ? aHash : HashGlyphs(aBuffer, aTransform)) {
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
    const Matrix& aTransform, const IntRect& aBounds, HashNumber aHash) {
  if (!aHash) {
    aHash = GlyphCacheEntry::HashGlyphs(aBuffer, aTransform);
  }
  for (const RefPtr<GlyphCacheEntry>& entry : mEntries) {
    if (entry->MatchesGlyphs(aBuffer, aColor, aTransform, aBounds, aHash)) {
      return do_AddRef(entry);
    }
  }
  RefPtr<GlyphCacheEntry> entry =
      new GlyphCacheEntry(aBuffer, aColor, aTransform, aBounds, aHash);
  mEntries.insertFront(entry);
  return entry.forget();
}

GlyphCache::GlyphCache(ScaledFont* aFont) : mFont(aFont) {}

void GlyphCacheEntry::Link(const RefPtr<TextureHandle>& aHandle) {
  mHandle = aHandle;
  mHandle->SetGlyphCacheEntry(this);
}

// When the GlyphCacheEntry becomes unused, it marks the corresponding
// TextureHandle as unused and unlinks it from the GlyphCacheEntry. The
// entry is removed from its containing GlyphCache, if applicable.
void GlyphCacheEntry::Unlink() {
  if (isInList()) {
    remove();
  }
  // The entry may not have a valid handle if rasterization failed.
  if (mHandle) {
    mHandle->SetGlyphCacheEntry(nullptr);
    mHandle = nullptr;
  }
}

GlyphCache::~GlyphCache() {
  while (RefPtr<GlyphCacheEntry> entry = mEntries.popLast()) {
    entry->Unlink();
  }
}

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

void DrawTargetWebgl::FillGlyphs(ScaledFont* aFont, const GlyphBuffer& aBuffer,
                                 const Pattern& aPattern,
                                 const DrawOptions& aOptions) {
  // Draws glyphs to the WebGL target by trying to generate a cached texture for
  // the text run that can be subsequently reused to quickly render the text run
  // without using any software surfaces.
  if (!aFont || !aBuffer.mNumGlyphs) {
    return;
  }
  if (mWebglValid && SupportsDrawOptions(aOptions) &&
      aPattern.GetType() == PatternType::COLOR) {
    // Get the local bounds of the text run.
    Maybe<Rect> bounds =
        mSkia->GetGlyphLocalBounds(aFont, aBuffer, aPattern, nullptr, aOptions);
    if (bounds) {
      // Transform the local bounds into device space so that we know how big
      // the cached texture will be.
      Rect xformBounds =
          mTransform.TransformBounds(*bounds).Intersect(Rect(mSkia->GetRect()));
      if (xformBounds.IsEmpty()) {
        return;
      }
      IntRect intBounds = RoundedOut(xformBounds);

      // Determine what type of surface we will need to render the text run to.
      // Grayscale AA can render to an A8 texture, whereas subpixel AA requires
      // rendering the subpixel masks to a BGRA texture.
      AntialiasMode aaMode = aFont->GetDefaultAAMode();
      if (aOptions.mAntialiasMode != AntialiasMode::DEFAULT) {
        aaMode = aOptions.mAntialiasMode;
      }
      bool useSubpixelAA =
          GetPermitSubpixelAA() && (aaMode == AntialiasMode::DEFAULT ||
                                    aaMode == AntialiasMode::SUBPIXEL);
      // Whether to render the text as a full color result as opposed to as a
      // grayscale mask. Subpixel AA or fonts with color glyphs require this.
      // We currently have to check to check the rasterized result to see if
      // there are any color glyphs as there is not yet a way to a priori know
      // this.
      bool useColor = useSubpixelAA;

      // Look for an existing glyph cache on the font. If not there, create it.
      GlyphCache* cache =
          static_cast<GlyphCache*>(aFont->GetUserData(&mGlyphCacheKey));
      if (!cache) {
        cache = new GlyphCache(aFont);
        aFont->AddUserData(&mGlyphCacheKey, cache, ReleaseGlyphCache);
        mGlyphCaches.insertFront(cache);
      }
      // Hash the incoming text run and looking for a matching entry.
      HashNumber hash = GlyphCacheEntry::HashGlyphs(aBuffer, mTransform);
      DeviceColor color = static_cast<const ColorPattern&>(aPattern).mColor;
      color.a *= aOptions.mAlpha;
      DeviceColor aaColor =
          useColor ? color : DeviceColor(1.0f, 1.0f, 1.0f, 1.0f);
      RefPtr<GlyphCacheEntry> entry = cache->FindOrInsertEntry(
          aBuffer, aaColor, mTransform, intBounds, hash);
      if (entry) {
        RefPtr<TextureHandle> handle = entry->GetHandle();
        if (handle && handle->IsValid()) {
          // If there is an entry with a valid cached texture handle, then try
          // to draw with that. If that for some reason failed, then fall back
          // to using the Skia target as that means we were preventing from
          // drawing to the WebGL context based on something other than the
          // texture.
          SurfacePattern pattern(nullptr, ExtendMode::CLAMP,
                                 Matrix::Translation(intBounds.TopLeft()));
          if (!DrawRect(Rect(intBounds), pattern, aOptions,
                        handle->GetFormat() == SurfaceFormat::A8 ? Some(color)
                                                                 : Nothing(),
                        &handle, false, true, true)) {
            MarkSkiaChanged();
            mSkia->FillGlyphs(aFont, aBuffer, aPattern, aOptions);
          }
          return;
        }
        handle = nullptr;

        // If we get here, either there wasn't a cached texture handle or it
        // wasn't valid. Render the text run into a temporary target.
        RefPtr<DrawTargetSkia> textDT = new DrawTargetSkia;
        if (textDT->Init(intBounds.Size(), SurfaceFormat::B8G8R8A8)) {
          textDT->SetTransform(mTransform *
                               Matrix::Translation(-intBounds.TopLeft()));
          textDT->FillGlyphs(aFont, aBuffer, ColorPattern(aaColor));
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
                  return;
                }
              }
            }
            // Attempt to upload the rendered text surface into a texture
            // handle and draw it.
            SurfacePattern pattern(textSurface, ExtendMode::CLAMP,
                                   Matrix::Translation(intBounds.TopLeft()));
            if (DrawRect(Rect(intBounds), pattern, aOptions,
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
            return;
          }
        }
      }
    }
  }

  // If not able to cache the text run to a texture, then just fall back to
  // drawing with the Skia target.
  MarkSkiaChanged();
  mSkia->FillGlyphs(aFont, aBuffer, aPattern, aOptions);
}

// Attempts to read the contents of the WebGL context into the Skia target.
void DrawTargetWebgl::ReadIntoSkia() {
  if (mSkiaValid) {
    return;
  }
  if (mWebglValid && !mWebgl->IsContextLost()) {
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
}

// Attempts to draw the contents of the Skia target into the WebGL context.
bool DrawTargetWebgl::FlushFromSkia() {
  // If the WebGL context has been lost, then mark it as invalid and fail.
  if (mWebgl->IsContextLost()) {
    mWebglValid = false;
    return false;
  }
  if (mWebglValid) {
    return true;
  }
  mWebglValid = true;
  if (mSkiaValid) {
    RefPtr<SourceSurface> skiaSnapshot = mSkia->Snapshot();
    if (skiaSnapshot) {
      SurfacePattern pattern(skiaSnapshot, ExtendMode::CLAMP);
      DrawRect(Rect(GetRect()), pattern,
               DrawOptions(1.0f, CompositionOp::OP_SOURCE), Nothing(),
               &mSnapshotTexture, false, false, false, true);
    }
  }
  return true;
}

// For use within CanvasRenderingContext2D, Flush is only called at the end of
// a frame. This ensures the WebGL context is ready and presents it.
void DrawTargetWebgl::Flush() {
  // If still rendering into the Skia target, switch back to the WebGL context.
  if (!mWebglValid) {
    FlushFromSkia();
  }
  // Present the WebGL context.
  mWebgl->OnBeforePaintTransaction();
  // Ensure we're not somehow using more than the allowed texture memory.
  PruneTextureMemory();
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
  MarkSkiaChanged();
  mSkia->DrawFilter(aNode, aSourceRect, aDestPoint, aOptions);
}

bool DrawTargetWebgl::Draw3DTransformedSurface(SourceSurface* aSurface,
                                               const Matrix4x4& aMatrix) {
  MarkSkiaChanged();
  return mSkia->Draw3DTransformedSurface(aSurface, aMatrix);
}

}  // namespace mozilla::gfx
