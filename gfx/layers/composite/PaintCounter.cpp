/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/gfx/Point.h"          // for IntSize, Point
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/gfx/Types.h"          // for Color, SurfaceFormat
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/Effects.h"     // for Effect, EffectChain, etc
#include "mozilla/TimeStamp.h"          // for TimeStamp, TimeDuration
#include "mozilla/Snprintf.h"

#include "SkColor.h"
#include "mozilla/gfx/HelpersSkia.h"
#include "skia/include/core/SkBitmapDevice.h"
#include "PaintCounter.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

// Positioned below the chrome UI
IntRect PaintCounter::mRect = IntRect(0, 175, 300, 60);

PaintCounter::PaintCounter()
{
  mFormat = SurfaceFormat::B8G8R8A8;
  mSurface = Factory::CreateDataSourceSurface(mRect.Size(), mFormat);
  mStride = mSurface->Stride();

  SkBitmap bitmap;
  bitmap.setInfo(MakeSkiaImageInfo(mRect.Size(), mFormat), mStride);
  bitmap.setPixels(mSurface->GetData());
  bitmap.eraseColor(SK_ColorWHITE);

  mCanvas.adopt(new SkCanvas(bitmap));
}

PaintCounter::~PaintCounter()
{
  mSurface = nullptr;
  mTextureSource = nullptr;
  mTexturedEffect = nullptr;
}

void
PaintCounter::Draw(Compositor* aCompositor, TimeDuration aPaintTime, TimeDuration aCompositeTime) {
  const int buffer_size = 48;
  char buffer[buffer_size];
  snprintf(buffer, buffer_size, "P: %.2f  C: %.2f",
           aPaintTime.ToMilliseconds(),
           aCompositeTime.ToMilliseconds());

  SkPaint paint;
  paint.setTextSize(32);
  paint.setColor(SkColorSetRGB(0, 255, 0));
  paint.setAntiAlias(true);

  mCanvas->clear(SK_ColorTRANSPARENT);
  mCanvas->drawText(buffer, strlen(buffer), 10, 30, paint);
  mCanvas->flush();

  if (!mTextureSource) {
    mTextureSource = aCompositor->CreateDataTextureSource();
    mTexturedEffect = CreateTexturedEffect(mFormat, mTextureSource,
                                           SamplingFilter::POINT, true);
    mTexturedEffect->mTextureCoords = Rect(0, 0, 1.0f, 1.0f);
  }

  mTextureSource->Update(mSurface);

  EffectChain effectChain;
  effectChain.mPrimaryEffect = mTexturedEffect;

  gfx::Matrix4x4 identity;
  Rect rect(mRect.x, mRect.y, mRect.width, mRect.height);
  aCompositor->DrawQuad(rect, mRect, effectChain, 1.0, identity);
}

} // end namespace layers
} // end namespace mozilla
