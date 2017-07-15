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
  mFormat = aRefDT->GetFormat();
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

void DrawTargetCaptureImpl::StrokeGlyphs(ScaledFont* aFont,
                                         const GlyphBuffer& aBuffer,
                                         const Pattern& aPattern,
                                         const StrokeOptions& aStrokeOptions,
                                         const DrawOptions& aOptions,
                                         const GlyphRenderingOptions* aRenderingOptions)
{
  AppendCommand(StrokeGlyphsCommand)(aFont, aBuffer, aPattern, aStrokeOptions, aOptions, aRenderingOptions);
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
DrawTargetCaptureImpl::PushLayer(bool aOpaque,
                                 Float aOpacity,
                                 SourceSurface* aMask,
                                 const Matrix& aMaskTransform,
                                 const IntRect& aBounds,
                                 bool aCopyBackground)
{
  AppendCommand(PushLayerCommand)(aOpaque,
                                  aOpacity,
                                  aMask,
                                  aMaskTransform,
                                  aBounds,
                                  aCopyBackground);
}

void
DrawTargetCaptureImpl::PopLayer()
{
  AppendCommand(PopLayerCommand)();
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

  // Have to update the transform for this DT
  // because some code paths query the current transform
  // to render specific things.
  DrawTarget::SetTransform(aTransform);
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

bool
DrawTargetCaptureImpl::ContainsOnlyColoredGlyphs(RefPtr<ScaledFont>& aScaledFont,
                                                 Color& aColor,
                                                 std::vector<Glyph>& aGlyphs)
{
  uint8_t* start = &mDrawCommandStorage.front();
  uint8_t* current = start;
  bool result = false;

  while (current < start + mDrawCommandStorage.size()) {
    DrawingCommand* command =
      reinterpret_cast<DrawingCommand*>(current + sizeof(uint32_t));
    current += *(uint32_t*)current;

    if (command->GetType() != CommandType::FILLGLYPHS &&
        command->GetType() != CommandType::SETTRANSFORM) {
      return false;
    }

    if (command->GetType() == CommandType::SETTRANSFORM) {
      SetTransformCommand* transform = static_cast<SetTransformCommand*>(command);
      if (transform->mTransform != Matrix()) {
        return false;
      }
      continue;
    }

    FillGlyphsCommand* fillGlyphs = static_cast<FillGlyphsCommand*>(command);
    if (aScaledFont && fillGlyphs->mFont != aScaledFont) {
      return false;
    }
    aScaledFont = fillGlyphs->mFont;

    Pattern& pat = fillGlyphs->mPattern;

    if (pat.GetType() != PatternType::COLOR) {
      return false;
    }

    ColorPattern* colorPat = static_cast<ColorPattern*>(&pat);
    if (aColor != Color() && colorPat->mColor != aColor) {
      return false;
    }
    aColor = colorPat->mColor;

    if (fillGlyphs->mOptions.mCompositionOp != CompositionOp::OP_OVER ||
        fillGlyphs->mOptions.mAlpha != 1.0f) {
      return false;
    }

    //TODO: Deal with AA on the DrawOptions, and the GlyphRenderingOptions

    aGlyphs.insert(aGlyphs.end(),
                   fillGlyphs->mGlyphs.begin(),
                   fillGlyphs->mGlyphs.end());
    result = true;
  }
  return result;
}

} // namespace gfx
} // namespace mozilla
