/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <initguid.h>
#include "DrawTargetD2D1.h"
#include "FilterNodeSoftware.h"
#include "GradientStopsD2D.h"
#include "SourceSurfaceD2D1.h"
#include "SourceSurfaceDual.h"
#include "RadialGradientEffectD2D1.h"

#include "HelpersD2D.h"
#include "FilterNodeD2D1.h"
#include "ExtendInputEffectD2D1.h"
#include "Tools.h"

using namespace std;

// decltype is not usable for overloaded functions.
typedef HRESULT (WINAPI*D2D1CreateFactoryFunc)(
    D2D1_FACTORY_TYPE factoryType,
    REFIID iid,
    CONST D2D1_FACTORY_OPTIONS *pFactoryOptions,
    void **factory
);

namespace mozilla {
namespace gfx {

uint64_t DrawTargetD2D1::mVRAMUsageDT;
uint64_t DrawTargetD2D1::mVRAMUsageSS;
IDWriteFactory *DrawTargetD2D1::mDWriteFactory;
ID2D1Factory1* DrawTargetD2D1::mFactory = nullptr;

ID2D1Factory1 *D2DFactory1()
{
  return DrawTargetD2D1::factory();
}

DrawTargetD2D1::DrawTargetD2D1()
  : mPushedLayers(1)
  , mUsedCommandListsSincePurge(0)
  , mDidComplexBlendWithListInList(false)
  , mDeviceSeq(0)
{
}

DrawTargetD2D1::~DrawTargetD2D1()
{
  PopAllClips();

  if (mSnapshot) {
    // We may hold the only reference. MarkIndependent will clear mSnapshot;
    // keep the snapshot object alive so it doesn't get destroyed while
    // MarkIndependent is running.
    RefPtr<SourceSurfaceD2D1> deathGrip = mSnapshot;
    // mSnapshot can be treated as independent of this DrawTarget since we know
    // this DrawTarget won't change again.
    deathGrip->MarkIndependent();
    // mSnapshot will be cleared now.
  }

  if (mDC && IsDeviceContextValid()) {
    // The only way mDC can be null is if Init failed, but it can happen and the
    // destructor is the only place where we need to check for it since the
    // DrawTarget will destroyed right after Init fails.
    mDC->EndDraw();
  }

  // Targets depending on us can break that dependency, since we're obviously not going to
  // be modified in the future.
  for (auto iter = mDependentTargets.begin();
       iter != mDependentTargets.end(); iter++) {
    (*iter)->mDependingOnTargets.erase(this);
  }
  // Our dependencies on other targets no longer matter.
  for (TargetSet::iterator iter = mDependingOnTargets.begin();
       iter != mDependingOnTargets.end(); iter++) {
    (*iter)->mDependentTargets.erase(this);
  }
}

already_AddRefed<SourceSurface>
DrawTargetD2D1::Snapshot()
{
  if (mSnapshot) {
    RefPtr<SourceSurface> snapshot(mSnapshot);
    return snapshot.forget();
  }
  PopAllClips();

  Flush();

  mSnapshot = new SourceSurfaceD2D1(mBitmap, mDC, mFormat, mSize, this);

  RefPtr<SourceSurface> snapshot(mSnapshot);
  return snapshot.forget();
}

// Command lists are kept around by device contexts until EndDraw is called,
// this can cause issues with memory usage (see bug 1238328). EndDraw/BeginDraw
// are expensive though, especially relatively when little work is done, so
// we try to reduce the amount of times we execute these purges.
static const uint32_t kPushedLayersBeforePurge = 25;

void
DrawTargetD2D1::Flush()
{
  if (IsDeviceContextValid()) {
    if ((mUsedCommandListsSincePurge >= kPushedLayersBeforePurge) &&
        mPushedLayers.size() == 1) {
      // It's important to pop all clips as otherwise layers can forget about
      // their clip when doing an EndDraw. When we have layers pushed we cannot
      // easily pop all underlying clips to delay the purge until we have no
      // layers pushed.
      PopAllClips();
      mUsedCommandListsSincePurge = 0;
      mDC->EndDraw();
      mDC->BeginDraw();
    } else {
      mDC->Flush();
    }
  }

  // We no longer depend on any target.
  for (TargetSet::iterator iter = mDependingOnTargets.begin();
       iter != mDependingOnTargets.end(); iter++) {
    (*iter)->mDependentTargets.erase(this);
  }
  mDependingOnTargets.clear();
}

void
DrawTargetD2D1::DrawSurface(SourceSurface *aSurface,
                            const Rect &aDest,
                            const Rect &aSource,
                            const DrawSurfaceOptions &aSurfOptions,
                            const DrawOptions &aOptions)
{
  PrepareForDrawing(aOptions.mCompositionOp, ColorPattern(Color()));

  D2D1_RECT_F samplingBounds;

  if (aSurfOptions.mSamplingBounds == SamplingBounds::BOUNDED) {
    samplingBounds = D2DRect(aSource);
  } else {
    samplingBounds = D2D1::RectF(0, 0, Float(aSurface->GetSize().width), Float(aSurface->GetSize().height));
  }

  Float xScale = aDest.width / aSource.width;
  Float yScale = aDest.height / aSource.height;

  RefPtr<ID2D1ImageBrush> brush;

  // Here we scale the source pattern up to the size and position where we want
  // it to be.
  Matrix transform;
  transform.PreTranslate(aDest.x - aSource.x * xScale, aDest.y - aSource.y * yScale);
  transform.PreScale(xScale, yScale);

  RefPtr<ID2D1Image> image = GetImageForSurface(aSurface, transform, ExtendMode::CLAMP);

  if (!image) {
    gfxWarning() << *this << ": Unable to get D2D image for surface.";
    return;
  }

  RefPtr<ID2D1Bitmap> bitmap;
  HRESULT hr;
  if (aSurface->GetType() == SurfaceType::D2D1_1_IMAGE) {
    // If this is called with a DataSourceSurface it might do a partial upload
    // that our DrawBitmap call doesn't support.
    hr = image->QueryInterface((ID2D1Bitmap**)getter_AddRefs(bitmap));
  }

  if (SUCCEEDED(hr) && bitmap && aSurfOptions.mSamplingBounds == SamplingBounds::UNBOUNDED) {
    mDC->DrawBitmap(bitmap, D2DRect(aDest), aOptions.mAlpha,
                    D2DFilter(aSurfOptions.mSamplingFilter), D2DRect(aSource));
  } else {
    // This has issues ignoring the alpha channel on windows 7 with images marked opaque.
    MOZ_ASSERT(aSurface->GetFormat() != SurfaceFormat::B8G8R8X8);

    // Bug 1275478 - D2D1 cannot draw A8 surface correctly.
    MOZ_ASSERT(aSurface->GetFormat() != SurfaceFormat::A8);

    mDC->CreateImageBrush(image,
                          D2D1::ImageBrushProperties(samplingBounds,
                                                     D2D1_EXTEND_MODE_CLAMP,
                                                     D2D1_EXTEND_MODE_CLAMP,
                                                     D2DInterpolationMode(aSurfOptions.mSamplingFilter)),
                          D2D1::BrushProperties(aOptions.mAlpha, D2DMatrix(transform)),
                          getter_AddRefs(brush));
    mDC->FillRectangle(D2DRect(aDest), brush);
  }

  FinalizeDrawing(aOptions.mCompositionOp, ColorPattern(Color()));
}

void
DrawTargetD2D1::DrawFilter(FilterNode *aNode,
                           const Rect &aSourceRect,
                           const Point &aDestPoint,
                           const DrawOptions &aOptions)
{
  if (aNode->GetBackendType() != FILTER_BACKEND_DIRECT2D1_1) {
    gfxWarning() << *this << ": Incompatible filter passed to DrawFilter.";
    return;
  }

  PrepareForDrawing(aOptions.mCompositionOp, ColorPattern(Color()));

  mDC->SetAntialiasMode(D2DAAMode(aOptions.mAntialiasMode));

  FilterNodeD2D1* node = static_cast<FilterNodeD2D1*>(aNode);
  node->WillDraw(this);

  mDC->DrawImage(node->OutputEffect(), D2DPoint(aDestPoint), D2DRect(aSourceRect));

  FinalizeDrawing(aOptions.mCompositionOp, ColorPattern(Color()));
}

void
DrawTargetD2D1::DrawSurfaceWithShadow(SourceSurface *aSurface,
                                      const Point &aDest,
                                      const Color &aColor,
                                      const Point &aOffset,
                                      Float aSigma,
                                      CompositionOp aOperator)
{
  MarkChanged();
  mDC->SetTransform(D2D1::IdentityMatrix());
  mTransformDirty = true;

  Matrix mat;
  RefPtr<ID2D1Image> image = GetImageForSurface(aSurface, mat, ExtendMode::CLAMP, nullptr, false);

  if (!image) {
    gfxWarning() << "Couldn't get image for surface.";
    return;
  }

  if (!mat.IsIdentity()) {
    gfxDebug() << *this << ": At this point complex partial uploads are not supported for Shadow surfaces.";
    return;
  }

  // Step 1, create the shadow effect.
  RefPtr<ID2D1Effect> shadowEffect;
  HRESULT hr = mDC->CreateEffect(mFormat == SurfaceFormat::A8 ? CLSID_D2D1GaussianBlur : CLSID_D2D1Shadow,
                                 getter_AddRefs(shadowEffect));
  if (FAILED(hr) || !shadowEffect) {
    gfxWarning() << "Failed to create shadow effect. Code: " << hexa(hr);
    return;
  }
  shadowEffect->SetInput(0, image);
  if (mFormat == SurfaceFormat::A8) {
    shadowEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, aSigma);
    shadowEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD);
  } else {
    shadowEffect->SetValue(D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION, aSigma);
    D2D1_VECTOR_4F color = { aColor.r, aColor.g, aColor.b, aColor.a };
    shadowEffect->SetValue(D2D1_SHADOW_PROP_COLOR, color);
  }

  D2D1_POINT_2F shadowPoint = D2DPoint(aDest + aOffset);
  mDC->DrawImage(shadowEffect, &shadowPoint, nullptr, D2D1_INTERPOLATION_MODE_LINEAR, D2DCompositionMode(aOperator));

  if (aSurface->GetFormat() != SurfaceFormat::A8) {
    D2D1_POINT_2F imgPoint = D2DPoint(aDest);
    mDC->DrawImage(image, &imgPoint, nullptr, D2D1_INTERPOLATION_MODE_LINEAR, D2DCompositionMode(aOperator));
  }
}

void
DrawTargetD2D1::ClearRect(const Rect &aRect)
{
  MarkChanged();

  PopAllClips();

  PushClipRect(aRect);

  if (mTransformDirty ||
      !mTransform.IsIdentity()) {
    mDC->SetTransform(D2D1::IdentityMatrix());
    mTransformDirty = true;
  }

  D2D1_RECT_F clipRect;
  bool isPixelAligned;
  if (mTransform.IsRectilinear() &&
      GetDeviceSpaceClipRect(clipRect, isPixelAligned)) {
    mDC->PushAxisAlignedClip(clipRect, isPixelAligned ? D2D1_ANTIALIAS_MODE_ALIASED : D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    mDC->Clear();
    mDC->PopAxisAlignedClip();

    PopClip();
    return;
  }

  RefPtr<ID2D1CommandList> list;
  mUsedCommandListsSincePurge++;
  mDC->CreateCommandList(getter_AddRefs(list));
  mDC->SetTarget(list);

  IntRect addClipRect;
  RefPtr<ID2D1Geometry> geom = GetClippedGeometry(&addClipRect);

  RefPtr<ID2D1SolidColorBrush> brush;
  mDC->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), getter_AddRefs(brush));
  mDC->PushAxisAlignedClip(D2D1::RectF(addClipRect.x, addClipRect.y, addClipRect.XMost(), addClipRect.YMost()), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
  mDC->FillGeometry(geom, brush);
  mDC->PopAxisAlignedClip();

  mDC->SetTarget(CurrentTarget());
  list->Close();

  mDC->DrawImage(list, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR, D2D1_COMPOSITE_MODE_DESTINATION_OUT);

  PopClip();

  return;
}

void
DrawTargetD2D1::MaskSurface(const Pattern &aSource,
                            SourceSurface *aMask,
                            Point aOffset,
                            const DrawOptions &aOptions)
{
  MarkChanged();

  RefPtr<ID2D1Bitmap> bitmap;

  Matrix mat = Matrix::Translation(aOffset);
  RefPtr<ID2D1Image> image = GetImageForSurface(aMask, mat, ExtendMode::CLAMP, nullptr);

  MOZ_ASSERT(!mat.HasNonTranslation());
  aOffset.x = mat._31;
  aOffset.y = mat._32;

  if (!image) {
    gfxWarning() << "Failed to get image for surface.";
    return;
  }

  PrepareForDrawing(aOptions.mCompositionOp, aSource);

  // FillOpacityMask only works if the antialias mode is MODE_ALIASED
  mDC->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

  image->QueryInterface((ID2D1Bitmap**)getter_AddRefs(bitmap));
  if (!bitmap) {
    gfxWarning() << "FillOpacityMask only works with Bitmap source surfaces.";
    return;
  }

  IntSize size = IntSize::Truncate(bitmap->GetSize().width, bitmap->GetSize().height);

  Rect maskRect = Rect(0.f, 0.f, Float(size.width), Float(size.height));

  Rect dest = Rect(aOffset.x, aOffset.y, Float(size.width), Float(size.height));
  RefPtr<ID2D1Brush> brush = CreateBrushForPattern(aSource, aOptions.mAlpha);
  mDC->FillOpacityMask(bitmap, brush, D2D1_OPACITY_MASK_CONTENT_GRAPHICS, D2DRect(dest), D2DRect(maskRect));

  mDC->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

  FinalizeDrawing(aOptions.mCompositionOp, aSource);
}

void
DrawTargetD2D1::CopySurface(SourceSurface *aSurface,
                            const IntRect &aSourceRect,
                            const IntPoint &aDestination)
{
  MarkChanged();

  PopAllClips();

  mDC->SetTransform(D2D1::IdentityMatrix());
  mTransformDirty = true;

  Matrix mat = Matrix::Translation(aDestination.x - aSourceRect.x, aDestination.y - aSourceRect.y);
  RefPtr<ID2D1Image> image = GetImageForSurface(aSurface, mat, ExtendMode::CLAMP, nullptr, false);

  if (!image) {
    gfxWarning() << "Couldn't get image for surface.";
    return;
  }

  if (mat.HasNonIntegerTranslation()) {
    gfxDebug() << *this << ": At this point scaled partial uploads are not supported for CopySurface.";
    return;
  }

  IntRect sourceRect = aSourceRect;
  sourceRect.x += (aDestination.x - aSourceRect.x) - mat._31;
  sourceRect.width -= (aDestination.x - aSourceRect.x) - mat._31;
  sourceRect.y += (aDestination.y - aSourceRect.y) - mat._32;
  sourceRect.height -= (aDestination.y - aSourceRect.y) - mat._32;

  RefPtr<ID2D1Bitmap> bitmap;
  HRESULT hr = image->QueryInterface((ID2D1Bitmap**)getter_AddRefs(bitmap));

  if (SUCCEEDED(hr) && bitmap && mFormat == SurfaceFormat::A8) {
    RefPtr<ID2D1SolidColorBrush> brush;
    mDC->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White),
                               D2D1::BrushProperties(), getter_AddRefs(brush));
    mDC->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
    mDC->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
    mDC->FillOpacityMask(bitmap, brush, D2D1_OPACITY_MASK_CONTENT_GRAPHICS);
    mDC->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    mDC->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
    return;
  }

  Rect srcRect(Float(sourceRect.x), Float(sourceRect.y),
               Float(aSourceRect.width), Float(aSourceRect.height));

  Rect dstRect(Float(aDestination.x), Float(aDestination.y),
               Float(aSourceRect.width), Float(aSourceRect.height));

  if (SUCCEEDED(hr) && bitmap) {
    mDC->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
    mDC->DrawBitmap(bitmap, D2DRect(dstRect), 1.0f,
                    D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                    D2DRect(srcRect));
    mDC->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
    return;
  }

  mDC->DrawImage(image, D2D1::Point2F(Float(aDestination.x), Float(aDestination.y)),
                 D2DRect(srcRect), D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                 D2D1_COMPOSITE_MODE_BOUNDED_SOURCE_COPY);
}

void
DrawTargetD2D1::FillRect(const Rect &aRect,
                         const Pattern &aPattern,
                         const DrawOptions &aOptions)
{
  PrepareForDrawing(aOptions.mCompositionOp, aPattern);

  mDC->SetAntialiasMode(D2DAAMode(aOptions.mAntialiasMode));

  RefPtr<ID2D1Brush> brush = CreateBrushForPattern(aPattern, aOptions.mAlpha);
  mDC->FillRectangle(D2DRect(aRect), brush);

  FinalizeDrawing(aOptions.mCompositionOp, aPattern);
}

void
DrawTargetD2D1::StrokeRect(const Rect &aRect,
                           const Pattern &aPattern,
                           const StrokeOptions &aStrokeOptions,
                           const DrawOptions &aOptions)
{
  PrepareForDrawing(aOptions.mCompositionOp, aPattern);

  mDC->SetAntialiasMode(D2DAAMode(aOptions.mAntialiasMode));

  RefPtr<ID2D1Brush> brush = CreateBrushForPattern(aPattern, aOptions.mAlpha);
  RefPtr<ID2D1StrokeStyle> strokeStyle = CreateStrokeStyleForOptions(aStrokeOptions);

  mDC->DrawRectangle(D2DRect(aRect), brush, aStrokeOptions.mLineWidth, strokeStyle);

  FinalizeDrawing(aOptions.mCompositionOp, aPattern);
}

void
DrawTargetD2D1::StrokeLine(const Point &aStart,
                           const Point &aEnd,
                           const Pattern &aPattern,
                           const StrokeOptions &aStrokeOptions,
                           const DrawOptions &aOptions)
{
  PrepareForDrawing(aOptions.mCompositionOp, aPattern);

  mDC->SetAntialiasMode(D2DAAMode(aOptions.mAntialiasMode));

  RefPtr<ID2D1Brush> brush = CreateBrushForPattern(aPattern, aOptions.mAlpha);
  RefPtr<ID2D1StrokeStyle> strokeStyle = CreateStrokeStyleForOptions(aStrokeOptions);

  mDC->DrawLine(D2DPoint(aStart), D2DPoint(aEnd), brush, aStrokeOptions.mLineWidth, strokeStyle);

  FinalizeDrawing(aOptions.mCompositionOp, aPattern);
}

void
DrawTargetD2D1::Stroke(const Path *aPath,
                       const Pattern &aPattern,
                       const StrokeOptions &aStrokeOptions,
                       const DrawOptions &aOptions)
{
  if (aPath->GetBackendType() != BackendType::DIRECT2D1_1) {
    gfxDebug() << *this << ": Ignoring drawing call for incompatible path.";
    return;
  }
  const PathD2D *d2dPath = static_cast<const PathD2D*>(aPath);

  PrepareForDrawing(aOptions.mCompositionOp, aPattern);

  mDC->SetAntialiasMode(D2DAAMode(aOptions.mAntialiasMode));

  RefPtr<ID2D1Brush> brush = CreateBrushForPattern(aPattern, aOptions.mAlpha);
  RefPtr<ID2D1StrokeStyle> strokeStyle = CreateStrokeStyleForOptions(aStrokeOptions);

  mDC->DrawGeometry(d2dPath->mGeometry, brush, aStrokeOptions.mLineWidth, strokeStyle);

  FinalizeDrawing(aOptions.mCompositionOp, aPattern);
}

void
DrawTargetD2D1::Fill(const Path *aPath,
                     const Pattern &aPattern,
                     const DrawOptions &aOptions)
{
  if (!aPath || aPath->GetBackendType() != BackendType::DIRECT2D1_1) {
    gfxDebug() << *this << ": Ignoring drawing call for incompatible path.";
    return;
  }
  const PathD2D *d2dPath = static_cast<const PathD2D*>(aPath);

  PrepareForDrawing(aOptions.mCompositionOp, aPattern);

  mDC->SetAntialiasMode(D2DAAMode(aOptions.mAntialiasMode));

  RefPtr<ID2D1Brush> brush = CreateBrushForPattern(aPattern, aOptions.mAlpha);

  mDC->FillGeometry(d2dPath->mGeometry, brush);

  FinalizeDrawing(aOptions.mCompositionOp, aPattern);
}

void
DrawTargetD2D1::FillGlyphs(ScaledFont *aFont,
                           const GlyphBuffer &aBuffer,
                           const Pattern &aPattern,
                           const DrawOptions &aOptions,
                           const GlyphRenderingOptions *aRenderingOptions)
{
  if (aFont->GetType() != FontType::DWRITE) {
    gfxDebug() << *this << ": Ignoring drawing call for incompatible font.";
    return;
  }

  ScaledFontDWrite *font = static_cast<ScaledFontDWrite*>(aFont);

  IDWriteRenderingParams *params = nullptr;
  if (aRenderingOptions) {
    if (aRenderingOptions->GetType() != FontType::DWRITE) {
      gfxDebug() << *this << ": Ignoring incompatible GlyphRenderingOptions.";
      // This should never happen.
      MOZ_ASSERT(false);
    } else {
      params = static_cast<const GlyphRenderingOptionsDWrite*>(aRenderingOptions)->mParams;
    }
  }

  AntialiasMode aaMode = font->GetDefaultAAMode();

  if (aOptions.mAntialiasMode != AntialiasMode::DEFAULT) {
    aaMode = aOptions.mAntialiasMode;
  }

  PrepareForDrawing(aOptions.mCompositionOp, aPattern);

  bool forceClearType = false;
  if (!CurrentLayer().mIsOpaque && mPermitSubpixelAA &&
      aOptions.mCompositionOp == CompositionOp::OP_OVER && aaMode == AntialiasMode::SUBPIXEL) {
    forceClearType = true;    
  }


  D2D1_TEXT_ANTIALIAS_MODE d2dAAMode = D2D1_TEXT_ANTIALIAS_MODE_DEFAULT;

  switch (aaMode) {
  case AntialiasMode::NONE:
    d2dAAMode = D2D1_TEXT_ANTIALIAS_MODE_ALIASED;
    break;
  case AntialiasMode::GRAY:
    d2dAAMode = D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE;
    break;
  case AntialiasMode::SUBPIXEL:
    d2dAAMode = D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE;
    break;
  default:
    d2dAAMode = D2D1_TEXT_ANTIALIAS_MODE_DEFAULT;
  }

  if (d2dAAMode == D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE &&
      !CurrentLayer().mIsOpaque && !forceClearType) {
    d2dAAMode = D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE;
  }

  mDC->SetTextAntialiasMode(d2dAAMode);

  if (params != mTextRenderingParams) {
    mDC->SetTextRenderingParams(params);
    mTextRenderingParams = params;
  }

  RefPtr<ID2D1Brush> brush = CreateBrushForPattern(aPattern, aOptions.mAlpha);

  AutoDWriteGlyphRun autoRun;
  DWriteGlyphRunFromGlyphs(aBuffer, font, &autoRun);

  bool needsRepushedLayers = false;
  if (forceClearType) {
    D2D1_RECT_F rect;
    bool isAligned;
    needsRepushedLayers = CurrentLayer().mPushedClips.size() && !GetDeviceSpaceClipRect(rect, isAligned);

    // If we have a complex clip in our stack and we have a transparent
    // background, and subpixel AA is permitted, we need to repush our layer
    // stack limited by the glyph run bounds initializing our layers for
    // subpixel AA.
    if (needsRepushedLayers) {
      mDC->GetGlyphRunWorldBounds(D2D1::Point2F(), &autoRun,
                                  DWRITE_MEASURING_MODE_NATURAL, &rect);
      rect.left = std::floor(rect.left);
      rect.right = std::ceil(rect.right);
      rect.top = std::floor(rect.top);
      rect.bottom = std::ceil(rect.bottom);

      PopAllClips();

      if (!mTransform.IsRectilinear()) {
        // We must limit the pixels we touch to the -user space- bounds of
        // the glyphs being drawn. In order not to get transparent pixels
        // copied up in our pushed layer stack.
        D2D1_RECT_F userRect;
        mDC->SetTransform(D2D1::IdentityMatrix());
        mDC->GetGlyphRunWorldBounds(D2D1::Point2F(), &autoRun,
                                    DWRITE_MEASURING_MODE_NATURAL, &userRect);

        RefPtr<ID2D1PathGeometry> path;
        factory()->CreatePathGeometry(getter_AddRefs(path));
        RefPtr<ID2D1GeometrySink> sink;
        path->Open(getter_AddRefs(sink));
        AddRectToSink(sink, userRect);
        sink->Close();

        mDC->PushLayer(D2D1::LayerParameters1(D2D1::InfiniteRect(), path, D2D1_ANTIALIAS_MODE_ALIASED,
                                              D2DMatrix(mTransform), 1.0f, nullptr,
                                              D2D1_LAYER_OPTIONS1_INITIALIZE_FROM_BACKGROUND |
                                              D2D1_LAYER_OPTIONS1_IGNORE_ALPHA), nullptr);
      }

      PushClipsToDC(mDC, true, rect);
      mDC->SetTransform(D2DMatrix(mTransform));
    }
  }

  if (brush) {
    mDC->DrawGlyphRun(D2D1::Point2F(), &autoRun, brush);
  }

  if (needsRepushedLayers) {
    PopClipsFromDC(mDC);

    if (!mTransform.IsRectilinear()) {
      mDC->PopLayer();
    }
  }

  FinalizeDrawing(aOptions.mCompositionOp, aPattern);
}

void
DrawTargetD2D1::Mask(const Pattern &aSource,
                     const Pattern &aMask,
                     const DrawOptions &aOptions)
{
  PrepareForDrawing(aOptions.mCompositionOp, aSource);

  RefPtr<ID2D1Brush> source = CreateBrushForPattern(aSource, aOptions.mAlpha);
  RefPtr<ID2D1Brush> mask = CreateBrushForPattern(aMask, 1.0f);
  mDC->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), nullptr,
                                       D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                                       D2D1::IdentityMatrix(),
                                       1.0f, mask),
                 nullptr);

  Rect rect(0, 0, (Float)mSize.width, (Float)mSize.height);
  Matrix mat = mTransform;
  mat.Invert();
  
  mDC->FillRectangle(D2DRect(mat.TransformBounds(rect)), source);

  mDC->PopLayer();

  FinalizeDrawing(aOptions.mCompositionOp, aSource);
}

void
DrawTargetD2D1::PushClipGeometry(ID2D1Geometry* aGeometry,
                                 const D2D1_MATRIX_3X2_F& aTransform,
                                 bool aPixelAligned)
{
  mCurrentClippedGeometry = nullptr;

  PushedClip clip;
  clip.mGeometry = aGeometry;
  clip.mTransform = aTransform;
  clip.mIsPixelAligned = aPixelAligned;

  aGeometry->GetBounds(aTransform, &clip.mBounds);

  CurrentLayer().mPushedClips.push_back(clip);

  // The transform of clips is relative to the world matrix, since we use the total
  // transform for the clips, make the world matrix identity.
  mDC->SetTransform(D2D1::IdentityMatrix());
  mTransformDirty = true;

  if (CurrentLayer().mClipsArePushed) {
    PushD2DLayer(mDC, clip.mGeometry, clip.mTransform, clip.mIsPixelAligned);
  }
}

void
DrawTargetD2D1::PushClip(const Path *aPath)
{
  if (aPath->GetBackendType() != BackendType::DIRECT2D1_1) {
    gfxDebug() << *this << ": Ignoring clipping call for incompatible path.";
    return;
  }

  RefPtr<PathD2D> pathD2D = static_cast<PathD2D*>(const_cast<Path*>(aPath));

  PushClipGeometry(pathD2D->GetGeometry(), D2DMatrix(mTransform));
}

void
DrawTargetD2D1::PushClipRect(const Rect &aRect)
{
  if (!mTransform.IsRectilinear()) {
    // Whoops, this isn't a rectangle in device space, Direct2D will not deal
    // with this transform the way we want it to.
    // See remarks: http://msdn.microsoft.com/en-us/library/dd316860%28VS.85%29.aspx
    RefPtr<ID2D1Geometry> geom = ConvertRectToGeometry(D2DRect(aRect));
    return PushClipGeometry(geom, D2DMatrix(mTransform));
  }

  mCurrentClippedGeometry = nullptr;

  PushedClip clip;
  Rect rect = mTransform.TransformBounds(aRect);
  IntRect intRect;
  clip.mIsPixelAligned = rect.ToIntRect(&intRect);

  // Do not store the transform, just store the device space rectangle directly.
  clip.mBounds = D2DRect(rect);

  CurrentLayer().mPushedClips.push_back(clip);

  mDC->SetTransform(D2D1::IdentityMatrix());
  mTransformDirty = true;

  if (CurrentLayer().mClipsArePushed) {
    mDC->PushAxisAlignedClip(clip.mBounds, clip.mIsPixelAligned ? D2D1_ANTIALIAS_MODE_ALIASED : D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
  }
}

void
DrawTargetD2D1::PushDeviceSpaceClipRects(const IntRect* aRects, uint32_t aCount)
{
  // Build a path for the union of the rects.
  RefPtr<ID2D1PathGeometry> path;
  factory()->CreatePathGeometry(getter_AddRefs(path));
  RefPtr<ID2D1GeometrySink> sink;
  path->Open(getter_AddRefs(sink));
  sink->SetFillMode(D2D1_FILL_MODE_WINDING);
  for (uint32_t i = 0; i < aCount; i++) {
    const IntRect& rect = aRects[i];
    sink->BeginFigure(D2DPoint(rect.TopLeft()), D2D1_FIGURE_BEGIN_FILLED);
    D2D1_POINT_2F lines[3] = { D2DPoint(rect.TopRight()), D2DPoint(rect.BottomRight()), D2DPoint(rect.BottomLeft()) };
    sink->AddLines(lines, 3);
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
  }
  sink->Close();

  // The path is in device-space, so there is no transform needed,
  // and all rects are pixel aligned.
  PushClipGeometry(path, D2D1::IdentityMatrix(), true);
}

void
DrawTargetD2D1::PopClip()
{
  mCurrentClippedGeometry = nullptr;
  if (CurrentLayer().mPushedClips.empty()) {
    gfxDevCrash(LogReason::UnbalancedClipStack) << "DrawTargetD2D1::PopClip: No clip to pop.";
    return;
  }

  if (CurrentLayer().mClipsArePushed) {
    if (CurrentLayer().mPushedClips.back().mGeometry) {
      mDC->PopLayer();
    } else {
      mDC->PopAxisAlignedClip();
    }
  }
  CurrentLayer().mPushedClips.pop_back();
}

void
DrawTargetD2D1::PushLayer(bool aOpaque, Float aOpacity, SourceSurface* aMask,
                          const Matrix& aMaskTransform, const IntRect& aBounds,
                          bool aCopyBackground)
{
  D2D1_LAYER_OPTIONS1 options = D2D1_LAYER_OPTIONS1_NONE;

  if (aOpaque) {
    options |= D2D1_LAYER_OPTIONS1_IGNORE_ALPHA;
  }
  if (aCopyBackground) {
    options |= D2D1_LAYER_OPTIONS1_INITIALIZE_FROM_BACKGROUND;
  }

  RefPtr<ID2D1ImageBrush> mask;
  Matrix maskTransform = aMaskTransform;
  RefPtr<ID2D1PathGeometry> clip;

  if (aMask) {
    RefPtr<ID2D1Image> image = GetImageForSurface(aMask, maskTransform, ExtendMode::CLAMP);
    mDC->SetTransform(D2D1::IdentityMatrix());
    mTransformDirty = true;

    // The mask is given in user space. Our layer will apply it in device space.
    maskTransform = maskTransform * mTransform;

    if (image) {
      IntSize maskSize = aMask->GetSize();
      HRESULT hr = mDC->CreateImageBrush(image,
                                         D2D1::ImageBrushProperties(D2D1::RectF(0, 0, maskSize.width, maskSize.height)),
                                         D2D1::BrushProperties(1.0f, D2DMatrix(maskTransform)),
                                         getter_AddRefs(mask));
      if (FAILED(hr)) {
        gfxWarning() <<"[D2D1.1] Failed to create a ImageBrush, code: " << hexa(hr);
      }

      factory()->CreatePathGeometry(getter_AddRefs(clip));
      RefPtr<ID2D1GeometrySink> sink;
      clip->Open(getter_AddRefs(sink));
      AddRectToSink(sink, D2D1::RectF(0, 0, aMask->GetSize().width, aMask->GetSize().height));
      sink->Close();
    } else {
      gfxCriticalError() << "Failed to get image for mask surface!";
    }
  }

  PushAllClips();

  mDC->PushLayer(D2D1::LayerParameters1(D2D1::InfiniteRect(), clip, D2D1_ANTIALIAS_MODE_ALIASED, D2DMatrix(maskTransform), aOpacity, mask, options), nullptr);
  PushedLayer pushedLayer;
  pushedLayer.mClipsArePushed = false;
  pushedLayer.mIsOpaque = aOpaque;
  pushedLayer.mOldPermitSubpixelAA = mPermitSubpixelAA;
  mPermitSubpixelAA = aOpaque;

  mDC->CreateCommandList(getter_AddRefs(pushedLayer.mCurrentList));
  mPushedLayers.push_back(pushedLayer);

  mDC->SetTarget(CurrentTarget());

  mUsedCommandListsSincePurge++;
}

void
DrawTargetD2D1::PopLayer()
{
  MOZ_ASSERT(CurrentLayer().mPushedClips.size() == 0);

  RefPtr<ID2D1CommandList> list = CurrentLayer().mCurrentList;
  mPermitSubpixelAA = CurrentLayer().mOldPermitSubpixelAA;

  mPushedLayers.pop_back();
  mDC->SetTarget(CurrentTarget());

  list->Close();
  mDC->SetTransform(D2D1::IdentityMatrix());
  mTransformDirty = true;

  DCCommandSink sink(mDC);
  list->Stream(&sink);

  mDC->PopLayer();
}

already_AddRefed<SourceSurface>
DrawTargetD2D1::CreateSourceSurfaceFromData(unsigned char *aData,
                                            const IntSize &aSize,
                                            int32_t aStride,
                                            SurfaceFormat aFormat) const
{
  RefPtr<ID2D1Bitmap1> bitmap;

  HRESULT hr = mDC->CreateBitmap(D2DIntSize(aSize), aData, aStride,
                                 D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE, D2DPixelFormat(aFormat)),
                                 getter_AddRefs(bitmap));

  if (FAILED(hr) || !bitmap) {
    gfxCriticalError(CriticalLog::DefaultOptions(Factory::ReasonableSurfaceSize(aSize))) << "[D2D1.1] 1CreateBitmap failure " << aSize << " Code: " << hexa(hr) << " format " << (int)aFormat;
    return nullptr;
  }

  return MakeAndAddRef<SourceSurfaceD2D1>(bitmap.get(), mDC, aFormat, aSize);
}

already_AddRefed<DrawTarget>
DrawTargetD2D1::CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const
{
  RefPtr<DrawTargetD2D1> dt = new DrawTargetD2D1();

  if (!dt->Init(aSize, aFormat)) {
    return nullptr;
  }

  return dt.forget();
}

already_AddRefed<PathBuilder>
DrawTargetD2D1::CreatePathBuilder(FillRule aFillRule) const
{
  RefPtr<ID2D1PathGeometry> path;
  HRESULT hr = factory()->CreatePathGeometry(getter_AddRefs(path));

  if (FAILED(hr)) {
    gfxWarning() << *this << ": Failed to create Direct2D Path Geometry. Code: " << hexa(hr);
    return nullptr;
  }

  RefPtr<ID2D1GeometrySink> sink;
  hr = path->Open(getter_AddRefs(sink));
  if (FAILED(hr)) {
    gfxWarning() << *this << ": Failed to access Direct2D Path Geometry. Code: " << hexa(hr);
    return nullptr;
  }

  if (aFillRule == FillRule::FILL_WINDING) {
    sink->SetFillMode(D2D1_FILL_MODE_WINDING);
  }

  return MakeAndAddRef<PathBuilderD2D>(sink, path, aFillRule, BackendType::DIRECT2D1_1);
}

already_AddRefed<GradientStops>
DrawTargetD2D1::CreateGradientStops(GradientStop *rawStops, uint32_t aNumStops, ExtendMode aExtendMode) const
{
  if (aNumStops == 0) {
    gfxWarning() << *this << ": Failed to create GradientStopCollection with no stops.";
    return nullptr;
  }

  D2D1_GRADIENT_STOP *stops = new D2D1_GRADIENT_STOP[aNumStops];

  for (uint32_t i = 0; i < aNumStops; i++) {
    stops[i].position = rawStops[i].offset;
    stops[i].color = D2DColor(rawStops[i].color);
  }

  RefPtr<ID2D1GradientStopCollection> stopCollection;

  HRESULT hr =
    mDC->CreateGradientStopCollection(stops, aNumStops,
                                      D2D1_GAMMA_2_2, D2DExtend(aExtendMode, Axis::BOTH),
                                      getter_AddRefs(stopCollection));
  delete [] stops;

  if (FAILED(hr)) {
    gfxWarning() << *this << ": Failed to create GradientStopCollection. Code: " << hexa(hr);
    return nullptr;
  }

  return MakeAndAddRef<GradientStopsD2D>(stopCollection, Factory::GetDirect3D11Device());
}

already_AddRefed<FilterNode>
DrawTargetD2D1::CreateFilter(FilterType aType)
{
  return FilterNodeD2D1::Create(mDC, aType);
}

void
DrawTargetD2D1::GetGlyphRasterizationMetrics(ScaledFont *aScaledFont, const uint16_t* aGlyphIndices,
                                             uint32_t aNumGlyphs, GlyphMetrics* aGlyphMetrics)
{
  MOZ_ASSERT(aScaledFont->GetType() == FontType::DWRITE);

  aScaledFont->GetGlyphDesignMetrics(aGlyphIndices, aNumGlyphs, aGlyphMetrics);

  // GetDesignGlyphMetrics returns 'ideal' glyph metrics, we need to pad to
  // account for antialiasing.
  for (uint32_t i = 0; i < aNumGlyphs; i++) {
    if (aGlyphMetrics[i].mWidth > 0 && aGlyphMetrics[i].mHeight > 0) {
      aGlyphMetrics[i].mWidth += 2.0f;
      aGlyphMetrics[i].mXBearing -= 1.0f;
    }
  }
}

bool
DrawTargetD2D1::Init(ID3D11Texture2D* aTexture, SurfaceFormat aFormat)
{
  HRESULT hr;

  ID2D1Device* device = Factory::GetD2D1Device();
  if (!device) {
    gfxCriticalNote << "[D2D1.1] Failed to obtain a device for DrawTargetD2D1::Init(ID3D11Texture2D*, SurfaceFormat).";
    return false;
  } else {
    mDeviceSeq = Factory::GetD2D1DeviceSeq();
  }

  hr = device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS, getter_AddRefs(mDC));

  if (FAILED(hr)) {
    gfxCriticalError() <<"[D2D1.1] 1Failed to create a DeviceContext, code: " << hexa(hr) << " format " << (int)aFormat;
    return false;
  }

  RefPtr<IDXGISurface> dxgiSurface;
  aTexture->QueryInterface(__uuidof(IDXGISurface),
                           (void**)((IDXGISurface**)getter_AddRefs(dxgiSurface)));
  if (!dxgiSurface) {
    gfxCriticalError() <<"[D2D1.1] Failed to obtain a DXGI surface.";
    return false;
  }

  D2D1_BITMAP_PROPERTIES1 props;
  props.dpiX = 96;
  props.dpiY = 96;
  props.pixelFormat = D2DPixelFormat(aFormat);
  props.colorContext = nullptr;
  props.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
  hr = mDC->CreateBitmapFromDxgiSurface(dxgiSurface, props, (ID2D1Bitmap1**)getter_AddRefs(mBitmap));

  if (FAILED(hr)) {
    gfxCriticalError() << "[D2D1.1] CreateBitmapFromDxgiSurface failure Code: " << hexa(hr) << " format " << (int)aFormat;
    return false;
  }

  mFormat = aFormat;
  D3D11_TEXTURE2D_DESC desc;
  aTexture->GetDesc(&desc);
  mSize.width = desc.Width;
  mSize.height = desc.Height;

  // This single solid color brush system is not very 'threadsafe', however,
  // issueing multiple drawing commands simultaneously to a single drawtarget
  // from multiple threads is unexpected since there's no way to guarantee
  // ordering in that situation anyway.
  hr = mDC->CreateSolidColorBrush(D2D1::ColorF(0, 0), getter_AddRefs(mSolidColorBrush));

  if (FAILED(hr)) {
    gfxCriticalError() << "[D2D1.1] Failure creating solid color brush (I1).";
    return false;
  }

  mDC->SetTarget(CurrentTarget());

  mDC->BeginDraw();

  CurrentLayer().mIsOpaque = aFormat == SurfaceFormat::B8G8R8X8;

  return true;
}

bool
DrawTargetD2D1::Init(const IntSize &aSize, SurfaceFormat aFormat)
{
  HRESULT hr;

  ID2D1Device* device = Factory::GetD2D1Device();
  if (!device) {
    gfxCriticalNote << "[D2D1.1] Failed to obtain a device for DrawTargetD2D1::Init(IntSize, SurfaceFormat).";
    return false;
  } else {
    mDeviceSeq = Factory::GetD2D1DeviceSeq();
  }

  hr = device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS, getter_AddRefs(mDC));

  if (FAILED(hr)) {
    gfxCriticalError() <<"[D2D1.1] 2Failed to create a DeviceContext, code: " << hexa(hr) << " format " << (int)aFormat;
    return false;
  }

  if (mDC->GetMaximumBitmapSize() < UINT32(aSize.width) ||
      mDC->GetMaximumBitmapSize() < UINT32(aSize.height)) {
    // This is 'ok', so don't assert
    gfxCriticalNote << "[D2D1.1] Attempt to use unsupported surface size " << aSize;
    return false;
  }

  D2D1_BITMAP_PROPERTIES1 props;
  props.dpiX = 96;
  props.dpiY = 96;
  props.pixelFormat = D2DPixelFormat(aFormat);
  props.colorContext = nullptr;
  props.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
  hr = mDC->CreateBitmap(D2DIntSize(aSize), nullptr, 0, props, (ID2D1Bitmap1**)getter_AddRefs(mBitmap));

  if (FAILED(hr)) {
    gfxCriticalError() << "[D2D1.1] 3CreateBitmap failure " << aSize << " Code: " << hexa(hr) << " format " << (int)aFormat;
    return false;
  }

  mDC->SetTarget(CurrentTarget());

  hr = mDC->CreateSolidColorBrush(D2D1::ColorF(0, 0), getter_AddRefs(mSolidColorBrush));

  if (FAILED(hr)) {
    gfxCriticalError() << "[D2D1.1] Failure creating solid color brush (I2).";
    return false;
  }

  mDC->BeginDraw();

  CurrentLayer().mIsOpaque = aFormat == SurfaceFormat::B8G8R8X8;

  mDC->Clear();

  mFormat = aFormat;
  mSize = aSize;

  return true;
}

/**
 * Private helpers.
 */
uint32_t
DrawTargetD2D1::GetByteSize() const
{
  return mSize.width * mSize.height * BytesPerPixel(mFormat);
}

ID2D1Factory1*
DrawTargetD2D1::factory()
{
  if (mFactory) {
    return mFactory;
  }

  RefPtr<ID2D1Factory> factory;
  D2D1CreateFactoryFunc createD2DFactory;
  HMODULE d2dModule = LoadLibraryW(L"d2d1.dll");
  createD2DFactory = (D2D1CreateFactoryFunc)
      GetProcAddress(d2dModule, "D2D1CreateFactory");

  if (!createD2DFactory) {
    gfxWarning() << "Failed to locate D2D1CreateFactory function.";
    return nullptr;
  }

  D2D1_FACTORY_OPTIONS options;
#ifdef _DEBUG
  options.debugLevel = D2D1_DEBUG_LEVEL_WARNING;
#else
  options.debugLevel = D2D1_DEBUG_LEVEL_NONE;
#endif
  //options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;

  HRESULT hr = createD2DFactory(D2D1_FACTORY_TYPE_MULTI_THREADED,
                                __uuidof(ID2D1Factory),
                                &options,
                                getter_AddRefs(factory));

  if (FAILED(hr) || !factory) {
    gfxCriticalNote << "Failed to create a D2D1 content device: " << hexa(hr);
    return nullptr;
  }

  hr = factory->QueryInterface((ID2D1Factory1**)&mFactory);
  if (FAILED(hr) || !mFactory) {
    return nullptr;
  }

  ExtendInputEffectD2D1::Register(mFactory);
  RadialGradientEffectD2D1::Register(mFactory);

  return mFactory;
}

IDWriteFactory*
DrawTargetD2D1::GetDWriteFactory()
{
  if (mDWriteFactory) {
    return mDWriteFactory;
  }

  decltype(DWriteCreateFactory)* createDWriteFactory;
  HMODULE dwriteModule = LoadLibraryW(L"dwrite.dll");
  createDWriteFactory = (decltype(DWriteCreateFactory)*)
    GetProcAddress(dwriteModule, "DWriteCreateFactory");

  if (!createDWriteFactory) {
    gfxWarning() << "Failed to locate DWriteCreateFactory function.";
    return nullptr;
  }

  HRESULT hr = createDWriteFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown**>(&mDWriteFactory));

  if (FAILED(hr)) {
    gfxWarning() << "Failed to create DWrite Factory.";
  }

  return mDWriteFactory;
}

void
DrawTargetD2D1::CleanupD2D()
{
  if (mFactory) {
    RadialGradientEffectD2D1::Unregister(mFactory);
    ExtendInputEffectD2D1::Unregister(mFactory);
    mFactory->Release();
    mFactory = nullptr;
  }
}

void
DrawTargetD2D1::MarkChanged()
{
  if (mSnapshot) {
    if (mSnapshot->hasOneRef()) {
      // Just destroy it, since no-one else knows about it.
      mSnapshot = nullptr;
    } else {
      mSnapshot->DrawTargetWillChange();
      // The snapshot will no longer depend on this target.
      MOZ_ASSERT(!mSnapshot);
    }
  }
  if (mDependentTargets.size()) {
    // Copy mDependentTargets since the Flush()es below will modify it.
    TargetSet tmpTargets = mDependentTargets;
    for (TargetSet::iterator iter = tmpTargets.begin();
         iter != tmpTargets.end(); iter++) {
      (*iter)->Flush();
    }
    // The Flush() should have broken all dependencies on this target.
    MOZ_ASSERT(!mDependentTargets.size());
  }
}

bool
DrawTargetD2D1::ShouldClipTemporarySurfaceDrawing(CompositionOp aOp,
                                                  const Pattern& aPattern,
                                                  bool aClipIsComplex)
{
  bool patternSupported = IsPatternSupportedByD2D(aPattern);
  return patternSupported && !CurrentLayer().mIsOpaque && D2DSupportsCompositeMode(aOp) &&
         IsOperatorBoundByMask(aOp) && aClipIsComplex;
}

void
DrawTargetD2D1::PrepareForDrawing(CompositionOp aOp, const Pattern &aPattern)
{
  MarkChanged();

  bool patternSupported = IsPatternSupportedByD2D(aPattern);

  if (D2DSupportsPrimitiveBlendMode(aOp) && patternSupported) {
    // It's important to do this before FlushTransformToDC! As this will cause
    // the transform to become dirty.
    PushAllClips();

    FlushTransformToDC();

    if (aOp != CompositionOp::OP_OVER)
      mDC->SetPrimitiveBlend(D2DPrimitiveBlendMode(aOp));

    return;
  }

  HRESULT result = mDC->CreateCommandList(getter_AddRefs(mCommandList));
  mDC->SetTarget(mCommandList);
  mUsedCommandListsSincePurge++;

  // This is where we should have a valid command list.  If we don't, something is
  // wrong, and it's likely an OOM.
  if (!mCommandList) {
    gfxDevCrash(LogReason::InvalidCommandList) << "Invalid D2D1.1 command list on creation " << mUsedCommandListsSincePurge << ", " << gfx::hexa(result);
  }

  D2D1_RECT_F rect;
  bool isAligned;
  bool clipIsComplex = CurrentLayer().mPushedClips.size() && !GetDeviceSpaceClipRect(rect, isAligned);

  if (ShouldClipTemporarySurfaceDrawing(aOp, aPattern, clipIsComplex)) {
    PushClipsToDC(mDC);
  }

  FlushTransformToDC();
}

void
DrawTargetD2D1::FinalizeDrawing(CompositionOp aOp, const Pattern &aPattern)
{
  bool patternSupported = IsPatternSupportedByD2D(aPattern);

  if (D2DSupportsPrimitiveBlendMode(aOp) && patternSupported) {
    if (aOp != CompositionOp::OP_OVER)
      mDC->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
    return;
  }

  D2D1_RECT_F rect;
  bool isAligned;
  bool clipIsComplex = CurrentLayer().mPushedClips.size() && !GetDeviceSpaceClipRect(rect, isAligned);

  if (ShouldClipTemporarySurfaceDrawing(aOp, aPattern, clipIsComplex)) {
    PopClipsFromDC(mDC);
  }

  mDC->SetTarget(CurrentTarget());
  if (!mCommandList) {
    gfxDevCrash(LogReason::InvalidCommandList) << "Invalid D21.1 command list on finalize";
    return;
  }
  mCommandList->Close();

  RefPtr<ID2D1CommandList> source = mCommandList;
  mCommandList = nullptr;

  mDC->SetTransform(D2D1::IdentityMatrix());
  mTransformDirty = true;

  if (patternSupported) {
    if (D2DSupportsCompositeMode(aOp)) {
      RefPtr<ID2D1Image> tmpImage;
      if (clipIsComplex) {
        PopAllClips();
        if (!IsOperatorBoundByMask(aOp)) {
          tmpImage = GetImageForLayerContent();
        }
      }
      mDC->DrawImage(source, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR, D2DCompositionMode(aOp));

      if (tmpImage) {
        RefPtr<ID2D1ImageBrush> brush;
        RefPtr<ID2D1Geometry> inverseGeom = GetInverseClippedGeometry();
        mDC->CreateImageBrush(tmpImage, D2D1::ImageBrushProperties(D2D1::RectF(0, 0, mSize.width, mSize.height)),
                              getter_AddRefs(brush));

        mDC->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
        mDC->FillGeometry(inverseGeom, brush);
        mDC->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
      }
      return;
    }

    RefPtr<ID2D1Effect> blendEffect;
    HRESULT hr = mDC->CreateEffect(CLSID_D2D1Blend, getter_AddRefs(blendEffect));

    if (FAILED(hr) || !blendEffect) {
      gfxWarning() << "Failed to create blend effect!";
      return;
    }

    // We don't need to preserve the current content of this layer as the output
    // of the blend effect should completely replace it.
    RefPtr<ID2D1Image> tmpImage = GetImageForLayerContent(false);
    if (!tmpImage) {
      return;
    }

    blendEffect->SetInput(0, tmpImage);
    blendEffect->SetInput(1, source);
    blendEffect->SetValue(D2D1_BLEND_PROP_MODE, D2DBlendMode(aOp));

    mDC->DrawImage(blendEffect, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR, D2D1_COMPOSITE_MODE_BOUNDED_SOURCE_COPY);

    // This may seem a little counter intuitive. If this is false, we go through the regular
    // codepaths and set it to true. When this was true, GetImageForLayerContent will return
    // a bitmap for the current command list and we will no longer have a complex blend
    // with a list for tmpImage. Therefore we can set it to false again.
    mDidComplexBlendWithListInList = !mDidComplexBlendWithListInList;

    return;
  }

  const RadialGradientPattern *pat = static_cast<const RadialGradientPattern*>(&aPattern);
  if (pat->mCenter1 == pat->mCenter2 && pat->mRadius1 == pat->mRadius2) {
    // Draw nothing!
    return;
  }

  if (!pat->mStops) {
    // Draw nothing because of no color stops
    return;
  }

  RefPtr<ID2D1Effect> radialGradientEffect;

  HRESULT hr = mDC->CreateEffect(CLSID_RadialGradientEffect, getter_AddRefs(radialGradientEffect));
  if (FAILED(hr) || !radialGradientEffect) {
    gfxWarning() << "Failed to create radial gradient effect. Code: " << hexa(hr);
    return;
  }

  radialGradientEffect->SetValue(RADIAL_PROP_STOP_COLLECTION,
                                 static_cast<const GradientStopsD2D*>(pat->mStops.get())->mStopCollection);
  radialGradientEffect->SetValue(RADIAL_PROP_CENTER_1, D2D1::Vector2F(pat->mCenter1.x, pat->mCenter1.y));
  radialGradientEffect->SetValue(RADIAL_PROP_CENTER_2, D2D1::Vector2F(pat->mCenter2.x, pat->mCenter2.y));
  radialGradientEffect->SetValue(RADIAL_PROP_RADIUS_1, pat->mRadius1);
  radialGradientEffect->SetValue(RADIAL_PROP_RADIUS_2, pat->mRadius2);
  radialGradientEffect->SetValue(RADIAL_PROP_RADIUS_2, pat->mRadius2);
  radialGradientEffect->SetValue(RADIAL_PROP_TRANSFORM, D2DMatrix(pat->mMatrix * mTransform));
  radialGradientEffect->SetInput(0, source);

  mDC->DrawImage(radialGradientEffect, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR, D2DCompositionMode(aOp));
}

void
DrawTargetD2D1::AddDependencyOnSource(SourceSurfaceD2D1* aSource)
{
  if (aSource->mDrawTarget && !mDependingOnTargets.count(aSource->mDrawTarget)) {
    aSource->mDrawTarget->mDependentTargets.insert(this);
    mDependingOnTargets.insert(aSource->mDrawTarget);
  }
}

static D2D1_RECT_F
IntersectRect(const D2D1_RECT_F& aRect1, const D2D1_RECT_F& aRect2)
{
  D2D1_RECT_F result;
  result.left = max(aRect1.left, aRect2.left);
  result.top = max(aRect1.top, aRect2.top);
  result.right = min(aRect1.right, aRect2.right);
  result.bottom = min(aRect1.bottom, aRect2.bottom);

  result.right = max(result.right, result.left);
  result.bottom = max(result.bottom, result.top);

  return result;
}

bool
DrawTargetD2D1::GetDeviceSpaceClipRect(D2D1_RECT_F& aClipRect, bool& aIsPixelAligned)
{
  if (!CurrentLayer().mPushedClips.size()) {
    return false;
  }

  aIsPixelAligned = true;
  aClipRect = D2D1::RectF(0, 0, mSize.width, mSize.height);
  for (auto iter = CurrentLayer().mPushedClips.begin();iter != CurrentLayer().mPushedClips.end(); iter++) {
    if (iter->mGeometry) {
      return false;
    }
    aClipRect = IntersectRect(aClipRect, iter->mBounds);
    if (!iter->mIsPixelAligned) {
      aIsPixelAligned = false;
    }
  }
  return true;
}

already_AddRefed<ID2D1Image>
DrawTargetD2D1::GetImageForLayerContent(bool aShouldPreserveContent)
{
  PopAllClips();

  if (!CurrentLayer().mCurrentList) {
    RefPtr<ID2D1Bitmap> tmpBitmap;
    HRESULT hr = mDC->CreateBitmap(D2DIntSize(mSize), D2D1::BitmapProperties(D2DPixelFormat(mFormat)), getter_AddRefs(tmpBitmap));
    if (FAILED(hr)) {
      gfxCriticalError(CriticalLog::DefaultOptions(Factory::ReasonableSurfaceSize(mSize))) << "[D2D1.1] 6CreateBitmap failure " << mSize << " Code: " << hexa(hr) << " format " << (int)mFormat;
      // If it's a recreate target error, return and handle it elsewhere.
      if (hr == D2DERR_RECREATE_TARGET) {
        mDC->Flush();
        return nullptr;
      }
      // For now, crash in other scenarios; this should happen because tmpBitmap is
      // null and CopyFromBitmap call below dereferences it.
    }
    mDC->Flush();

    tmpBitmap->CopyFromBitmap(nullptr, mBitmap, nullptr);
    return tmpBitmap.forget();
  } else {
    RefPtr<ID2D1CommandList> list = CurrentLayer().mCurrentList;
    mDC->CreateCommandList(getter_AddRefs(CurrentLayer().mCurrentList));
    mDC->SetTarget(CurrentTarget());
    list->Close();

    RefPtr<ID2D1Bitmap1> tmpBitmap;
    if (mDidComplexBlendWithListInList) {
      D2D1_BITMAP_PROPERTIES1 props =
        D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET,
                                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                                                  D2D1_ALPHA_MODE_PREMULTIPLIED));
      mDC->CreateBitmap(mBitmap->GetPixelSize(), nullptr, 0, &props, getter_AddRefs(tmpBitmap));
      mDC->SetTransform(D2D1::IdentityMatrix());
      mDC->SetTarget(tmpBitmap);
      mDC->DrawImage(list, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR, D2D1_COMPOSITE_MODE_BOUNDED_SOURCE_COPY);
      mDC->SetTarget(CurrentTarget());
    }

    DCCommandSink sink(mDC);

    if (aShouldPreserveContent) {
      list->Stream(&sink);
      PushAllClips();
    }

    if (mDidComplexBlendWithListInList) {
      return tmpBitmap.forget();
    }

    return list.forget();
  }
}

already_AddRefed<ID2D1Geometry>
DrawTargetD2D1::GetClippedGeometry(IntRect *aClipBounds)
{
  if (mCurrentClippedGeometry) {
    *aClipBounds = mCurrentClipBounds;
    RefPtr<ID2D1Geometry> clippedGeometry(mCurrentClippedGeometry);
    return clippedGeometry.forget();
  }

  MOZ_ASSERT(CurrentLayer().mPushedClips.size());

  mCurrentClipBounds = IntRect(IntPoint(0, 0), mSize);

  // if pathGeom is null then pathRect represents the path.
  RefPtr<ID2D1Geometry> pathGeom;
  D2D1_RECT_F pathRect;
  bool pathRectIsAxisAligned = false;
  auto iter = CurrentLayer().mPushedClips.begin();

  if (iter->mGeometry) {
    pathGeom = GetTransformedGeometry(iter->mGeometry, iter->mTransform);
  } else {
    pathRect = iter->mBounds;
    pathRectIsAxisAligned = iter->mIsPixelAligned;
  }

  iter++;
  for (;iter != CurrentLayer().mPushedClips.end(); iter++) {
    // Do nothing but add it to the current clip bounds.
    if (!iter->mGeometry && iter->mIsPixelAligned) {
      mCurrentClipBounds.IntersectRect(mCurrentClipBounds,
        IntRect(int32_t(iter->mBounds.left), int32_t(iter->mBounds.top),
                int32_t(iter->mBounds.right - iter->mBounds.left),
                int32_t(iter->mBounds.bottom - iter->mBounds.top)));
      continue;
    }

    if (!pathGeom) {
      if (pathRectIsAxisAligned) {
        mCurrentClipBounds.IntersectRect(mCurrentClipBounds,
          IntRect(int32_t(pathRect.left), int32_t(pathRect.top),
                  int32_t(pathRect.right - pathRect.left),
                  int32_t(pathRect.bottom - pathRect.top)));
      }
      if (iter->mGeometry) {
        // See if pathRect needs to go into the path geometry.
        if (!pathRectIsAxisAligned) {
          pathGeom = ConvertRectToGeometry(pathRect);
        } else {
          pathGeom = GetTransformedGeometry(iter->mGeometry, iter->mTransform);
        }
      } else {
        pathRect = IntersectRect(pathRect, iter->mBounds);
        pathRectIsAxisAligned = false;
        continue;
      }
    }

    RefPtr<ID2D1PathGeometry> newGeom;
    factory()->CreatePathGeometry(getter_AddRefs(newGeom));

    RefPtr<ID2D1GeometrySink> currentSink;
    newGeom->Open(getter_AddRefs(currentSink));

    if (iter->mGeometry) {
      pathGeom->CombineWithGeometry(iter->mGeometry, D2D1_COMBINE_MODE_INTERSECT,
                                    iter->mTransform, currentSink);
    } else {
      RefPtr<ID2D1Geometry> rectGeom = ConvertRectToGeometry(iter->mBounds);
      pathGeom->CombineWithGeometry(rectGeom, D2D1_COMBINE_MODE_INTERSECT,
                                    D2D1::IdentityMatrix(), currentSink);
    }

    currentSink->Close();

    pathGeom = newGeom.forget();
  }

  // For now we need mCurrentClippedGeometry to always be non-nullptr. This
  // method might seem a little strange but it is just fine, if pathGeom is
  // nullptr pathRect will always still contain 1 clip unaccounted for
  // regardless of mCurrentClipBounds.
  if (!pathGeom) {
    pathGeom = ConvertRectToGeometry(pathRect);
  }
  mCurrentClippedGeometry = pathGeom.forget();
  *aClipBounds = mCurrentClipBounds;
  RefPtr<ID2D1Geometry> clippedGeometry(mCurrentClippedGeometry);
  return clippedGeometry.forget();
}

already_AddRefed<ID2D1Geometry>
DrawTargetD2D1::GetInverseClippedGeometry()
{
  IntRect bounds;
  RefPtr<ID2D1Geometry> geom = GetClippedGeometry(&bounds);
  RefPtr<ID2D1RectangleGeometry> rectGeom;
  RefPtr<ID2D1PathGeometry> inverseGeom;

  factory()->CreateRectangleGeometry(D2D1::RectF(0, 0, mSize.width, mSize.height), getter_AddRefs(rectGeom));
  factory()->CreatePathGeometry(getter_AddRefs(inverseGeom));
  RefPtr<ID2D1GeometrySink> sink;
  inverseGeom->Open(getter_AddRefs(sink));
  rectGeom->CombineWithGeometry(geom, D2D1_COMBINE_MODE_EXCLUDE, D2D1::IdentityMatrix(), sink);
  sink->Close();

  return inverseGeom.forget();
}

void
DrawTargetD2D1::PopAllClips()
{
  if (CurrentLayer().mClipsArePushed) {
    PopClipsFromDC(mDC);
  
    CurrentLayer().mClipsArePushed = false;
  }
}

void
DrawTargetD2D1::PushAllClips()
{
  if (!CurrentLayer().mClipsArePushed) {
    PushClipsToDC(mDC);
  
    CurrentLayer().mClipsArePushed = true;
  }
}

void
DrawTargetD2D1::PushClipsToDC(ID2D1DeviceContext *aDC, bool aForceIgnoreAlpha, const D2D1_RECT_F& aMaxRect)
{
  mDC->SetTransform(D2D1::IdentityMatrix());
  mTransformDirty = true;

  for (auto iter = CurrentLayer().mPushedClips.begin(); iter != CurrentLayer().mPushedClips.end(); iter++) {
    if (iter->mGeometry) {
      PushD2DLayer(aDC, iter->mGeometry, iter->mTransform, iter->mIsPixelAligned, aForceIgnoreAlpha, aMaxRect);
    } else {
      mDC->PushAxisAlignedClip(iter->mBounds, iter->mIsPixelAligned ? D2D1_ANTIALIAS_MODE_ALIASED : D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    }
  }
}

void
DrawTargetD2D1::PopClipsFromDC(ID2D1DeviceContext *aDC)
{
  for (int i = CurrentLayer().mPushedClips.size() - 1; i >= 0; i--) {
    if (CurrentLayer().mPushedClips[i].mGeometry) {
      aDC->PopLayer();
    } else {
      aDC->PopAxisAlignedClip();
    }
  }
}

already_AddRefed<ID2D1Brush>
DrawTargetD2D1::CreateTransparentBlackBrush()
{
  return GetSolidColorBrush(D2D1::ColorF(0, 0));
}

already_AddRefed<ID2D1SolidColorBrush>
DrawTargetD2D1::GetSolidColorBrush(const D2D_COLOR_F& aColor)
{
  RefPtr<ID2D1SolidColorBrush> brush = mSolidColorBrush;
  brush->SetColor(aColor);
  return brush.forget();
}

already_AddRefed<ID2D1Brush>
DrawTargetD2D1::CreateBrushForPattern(const Pattern &aPattern, Float aAlpha)
{
  if (!IsPatternSupportedByD2D(aPattern)) {
    return GetSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f));
  }

  if (aPattern.GetType() == PatternType::COLOR) {
    Color color = static_cast<const ColorPattern*>(&aPattern)->mColor;
    return GetSolidColorBrush(D2D1::ColorF(color.r, color.g, color.b, color.a * aAlpha));
  }
  if (aPattern.GetType() == PatternType::LINEAR_GRADIENT) {
    RefPtr<ID2D1LinearGradientBrush> gradBrush;
    const LinearGradientPattern *pat =
      static_cast<const LinearGradientPattern*>(&aPattern);

    GradientStopsD2D *stops = static_cast<GradientStopsD2D*>(pat->mStops.get());

    if (!stops) {
      gfxDebug() << "No stops specified for gradient pattern.";
      return CreateTransparentBlackBrush();
    }

    if (pat->mBegin == pat->mEnd) {
      uint32_t stopCount = stops->mStopCollection->GetGradientStopCount();
      vector<D2D1_GRADIENT_STOP> d2dStops(stopCount);
      stops->mStopCollection->GetGradientStops(&d2dStops.front(), stopCount);
      d2dStops.back().color.a *= aAlpha;
      return GetSolidColorBrush(d2dStops.back().color);
    }

    mDC->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2DPoint(pat->mBegin),
                                                                       D2DPoint(pat->mEnd)),
                                   D2D1::BrushProperties(aAlpha, D2DMatrix(pat->mMatrix)),
                                   stops->mStopCollection,
                                   getter_AddRefs(gradBrush));

    if (!gradBrush) {
      gfxWarning() << "Couldn't create gradient brush.";
      return CreateTransparentBlackBrush();
    }

    return gradBrush.forget();
  }
  if (aPattern.GetType() == PatternType::RADIAL_GRADIENT) {
    RefPtr<ID2D1RadialGradientBrush> gradBrush;
    const RadialGradientPattern *pat =
      static_cast<const RadialGradientPattern*>(&aPattern);

    GradientStopsD2D *stops = static_cast<GradientStopsD2D*>(pat->mStops.get());

    if (!stops) {
      gfxDebug() << "No stops specified for gradient pattern.";
      return CreateTransparentBlackBrush();
    }

    // This will not be a complex radial gradient brush.
    mDC->CreateRadialGradientBrush(
      D2D1::RadialGradientBrushProperties(D2DPoint(pat->mCenter2),
                                          D2DPoint(pat->mCenter1 - pat->mCenter2),
                                          pat->mRadius2, pat->mRadius2),
      D2D1::BrushProperties(aAlpha, D2DMatrix(pat->mMatrix)),
                            stops->mStopCollection,
                            getter_AddRefs(gradBrush));

    if (!gradBrush) {
      gfxWarning() << "Couldn't create gradient brush.";
      return CreateTransparentBlackBrush();
    }

    return gradBrush.forget();
  }
  if (aPattern.GetType() == PatternType::SURFACE) {
    const SurfacePattern *pat =
      static_cast<const SurfacePattern*>(&aPattern);

    if (!pat->mSurface) {
      gfxDebug() << "No source surface specified for surface pattern";
      return CreateTransparentBlackBrush();
    }

    D2D1_RECT_F samplingBounds;
    Matrix mat = pat->mMatrix;

    MOZ_ASSERT(pat->mSurface->IsValid());

    RefPtr<ID2D1Image> image = GetImageForSurface(pat->mSurface, mat, pat->mExtendMode, !pat->mSamplingRect.IsEmpty() ? &pat->mSamplingRect : nullptr);

    if (pat->mSurface->GetFormat() == SurfaceFormat::A8) {
      // See bug 1251431, at least FillOpacityMask does not appear to allow a source bitmapbrush
      // with source format A8. This creates a BGRA surface with the same alpha values that
      // the A8 surface has.
      RefPtr<ID2D1Bitmap> bitmap;
      HRESULT hr = image->QueryInterface((ID2D1Bitmap**)getter_AddRefs(bitmap));
      if (SUCCEEDED(hr) && bitmap) {
        RefPtr<ID2D1Image> oldTarget;
        RefPtr<ID2D1Bitmap1> tmpBitmap;
        mDC->CreateBitmap(D2D1::SizeU(pat->mSurface->GetSize().width, pat->mSurface->GetSize().height), nullptr, 0,
                          D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
                          getter_AddRefs(tmpBitmap));
        mDC->GetTarget(getter_AddRefs(oldTarget));
        mDC->SetTarget(tmpBitmap);

        RefPtr<ID2D1SolidColorBrush> brush;
        mDC->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), getter_AddRefs(brush));
        mDC->FillOpacityMask(bitmap, brush);
        mDC->SetTarget(oldTarget);
        image = tmpBitmap;
      }
    }

    if (!image) {
      return CreateTransparentBlackBrush();
    }

    if (pat->mSamplingRect.IsEmpty()) {
      RefPtr<ID2D1Bitmap> bitmap;
      HRESULT hr = image->QueryInterface((ID2D1Bitmap**)getter_AddRefs(bitmap));
      if (SUCCEEDED(hr) && bitmap) {
        /**
         * Create the brush with the proper repeat modes.
         */
        RefPtr<ID2D1BitmapBrush> bitmapBrush;
        D2D1_EXTEND_MODE xRepeat = D2DExtend(pat->mExtendMode, Axis::X_AXIS);
        D2D1_EXTEND_MODE yRepeat = D2DExtend(pat->mExtendMode, Axis::Y_AXIS);

        mDC->CreateBitmapBrush(bitmap,
                               D2D1::BitmapBrushProperties(xRepeat, yRepeat,
                                                           D2DFilter(pat->mSamplingFilter)),
                               D2D1::BrushProperties(aAlpha, D2DMatrix(mat)),
                               getter_AddRefs(bitmapBrush));
        if (!bitmapBrush) {
          gfxWarning() << "Couldn't create bitmap brush!";
          return CreateTransparentBlackBrush();
        }
        return bitmapBrush.forget();
      }
    }

    RefPtr<ID2D1ImageBrush> imageBrush;
    if (pat->mSamplingRect.IsEmpty()) {
      samplingBounds = D2D1::RectF(0, 0,
                                   Float(pat->mSurface->GetSize().width),
                                   Float(pat->mSurface->GetSize().height));
    } else if (pat->mSurface->GetType() == SurfaceType::D2D1_1_IMAGE) {
      samplingBounds = D2DRect(pat->mSamplingRect);
      mat.PreTranslate(pat->mSamplingRect.x, pat->mSamplingRect.y);
    } else {
      // We will do a partial upload of the sampling restricted area from GetImageForSurface.
      samplingBounds = D2D1::RectF(0, 0, pat->mSamplingRect.width, pat->mSamplingRect.height);
    }

    D2D1_EXTEND_MODE xRepeat = D2DExtend(pat->mExtendMode, Axis::X_AXIS);
    D2D1_EXTEND_MODE yRepeat = D2DExtend(pat->mExtendMode, Axis::Y_AXIS);

    mDC->CreateImageBrush(image,
                          D2D1::ImageBrushProperties(samplingBounds,
                                                     xRepeat,
                                                     yRepeat,
                                                     D2DInterpolationMode(pat->mSamplingFilter)),
                          D2D1::BrushProperties(aAlpha, D2DMatrix(mat)),
                          getter_AddRefs(imageBrush));

    if (!imageBrush) {
      gfxWarning() << "Couldn't create image brush!";
      return CreateTransparentBlackBrush();
    }

    return imageBrush.forget();
  }

  gfxWarning() << "Invalid pattern type detected.";
  return CreateTransparentBlackBrush();
}

already_AddRefed<ID2D1Image>
DrawTargetD2D1::GetImageForSurface(SourceSurface *aSurface, Matrix &aSourceTransform,
                                   ExtendMode aExtendMode, const IntRect* aSourceRect,
                                   bool aUserSpace)
{
  RefPtr<ID2D1Image> image;

  switch (aSurface->GetType()) {
  case SurfaceType::D2D1_1_IMAGE:
    {
      SourceSurfaceD2D1 *surf = static_cast<SourceSurfaceD2D1*>(aSurface);
      image = surf->GetImage();
      AddDependencyOnSource(surf);
    }
    break;
  case SurfaceType::DUAL_DT:
    {
      // Sometimes we have a dual drawtarget but the underlying targets
      // are d2d surfaces. Let's not readback and reupload in those cases.
      SourceSurfaceDual* surface = static_cast<SourceSurfaceDual*>(aSurface);
      SourceSurface* first = surface->GetFirstSurface();
      if (first->GetType() == SurfaceType::D2D1_1_IMAGE) {
        MOZ_ASSERT(surface->SameSurfaceTypes());
        SourceSurfaceD2D1* d2dSurface = static_cast<SourceSurfaceD2D1*>(first);
        image = d2dSurface->GetImage();
        AddDependencyOnSource(d2dSurface); 
        break;
      }
      // Otherwise fall through
  }
  default:
    {
      RefPtr<DataSourceSurface> dataSurf = aSurface->GetDataSurface();
      if (!dataSurf) {
        gfxWarning() << "Invalid surface type.";
        return nullptr;
      }
      Matrix transform = aUserSpace ? mTransform : Matrix();
      return CreatePartialBitmapForSurface(dataSurf, transform, mSize, aExtendMode,
                                           aSourceTransform, mDC, aSourceRect);
    }
    break;
  }

  return image.forget();
}

already_AddRefed<SourceSurface>
DrawTargetD2D1::OptimizeSourceSurface(SourceSurface* aSurface) const
{
  if (aSurface->GetType() == SurfaceType::D2D1_1_IMAGE) {
    RefPtr<SourceSurface> surface(aSurface);
    return surface.forget();
  }

  RefPtr<DataSourceSurface> data = aSurface->GetDataSurface();

  RefPtr<ID2D1Bitmap1> bitmap;
  {
    DataSourceSurface::ScopedMap map(data, DataSourceSurface::READ);
    if (MOZ2D_WARN_IF(!map.IsMapped())) {
      return nullptr;
    }

    HRESULT hr = mDC->CreateBitmap(D2DIntSize(data->GetSize()), map.GetData(), map.GetStride(),
                                   D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE, D2DPixelFormat(data->GetFormat())),
                                   getter_AddRefs(bitmap));

    if (FAILED(hr)) {
      gfxCriticalError(CriticalLog::DefaultOptions(Factory::ReasonableSurfaceSize(data->GetSize()))) << "[D2D1.1] 4CreateBitmap failure " << data->GetSize() << " Code: " << hexa(hr) << " format " << (int)data->GetFormat();
    }
  }

  if (!bitmap) {
    return data.forget();
  }

  return MakeAndAddRef<SourceSurfaceD2D1>(bitmap.get(), mDC, data->GetFormat(), data->GetSize());
}

void
DrawTargetD2D1::PushD2DLayer(ID2D1DeviceContext *aDC, ID2D1Geometry *aGeometry, const D2D1_MATRIX_3X2_F &aTransform,
                             bool aPixelAligned, bool aForceIgnoreAlpha, const D2D1_RECT_F& aMaxRect)
{
  D2D1_LAYER_OPTIONS1 options = D2D1_LAYER_OPTIONS1_NONE;

  if (CurrentLayer().mIsOpaque || aForceIgnoreAlpha) {
    options = D2D1_LAYER_OPTIONS1_IGNORE_ALPHA | D2D1_LAYER_OPTIONS1_INITIALIZE_FROM_BACKGROUND;
  }

  D2D1_ANTIALIAS_MODE antialias =
    aPixelAligned ? D2D1_ANTIALIAS_MODE_ALIASED : D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;

  mDC->PushLayer(D2D1::LayerParameters1(aMaxRect, aGeometry, antialias, aTransform,
                                        1.0, nullptr, options), nullptr);
}

bool
DrawTargetD2D1::IsDeviceContextValid() {
  return (mDeviceSeq == Factory::GetD2D1DeviceSeq()) && Factory::GetD2D1Device();
}

}
}
