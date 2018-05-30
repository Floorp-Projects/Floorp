/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * An interface for objects which can either store a surface or dynamically
 * generate one, and various implementations.
 */

#ifndef mozilla_image_ISurfaceProvider_h
#define mozilla_image_ISurfaceProvider_h

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/NotNull.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Variant.h"
#include "mozilla/gfx/2D.h"

#include "imgFrame.h"
#include "SurfaceCache.h"

namespace mozilla {
namespace image {

class CachedSurface;
class DrawableSurface;

/**
 * An interface for objects which can either store a surface or dynamically
 * generate one.
 */
class ISurfaceProvider
{
public:
  // Subclasses may or may not be XPCOM classes, so we just require that they
  // implement AddRef and Release.
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  /// @return key data used for identifying which image this ISurfaceProvider is
  /// associated with in the surface cache.
  ImageKey GetImageKey() const { return mImageKey; }

  /// @return key data used to uniquely identify this ISurfaceProvider's cache
  /// entry in the surface cache.
  const SurfaceKey& GetSurfaceKey() const { return mSurfaceKey; }

  /// @return a (potentially lazily computed) drawable reference to a surface.
  virtual DrawableSurface Surface();

  /// @return true if DrawableRef() will return a completely decoded surface.
  virtual bool IsFinished() const = 0;

  /// @return true if the underlying decoder is currently fully decoded. For
  /// animated images, this means that at least every frame has been decoded
  /// at least once. It does not guarantee that all of the frames are present,
  /// as the surface provider has the option to discard as it deems necessary.
  virtual bool IsFullyDecoded() const { return IsFinished(); }

  /// @return the number of bytes of memory this ISurfaceProvider is expected to
  /// require. Optimizations may result in lower real memory usage. Trivial
  /// overhead is ignored. Because this value is used in bookkeeping, it's
  /// important that it be constant over the lifetime of this object.
  virtual size_t LogicalSizeInBytes() const = 0;

  /// @return the actual number of bytes of memory this ISurfaceProvider is
  /// using. May vary over the lifetime of the ISurfaceProvider. The default
  /// implementation is appropriate for static ISurfaceProviders.
  virtual void AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                                      size_t& aHeapSizeOut,
                                      size_t& aNonHeapSizeOut,
                                      size_t& aExtHandlesOut)
  {
    DrawableFrameRef ref = DrawableRef(/* aFrame = */ 0);
    if (!ref) {
      return;
    }

    ref->AddSizeOfExcludingThis(aMallocSizeOf, aHeapSizeOut,
                                aNonHeapSizeOut, aExtHandlesOut);
  }

  virtual void Reset() { }
  virtual void Advance(size_t aFrame) { }

  /// @return the availability state of this ISurfaceProvider, which indicates
  /// whether DrawableRef() could successfully return a surface. Should only be
  /// called from SurfaceCache code as it relies on SurfaceCache for
  /// synchronization.
  AvailabilityState& Availability() { return mAvailability; }
  const AvailabilityState& Availability() const { return mAvailability; }

protected:
  ISurfaceProvider(const ImageKey aImageKey,
                   const SurfaceKey& aSurfaceKey,
                   AvailabilityState aAvailability)
    : mImageKey(aImageKey)
    , mSurfaceKey(aSurfaceKey)
    , mAvailability(aAvailability)
  {
    MOZ_ASSERT(aImageKey, "Must have a valid image key");
  }

  virtual ~ISurfaceProvider() { }

  /// @return an eagerly computed drawable reference to a surface. For
  /// dynamically generated animation surfaces, @aFrame specifies the 0-based
  /// index of the desired frame.
  virtual DrawableFrameRef DrawableRef(size_t aFrame) = 0;

  /// @return an eagerly computed raw access reference to a surface. For
  /// dynamically generated animation surfaces, @aFrame specifies the 0-based
  /// index of the desired frame.
  virtual RawAccessFrameRef RawAccessRef(size_t aFrame)
  {
    MOZ_ASSERT_UNREACHABLE("Surface provider does not support raw access!");
    return RawAccessFrameRef();
  }

  /// @return true if this ISurfaceProvider is locked. (@see SetLocked())
  /// Should only be called from SurfaceCache code as it relies on SurfaceCache
  /// for synchronization.
  virtual bool IsLocked() const = 0;

  /// If @aLocked is true, hint that this ISurfaceProvider is in use and it
  /// should avoid releasing its resources. Should only be called from
  /// SurfaceCache code as it relies on SurfaceCache for synchronization.
  virtual void SetLocked(bool aLocked) = 0;

private:
  friend class CachedSurface;
  friend class DrawableSurface;

  const ImageKey mImageKey;
  const SurfaceKey mSurfaceKey;
  AvailabilityState mAvailability;
};


/**
 * A reference to a surface (stored in an imgFrame) that holds the surface in
 * memory, guaranteeing that it can be drawn. If you have a DrawableSurface
 * |surf| and |if (surf)| returns true, then calls to |surf->Draw()| and
 * |surf->GetSourceSurface()| are guaranteed to succeed.
 *
 * Note that the surface may be computed lazily, so a DrawableSurface should not
 * be dereferenced (i.e., operator->() should not be called) until you're
 * sure that you want to draw it.
 */
class MOZ_STACK_CLASS DrawableSurface final
{
public:
  DrawableSurface() : mHaveSurface(false) { }

  explicit DrawableSurface(DrawableFrameRef&& aDrawableRef)
    : mDrawableRef(std::move(aDrawableRef))
    , mHaveSurface(bool(mDrawableRef))
  { }

  explicit DrawableSurface(NotNull<ISurfaceProvider*> aProvider)
    : mProvider(aProvider)
    , mHaveSurface(true)
  { }

  DrawableSurface(DrawableSurface&& aOther)
    : mDrawableRef(std::move(aOther.mDrawableRef))
    , mProvider(std::move(aOther.mProvider))
    , mHaveSurface(aOther.mHaveSurface)
  {
    aOther.mHaveSurface = false;
  }

  DrawableSurface& operator=(DrawableSurface&& aOther)
  {
    MOZ_ASSERT(this != &aOther, "Self-moves are prohibited");
    mDrawableRef = std::move(aOther.mDrawableRef);
    mProvider = std::move(aOther.mProvider);
    mHaveSurface = aOther.mHaveSurface;
    aOther.mHaveSurface = false;
    return *this;
  }

  /**
   * If this DrawableSurface is dynamically generated from an animation, attempt
   * to seek to frame @aFrame, where @aFrame is a 0-based index into the frames
   * of the animation. Otherwise, nothing will blow up at runtime, but we assert
   * in debug builds, since calling this in an unexpected situation probably
   * indicates a bug.
   *
   * @return a successful result if we could obtain frame @aFrame. Note that
   * |mHaveSurface| being true means that we're guaranteed to have *some* frame,
   * so the caller can dereference this DrawableSurface even if Seek() fails,
   * but while nothing will blow up, the frame won't be the one they expect.
   */
  nsresult Seek(size_t aFrame)
  {
    MOZ_ASSERT(mHaveSurface, "Trying to seek an empty DrawableSurface?");

    if (!mProvider) {
      MOZ_ASSERT_UNREACHABLE("Trying to seek a static DrawableSurface?");
      return NS_ERROR_FAILURE;
    }

    mDrawableRef = mProvider->DrawableRef(aFrame);

    return mDrawableRef ? NS_OK : NS_ERROR_FAILURE;
  }

  RawAccessFrameRef RawAccessRef(size_t aFrame)
  {
    MOZ_ASSERT(mHaveSurface, "Trying to get on an empty DrawableSurface?");

    if (!mProvider) {
      MOZ_ASSERT_UNREACHABLE("Trying to get on a static DrawableSurface?");
      return RawAccessFrameRef();
    }

    return mProvider->RawAccessRef(aFrame);
  }

  void Reset()
  {
    if (!mProvider) {
      MOZ_ASSERT_UNREACHABLE("Trying to reset a static DrawableSurface?");
      return;
    }

    mProvider->Reset();
  }

  void Advance(size_t aFrame)
  {
    if (!mProvider) {
      MOZ_ASSERT_UNREACHABLE("Trying to advance a static DrawableSurface?");
      return;
    }

    mProvider->Advance(aFrame);
  }

  bool IsFullyDecoded() const
  {
    if (!mProvider) {
      MOZ_ASSERT_UNREACHABLE("Trying to check decoding state of a static DrawableSurface?");
      return false;
    }

    return mProvider->IsFullyDecoded();
  }

  explicit operator bool() const { return mHaveSurface; }
  imgFrame* operator->() { return DrawableRef().get(); }

private:
  DrawableSurface(const DrawableSurface& aOther) = delete;
  DrawableSurface& operator=(const DrawableSurface& aOther) = delete;

  DrawableFrameRef& DrawableRef()
  {
    MOZ_ASSERT(mHaveSurface);

    // If we weren't created with a DrawableFrameRef directly, we should've been
    // created with an ISurfaceProvider which can give us one. Note that if
    // Seek() has been called, we'll already have a DrawableFrameRef, so we
    // won't need to get one here.
    if (!mDrawableRef) {
      MOZ_ASSERT(mProvider);
      mDrawableRef = mProvider->DrawableRef(/* aFrame = */ 0);
    }

    MOZ_ASSERT(mDrawableRef);
    return mDrawableRef;
  }

  DrawableFrameRef mDrawableRef;
  RefPtr<ISurfaceProvider> mProvider;
  bool mHaveSurface;
};


// Surface() is implemented here so that DrawableSurface's definition is
// visible. This default implementation eagerly obtains a DrawableFrameRef for
// the first frame and is intended for static ISurfaceProviders.
inline DrawableSurface
ISurfaceProvider::Surface()
{
  return DrawableSurface(DrawableRef(/* aFrame = */ 0));
}


/**
 * An ISurfaceProvider that stores a single surface.
 */
class SimpleSurfaceProvider final : public ISurfaceProvider
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SimpleSurfaceProvider, override)

  SimpleSurfaceProvider(const ImageKey aImageKey,
                        const SurfaceKey& aSurfaceKey,
                        NotNull<imgFrame*> aSurface)
    : ISurfaceProvider(aImageKey, aSurfaceKey,
                       AvailabilityState::StartAvailable())
    , mSurface(aSurface)
  {
    MOZ_ASSERT(aSurfaceKey.Size() == mSurface->GetSize());
  }

  bool IsFinished() const override { return mSurface->IsFinished(); }

  size_t LogicalSizeInBytes() const override
  {
    gfx::IntSize size = mSurface->GetSize();
    return size.width * size.height * mSurface->GetBytesPerPixel();
  }

protected:
  DrawableFrameRef DrawableRef(size_t aFrame) override
  {
    MOZ_ASSERT(aFrame == 0,
               "Requesting an animation frame from a SimpleSurfaceProvider?");
    return mSurface->DrawableRef();
  }

  bool IsLocked() const override { return bool(mLockRef); }

  void SetLocked(bool aLocked) override
  {
    if (aLocked == IsLocked()) {
      return;  // Nothing changed.
    }

    // If we're locked, hold a DrawableFrameRef to |mSurface|, which will keep
    // any volatile buffer it owns in memory.
    mLockRef = aLocked ? mSurface->DrawableRef()
                       : DrawableFrameRef();
  }

private:
  virtual ~SimpleSurfaceProvider() { }

  NotNull<RefPtr<imgFrame>> mSurface;
  DrawableFrameRef mLockRef;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_ISurfaceProvider_h
