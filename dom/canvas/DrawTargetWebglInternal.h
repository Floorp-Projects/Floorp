/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_GFX_DRAWTARGETWEBGL_INTERNAL_H
#define _MOZILLA_GFX_DRAWTARGETWEBGL_INTERNAL_H

#include "DrawTargetWebgl.h"

#include "mozilla/HashFunctions.h"
#include "mozilla/gfx/PathSkia.h"

namespace mozilla::gfx {

// TexturePacker implements a bin-packing algorithm for 2D rectangles. It uses
// a binary tree that partitions the space of a node at a given split. This
// produces two children, one on either side of the split. This subdivision
// proceeds recursively as necessary.
class TexturePacker {
 public:
  explicit TexturePacker(const IntRect& aBounds, bool aAvailable = true)
      : mBounds(aBounds),
        mAvailable(aAvailable ? std::min(aBounds.width, aBounds.height) : 0) {}

  Maybe<IntPoint> Insert(const IntSize& aSize);

  bool Remove(const IntRect& aBounds);

  const IntRect& GetBounds() const { return mBounds; }

 private:
  bool IsLeaf() const { return !mChildren[0]; }
  bool IsFullyAvailable() const { return IsLeaf() && mAvailable > 0; }

  void DiscardChildren() {
    mChildren[0] = nullptr;
    mChildren[1] = nullptr;
  }

  // If applicable, the two children produced by picking a single axis split
  // within the node's bounds and subdividing the bounds there.
  UniquePtr<TexturePacker> mChildren[2];
  // The bounds enclosing this node and any children within it.
  IntRect mBounds;
  // For a leaf node, specifies the size of the smallest dimension available to
  // allocate. For a branch node, specifies largest potential available size of
  // all children. This can be used during the allocation process to rapidly
  // reject certain sub-trees without having to search all the way to a leaf
  // node if we know that largest available size within the sub-tree wouldn't
  // fit the requested size.
  int mAvailable = 0;
};

// CacheEnty is a generic interface for various items that need to be cached to
// a texture.
class CacheEntry : public RefCounted<CacheEntry> {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(CacheEntry)

  CacheEntry(const Matrix& aTransform, const IntRect& aBounds, HashNumber aHash)
      : mTransform(aTransform), mBounds(aBounds), mHash(aHash) {}
  virtual ~CacheEntry() = default;

  void Link(const RefPtr<TextureHandle>& aHandle);
  void Unlink();

  const RefPtr<TextureHandle>& GetHandle() const { return mHandle; }

  const Matrix& GetTransform() const { return mTransform; }
  const IntRect& GetBounds() const { return mBounds; }
  HashNumber GetHash() const { return mHash; }

  virtual bool IsValid() const { return true; }

 protected:
  virtual void RemoveFromList() = 0;

  // The handle of the rendered cache item.
  RefPtr<TextureHandle> mHandle;
  // The transform that was used to render the entry. This is necessary as
  // the geometry might only be correctly rendered in device space after
  // the transform is applied, so in general we can't cache untransformed
  // geometry.
  Matrix mTransform;
  // The device space bounds of the rendered geometry.
  IntRect mBounds;
  // A hash of the geometry that may be used for quickly rejecting entries.
  HashNumber mHash;
};

// CacheEntryImpl provides type-dependent boilerplate code for implementations
// of CacheEntry.
template <typename T>
class CacheEntryImpl : public CacheEntry, public LinkedListElement<RefPtr<T>> {
  typedef LinkedListElement<RefPtr<T>> ListType;

 public:
  CacheEntryImpl(const Matrix& aTransform, const IntRect& aBounds,
                 HashNumber aHash)
      : CacheEntry(aTransform, aBounds, aHash) {}

 protected:
  void RemoveFromList() override {
    if (ListType::isInList()) {
      ListType::remove();
    }
  }
};

// CacheImpl manages a list of CacheEntry.
template <typename T>
class CacheImpl {
 public:
  ~CacheImpl() {
    while (RefPtr<T> entry = mEntries.popLast()) {
      entry->Unlink();
    }
  }

 protected:
  LinkedList<RefPtr<T>> mEntries;
};

// TextureHandle is an abstract base class for supplying textures to drawing
// commands that may be backed by different resource types (such as a shared
// or standalone texture). It may be further linked to use-specific metadata
// such as for shadow drawing or for cached entries in the glyph cache.
class TextureHandle : public RefCounted<TextureHandle>,
                      public LinkedListElement<RefPtr<TextureHandle>> {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(TextureHandle)

  enum Type { SHARED, STANDALONE };

  virtual Type GetType() const = 0;
  virtual const RefPtr<WebGLTextureJS>& GetWebGLTexture() const = 0;
  virtual IntRect GetBounds() const = 0;
  IntSize GetSize() const { return GetBounds().Size(); }
  virtual IntSize GetBackingSize() const = 0;
  virtual SurfaceFormat GetFormat() const = 0;
  virtual size_t UsedBytes() const = 0;

  static inline size_t UsedBytes(SurfaceFormat aFormat, const IntSize& aSize) {
    return size_t(BytesPerPixel(aFormat)) * size_t(aSize.width) *
           size_t(aSize.height);
  }

  virtual void UpdateSize(const IntSize& aSize) {}

  virtual void Cleanup(DrawTargetWebgl::SharedContext& aContext) {}

  virtual ~TextureHandle() {}

  bool IsValid() const { return mValid; }
  void Invalidate() { mValid = false; }

  void SetSurface(SourceSurface* aSurface) { mSurface = aSurface; }
  SourceSurface* GetSurface() const { return mSurface; }

  float GetSigma() const { return mSigma; }
  void SetSigma(float aSigma) { mSigma = aSigma; }
  bool IsShadow() const { return mSigma >= 0.0f; }

  void SetSamplingOffset(const IntPoint& aSamplingOffset) {
    mSamplingOffset = aSamplingOffset;
  }
  const IntPoint& GetSamplingOffset() const { return mSamplingOffset; }
  IntRect GetSamplingRect() const {
    return IntRect(GetSamplingOffset(), GetSize());
  }

  const RefPtr<CacheEntry>& GetCacheEntry() const { return mCacheEntry; }
  void SetCacheEntry(const RefPtr<CacheEntry>& aEntry) { mCacheEntry = aEntry; }

  // Note as used if there is corresponding surface or cache entry.
  bool IsUsed() const {
    return mSurface || (mCacheEntry && mCacheEntry->IsValid());
  }

 private:
  bool mValid = true;
  // If applicable, weak pointer to the SourceSurface that is linked to this
  // TextureHandle.
  SourceSurface* mSurface = nullptr;
  // If this TextureHandle stores a cached shadow, then we need to remember the
  // blur sigma used to produce the shadow.
  float mSigma = -1.0f;
  // If the originating surface requested a sampling rect, then we need to know
  // the offset of the subrect within the surface for texture coordinates.
  IntPoint mSamplingOffset;
  // If applicable, the CacheEntry that is linked to this TextureHandle.
  RefPtr<CacheEntry> mCacheEntry;
};

class SharedTextureHandle;

// SharedTexture is a large slab texture that is subdivided (by using a
// TexturePacker) to hold many small SharedTextureHandles. This avoids needing
// to allocate many WebGL textures for every single small Canvas 2D texture.
class SharedTexture : public RefCounted<SharedTexture> {
 public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(SharedTexture)

  SharedTexture(const IntSize& aSize, SurfaceFormat aFormat,
                const RefPtr<WebGLTextureJS>& aTexture);

  already_AddRefed<SharedTextureHandle> Allocate(const IntSize& aSize);
  bool Free(const SharedTextureHandle& aHandle);

  SurfaceFormat GetFormat() const { return mFormat; }
  IntSize GetSize() const { return mPacker.GetBounds().Size(); }

  size_t UsedBytes() const {
    return TextureHandle::UsedBytes(GetFormat(), GetSize());
  }

  bool HasAllocatedHandles() const { return mAllocatedHandles > 0; }

  const RefPtr<WebGLTextureJS>& GetWebGLTexture() const { return mTexture; }

 private:
  TexturePacker mPacker;
  SurfaceFormat mFormat;
  RefPtr<WebGLTextureJS> mTexture;
  size_t mAllocatedHandles = 0;
};

// SharedTextureHandle is an allocated region within a large SharedTexture page
// that owns it.
class SharedTextureHandle : public TextureHandle {
  friend class SharedTexture;

 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(SharedTextureHandle, override)

  SharedTextureHandle(const IntRect& aBounds, SharedTexture* aTexture);

  Type GetType() const override { return Type::SHARED; }

  const RefPtr<WebGLTextureJS>& GetWebGLTexture() const override {
    return mTexture->GetWebGLTexture();
  }

  IntRect GetBounds() const override { return mBounds; }
  IntSize GetBackingSize() const override { return mTexture->GetSize(); }

  SurfaceFormat GetFormat() const override { return mTexture->GetFormat(); }

  size_t UsedBytes() const override {
    return TextureHandle::UsedBytes(GetFormat(), mBounds.Size());
  }

  void Cleanup(DrawTargetWebgl::SharedContext& aContext) override;

  const RefPtr<SharedTexture>& GetOwner() const { return mTexture; }

 private:
  IntRect mBounds;
  RefPtr<SharedTexture> mTexture;
};

// StandaloneTexture is a texture that can not be effectively shared within
// a SharedTexture page, such that it is better to assign it its own WebGL
// texture.
class StandaloneTexture : public TextureHandle {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(StandaloneTexture, override)

  StandaloneTexture(const IntSize& aSize, SurfaceFormat aFormat,
                    const RefPtr<WebGLTextureJS>& aTexture);

  Type GetType() const override { return Type::STANDALONE; }

  SurfaceFormat GetFormat() const override { return mFormat; }

  const RefPtr<WebGLTextureJS>& GetWebGLTexture() const override {
    return mTexture;
  }

  IntRect GetBounds() const override { return IntRect(IntPoint(0, 0), mSize); }
  IntSize GetBackingSize() const override { return mSize; }

  size_t UsedBytes() const override {
    return TextureHandle::UsedBytes(mFormat, mSize);
  }

  void UpdateSize(const IntSize& aSize) override { mSize = aSize; }

  void Cleanup(DrawTargetWebgl::SharedContext& aContext) override;

 private:
  IntSize mSize;
  SurfaceFormat mFormat;
  RefPtr<WebGLTextureJS> mTexture;
};

// GlyphCacheEntry stores rendering metadata for a rendered text run, as well
// the handle to the texture it was rendered into, so that it can be located
// for reuse under similar rendering circumstances.
class GlyphCacheEntry : public CacheEntryImpl<GlyphCacheEntry> {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(GlyphCacheEntry, override)

  GlyphCacheEntry(const GlyphBuffer& aBuffer, const DeviceColor& aColor,
                  const Matrix& aTransform, const IntRect& aBounds,
                  HashNumber aHash);
  ~GlyphCacheEntry();

  const GlyphBuffer& GetGlyphBuffer() const { return mBuffer; }

  bool MatchesGlyphs(const GlyphBuffer& aBuffer, const DeviceColor& aColor,
                     const Matrix& aTransform, const IntRect& aBounds,
                     HashNumber aHash);

  static HashNumber HashGlyphs(const GlyphBuffer& aBuffer,
                               const Matrix& aTransform);

 private:
  // The glyph keys used to render the text run.
  GlyphBuffer mBuffer = {nullptr, 0};
  // The color of the text run.
  DeviceColor mColor;
};

// GlyphCache maintains a list of GlyphCacheEntry's representing previously
// rendered text runs. The cache is searched to see if a given incoming text
// run has already been rendered to a texture, and if so, just reuses it.
// Otherwise, the text run will be rendered to a new texture handle and
// inserted into a new GlyphCacheEntry to represent it.
class GlyphCache : public LinkedListElement<GlyphCache>,
                   public CacheImpl<GlyphCacheEntry> {
 public:
  explicit GlyphCache(ScaledFont* aFont);

  ScaledFont* GetFont() const { return mFont; }

  already_AddRefed<GlyphCacheEntry> FindOrInsertEntry(
      const GlyphBuffer& aBuffer, const DeviceColor& aColor,
      const Matrix& aTransform, const IntRect& aBounds);

 private:
  // Weak pointer to the owning font
  ScaledFont* mFont;
};

// PathCacheEntry stores a rasterized version of a supplied path with a given
// pattern.
class PathCacheEntry : public CacheEntryImpl<PathCacheEntry> {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(PathCacheEntry, override)

  PathCacheEntry(const SkPath& aPath, Pattern* aPattern,
                 StoredStrokeOptions* aStrokeOptions, const Matrix& aTransform,
                 const IntRect& aBounds, const Point& aOrigin, HashNumber aHash,
                 float aSigma = -1.0f);

  bool MatchesPath(const SkPath& aPath, const Pattern* aPattern,
                   const StrokeOptions* aStrokeOptions,
                   const Matrix& aTransform, const IntRect& aBounds,
                   const Point& aOrigin, HashNumber aHash, float aSigma);

  static HashNumber HashPath(const SkPath& aPath, const Pattern* aPattern,
                             const Matrix& aTransform, const IntRect& aBounds);

  const Point& GetOrigin() const { return mOrigin; }

  // Valid if either a mask (no pattern) or there is valid pattern.
  bool IsValid() const override { return !mPattern || mPattern->IsValid(); }

 private:
  // The actual path geometry supplied
  SkPath mPath;
  // The transformed origin of the path
  Point mOrigin;
  // The pattern used to rasterize the path, if not a mask
  UniquePtr<Pattern> mPattern;
  // The StrokeOptions used for stroked paths, if applicable
  UniquePtr<StoredStrokeOptions> mStrokeOptions;
  // The shadow blur sigma
  float mSigma;
};

class PathCache : public CacheImpl<PathCacheEntry> {
 public:
  PathCache() = default;

  already_AddRefed<PathCacheEntry> FindOrInsertEntry(
      const SkPath& aPath, const Pattern* aPattern,
      const StrokeOptions* aStrokeOptions, const Matrix& aTransform,
      const IntRect& aBounds, const Point& aOrigin, float aSigma = -1.0f);
};

}  // namespace mozilla::gfx

#endif  // _MOZILLA_GFX_DRAWTARGETWEBGL_INTERNAL_H
