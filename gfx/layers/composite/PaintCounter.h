/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_PaintCounter_h_
#define mozilla_layers_PaintCounter_h_

#include <memory>
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"     // for already_AddRefed, RefCounted
#include "mozilla/TimeStamp.h"  // for TimeStamp, TimeDuration
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/Types.h"
#include "nsISupports.h"

class SkCanvas;

namespace mozilla {
namespace layers {

class Compositor;
class DataTextureSource;
struct TexturedEffect;

// Keeps track and paints how long a full invalidation paint takes to rasterize
// and composite.
class PaintCounter {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PaintCounter)

  PaintCounter();
  void Draw(Compositor* aCompositor, TimeDuration aPaintTime,
            TimeDuration aCompositeTime);
  static gfx::IntRect GetPaintRect() { return PaintCounter::mRect; }

 private:
  virtual ~PaintCounter();

  gfx::SurfaceFormat mFormat;
  std::unique_ptr<SkCanvas> mCanvas;
  gfx::IntSize mSize;
  int mStride;

  RefPtr<gfx::DataSourceSurface> mSurface;
  RefPtr<DataTextureSource> mTextureSource;
  RefPtr<TexturedEffect> mTexturedEffect;
  Maybe<gfx::DataSourceSurface::ScopedMap> mMap;
  static gfx::IntRect mRect;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_opengl_PaintCounter_h_
