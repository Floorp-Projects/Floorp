/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

  mDC->EndDraw();
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
}

void
DrawTargetD2D1::DrawSurface(SourceSurface *aSurface,
                            const Rect &aDest,
                            const Rect &aSource,
                            const DrawSurfaceOptions &aSurfOptions,
                            const DrawOptions &aOptions)
{
  RefPtr<ID2D1Image> image = GetImageForSurface(aSurface, ExtendMode::CLAMP);

  if (!image) {
    gfxWarning() << *this << ": Unable to get D2D image for surface.";
    return;
  }

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
  transform.Translate(aDest.x, aDest.y);
  transform.Scale(xScale, yScale);

  mDC->CreateImageBrush(image, D2D1::ImageBrushProperties(samplingBounds),
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

  mDC->DrawImage(static_cast<FilterNodeD2D1*>(aNode)->OutputEffect(), D2DPoint(aDestPoint), D2DRect(aSourceRect));
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

  // Step 2, move the shadow effect into place.
  RefPtr<ID2D1Effect> affineTransformEffect;
  mDC->CreateEffect(CLSID_D2D12DAffineTransform, byRef(affineTransformEffect));
  affineTransformEffect->SetInputEffect(0, shadowEffect);
  D2D1_MATRIX_3X2_F matrix = D2D1::Matrix3x2F::Translation(aOffset.x, aOffset.y);
  affineTransformEffect->SetValue(D2D1_2DAFFINETRANSFORM_PROP_TRANSFORM_MATRIX, matrix);

  // Step 3, create an effect that combines shadow and bitmap in one image.
  RefPtr<ID2D1Effect> compositeEffect;
  mDC->CreateEffect(CLSID_D2D1Composite, byRef(compositeEffect));
  compositeEffect->SetInputEffect(0, affineTransformEffect);
  compositeEffect->SetInput(1, image);
  compositeEffect->SetValue(D2D1_COMPOSITE_PROP_MODE, D2DCompositionMode(aOperator));

  D2D1_POINT_2F surfPoint = D2DPoint(aDest);
  mDC->DrawImage(compositeEffect, &surfPoint, nullptr, D2D1_INTERPOLATION_MODE_LINEAR, D2DCompositionMode(aOperator));
}

void
DrawTargetD2D1::ClearRect(const Rect &aRect)
{
  MarkChanged();

  mDC->PushAxisAlignedClip(D2DRect(aRect), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
  mDC->Clear();
  mDC->PopAxisAlignedClip();
}

void
DrawTargetD2D1::MaskSurface(const Pattern &aSource,
                            SourceSurface *aMask,
                            Point aOffset,
                            const DrawOptions &aOptions)
{
  RefPtr<ID2D1Bitmap> bitmap;

  RefPtr<ID2D1Image> image = GetImageForSurface(aMask, ExtendMode::CLAMP);

  PrepareForDrawing(aOptions.mCompositionOp, aSource);

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

  mDC->SetTransform(D2D1::IdentityMatrix());
  mTransformDirty = true;

  Matrix mat;
  RefPtr<ID2D1Image> image = GetImageForSurface(aSurface, mat, ExtendMode::CLAMP);

  if (!mat.IsIdentity()) {
    gfxDebug() << *this << ": At this point complex partial uploads are not supported for CopySurface.";
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

  return dt;
}

TemporaryRef<PathBuilder>
DrawTargetD2D1::CreatePathBuilder(FillRule aFillRule) const
{
  RefPtr<ID2D1PathGeometry> path;
  HRESULT hr = factory()->CreatePathGeometry(byRef(path));

  if (FAILED(hr)) {
    gfxWarning() << *this << ": Failed to create Direct2D Path Geometry. Code: " << hr;
    return nullptr;
  }

  RefPtr<ID2D1GeometrySink> sink;
  hr = path->Open(byRef(sink));
  if (FAILED(hr)) {
    gfxWarning() << *this << ": Failed to access Direct2D Path Geometry. Code: " << hr;
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
    gfxWarning() << *this << ": Failed to create GradientStopCollection. Code: " << hr;
    return nullptr;
  }

  return new GradientStopsD2D(stopCollection);
}

TemporaryRef<FilterNode>
DrawTargetD2D1::CreateFilter(FilterType aType)
{
  return FilterNodeD2D1::Create(this, mDC, aType);
}

bool
DrawTargetD2D1::Init(const IntSize &aSize, SurfaceFormat aFormat)
{
  HRESULT hr;
  
  hr = Factory::GetD2D1Device()->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS, byRef(mDC));

  if (FAILED(hr)) {
    gfxWarning() << *this << ": Error " << hr << " failed to initialize new DeviceContext.";
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
    gfxWarning() << *this << ": Error " << hr << " failed to create new CommandList.";
    return false;
  }

  mDC->CreateBitmap(D2DIntSize(aSize), nullptr, 0, props, (ID2D1Bitmap1**)byRef(mTempBitmap));

  mDC->SetTarget(mBitmap);

  mDC->BeginDraw();

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

  if (patternSupported) {
    mDC->DrawImage(image, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR, D2DCompositionMode(aOp));
    return;
  }

  mDC->SetTransform(D2D1::IdentityMatrix());
  mTransformDirty = true;

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
DrawTargetD2D1::CreateBrushForPattern(const Pattern &aPattern, Float aAlpha)
{
  if (!IsPatternSupportedByD2D(aPattern)) {
    RefPtr<ID2D1SolidColorBrush> colBrush;
    mDC->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), byRef(colBrush));
    return colBrush;
  }

  if (aPattern.GetType() == PatternType::COLOR) {
    RefPtr<ID2D1SolidColorBrush> colBrush;
    Color color = static_cast<const ColorPattern*>(&aPattern)->mColor;
    mDC->CreateSolidColorBrush(D2D1::ColorF(color.r, color.g,
                                            color.b, color.a),
                               D2D1::BrushProperties(aAlpha),
                               byRef(colBrush));
    return colBrush;
  } else if (aPattern.GetType() == PatternType::LINEAR_GRADIENT) {
    RefPtr<ID2D1LinearGradientBrush> gradBrush;
    const LinearGradientPattern *pat =
      static_cast<const LinearGradientPattern*>(&aPattern);

    GradientStopsD2D *stops = static_cast<GradientStopsD2D*>(pat->mStops.get());

    if (!stops) {
      gfxDebug() << "No stops specified for gradient pattern.";
      return nullptr;
    }

    if (pat->mBegin == pat->mEnd) {
      RefPtr<ID2D1SolidColorBrush> colBrush;
      uint32_t stopCount = stops->mStopCollection->GetGradientStopCount();
      vector<D2D1_GRADIENT_STOP> d2dStops(stopCount);
      stops->mStopCollection->GetGradientStops(&d2dStops.front(), stopCount);
      mDC->CreateSolidColorBrush(d2dStops.back().color,
                                 D2D1::BrushProperties(aAlpha),
                                 byRef(colBrush));
      return colBrush;
    }

    mDC->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2DPoint(pat->mBegin),
                                                                       D2DPoint(pat->mEnd)),
                                   D2D1::BrushProperties(aAlpha, D2DMatrix(pat->mMatrix)),
                                   stops->mStopCollection,
                                   byRef(gradBrush));
    return gradBrush;
  } else if (aPattern.GetType() == PatternType::RADIAL_GRADIENT) {
    RefPtr<ID2D1RadialGradientBrush> gradBrush;
    const RadialGradientPattern *pat =
      static_cast<const RadialGradientPattern*>(&aPattern);

    GradientStopsD2D *stops = static_cast<GradientStopsD2D*>(pat->mStops.get());

    if (!stops) {
      gfxDebug() << "No stops specified for gradient pattern.";
      return nullptr;
    }

    // This will not be a complex radial gradient brush.
    mDC->CreateRadialGradientBrush(
      D2D1::RadialGradientBrushProperties(D2DPoint(pat->mCenter2),
                                          D2DPoint(pat->mCenter1 - pat->mCenter2),
                                          pat->mRadius2, pat->mRadius2),
      D2D1::BrushProperties(aAlpha, D2DMatrix(pat->mMatrix)),
      stops->mStopCollection,
      byRef(gradBrush));

    return gradBrush;
  } else if (aPattern.GetType() == PatternType::SURFACE) {
    const SurfacePattern *pat =
      static_cast<const SurfacePattern*>(&aPattern);

    if (!pat->mSurface) {
      gfxDebug() << "No source surface specified for surface pattern";
      return nullptr;
    }


    Matrix mat = pat->mMatrix;
    
    RefPtr<ID2D1ImageBrush> imageBrush;
    RefPtr<ID2D1Image> image = GetImageForSurface(pat->mSurface, mat, pat->mExtendMode);
    mDC->CreateImageBrush(image,
                          D2D1::ImageBrushProperties(D2D1::RectF(0, 0,
                                                                  Float(pat->mSurface->GetSize().width),
                                                                  Float(pat->mSurface->GetSize().height)),
                                  D2DExtend(pat->mExtendMode), D2DExtend(pat->mExtendMode),
                                  D2DInterpolationMode(pat->mFilter)),
                          D2D1::BrushProperties(aAlpha, D2DMatrix(mat)),
                          byRef(imageBrush));
    return imageBrush;
  }

  gfxWarning() << "Invalid pattern type detected.";
  return nullptr;
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

      image = CreatePartialBitmapForSurface(dataSurf, mTransform, mSize, aExtendMode,
                                            aSourceTransform, mDC); 

      return image;
    }
    break;
  }

  return image;
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
