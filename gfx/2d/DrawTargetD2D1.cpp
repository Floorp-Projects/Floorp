/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <initguid.h>
#include "DrawTargetD2D1.h"
#include "DrawTargetD2D.h"
#include "FilterNodeSoftware.h"
#include "GradientStopsD2D.h"
#include "SourceSurfaceD2D1.h"
#include "SourceSurfaceD2D.h"
#include "RadialGradientEffectD2D1.h"

#include "HelpersD2D.h"
#include "FilterNodeD2D1.h"
#include "Tools.h"

using namespace std;

namespace mozilla {
namespace gfx {

uint64_t DrawTargetD2D1::mVRAMUsageDT;
uint64_t DrawTargetD2D1::mVRAMUsageSS;
ID2D1Factory1* DrawTargetD2D1::mFactory = nullptr;

ID2D1Factory1 *D2DFactory1()
{
  return DrawTargetD2D1::factory();
}

DrawTargetD2D1::DrawTargetD2D1()
  : mClipsArePushed(false)
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

  mDC->EndDraw();

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

TemporaryRef<SourceSurface>
DrawTargetD2D1::Snapshot()
{
  if (mSnapshot) {
    return mSnapshot;
  }
  PopAllClips();

  mDC->Flush();

  mSnapshot = new SourceSurfaceD2D1(mBitmap, mDC, mFormat, mSize, this);

  return mSnapshot;
}

void
DrawTargetD2D1::Flush()
{
  mDC->Flush();

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

  mDC->CreateImageBrush(image,
                        D2D1::ImageBrushProperties(samplingBounds,
                                                   D2D1_EXTEND_MODE_CLAMP,
                                                   D2D1_EXTEND_MODE_CLAMP,
                                                   D2DInterpolationMode(aSurfOptions.mFilter)),
                        D2D1::BrushProperties(aOptions.mAlpha, D2DMatrix(transform)),
                        byRef(brush));
  mDC->FillRectangle(D2DRect(aDest), brush);

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
  RefPtr<ID2D1Image> image = GetImageForSurface(aSurface, mat, ExtendMode::CLAMP);

  if (!mat.IsIdentity()) {
    gfxDebug() << *this << ": At this point complex partial uploads are not supported for Shadow surfaces.";
    return;
  }

  // Step 1, create the shadow effect.
  RefPtr<ID2D1Effect> shadowEffect;
  mDC->CreateEffect(CLSID_D2D1Shadow, byRef(shadowEffect));
  shadowEffect->SetInput(0, image);
  shadowEffect->SetValue(D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION, aSigma);
  D2D1_VECTOR_4F color = { aColor.r, aColor.g, aColor.b, aColor.a };
  shadowEffect->SetValue(D2D1_SHADOW_PROP_COLOR, color);

  D2D1_POINT_2F shadowPoint = D2DPoint(aDest + aOffset);
  mDC->DrawImage(shadowEffect, &shadowPoint, nullptr, D2D1_INTERPOLATION_MODE_LINEAR, D2DCompositionMode(aOperator));

  D2D1_POINT_2F imgPoint = D2DPoint(aDest);
  mDC->DrawImage(image, &imgPoint, nullptr, D2D1_INTERPOLATION_MODE_LINEAR, D2DCompositionMode(aOperator));
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

  mDC->SetTarget(mTempBitmap);
  mDC->Clear();

  IntRect addClipRect;
  RefPtr<ID2D1Geometry> geom = GetClippedGeometry(&addClipRect);

  RefPtr<ID2D1SolidColorBrush> brush;
  mDC->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), byRef(brush));
  mDC->PushAxisAlignedClip(D2D1::RectF(addClipRect.x, addClipRect.y, addClipRect.XMost(), addClipRect.YMost()), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
  mDC->FillGeometry(geom, brush);
  mDC->PopAxisAlignedClip();

  mDC->SetTarget(mBitmap);
  mDC->DrawImage(mTempBitmap, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR, D2D1_COMPOSITE_MODE_DESTINATION_OUT);

  PopClip();

  return;
}

void
DrawTargetD2D1::MaskSurface(const Pattern &aSource,
                            SourceSurface *aMask,
                            Point aOffset,
                            const DrawOptions &aOptions)
{
  PrepareForDrawing(aOptions.mCompositionOp, aSource);

  RefPtr<ID2D1Bitmap> bitmap;

  RefPtr<ID2D1Image> image = GetImageForSurface(aMask, ExtendMode::CLAMP);

  // FillOpacityMask only works if the antialias mode is MODE_ALIASED
  mDC->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

  IntSize size = aMask->GetSize();
  Rect maskRect = Rect(0.f, 0.f, Float(size.width), Float(size.height));
  image->QueryInterface((ID2D1Bitmap**)&bitmap);
  if (!bitmap) {
    gfxWarning() << "FillOpacityMask only works with Bitmap source surfaces.";
    return;
  }

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

  Matrix mat;
  RefPtr<ID2D1Image> image = GetImageForSurface(aSurface, mat, ExtendMode::CLAMP);

  if (!mat.IsIdentity()) {
    gfxDebug() << *this << ": At this point complex partial uploads are not supported for CopySurface.";
    return;
  }

  if (mFormat == SurfaceFormat::A8) {
    RefPtr<ID2D1Bitmap> bitmap;
    image->QueryInterface((ID2D1Bitmap**)byRef(bitmap));

    mDC->PushAxisAlignedClip(D2D1::RectF(aDestination.x, aDestination.y,
                                         aDestination.x + aSourceRect.width,
                                         aDestination.y + aSourceRect.height),
                             D2D1_ANTIALIAS_MODE_ALIASED);
    mDC->Clear();
    mDC->PopAxisAlignedClip();

    RefPtr<ID2D1SolidColorBrush> brush;
    mDC->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White),
                               D2D1::BrushProperties(), byRef(brush));
    mDC->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
    mDC->FillOpacityMask(bitmap, brush, D2D1_OPACITY_MASK_CONTENT_GRAPHICS);
    mDC->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    return;
  }

  mDC->DrawImage(image, D2D1::Point2F(Float(aDestination.x), Float(aDestination.y)),
                 D2D1::RectF(Float(aSourceRect.x), Float(aSourceRect.y), 
                             Float(aSourceRect.XMost()), Float(aSourceRect.YMost())),
                 D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR, D2D1_COMPOSITE_MODE_BOUNDED_SOURCE_COPY);
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
  if (aPath->GetBackendType() != BackendType::DIRECT2D) {
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
  if (aPath->GetBackendType() != BackendType::DIRECT2D) {
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
  if (mFormat == SurfaceFormat::B8G8R8A8 && mPermitSubpixelAA &&
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
      mFormat != SurfaceFormat::B8G8R8X8 && !forceClearType) {
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

  if (brush) {
    mDC->DrawGlyphRun(D2D1::Point2F(), &autoRun, brush);
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
DrawTargetD2D1::PushClip(const Path *aPath)
{
  if (aPath->GetBackendType() != BackendType::DIRECT2D) {
    gfxDebug() << *this << ": Ignoring clipping call for incompatible path.";
    return;
  }

  mCurrentClippedGeometry = nullptr;

  RefPtr<PathD2D> pathD2D = static_cast<PathD2D*>(const_cast<Path*>(aPath));

  PushedClip clip;
  clip.mTransform = D2DMatrix(mTransform);
  clip.mPath = pathD2D;
  
  pathD2D->mGeometry->GetBounds(clip.mTransform, &clip.mBounds);

  mPushedClips.push_back(clip);

  // The transform of clips is relative to the world matrix, since we use the total
  // transform for the clips, make the world matrix identity.
  mDC->SetTransform(D2D1::IdentityMatrix());
  mTransformDirty = true;

  if (mClipsArePushed) {
    PushD2DLayer(mDC, pathD2D->mGeometry, clip.mTransform);
  }
}

void
DrawTargetD2D1::PushClipRect(const Rect &aRect)
{
  if (!mTransform.IsRectilinear()) {
    // Whoops, this isn't a rectangle in device space, Direct2D will not deal
    // with this transform the way we want it to.
    // See remarks: http://msdn.microsoft.com/en-us/library/dd316860%28VS.85%29.aspx

    RefPtr<PathBuilder> pathBuilder = CreatePathBuilder();
    pathBuilder->MoveTo(aRect.TopLeft());
    pathBuilder->LineTo(aRect.TopRight());
    pathBuilder->LineTo(aRect.BottomRight());
    pathBuilder->LineTo(aRect.BottomLeft());
    pathBuilder->Close();
    RefPtr<Path> path = pathBuilder->Finish();
    return PushClip(path);
  }

  mCurrentClippedGeometry = nullptr;

  PushedClip clip;
  Rect rect = mTransform.TransformBounds(aRect);
  IntRect intRect;
  clip.mIsPixelAligned = rect.ToIntRect(&intRect);

  // Do not store the transform, just store the device space rectangle directly.
  clip.mBounds = D2DRect(rect);

  mPushedClips.push_back(clip);

  mDC->SetTransform(D2D1::IdentityMatrix());
  mTransformDirty = true;

  if (mClipsArePushed) {
    mDC->PushAxisAlignedClip(clip.mBounds, clip.mIsPixelAligned ? D2D1_ANTIALIAS_MODE_ALIASED : D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
  }
}

void
DrawTargetD2D1::PopClip()
{
  mCurrentClippedGeometry = nullptr;

  if (mClipsArePushed) {
    if (mPushedClips.back().mPath) {
      mDC->PopLayer();
    } else {
      mDC->PopAxisAlignedClip();
    }
  }
  mPushedClips.pop_back();
}

TemporaryRef<SourceSurface>
DrawTargetD2D1::CreateSourceSurfaceFromData(unsigned char *aData,
                                            const IntSize &aSize,
                                            int32_t aStride,
                                            SurfaceFormat aFormat) const
{
  RefPtr<ID2D1Bitmap1> bitmap;

  HRESULT hr = mDC->CreateBitmap(D2DIntSize(aSize), aData, aStride,
                                 D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE, D2DPixelFormat(aFormat)),
                                 byRef(bitmap));

  if (FAILED(hr)) {
    gfxCriticalError() << "[D2D1.1] CreateBitmap failure " << aSize << " Code: " << hexa(hr);
  }

  if (!bitmap) {
    return nullptr;
  }

  return new SourceSurfaceD2D1(bitmap.get(), mDC, aFormat, aSize);
}

TemporaryRef<DrawTarget>
DrawTargetD2D1::CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const
{
  RefPtr<DrawTargetD2D1> dt = new DrawTargetD2D1();

  if (!dt->Init(aSize, aFormat)) {
    return nullptr;
  }

  return dt.forget();
}

TemporaryRef<PathBuilder>
DrawTargetD2D1::CreatePathBuilder(FillRule aFillRule) const
{
  RefPtr<ID2D1PathGeometry> path;
  HRESULT hr = factory()->CreatePathGeometry(byRef(path));

  if (FAILED(hr)) {
    gfxWarning() << *this << ": Failed to create Direct2D Path Geometry. Code: " << hexa(hr);
    return nullptr;
  }

  RefPtr<ID2D1GeometrySink> sink;
  hr = path->Open(byRef(sink));
  if (FAILED(hr)) {
    gfxWarning() << *this << ": Failed to access Direct2D Path Geometry. Code: " << hexa(hr);
    return nullptr;
  }

  if (aFillRule == FillRule::FILL_WINDING) {
    sink->SetFillMode(D2D1_FILL_MODE_WINDING);
  }

  return new PathBuilderD2D(sink, path, aFillRule);
}

TemporaryRef<GradientStops>
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
                                      D2D1_GAMMA_2_2, D2DExtend(aExtendMode),
                                      byRef(stopCollection));
  delete [] stops;

  if (FAILED(hr)) {
    gfxWarning() << *this << ": Failed to create GradientStopCollection. Code: " << hexa(hr);
    return nullptr;
  }

  return new GradientStopsD2D(stopCollection);
}

TemporaryRef<FilterNode>
DrawTargetD2D1::CreateFilter(FilterType aType)
{
  return FilterNodeD2D1::Create(mDC, aType);
}

bool
DrawTargetD2D1::Init(ID3D11Texture2D* aTexture, SurfaceFormat aFormat)
{
  HRESULT hr;

  hr = Factory::GetD2D1Device()->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS, byRef(mDC));

  if (FAILED(hr)) {
    gfxWarning() << *this << ": Error " << hexa(hr) << " failed to initialize new DeviceContext.";
    return false;
  }

  RefPtr<IDXGISurface> dxgiSurface;
  aTexture->QueryInterface(__uuidof(IDXGISurface),
                           (void**)((IDXGISurface**)byRef(dxgiSurface)));
  if (!dxgiSurface) {
    return false;
  }

  D2D1_BITMAP_PROPERTIES1 props;
  props.dpiX = 96;
  props.dpiY = 96;
  props.pixelFormat = D2DPixelFormat(aFormat);
  props.colorContext = nullptr;
  props.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
  hr = mDC->CreateBitmapFromDxgiSurface(dxgiSurface, props, (ID2D1Bitmap1**)byRef(mBitmap));

  if (FAILED(hr)) {
    gfxCriticalError() << "[D2D1.1] CreateBitmapFromDxgiSurface failure Code: " << hexa(hr);
    return false;
  }

  mFormat = aFormat;
  D3D11_TEXTURE2D_DESC desc;
  aTexture->GetDesc(&desc);
  desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  mSize.width = desc.Width;
  mSize.height = desc.Height;
  props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
  props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;

  hr = mDC->CreateBitmap(D2DIntSize(mSize), nullptr, 0, props, (ID2D1Bitmap1**)byRef(mTempBitmap));

  if (FAILED(hr)) {
    gfxCriticalError() << "[D2D1.1] CreateBitmap failure " << mSize << " Code: " << hexa(hr);
    return false;
  }

  mDC->SetTarget(mBitmap);

  mDC->BeginDraw();
  return true;
}

bool
DrawTargetD2D1::Init(const IntSize &aSize, SurfaceFormat aFormat)
{
  HRESULT hr;

  hr = Factory::GetD2D1Device()->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS, byRef(mDC));

  if (FAILED(hr)) {
    gfxWarning() << *this << ": Error " << hexa(hr) << " failed to initialize new DeviceContext.";
    return false;
  }

  if (mDC->GetMaximumBitmapSize() < UINT32(aSize.width) ||
      mDC->GetMaximumBitmapSize() < UINT32(aSize.height)) {
    // This is 'ok'
    gfxDebug() << *this << ": Attempt to use unsupported surface size for D2D 1.1.";
    return false;
  }

  D2D1_BITMAP_PROPERTIES1 props;
  props.dpiX = 96;
  props.dpiY = 96;
  props.pixelFormat = D2DPixelFormat(aFormat);
  props.colorContext = nullptr;
  props.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
  mDC->CreateBitmap(D2DIntSize(aSize), nullptr, 0, props, (ID2D1Bitmap1**)byRef(mBitmap));

  if (FAILED(hr)) {
    gfxWarning() << *this << ": Error " << hexa(hr) << " failed to create new CommandList.";
    return false;
  }

  props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
  props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;

  mDC->CreateBitmap(D2DIntSize(aSize), nullptr, 0, props, (ID2D1Bitmap1**)byRef(mTempBitmap));

  mDC->SetTarget(mBitmap);

  mDC->BeginDraw();

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

  HRESULT hr = D2DFactory()->QueryInterface((ID2D1Factory1**)&mFactory);

  if (FAILED(hr)) {
    return nullptr;
  }

  RadialGradientEffectD2D1::Register(mFactory);

  return mFactory;
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

void
DrawTargetD2D1::PrepareForDrawing(CompositionOp aOp, const Pattern &aPattern)
{
  MarkChanged();

  // It's important to do this before FlushTransformToDC! As this will cause
  // the transform to become dirty.
  if (!mClipsArePushed) {
    mClipsArePushed = true;
    PushClipsToDC(mDC);
  }

  FlushTransformToDC();

  if (aOp == CompositionOp::OP_OVER && IsPatternSupportedByD2D(aPattern)) {
    return;
  }

  mDC->SetTarget(mTempBitmap);
  mDC->Clear(D2D1::ColorF(0, 0));
}

void
DrawTargetD2D1::FinalizeDrawing(CompositionOp aOp, const Pattern &aPattern)
{
  bool patternSupported = IsPatternSupportedByD2D(aPattern);

  if (aOp == CompositionOp::OP_OVER && patternSupported) {
    return;
  }

  RefPtr<ID2D1Image> image;
  mDC->GetTarget(byRef(image));

  mDC->SetTarget(mBitmap);

  mDC->SetTransform(D2D1::IdentityMatrix());
  mTransformDirty = true;

  if (patternSupported) {
    if (D2DSupportsCompositeMode(aOp)) {
      mDC->DrawImage(image, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR, D2DCompositionMode(aOp));
      return;
    }

    if (!mBlendEffect) {
      mDC->CreateEffect(CLSID_D2D1Blend, byRef(mBlendEffect));

      if (!mBlendEffect) {
        gfxWarning() << "Failed to create blend effect!";
        return;
      }
    }

    RefPtr<ID2D1Bitmap> tmpBitmap;
    mDC->CreateBitmap(D2DIntSize(mSize), D2D1::BitmapProperties(D2DPixelFormat(mFormat)), byRef(tmpBitmap));

    // This flush is important since the copy method will not know about the context drawing to the surface.
    mDC->Flush();

    // We need to use a copy here because affects don't accept a surface on
    // both their in- and outputs.
    tmpBitmap->CopyFromBitmap(nullptr, mBitmap, nullptr);

    mBlendEffect->SetInput(0, tmpBitmap);
    mBlendEffect->SetInput(1, mTempBitmap);
    mBlendEffect->SetValue(D2D1_BLEND_PROP_MODE, D2DBlendMode(aOp));

    mDC->DrawImage(mBlendEffect, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR, D2D1_COMPOSITE_MODE_BOUNDED_SOURCE_COPY);
    return;
  }

  RefPtr<ID2D1Effect> radialGradientEffect;

  mDC->CreateEffect(CLSID_RadialGradientEffect, byRef(radialGradientEffect));
  const RadialGradientPattern *pat = static_cast<const RadialGradientPattern*>(&aPattern);

  radialGradientEffect->SetValue(RADIAL_PROP_STOP_COLLECTION,
                                 static_cast<const GradientStopsD2D*>(pat->mStops.get())->mStopCollection);
  radialGradientEffect->SetValue(RADIAL_PROP_CENTER_1, D2D1::Vector2F(pat->mCenter1.x, pat->mCenter1.y));
  radialGradientEffect->SetValue(RADIAL_PROP_CENTER_2, D2D1::Vector2F(pat->mCenter2.x, pat->mCenter2.y));
  radialGradientEffect->SetValue(RADIAL_PROP_RADIUS_1, pat->mRadius1);
  radialGradientEffect->SetValue(RADIAL_PROP_RADIUS_2, pat->mRadius2);
  radialGradientEffect->SetValue(RADIAL_PROP_RADIUS_2, pat->mRadius2);
  radialGradientEffect->SetValue(RADIAL_PROP_TRANSFORM, D2DMatrix(pat->mMatrix * mTransform));
  radialGradientEffect->SetInput(0, image);

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
  if (!mPushedClips.size()) {
    return false;
  }

  aClipRect = D2D1::RectF(0, 0, mSize.width, mSize.height);
  for (auto iter = mPushedClips.begin();iter != mPushedClips.end(); iter++) {
    if (iter->mPath) {
      return false;
    }
    aClipRect = IntersectRect(aClipRect, iter->mBounds);
    if (!iter->mIsPixelAligned) {
      aIsPixelAligned = false;
    }
  }
  return true;
}

TemporaryRef<ID2D1Geometry>
DrawTargetD2D1::GetClippedGeometry(IntRect *aClipBounds)
{
  if (mCurrentClippedGeometry) {
    *aClipBounds = mCurrentClipBounds;
    return mCurrentClippedGeometry;
  }

  mCurrentClipBounds = IntRect(IntPoint(0, 0), mSize);

  // if pathGeom is null then pathRect represents the path.
  RefPtr<ID2D1Geometry> pathGeom;
  D2D1_RECT_F pathRect;
  bool pathRectIsAxisAligned = false;
  auto iter = mPushedClips.begin();

  if (iter->mPath) {
    pathGeom = GetTransformedGeometry(iter->mPath->GetGeometry(), iter->mTransform);
  } else {
    pathRect = iter->mBounds;
    pathRectIsAxisAligned = iter->mIsPixelAligned;
  }

  iter++;
  for (;iter != mPushedClips.end(); iter++) {
    // Do nothing but add it to the current clip bounds.
    if (!iter->mPath && iter->mIsPixelAligned) {
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
      if (iter->mPath) {
        // See if pathRect needs to go into the path geometry.
        if (!pathRectIsAxisAligned) {
          pathGeom = ConvertRectToGeometry(pathRect);
        } else {
          pathGeom = GetTransformedGeometry(iter->mPath->GetGeometry(), iter->mTransform);
        }
      } else {
        pathRect = IntersectRect(pathRect, iter->mBounds);
        pathRectIsAxisAligned = false;
        continue;
      }
    }

    RefPtr<ID2D1PathGeometry> newGeom;
    factory()->CreatePathGeometry(byRef(newGeom));

    RefPtr<ID2D1GeometrySink> currentSink;
    newGeom->Open(byRef(currentSink));

    if (iter->mPath) {
      pathGeom->CombineWithGeometry(iter->mPath->GetGeometry(), D2D1_COMBINE_MODE_INTERSECT,
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
  return mCurrentClippedGeometry;
}

void
DrawTargetD2D1::PopAllClips()
{
  if (mClipsArePushed) {
    PopClipsFromDC(mDC);
  
    mClipsArePushed = false;
  }
}

void
DrawTargetD2D1::PushClipsToDC(ID2D1DeviceContext *aDC)
{
  mDC->SetTransform(D2D1::IdentityMatrix());
  mTransformDirty = true;

  for (std::vector<PushedClip>::iterator iter = mPushedClips.begin();
        iter != mPushedClips.end(); iter++) {
    if (iter->mPath) {
      PushD2DLayer(aDC, iter->mPath->mGeometry, iter->mTransform);
    } else {
      mDC->PushAxisAlignedClip(iter->mBounds, iter->mIsPixelAligned ? D2D1_ANTIALIAS_MODE_ALIASED : D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    }
  }
}

void
DrawTargetD2D1::PopClipsFromDC(ID2D1DeviceContext *aDC)
{
  for (int i = mPushedClips.size() - 1; i >= 0; i--) {
    if (mPushedClips[i].mPath) {
      aDC->PopLayer();
    } else {
      aDC->PopAxisAlignedClip();
    }
  }
}

TemporaryRef<ID2D1Brush>
DrawTargetD2D1::CreateTransparentBlackBrush()
{
  RefPtr<ID2D1SolidColorBrush> brush;
  mDC->CreateSolidColorBrush(D2D1::ColorF(0, 0), byRef(brush));
  return brush;
}

TemporaryRef<ID2D1Brush>
DrawTargetD2D1::CreateBrushForPattern(const Pattern &aPattern, Float aAlpha)
{
  if (!IsPatternSupportedByD2D(aPattern)) {
    RefPtr<ID2D1SolidColorBrush> colBrush;
    mDC->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), byRef(colBrush));
    return colBrush.forget();
  }

  if (aPattern.GetType() == PatternType::COLOR) {
    RefPtr<ID2D1SolidColorBrush> colBrush;
    Color color = static_cast<const ColorPattern*>(&aPattern)->mColor;
    mDC->CreateSolidColorBrush(D2D1::ColorF(color.r, color.g,
                                            color.b, color.a),
                               D2D1::BrushProperties(aAlpha),
                               byRef(colBrush));
    return colBrush.forget();
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
      RefPtr<ID2D1SolidColorBrush> colBrush;
      uint32_t stopCount = stops->mStopCollection->GetGradientStopCount();
      vector<D2D1_GRADIENT_STOP> d2dStops(stopCount);
      stops->mStopCollection->GetGradientStops(&d2dStops.front(), stopCount);
      mDC->CreateSolidColorBrush(d2dStops.back().color,
                                 D2D1::BrushProperties(aAlpha),
                                 byRef(colBrush));
      return colBrush.forget();
    }

    mDC->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2DPoint(pat->mBegin),
                                                                       D2DPoint(pat->mEnd)),
                                   D2D1::BrushProperties(aAlpha, D2DMatrix(pat->mMatrix)),
                                   stops->mStopCollection,
                                   byRef(gradBrush));
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
                            byRef(gradBrush));

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
    if (!pat->mSamplingRect.IsEmpty()) {
      samplingBounds = D2DRect(pat->mSamplingRect);
      mat.PreTranslate(pat->mSamplingRect.x, pat->mSamplingRect.y);
    } else {
      samplingBounds = D2D1::RectF(0, 0,
                                   Float(pat->mSurface->GetSize().width),
                                   Float(pat->mSurface->GetSize().height));
    }

    RefPtr<ID2D1ImageBrush> imageBrush;
    RefPtr<ID2D1Image> image = GetImageForSurface(pat->mSurface, mat, pat->mExtendMode);
    mDC->CreateImageBrush(image,
                          D2D1::ImageBrushProperties(samplingBounds,
                                                     D2DExtend(pat->mExtendMode),
                                                     D2DExtend(pat->mExtendMode),
                                                     D2DInterpolationMode(pat->mFilter)),
                          D2D1::BrushProperties(aAlpha, D2DMatrix(mat)),
                          byRef(imageBrush));
    return imageBrush.forget();
  }

  gfxWarning() << "Invalid pattern type detected.";
  return CreateTransparentBlackBrush();
}

TemporaryRef<ID2D1Image>
DrawTargetD2D1::GetImageForSurface(SourceSurface *aSurface, Matrix &aSourceTransform,
                                   ExtendMode aExtendMode)
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
  default:
    {
      RefPtr<DataSourceSurface> dataSurf = aSurface->GetDataSurface();
      if (!dataSurf) {
        gfxWarning() << "Invalid surface type.";
        return nullptr;
      }
      return CreatePartialBitmapForSurface(dataSurf, mTransform, mSize, aExtendMode,
                                           aSourceTransform, mDC);
    }
    break;
  }

  return image.forget();
}

TemporaryRef<SourceSurface>
DrawTargetD2D1::OptimizeSourceSurface(SourceSurface* aSurface) const
{
  if (aSurface->GetType() == SurfaceType::D2D1_1_IMAGE) {
    return aSurface;
  }

  RefPtr<DataSourceSurface> data = aSurface->GetDataSurface();

  DataSourceSurface::MappedSurface map;
  if (!data->Map(DataSourceSurface::MapType::READ, &map)) {
    return nullptr;
  }

  RefPtr<ID2D1Bitmap1> bitmap;
  HRESULT hr = mDC->CreateBitmap(D2DIntSize(data->GetSize()), map.mData, map.mStride,
                                 D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE, D2DPixelFormat(data->GetFormat())),
                                 byRef(bitmap));

  if (FAILED(hr)) {
    gfxCriticalError() << "[D2D1.1] CreateBitmap failure " << data->GetSize() << " Code: " << hexa(hr);
  }

  data->Unmap();

  if (!bitmap) {
    return data.forget();
  }

  return new SourceSurfaceD2D1(bitmap.get(), mDC, data->GetFormat(), data->GetSize());
}

void
DrawTargetD2D1::PushD2DLayer(ID2D1DeviceContext *aDC, ID2D1Geometry *aGeometry, const D2D1_MATRIX_3X2_F &aTransform)
{
  D2D1_LAYER_OPTIONS1 options = D2D1_LAYER_OPTIONS1_NONE;

  if (aDC->GetPixelFormat().alphaMode == D2D1_ALPHA_MODE_IGNORE) {
    options = D2D1_LAYER_OPTIONS1_IGNORE_ALPHA | D2D1_LAYER_OPTIONS1_INITIALIZE_FROM_BACKGROUND;
  }

  mDC->PushLayer(D2D1::LayerParameters1(D2D1::InfiniteRect(), aGeometry,
                                        D2D1_ANTIALIAS_MODE_PER_PRIMITIVE, aTransform,
                                        1.0, nullptr, options), nullptr);
}

}
}
