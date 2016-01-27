/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DrawTargetRecording.h"
#include "PathRecording.h"
#include <stdio.h>

#include "Logging.h"
#include "Tools.h"
#include "Filters.h"
#include "mozilla/UniquePtr.h"
#include "RecordingTypes.h"

namespace mozilla {
namespace gfx {

struct RecordingSourceSurfaceUserData
{
  void *refPtr;
  RefPtr<DrawEventRecorderPrivate> recorder;
};

void RecordingSourceSurfaceUserDataFunc(void *aUserData)
{
  RecordingSourceSurfaceUserData *userData =
    static_cast<RecordingSourceSurfaceUserData*>(aUserData);

  userData->recorder->RemoveStoredObject(userData->refPtr);
  userData->recorder->RecordEvent(
    RecordedSourceSurfaceDestruction(userData->refPtr));

  delete userData;
}

static void
StoreSourceSurface(DrawEventRecorderPrivate *aRecorder, SourceSurface *aSurface,
                   DataSourceSurface *aDataSurf, const char *reason)
{
  if (!aDataSurf) {
    gfxWarning() << "Recording failed to record SourceSurface for " << reason;
    // Insert a bogus source surface.
    int32_t stride = aSurface->GetSize().width * BytesPerPixel(aSurface->GetFormat());
    UniquePtr<uint8_t[]> sourceData(new uint8_t[stride * aSurface->GetSize().height]());
    aRecorder->RecordEvent(
      RecordedSourceSurfaceCreation(aSurface, sourceData.get(), stride,
                                    aSurface->GetSize(), aSurface->GetFormat()));
  } else {
    DataSourceSurface::ScopedMap map(aDataSurf, DataSourceSurface::READ);
    aRecorder->RecordEvent(
      RecordedSourceSurfaceCreation(aSurface, map.GetData(), map.GetStride(),
                                    aDataSurf->GetSize(), aDataSurf->GetFormat()));
  }
}

static void
EnsureSurfaceStored(DrawEventRecorderPrivate *aRecorder, SourceSurface *aSurface,
                    const char *reason)
{
  if (aRecorder->HasStoredObject(aSurface)) {
    return;
  }

  RefPtr<DataSourceSurface> dataSurf = aSurface->GetDataSurface();
  StoreSourceSurface(aRecorder, aSurface, dataSurf, reason);
  aRecorder->AddStoredObject(aSurface);

  RecordingSourceSurfaceUserData *userData = new RecordingSourceSurfaceUserData;
  userData->refPtr = aSurface;
  userData->recorder = aRecorder;
  aSurface->AddUserData(reinterpret_cast<UserDataKey*>(aRecorder),
                        userData, &RecordingSourceSurfaceUserDataFunc);
  return;
}

class SourceSurfaceRecording : public SourceSurface
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(SourceSurfaceRecording)
  SourceSurfaceRecording(SourceSurface *aFinalSurface, DrawEventRecorderPrivate *aRecorder)
    : mFinalSurface(aFinalSurface), mRecorder(aRecorder)
  {
    mRecorder->AddStoredObject(this);
  }

  ~SourceSurfaceRecording()
  {
    mRecorder->RemoveStoredObject(this);
    mRecorder->RecordEvent(RecordedSourceSurfaceDestruction(this));
  }

  virtual SurfaceType GetType() const { return SurfaceType::RECORDING; }
  virtual IntSize GetSize() const { return mFinalSurface->GetSize(); }
  virtual SurfaceFormat GetFormat() const { return mFinalSurface->GetFormat(); }
  virtual already_AddRefed<DataSourceSurface> GetDataSurface() { return mFinalSurface->GetDataSurface(); }

  RefPtr<SourceSurface> mFinalSurface;
  RefPtr<DrawEventRecorderPrivate> mRecorder;
};

class GradientStopsRecording : public GradientStops
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(GradientStopsRecording)
  GradientStopsRecording(GradientStops *aFinalGradientStops, DrawEventRecorderPrivate *aRecorder)
    : mFinalGradientStops(aFinalGradientStops), mRecorder(aRecorder)
  {
    mRecorder->AddStoredObject(this);
  }

  ~GradientStopsRecording()
  {
    mRecorder->RemoveStoredObject(this);
    mRecorder->RecordEvent(RecordedGradientStopsDestruction(this));
  }

  virtual BackendType GetBackendType() const { return BackendType::RECORDING; }

  RefPtr<GradientStops> mFinalGradientStops;
  RefPtr<DrawEventRecorderPrivate> mRecorder;
};

static SourceSurface *
GetSourceSurface(SourceSurface *aSurface)
{
  if (aSurface->GetType() != SurfaceType::RECORDING) {
    return aSurface;
  }

  return static_cast<SourceSurfaceRecording*>(aSurface)->mFinalSurface;
}

static GradientStops *
GetGradientStops(GradientStops *aStops)
{
  if (aStops->GetBackendType() != BackendType::RECORDING) {
    return aStops;
  }

  return static_cast<GradientStopsRecording*>(aStops)->mFinalGradientStops;
}

class FilterNodeRecording : public FilterNode
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeRecording, override)
  using FilterNode::SetAttribute;

  FilterNodeRecording(FilterNode *aFinalFilterNode, DrawEventRecorderPrivate *aRecorder)
    : mFinalFilterNode(aFinalFilterNode), mRecorder(aRecorder)
  {
    mRecorder->AddStoredObject(this);
  }

  ~FilterNodeRecording()
  {
    mRecorder->RemoveStoredObject(this);
    mRecorder->RecordEvent(RecordedFilterNodeDestruction(this));
  }

  static FilterNode*
  GetFilterNode(FilterNode* aNode)
  {
    if (aNode->GetBackendType() != FILTER_BACKEND_RECORDING) {
      gfxWarning() << "Non recording filter node used with recording DrawTarget!";
      return aNode;
    }

    return static_cast<FilterNodeRecording*>(aNode)->mFinalFilterNode;
  }

  virtual void SetInput(uint32_t aIndex, SourceSurface *aSurface) override
  {
    EnsureSurfaceStored(mRecorder, aSurface,  "SetInput");

    mRecorder->RecordEvent(RecordedFilterNodeSetInput(this, aIndex, aSurface));
    mFinalFilterNode->SetInput(aIndex, GetSourceSurface(aSurface));
  }
  virtual void SetInput(uint32_t aIndex, FilterNode *aFilter) override
  {
    MOZ_ASSERT(mRecorder->HasStoredObject(aFilter));

    mRecorder->RecordEvent(RecordedFilterNodeSetInput(this, aIndex, aFilter));
    mFinalFilterNode->SetInput(aIndex, GetFilterNode(aFilter));
  }


#define FORWARD_SET_ATTRIBUTE(type, argtype) \
  virtual void SetAttribute(uint32_t aIndex, type aValue) override { \
    mRecorder->RecordEvent(RecordedFilterNodeSetAttribute(this, aIndex, aValue, RecordedFilterNodeSetAttribute::ARGTYPE_##argtype)); \
    mFinalFilterNode->SetAttribute(aIndex, aValue); \
  }

  FORWARD_SET_ATTRIBUTE(bool, BOOL);
  FORWARD_SET_ATTRIBUTE(uint32_t, UINT32);
  FORWARD_SET_ATTRIBUTE(Float, FLOAT);
  FORWARD_SET_ATTRIBUTE(const Size&, SIZE);
  FORWARD_SET_ATTRIBUTE(const IntSize&, INTSIZE);
  FORWARD_SET_ATTRIBUTE(const IntPoint&, INTPOINT);
  FORWARD_SET_ATTRIBUTE(const Rect&, RECT);
  FORWARD_SET_ATTRIBUTE(const IntRect&, INTRECT);
  FORWARD_SET_ATTRIBUTE(const Point&, POINT);
  FORWARD_SET_ATTRIBUTE(const Matrix&, MATRIX);
  FORWARD_SET_ATTRIBUTE(const Matrix5x4&, MATRIX5X4);
  FORWARD_SET_ATTRIBUTE(const Point3D&, POINT3D);
  FORWARD_SET_ATTRIBUTE(const Color&, COLOR);

#undef FORWARD_SET_ATTRIBUTE

  virtual void SetAttribute(uint32_t aIndex, const Float* aFloat, uint32_t aSize) override {
    mRecorder->RecordEvent(RecordedFilterNodeSetAttribute(this, aIndex, aFloat, aSize));
    mFinalFilterNode->SetAttribute(aIndex, aFloat, aSize);
  }

  virtual FilterBackend GetBackendType() override { return FILTER_BACKEND_RECORDING; }

  RefPtr<FilterNode> mFinalFilterNode;
  RefPtr<DrawEventRecorderPrivate> mRecorder;
};

struct AdjustedPattern
{
  explicit AdjustedPattern(const Pattern &aPattern)
    : mPattern(nullptr)
  {
    mOrigPattern = const_cast<Pattern*>(&aPattern);
  }

  ~AdjustedPattern() {
    if (mPattern) {
      mPattern->~Pattern();
    }
  }

  operator Pattern*()
  {
    switch(mOrigPattern->GetType()) {
    case PatternType::COLOR:
      return mOrigPattern;
    case PatternType::SURFACE:
      {
        SurfacePattern *surfPat = static_cast<SurfacePattern*>(mOrigPattern);
        mPattern =
          new (mSurfPat) SurfacePattern(GetSourceSurface(surfPat->mSurface),
                                        surfPat->mExtendMode, surfPat->mMatrix,
                                        surfPat->mFilter);
        return mPattern;
      }
    case PatternType::LINEAR_GRADIENT:
      {
        LinearGradientPattern *linGradPat = static_cast<LinearGradientPattern*>(mOrigPattern);
        mPattern =
          new (mLinGradPat) LinearGradientPattern(linGradPat->mBegin, linGradPat->mEnd,
                                                  GetGradientStops(linGradPat->mStops),
                                                  linGradPat->mMatrix);
        return mPattern;
      }
    case PatternType::RADIAL_GRADIENT:
      {
        RadialGradientPattern *radGradPat = static_cast<RadialGradientPattern*>(mOrigPattern);
        mPattern =
          new (mRadGradPat) RadialGradientPattern(radGradPat->mCenter1, radGradPat->mCenter2,
                                                  radGradPat->mRadius1, radGradPat->mRadius2,
                                                  GetGradientStops(radGradPat->mStops),
                                                  radGradPat->mMatrix);
        return mPattern;
      }
    default:
      return new (mColPat) ColorPattern(Color());
    }

    return mPattern;
  }

  union {
    char mColPat[sizeof(ColorPattern)];
    char mLinGradPat[sizeof(LinearGradientPattern)];
    char mRadGradPat[sizeof(RadialGradientPattern)];
    char mSurfPat[sizeof(SurfacePattern)];
  };

  Pattern *mOrigPattern;
  Pattern *mPattern;
};

DrawTargetRecording::DrawTargetRecording(DrawEventRecorder *aRecorder, DrawTarget *aDT, bool aHasData)
  : mRecorder(static_cast<DrawEventRecorderPrivate*>(aRecorder))
  , mFinalDT(aDT)
{
  RefPtr<SourceSurface> snapshot = aHasData ? mFinalDT->Snapshot() : nullptr;
  mRecorder->RecordEvent(RecordedDrawTargetCreation(this,
                                                    mFinalDT->GetBackendType(),
                                                    mFinalDT->GetSize(),
                                                    mFinalDT->GetFormat(),
                                                    aHasData, snapshot));
  mFormat = mFinalDT->GetFormat();
}

DrawTargetRecording::DrawTargetRecording(const DrawTargetRecording *aDT,
                                         const IntSize &aSize,
                                         SurfaceFormat aFormat)
  : mRecorder(aDT->mRecorder)
  , mFinalDT(aDT->mFinalDT->CreateSimilarDrawTarget(aSize, aFormat))
{
  mRecorder->RecordEvent(RecordedCreateSimilarDrawTarget(this, aSize, aFormat));
  mFormat = mFinalDT->GetFormat();
}

DrawTargetRecording::~DrawTargetRecording()
{
  mRecorder->RecordEvent(RecordedDrawTargetDestruction(this));
}

void
DrawTargetRecording::FillRect(const Rect &aRect,
                              const Pattern &aPattern,
                              const DrawOptions &aOptions)
{
  EnsurePatternDependenciesStored(aPattern);

  mRecorder->RecordEvent(RecordedFillRect(this, aRect, aPattern, aOptions));
  mFinalDT->FillRect(aRect, *AdjustedPattern(aPattern), aOptions);
}

void
DrawTargetRecording::StrokeRect(const Rect &aRect,
                                const Pattern &aPattern,
                                const StrokeOptions &aStrokeOptions,
                                const DrawOptions &aOptions)
{
  EnsurePatternDependenciesStored(aPattern);

  mRecorder->RecordEvent(RecordedStrokeRect(this, aRect, aPattern, aStrokeOptions, aOptions));
  mFinalDT->StrokeRect(aRect, *AdjustedPattern(aPattern), aStrokeOptions, aOptions);
}

void
DrawTargetRecording::StrokeLine(const Point &aBegin,
                                const Point &aEnd,
                                const Pattern &aPattern,
                                const StrokeOptions &aStrokeOptions,
                                const DrawOptions &aOptions)
{
  EnsurePatternDependenciesStored(aPattern);

  mRecorder->RecordEvent(RecordedStrokeLine(this, aBegin, aEnd, aPattern, aStrokeOptions, aOptions));
  mFinalDT->StrokeLine(aBegin, aEnd, *AdjustedPattern(aPattern), aStrokeOptions, aOptions);
}

void
DrawTargetRecording::Fill(const Path *aPath,
                          const Pattern &aPattern,
                          const DrawOptions &aOptions)
{
  RefPtr<PathRecording> pathRecording = EnsurePathStored(aPath);
  EnsurePatternDependenciesStored(aPattern);

  mRecorder->RecordEvent(RecordedFill(this, pathRecording, aPattern, aOptions));
  mFinalDT->Fill(pathRecording->mPath, *AdjustedPattern(aPattern), aOptions);
}

struct RecordingFontUserData
{
  void *refPtr;
  RefPtr<DrawEventRecorderPrivate> recorder;
};

void RecordingFontUserDataDestroyFunc(void *aUserData)
{
  RecordingFontUserData *userData =
    static_cast<RecordingFontUserData*>(aUserData);

  // TODO support font in b2g recordings
#ifndef MOZ_WIDGET_GONK
  userData->recorder->RecordEvent(RecordedScaledFontDestruction(userData->refPtr));
#endif

  delete userData;
}

void
DrawTargetRecording::FillGlyphs(ScaledFont *aFont,
                                const GlyphBuffer &aBuffer,
                                const Pattern &aPattern,
                                const DrawOptions &aOptions,
                                const GlyphRenderingOptions *aRenderingOptions)
{
  EnsurePatternDependenciesStored(aPattern);

  if (!aFont->GetUserData(reinterpret_cast<UserDataKey*>(mRecorder.get()))) {
  // TODO support font in b2g recordings
#ifndef MOZ_WIDGET_GONK
    RecordedFontData fontData(aFont);
    RecordedFontDetails fontDetails;
    if (fontData.GetFontDetails(fontDetails)) {
      if (!mRecorder->HasStoredFontData(fontDetails.fontDataKey)) {
        mRecorder->RecordEvent(fontData);
        mRecorder->AddStoredFontData(fontDetails.fontDataKey);
      }
      mRecorder->RecordEvent(RecordedScaledFontCreation(aFont, fontDetails));
    }
#endif
    RecordingFontUserData *userData = new RecordingFontUserData;
    userData->refPtr = aFont;
    userData->recorder = mRecorder;
    aFont->AddUserData(reinterpret_cast<UserDataKey*>(mRecorder.get()), userData, 
                       &RecordingFontUserDataDestroyFunc);
  }

  // TODO support font in b2g recordings
#ifndef MOZ_WIDGET_GONK
  mRecorder->RecordEvent(RecordedFillGlyphs(this, aFont, aPattern, aOptions, aBuffer.mGlyphs, aBuffer.mNumGlyphs));
#endif
  mFinalDT->FillGlyphs(aFont, aBuffer, aPattern, aOptions, aRenderingOptions);
}

void
DrawTargetRecording::Mask(const Pattern &aSource,
                          const Pattern &aMask,
                          const DrawOptions &aOptions)
{
  EnsurePatternDependenciesStored(aSource);
  EnsurePatternDependenciesStored(aMask);

  mRecorder->RecordEvent(RecordedMask(this, aSource, aMask, aOptions));
  mFinalDT->Mask(*AdjustedPattern(aSource), *AdjustedPattern(aMask), aOptions);
}

void
DrawTargetRecording::MaskSurface(const Pattern &aSource,
                                 SourceSurface *aMask,
                                 Point aOffset,
                                 const DrawOptions &aOptions)
{
  EnsurePatternDependenciesStored(aSource);
  EnsureSurfaceStored(mRecorder, aMask, "MaskSurface");

  mRecorder->RecordEvent(RecordedMaskSurface(this, aSource, aMask, aOffset, aOptions));
  mFinalDT->MaskSurface(*AdjustedPattern(aSource), GetSourceSurface(aMask), aOffset, aOptions);
}

void
DrawTargetRecording::Stroke(const Path *aPath,
                            const Pattern &aPattern,
                            const StrokeOptions &aStrokeOptions,
                            const DrawOptions &aOptions)
{
  RefPtr<PathRecording> pathRecording = EnsurePathStored(aPath);
  EnsurePatternDependenciesStored(aPattern);

  mRecorder->RecordEvent(RecordedStroke(this, pathRecording, aPattern, aStrokeOptions, aOptions));
  mFinalDT->Stroke(pathRecording->mPath, *AdjustedPattern(aPattern), aStrokeOptions, aOptions);
}

already_AddRefed<SourceSurface>
DrawTargetRecording::Snapshot()
{
  RefPtr<SourceSurface> surf = mFinalDT->Snapshot();

  RefPtr<SourceSurface> retSurf = new SourceSurfaceRecording(surf, mRecorder);

  mRecorder->RecordEvent(RecordedSnapshot(retSurf, this));

  return retSurf.forget();
}

void
DrawTargetRecording::DrawSurface(SourceSurface *aSurface,
                                 const Rect &aDest,
                                 const Rect &aSource,
                                 const DrawSurfaceOptions &aSurfOptions,
                                 const DrawOptions &aOptions)
{
  EnsureSurfaceStored(mRecorder, aSurface, "DrawSurface");

  mRecorder->RecordEvent(RecordedDrawSurface(this, aSurface, aDest, aSource, aSurfOptions, aOptions));
  mFinalDT->DrawSurface(GetSourceSurface(aSurface), aDest, aSource, aSurfOptions, aOptions);
}

void
DrawTargetRecording::DrawSurfaceWithShadow(SourceSurface *aSurface,
                                           const Point &aDest,
                                           const Color &aColor,
                                           const Point &aOffset,
                                           Float aSigma,
                                           CompositionOp aOp)
{
  EnsureSurfaceStored(mRecorder, aSurface, "DrawSurfaceWithShadow");

  mRecorder->RecordEvent(RecordedDrawSurfaceWithShadow(this, aSurface, aDest, aColor, aOffset, aSigma, aOp));
  mFinalDT->DrawSurfaceWithShadow(GetSourceSurface(aSurface), aDest, aColor, aOffset, aSigma, aOp);
}

void
DrawTargetRecording::DrawFilter(FilterNode *aNode,
                                const Rect &aSourceRect,
                                const Point &aDestPoint,
                                const DrawOptions &aOptions)
{
  MOZ_ASSERT(mRecorder->HasStoredObject(aNode));

  mRecorder->RecordEvent(RecordedDrawFilter(this, aNode, aSourceRect, aDestPoint, aOptions));
  mFinalDT->DrawFilter(FilterNodeRecording::GetFilterNode(aNode), aSourceRect, aDestPoint, aOptions);
}

already_AddRefed<FilterNode>
DrawTargetRecording::CreateFilter(FilterType aType)
{
  RefPtr<FilterNode> node = mFinalDT->CreateFilter(aType);

  RefPtr<FilterNode> retNode = new FilterNodeRecording(node, mRecorder);

  mRecorder->RecordEvent(RecordedFilterNodeCreation(retNode, aType));

  return retNode.forget();
}

void
DrawTargetRecording::ClearRect(const Rect &aRect)
{
  mRecorder->RecordEvent(RecordedClearRect(this, aRect));
  mFinalDT->ClearRect(aRect);
}

void
DrawTargetRecording::CopySurface(SourceSurface *aSurface,
                                 const IntRect &aSourceRect,
                                 const IntPoint &aDestination)
{
  EnsureSurfaceStored(mRecorder, aSurface, "CopySurface");

  mRecorder->RecordEvent(RecordedCopySurface(this, aSurface, aSourceRect, aDestination));
  mFinalDT->CopySurface(GetSourceSurface(aSurface), aSourceRect, aDestination);
}

void
DrawTargetRecording::PushClip(const Path *aPath)
{
  RefPtr<PathRecording> pathRecording = EnsurePathStored(aPath);

  mRecorder->RecordEvent(RecordedPushClip(this, pathRecording));
  mFinalDT->PushClip(pathRecording->mPath);
}

void
DrawTargetRecording::PushClipRect(const Rect &aRect)
{
  mRecorder->RecordEvent(RecordedPushClipRect(this, aRect));
  mFinalDT->PushClipRect(aRect);
}

void
DrawTargetRecording::PopClip()
{
  mRecorder->RecordEvent(RecordedPopClip(this));
  mFinalDT->PopClip();
}

void
DrawTargetRecording::PushLayer(bool aOpaque, Float aOpacity,
                               SourceSurface* aMask,
                               const Matrix& aMaskTransform,
                               const IntRect& aBounds, bool aCopyBackground)
{
  if (aMask) {
    EnsureSurfaceStored(mRecorder, aMask, "PushLayer");
  }

  mRecorder->RecordEvent(RecordedPushLayer(this, aOpacity, aOpacity, aMask,
                                           aMaskTransform, aBounds,
                                           aCopyBackground));
  mFinalDT->PushLayer(aOpacity, aOpacity, aMask, aMaskTransform, aBounds,
                      aCopyBackground);
}

void
DrawTargetRecording::PopLayer()
{
  mRecorder->RecordEvent(RecordedPopLayer(this));
  mFinalDT->PopLayer();
}

already_AddRefed<SourceSurface>
DrawTargetRecording::CreateSourceSurfaceFromData(unsigned char *aData,
                                                 const IntSize &aSize,
                                                 int32_t aStride,
                                                 SurfaceFormat aFormat) const
{
  RefPtr<SourceSurface> surf = mFinalDT->CreateSourceSurfaceFromData(aData, aSize, aStride, aFormat);

  RefPtr<SourceSurface> retSurf = new SourceSurfaceRecording(surf, mRecorder);

  mRecorder->RecordEvent(RecordedSourceSurfaceCreation(retSurf, aData, aStride, aSize, aFormat));

  return retSurf.forget();
}

already_AddRefed<SourceSurface>
DrawTargetRecording::OptimizeSourceSurface(SourceSurface *aSurface) const
{
  RefPtr<SourceSurface> surf = mFinalDT->OptimizeSourceSurface(aSurface);

  RefPtr<SourceSurface> retSurf = new SourceSurfaceRecording(surf, mRecorder);

  RefPtr<DataSourceSurface> dataSurf = surf->GetDataSurface();

  if (!dataSurf) {
    // Let's try get it off the original surface.
    dataSurf = aSurface->GetDataSurface();
  }

  StoreSourceSurface(mRecorder, retSurf, dataSurf, "OptimizeSourceSurface");

  return retSurf.forget();
}

already_AddRefed<SourceSurface>
DrawTargetRecording::CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const
{
  RefPtr<SourceSurface> surf = mFinalDT->CreateSourceSurfaceFromNativeSurface(aSurface);

  RefPtr<SourceSurface> retSurf = new SourceSurfaceRecording(surf, mRecorder);

  RefPtr<DataSourceSurface> dataSurf = surf->GetDataSurface();
  StoreSourceSurface(mRecorder, retSurf, dataSurf, "CreateSourceSurfaceFromNativeSurface");

  return retSurf.forget();
}

already_AddRefed<DrawTarget>
DrawTargetRecording::CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const
{
  return MakeAndAddRef<DrawTargetRecording>(this, aSize, aFormat);
}

already_AddRefed<PathBuilder>
DrawTargetRecording::CreatePathBuilder(FillRule aFillRule) const
{
  RefPtr<PathBuilder> builder = mFinalDT->CreatePathBuilder(aFillRule);
  return MakeAndAddRef<PathBuilderRecording>(builder, aFillRule);
}

already_AddRefed<GradientStops>
DrawTargetRecording::CreateGradientStops(GradientStop *aStops,
                                         uint32_t aNumStops,
                                         ExtendMode aExtendMode) const
{
  RefPtr<GradientStops> stops = mFinalDT->CreateGradientStops(aStops, aNumStops, aExtendMode);

  RefPtr<GradientStops> retStops = new GradientStopsRecording(stops, mRecorder);

  mRecorder->RecordEvent(RecordedGradientStopsCreation(retStops, aStops, aNumStops, aExtendMode));

  return retStops.forget();
}

void
DrawTargetRecording::SetTransform(const Matrix &aTransform)
{
  mRecorder->RecordEvent(RecordedSetTransform(this, aTransform));
  DrawTarget::SetTransform(aTransform);
  mFinalDT->SetTransform(aTransform);
}

already_AddRefed<PathRecording>
DrawTargetRecording::EnsurePathStored(const Path *aPath)
{
  RefPtr<PathRecording> pathRecording;
  if (aPath->GetBackendType() == BackendType::RECORDING) {
    pathRecording = const_cast<PathRecording*>(static_cast<const PathRecording*>(aPath));
    if (mRecorder->HasStoredObject(aPath)) {
      return pathRecording.forget();
    }
  } else {
    MOZ_ASSERT(!mRecorder->HasStoredObject(aPath));
    FillRule fillRule = aPath->GetFillRule();
    RefPtr<PathBuilder> builder = mFinalDT->CreatePathBuilder(fillRule);
    RefPtr<PathBuilderRecording> builderRecording =
      new PathBuilderRecording(builder, fillRule);
    aPath->StreamToSink(builderRecording);
    pathRecording = builderRecording->Finish().downcast<PathRecording>();
  }

  mRecorder->RecordEvent(RecordedPathCreation(pathRecording));
  mRecorder->AddStoredObject(pathRecording);
  pathRecording->mStoredRecorders.push_back(mRecorder);

  return pathRecording.forget();
}

void
DrawTargetRecording::EnsurePatternDependenciesStored(const Pattern &aPattern)
{
  switch (aPattern.GetType()) {
  case PatternType::COLOR:
    // No dependencies here.
    return;
  case PatternType::LINEAR_GRADIENT:
    {
      MOZ_ASSERT(mRecorder->HasStoredObject(static_cast<const LinearGradientPattern*>(&aPattern)->mStops));
      return;
    }
  case PatternType::RADIAL_GRADIENT:
    {
      MOZ_ASSERT(mRecorder->HasStoredObject(static_cast<const RadialGradientPattern*>(&aPattern)->mStops));
      return;
    }
  case PatternType::SURFACE:
    {
      const SurfacePattern *pat = static_cast<const SurfacePattern*>(&aPattern);
      EnsureSurfaceStored(mRecorder, pat->mSurface, "EnsurePatternDependenciesStored");
      return;
    }
  }
}

} // namespace gfx
} // namespace mozilla
