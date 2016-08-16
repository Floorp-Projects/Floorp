/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_PaintCounter_h_
#define mozilla_layers_PaintCounter_h_

#include <map>                          // for std::map
#include "mozilla/RefPtr.h"             // for already_AddRefed, RefCounted
#include "mozilla/TimeStamp.h"          // for TimeStamp, TimeDuration
#include "skia/include/core/SkCanvas.h"
#include "mozilla/gfx/HelpersSkia.h"

namespace mozilla {
namespace layers {

class Compositor;

using namespace mozilla::gfx;
using namespace mozilla::gl;

// Keeps track and paints how long a full invalidation paint takes to rasterize
// and composite.
class PaintCounter {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PaintCounter)

  PaintCounter();
  void Draw(Compositor* aCompositor, TimeDuration aPaintTime, TimeDuration aCompositeTime);
  static IntRect GetPaintRect() { return PaintCounter::mRect; }

private:
  virtual ~PaintCounter();

  SurfaceFormat mFormat;
  RefPtrSkia<SkCanvas> mCanvas;
  IntSize mSize;
  int mStride;

  RefPtr<DataSourceSurface> mSurface;
  RefPtr<DataTextureSource> mTextureSource;
  RefPtr<TexturedEffect> mTexturedEffect;
  static IntRect mRect;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_opengl_PaintCounter_h_
