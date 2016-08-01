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

#include "mozilla/Maybe.h"
#include "mozilla/NotNull.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Variant.h"
#include "mozilla/gfx/2D.h"

#include "imgFrame.h"
#include "SurfaceCache.h"

namespace mozilla {
namespace image {

/**
 * An interface for objects which can either store a surface or dynamically
 * generate one.
 */
class ISurfaceProvider
{
public:
  // Subclasses may or may not be XPCOM classes, so we just require that they
  // implement AddRef and Release.
  NS_IMETHOD_(MozExternalRefCountType) AddRef() = 0;
  NS_IMETHOD_(MozExternalRefCountType) Release() = 0;

  /// @return a drawable reference to a surface.
  virtual DrawableFrameRef DrawableRef() = 0;

  /// @return true if DrawableRef() will return a completely decoded surface.
  virtual bool IsFinished() const = 0;

  /// @return true if this ISurfaceProvider is locked. (@see SetLocked())
  virtual bool IsLocked() const = 0;

  /// If @aLocked is true, hint that this ISurfaceProvider is in use and it
  /// should avoid releasing its resources.
  virtual void SetLocked(bool aLocked) = 0;

  /// @return the number of bytes of memory this ISurfaceProvider is expected to
  /// require. Optimizations may result in lower real memory usage. Trivial
  /// overhead is ignored.
  virtual size_t LogicalSizeInBytes() const = 0;

  /// @return the availability state of this ISurfaceProvider, which indicates
  /// whether DrawableRef() could successfully return a surface. Should only be
  /// called from SurfaceCache code as it relies on SurfaceCache for
  /// synchronization.
  AvailabilityState& Availability() { return mAvailability; }
  const AvailabilityState& Availability() const { return mAvailability; }

protected:
  explicit ISurfaceProvider(AvailabilityState aAvailability)
    : mAvailability(aAvailability)
  { }

  virtual ~ISurfaceProvider() { }

private:
  AvailabilityState mAvailability;
};

/**
 * An ISurfaceProvider that stores a single surface.
 */
class SimpleSurfaceProvider final : public ISurfaceProvider
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SimpleSurfaceProvider, override)

  explicit SimpleSurfaceProvider(NotNull<imgFrame*> aSurface)
    : ISurfaceProvider(AvailabilityState::StartAvailable())
    , mSurface(aSurface)
  { }

  DrawableFrameRef DrawableRef() override { return mSurface->DrawableRef(); }
  bool IsFinished() const override { return mSurface->IsFinished(); }
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

  size_t LogicalSizeInBytes() const override
  {
    gfx::IntSize size = mSurface->GetSize();
    return size.width * size.height * mSurface->GetBytesPerPixel();
  }

private:
  virtual ~SimpleSurfaceProvider() { }

  NotNull<RefPtr<imgFrame>> mSurface;
  DrawableFrameRef mLockRef;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_ISurfaceProvider_h
