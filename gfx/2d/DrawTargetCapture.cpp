/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DrawTargetCapture.h"
#include "DrawCommand.h"
#include "gfxPlatform.h"
#include "SourceSurfaceCapture.h"
#include "FilterNodeCapture.h"

namespace mozilla {
namespace gfx {


DrawTargetCaptureImpl::~DrawTargetCaptureImpl()
{
  if (mSnapshot && !mSnapshot->hasOneRef()) {
    mSnapshot->DrawTargetWillDestroy();
    mSnapshot = nullptr;
  }
}

DrawTargetCaptureImpl::DrawTargetCaptureImpl(BackendType aBackend,
                                             const IntSize& aSize,
                                             SurfaceFormat aFormat)
  : mSize(aSize),
    mSnapshot(nullptr),
    mStride(0),
    mSurfaceAllocationSize(0)
{
  RefPtr<DrawTarget> screenRefDT =
      gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget();

  mFormat = aFormat;
  SetPermitSubpixelAA(IsOpaque(mFormat));
  if (aBackend == screenRefDT->GetBackendType()) {
    mRefDT = screenRefDT;
  } else {
    // This situation can happen if a blur operation decides to
    // use an unaccelerated path even if the system backend is
    // Direct2D.
    //
    // We don't really want to encounter the reverse scenario:
    // we shouldn't pick an accelerated backend if the system
    // backend is skia.
    if (aBackend == BackendType::DIRECT2D1_1) {
      gfxWarning() << "Creating a RefDT in DrawTargetCapture.";
    }

    // Create a 1x1 size ref dt to create assets
    // If we have to snapshot, we'll just create the real DT
    IntSize size(1, 1);
    mRefDT = Factory::CreateDrawTarget(aBackend, size, mFormat);
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
  SetPermitSubpixelAA(IsOpaque(mFormat));
  return true;
}

void
DrawTargetCaptureImpl::InitForData(int32_t aStride, size_t aSurfaceAllocationSize)
{
  mStride = aStride;
  mSurfaceAllocationSize = aSurfaceAllocationSize;
}

already_AddRefed<SourceSurface>
DrawTargetCaptureImpl::Snapshot()
{
  if (!mSnapshot) {
    mSnapshot = new SourceSurfaceCapture(this);
  }

  RefPtr<SourceSurface> surface = mSnapshot;
  return surface.forget();
}

already_AddRefed<SourceSurface>
DrawTargetCaptureImpl::IntoLuminanceSource(LuminanceType aLuminanceType,
                                           float aOpacity)
{
  RefPtr<SourceSurface> surface = new SourceSurfaceCapture(this, aLuminanceType, aOpacity);
  return surface.forget();
}

already_AddRefed<SourceSurface>
DrawTargetCaptureImpl::OptimizeSourceSurface(SourceSurface *aSurface) const
{
  // If the surface is a recording, make sure it gets resolved on the paint thread.
  if (aSurface->GetType() == SurfaceType::CAPTURE) {
    RefPtr<SourceSurface> surface = aSurface;
    return surface.forget();
  }
  return mRefDT->OptimizeSourceSurface(aSurface);
}

void
DrawTargetCaptureImpl::DetachAllSnapshots()
{
  MarkChanged();
}

#define AppendCommand(arg) new (AppendToCommandList<arg>()) arg

void
DrawTargetCaptureImpl::SetPermitSubpixelAA(bool aPermitSubpixelAA)
{
  AppendCommand(SetPermitSubpixelAACommand)(aPermitSubpixelAA);

  // Have to update mPermitSubpixelAA for this DT
  // because some code paths query the current setting
  // to determine subpixel AA eligibility.
  DrawTarget::SetPermitSubpixelAA(aPermitSubpixelAA);
}

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
DrawTargetCaptureImpl::DrawSurfaceWithShadow(SourceSurface *aSurface,
                                             const Point &aDest,
                                             const Color &aColor,
                                             const Point &aOffset,
                                             Float aSigma,
                                             CompositionOp aOperator)
{
  aSurface->GuaranteePersistance();
  AppendCommand(DrawSurfaceWithShadowCommand)(aSurface, aDest, aColor, aOffset, aSigma, aOperator);
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
                                  const DrawOptions& aOptions)
{
  AppendCommand(FillGlyphsCommand)(aFont, aBuffer, aPattern, aOptions);
}

void DrawTargetCaptureImpl::StrokeGlyphs(ScaledFont* aFont,
                                         const GlyphBuffer& aBuffer,
                                         const Pattern& aPattern,
                                         const StrokeOptions& aStrokeOptions,
                                         const DrawOptions& aOptions)
{
  AppendCommand(StrokeGlyphsCommand)(aFont, aBuffer, aPattern, aStrokeOptions, aOptions);
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
  // Have to update mPermitSubpixelAA for this DT
  // because some code paths query the current setting
  // to determine subpixel AA eligibility.
  PushedLayer layer(GetPermitSubpixelAA());
  mPushedLayers.push_back(layer);
  DrawTarget::SetPermitSubpixelAA(aOpaque);

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
  MOZ_ASSERT(mPushedLayers.size());
  DrawTarget::SetPermitSubpixelAA(mPushedLayers.back().mOldPermitSubpixelAA);
  mPushedLayers.pop_back();

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
DrawTargetCaptureImpl::Blur(const AlphaBoxBlur& aBlur)
{
  // gfxAlphaBoxBlur should not use this if it takes the accelerated path.
  MOZ_ASSERT(GetBackendType() == BackendType::SKIA);

  AppendCommand(BlurCommand)(aBlur);
}

void
DrawTargetCaptureImpl::ReplayToDrawTarget(DrawTarget* aDT, const Matrix& aTransform)
{
  for (CaptureCommandList::iterator iter(mCommands); !iter.Done(); iter.Next()) {
    DrawingCommand* cmd = iter.Get();
    cmd->ExecuteOnDT(aDT, &aTransform);
  }
}

bool
DrawTargetCaptureImpl::ContainsOnlyColoredGlyphs(RefPtr<ScaledFont>& aScaledFont,
                                                 Color& aColor,
                                                 std::vector<Glyph>& aGlyphs)
{
  bool result = false;

  for (CaptureCommandList::iterator iter(mCommands); !iter.Done(); iter.Next()) {
    DrawingCommand* command = iter.Get();

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

    //TODO: Deal with AA on the DrawOptions

    aGlyphs.insert(aGlyphs.end(),
                   fillGlyphs->mGlyphs.begin(),
                   fillGlyphs->mGlyphs.end());
    result = true;
  }
  return result;
}

void
DrawTargetCaptureImpl::MarkChanged()
{
  if (!mSnapshot) {
    return;
  }

  if (mSnapshot->hasOneRef()) {
    mSnapshot = nullptr;
    return;
  }

  mSnapshot->DrawTargetWillChange();
  mSnapshot = nullptr;
}

already_AddRefed<DrawTarget>
DrawTargetCaptureImpl::CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const
{
  return MakeAndAddRef<DrawTargetCaptureImpl>(GetBackendType(), aSize, aFormat);
}

RefPtr<DrawTarget>
DrawTargetCaptureImpl::CreateSimilarRasterTarget(const IntSize& aSize, SurfaceFormat aFormat) const
{
  MOZ_ASSERT(!mRefDT->IsCaptureDT());
  return mRefDT->CreateSimilarDrawTarget(aSize, aFormat);
}

already_AddRefed<FilterNode>
DrawTargetCaptureImpl::CreateFilter(FilterType aType)
{
  if (mRefDT->GetBackendType() == BackendType::DIRECT2D1_1) {
    return MakeRefPtr<FilterNodeCapture>(aType).forget();
  } else {
    return mRefDT->CreateFilter(aType);
  }
}

} // namespace gfx
} // namespace mozilla
