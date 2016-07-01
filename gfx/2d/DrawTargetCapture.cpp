/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DrawTargetCapture.h"
#include "DrawCommand.h"

namespace mozilla {
namespace gfx {


DrawTargetCaptureImpl::~DrawTargetCaptureImpl()
{
  uint8_t* start = &mDrawCommandStorage.front();

  uint8_t* current = start;

  while (current < start + mDrawCommandStorage.size()) {
    reinterpret_cast<DrawingCommand*>(current + sizeof(uint32_t))->~DrawingCommand();
    current += *(uint32_t*)current;
  }
}

bool
DrawTargetCaptureImpl::Init(const IntSize& aSize, DrawTarget* aRefDT)
{
  if (!aRefDT) {
    return false;
  }

  mRefDT = aRefDT;

  mSize = aSize;
  return true;
}

already_AddRefed<SourceSurface>
DrawTargetCaptureImpl::Snapshot()
{
  RefPtr<DrawTarget> dt = mRefDT->CreateSimilarDrawTarget(mSize, mRefDT->GetFormat());

  ReplayToDrawTarget(dt, Matrix());

  return dt->Snapshot();
}

void
DrawTargetCaptureImpl::DetachAllSnapshots()
{}

#define AppendCommand(arg) new (AppendToCommandList<arg>()) arg

void
DrawTargetCaptureImpl::DrawSurface(SourceSurface *aSurface,
                                   const Rect &aDest,
                                   const Rect &aSource,
                                   const DrawSurfaceOptions &aSurfOptions,
                                   const DrawOptions &aOptions)
{
  aSurface->GuaranteePersistance();
  AppendCommand(DrawSurfaceCommand)(aSurface, aDest, aSource, aSurfOptions, aOptions);
}

void
DrawTargetCaptureImpl::DrawFilter(FilterNode *aNode,
                                  const Rect &aSourceRect,
                                  const Point &aDestPoint,
                                  const DrawOptions &aOptions)
{
  // @todo XXX - this won't work properly long term yet due to filternodes not
  // being immutable.
  AppendCommand(DrawFilterCommand)(aNode, aSourceRect, aDestPoint, aOptions);
}

void
DrawTargetCaptureImpl::ClearRect(const Rect &aRect)
{
  AppendCommand(ClearRectCommand)(aRect);
}

void
DrawTargetCaptureImpl::MaskSurface(const Pattern &aSource,
                                   SourceSurface *aMask,
                                   Point aOffset,
                                   const DrawOptions &aOptions)
{
  aMask->GuaranteePersistance();
  AppendCommand(MaskSurfaceCommand)(aSource, aMask, aOffset, aOptions);
}

void
DrawTargetCaptureImpl::CopySurface(SourceSurface* aSurface,
                                   const IntRect& aSourceRect,
                                   const IntPoint& aDestination)
{
  aSurface->GuaranteePersistance();
  AppendCommand(CopySurfaceCommand)(aSurface, aSourceRect, aDestination);
}

void
DrawTargetCaptureImpl::FillRect(const Rect& aRect,
                                const Pattern& aPattern,
                                const DrawOptions& aOptions)
{
  AppendCommand(FillRectCommand)(aRect, aPattern, aOptions);
}

void
DrawTargetCaptureImpl::StrokeRect(const Rect& aRect,
                                  const Pattern& aPattern,
                                  const StrokeOptions& aStrokeOptions,
                                  const DrawOptions& aOptions)
{
  AppendCommand(StrokeRectCommand)(aRect, aPattern, aStrokeOptions, aOptions);
}

void
DrawTargetCaptureImpl::StrokeLine(const Point& aStart,
                                  const Point& aEnd,
                                  const Pattern& aPattern,
                                  const StrokeOptions& aStrokeOptions,
                                  const DrawOptions& aOptions)
{
  AppendCommand(StrokeLineCommand)(aStart, aEnd, aPattern, aStrokeOptions, aOptions);
}

void
DrawTargetCaptureImpl::Stroke(const Path* aPath,
                              const Pattern& aPattern,
                              const StrokeOptions& aStrokeOptions,
                              const DrawOptions& aOptions)
{
  AppendCommand(StrokeCommand)(aPath, aPattern, aStrokeOptions, aOptions);
}

void
DrawTargetCaptureImpl::Fill(const Path* aPath,
                            const Pattern& aPattern,
                            const DrawOptions& aOptions)
{
  AppendCommand(FillCommand)(aPath, aPattern, aOptions);
}

void
DrawTargetCaptureImpl::FillGlyphs(ScaledFont* aFont,
                                  const GlyphBuffer& aBuffer,
                                  const Pattern& aPattern,
                                  const DrawOptions& aOptions,
                                  const GlyphRenderingOptions* aRenderingOptions)
{
  AppendCommand(FillGlyphsCommand)(aFont, aBuffer, aPattern, aOptions, aRenderingOptions);
}

void
DrawTargetCaptureImpl::Mask(const Pattern &aSource,
                            const Pattern &aMask,
                            const DrawOptions &aOptions)
{
  AppendCommand(MaskCommand)(aSource, aMask, aOptions);
}

void
DrawTargetCaptureImpl::PushClip(const Path* aPath)
{
  AppendCommand(PushClipCommand)(aPath);
}

void
DrawTargetCaptureImpl::PushClipRect(const Rect& aRect)
{
  AppendCommand(PushClipRectCommand)(aRect);
}

void
DrawTargetCaptureImpl::PopClip()
{
  AppendCommand(PopClipCommand)();
}

void
DrawTargetCaptureImpl::SetTransform(const Matrix& aTransform)
{
  AppendCommand(SetTransformCommand)(aTransform);
}

void
DrawTargetCaptureImpl::ReplayToDrawTarget(DrawTarget* aDT, const Matrix& aTransform)
{
  uint8_t* start = &mDrawCommandStorage.front();

  uint8_t* current = start;

  while (current < start + mDrawCommandStorage.size()) {
    reinterpret_cast<DrawingCommand*>(current + sizeof(uint32_t))->ExecuteOnDT(aDT, &aTransform);
    current += *(uint32_t*)current;
  }
}

} // namespace gfx
} // namespace mozilla
