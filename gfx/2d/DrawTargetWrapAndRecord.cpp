/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DrawTargetWrapAndRecord.h"
#include "PathRecording.h"
#include <stdio.h>

#include "Logging.h"
#include "Tools.h"
#include "Filters.h"
#include "mozilla/UniquePtr.h"
#include "RecordingTypes.h"
#include "RecordedEventImpl.h"

namespace mozilla {
namespace gfx {

struct WrapAndRecordSourceSurfaceUserData {
  void* refPtr;
  RefPtr<DrawEventRecorderPrivate> recorder;
};

static void WrapAndRecordSourceSurfaceUserDataFunc(void* aUserData) {
  WrapAndRecordSourceSurfaceUserData* userData =
      static_cast<WrapAndRecordSourceSurfaceUserData*>(aUserData);

  userData->recorder->RemoveSourceSurface((SourceSurface*)userData->refPtr);
  userData->recorder->RemoveStoredObject(userData->refPtr);
  userData->recorder->RecordEvent(
      RecordedSourceSurfaceDestruction(ReferencePtr(userData->refPtr)));

  delete userData;
}

static void StoreSourceSurface(DrawEventRecorderPrivate* aRecorder,
                               SourceSurface* aSurface,
                               DataSourceSurface* aDataSurf,
                               const char* reason) {
  if (!aDataSurf) {
    gfxWarning() << "Recording failed to record SourceSurface for " << reason;
    // Insert a bogus source surface.
    int32_t stride =
        aSurface->GetSize().width * BytesPerPixel(aSurface->GetFormat());
    UniquePtr<uint8_t[]> sourceData(
        new uint8_t[stride * aSurface->GetSize().height]());
    aRecorder->RecordEvent(RecordedSourceSurfaceCreation(
        aSurface, sourceData.get(), stride, aSurface->GetSize(),
        aSurface->GetFormat()));
  } else {
    DataSourceSurface::ScopedMap map(aDataSurf, DataSourceSurface::READ);
    aRecorder->RecordEvent(RecordedSourceSurfaceCreation(
        aSurface, map.GetData(), map.GetStride(), aDataSurf->GetSize(),
        aDataSurf->GetFormat()));
  }
}

static void EnsureSurfaceStored(DrawEventRecorderPrivate* aRecorder,
                                SourceSurface* aSurface, const char* reason) {
  if (aRecorder->HasStoredObject(aSurface)) {
    return;
  }

  RefPtr<DataSourceSurface> dataSurf = aSurface->GetDataSurface();
  StoreSourceSurface(aRecorder, aSurface, dataSurf, reason);
  aRecorder->AddStoredObject(aSurface);
  aRecorder->AddSourceSurface(aSurface);

  WrapAndRecordSourceSurfaceUserData* userData =
      new WrapAndRecordSourceSurfaceUserData;
  userData->refPtr = aSurface;
  userData->recorder = aRecorder;
  aSurface->AddUserData(reinterpret_cast<UserDataKey*>(aRecorder), userData,
                        &WrapAndRecordSourceSurfaceUserDataFunc);
}

class SourceSurfaceWrapAndRecord : public SourceSurface {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(SourceSurfaceWrapAndRecord, override)

  SourceSurfaceWrapAndRecord(SourceSurface* aFinalSurface,
                             DrawEventRecorderPrivate* aRecorder)
      : mFinalSurface(aFinalSurface), mRecorder(aRecorder) {
    mRecorder->AddStoredObject(this);
  }

  ~SourceSurfaceWrapAndRecord() {
    mRecorder->RemoveStoredObject(this);
    mRecorder->RecordEvent(
        RecordedSourceSurfaceDestruction(ReferencePtr(this)));
  }

  SurfaceType GetType() const override { return SurfaceType::WRAP_AND_RECORD; }
  IntSize GetSize() const override { return mFinalSurface->GetSize(); }
  SurfaceFormat GetFormat() const override {
    return mFinalSurface->GetFormat();
  }
  already_AddRefed<DataSourceSurface> GetDataSurface() override {
    return mFinalSurface->GetDataSurface();
  }

  RefPtr<SourceSurface> mFinalSurface;
  RefPtr<DrawEventRecorderPrivate> mRecorder;
};

class GradientStopsWrapAndRecord : public GradientStops {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(GradientStopsWrapAndRecord, override)

  GradientStopsWrapAndRecord(GradientStops* aFinalGradientStops,
                             DrawEventRecorderPrivate* aRecorder)
      : mFinalGradientStops(aFinalGradientStops), mRecorder(aRecorder) {
    mRecorder->AddStoredObject(this);
  }

  ~GradientStopsWrapAndRecord() {
    mRecorder->RemoveStoredObject(this);
    mRecorder->RecordEvent(
        RecordedGradientStopsDestruction(ReferencePtr(this)));
  }

  BackendType GetBackendType() const override { return BackendType::RECORDING; }

  RefPtr<GradientStops> mFinalGradientStops;
  RefPtr<DrawEventRecorderPrivate> mRecorder;
};

static SourceSurface* GetSourceSurface(SourceSurface* aSurface) {
  if (aSurface->GetType() != SurfaceType::WRAP_AND_RECORD) {
    return aSurface;
  }

  return static_cast<SourceSurfaceWrapAndRecord*>(aSurface)->mFinalSurface;
}

static GradientStops* GetGradientStops(GradientStops* aStops) {
  if (aStops->GetBackendType() != BackendType::RECORDING) {
    return aStops;
  }

  return static_cast<GradientStopsWrapAndRecord*>(aStops)->mFinalGradientStops;
}

class FilterNodeWrapAndRecord : public FilterNode {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeWrapAndRecord, override)
  using FilterNode::SetAttribute;

  FilterNodeWrapAndRecord(FilterNode* aFinalFilterNode,
                          DrawEventRecorderPrivate* aRecorder)
      : mFinalFilterNode(aFinalFilterNode), mRecorder(aRecorder) {
    mRecorder->AddStoredObject(this);
  }

  ~FilterNodeWrapAndRecord() {
    mRecorder->RemoveStoredObject(this);
    mRecorder->RecordEvent(RecordedFilterNodeDestruction(ReferencePtr(this)));
  }

  static FilterNode* GetFilterNode(FilterNode* aNode) {
    if (aNode->GetBackendType() != FILTER_BACKEND_RECORDING) {
      gfxWarning()
          << "Non recording filter node used with recording DrawTarget!";
      return aNode;
    }

    return static_cast<FilterNodeWrapAndRecord*>(aNode)->mFinalFilterNode;
  }

  void SetInput(uint32_t aIndex, SourceSurface* aSurface) override {
    EnsureSurfaceStored(mRecorder, aSurface, "SetInput");

    mRecorder->RecordEvent(RecordedFilterNodeSetInput(this, aIndex, aSurface));
    mFinalFilterNode->SetInput(aIndex, GetSourceSurface(aSurface));
  }
  void SetInput(uint32_t aIndex, FilterNode* aFilter) override {
    MOZ_ASSERT(mRecorder->HasStoredObject(aFilter));

    mRecorder->RecordEvent(RecordedFilterNodeSetInput(this, aIndex, aFilter));
    mFinalFilterNode->SetInput(aIndex, GetFilterNode(aFilter));
  }

#define FORWARD_SET_ATTRIBUTE(type, argtype)                 \
  void SetAttribute(uint32_t aIndex, type aValue) override { \
    mRecorder->RecordEvent(RecordedFilterNodeSetAttribute(   \
        this, aIndex, aValue,                                \
        RecordedFilterNodeSetAttribute::ARGTYPE_##argtype)); \
    mFinalFilterNode->SetAttribute(aIndex, aValue);          \
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
  FORWARD_SET_ATTRIBUTE(const DeviceColor&, COLOR);

#undef FORWARD_SET_ATTRIBUTE

  virtual void SetAttribute(uint32_t aIndex, const Float* aFloat,
                            uint32_t aSize) override {
    mRecorder->RecordEvent(
        RecordedFilterNodeSetAttribute(this, aIndex, aFloat, aSize));
    mFinalFilterNode->SetAttribute(aIndex, aFloat, aSize);
  }

  FilterBackend GetBackendType() override { return FILTER_BACKEND_RECORDING; }

  RefPtr<FilterNode> mFinalFilterNode;
  RefPtr<DrawEventRecorderPrivate> mRecorder;
};

struct AdjustedPattern final {
  explicit AdjustedPattern(const Pattern& aPattern) : mPattern(nullptr) {
    mOrigPattern = const_cast<Pattern*>(&aPattern);
  }

  ~AdjustedPattern() {
    if (mPattern) {
      mPattern->~Pattern();
    }
  }

  operator Pattern*() {
    switch (mOrigPattern->GetType()) {
      case PatternType::COLOR:
        return mOrigPattern;
      case PatternType::SURFACE: {
        SurfacePattern* surfPat = static_cast<SurfacePattern*>(mOrigPattern);
        mPattern = new (mSurfPat) SurfacePattern(
            GetSourceSurface(surfPat->mSurface), surfPat->mExtendMode,
            surfPat->mMatrix, surfPat->mSamplingFilter, surfPat->mSamplingRect);
        return mPattern;
      }
      case PatternType::LINEAR_GRADIENT: {
        LinearGradientPattern* linGradPat =
            static_cast<LinearGradientPattern*>(mOrigPattern);
        mPattern = new (mLinGradPat) LinearGradientPattern(
            linGradPat->mBegin, linGradPat->mEnd,
            GetGradientStops(linGradPat->mStops), linGradPat->mMatrix);
        return mPattern;
      }
      case PatternType::RADIAL_GRADIENT: {
        RadialGradientPattern* radGradPat =
            static_cast<RadialGradientPattern*>(mOrigPattern);
        mPattern = new (mRadGradPat) RadialGradientPattern(
            radGradPat->mCenter1, radGradPat->mCenter2, radGradPat->mRadius1,
            radGradPat->mRadius2, GetGradientStops(radGradPat->mStops),
            radGradPat->mMatrix);
        return mPattern;
      }
      case PatternType::CONIC_GRADIENT: {
        ConicGradientPattern* conGradPat =
            static_cast<ConicGradientPattern*>(mOrigPattern);
        mPattern = new (mConGradPat) ConicGradientPattern(
            conGradPat->mCenter, conGradPat->mAngle, conGradPat->mStartOffset,
            conGradPat->mEndOffset, GetGradientStops(conGradPat->mStops),
            conGradPat->mMatrix);
        return mPattern;
      }
      default:
        return new (mColPat) ColorPattern(DeviceColor());
    }

    return mPattern;
  }

  union {
    char mColPat[sizeof(ColorPattern)];
    char mLinGradPat[sizeof(LinearGradientPattern)];
    char mRadGradPat[sizeof(RadialGradientPattern)];
    char mConGradPat[sizeof(ConicGradientPattern)];
    char mSurfPat[sizeof(SurfacePattern)];
  };

  Pattern* mOrigPattern;
  Pattern* mPattern;
};

DrawTargetWrapAndRecord::DrawTargetWrapAndRecord(DrawEventRecorder* aRecorder,
                                                 DrawTarget* aDT, bool aHasData)
    : mRecorder(static_cast<DrawEventRecorderPrivate*>(aRecorder)),
      mFinalDT(aDT) {
  RefPtr<SourceSurface> snapshot = aHasData ? mFinalDT->Snapshot() : nullptr;
  mRecorder->RecordEvent(RecordedDrawTargetCreation(
      this, mFinalDT->GetBackendType(), mFinalDT->GetRect(),
      mFinalDT->GetFormat(), aHasData, snapshot));
  mFormat = mFinalDT->GetFormat();
}

DrawTargetWrapAndRecord::DrawTargetWrapAndRecord(
    const DrawTargetWrapAndRecord* aDT, DrawTarget* aSimilarDT)
    : mRecorder(aDT->mRecorder), mFinalDT(aSimilarDT) {
  mRecorder->RecordEvent(RecordedCreateSimilarDrawTarget(
      this, mFinalDT->GetSize(), mFinalDT->GetFormat()));
  mFormat = mFinalDT->GetFormat();
}

DrawTargetWrapAndRecord::~DrawTargetWrapAndRecord() {
  mRecorder->RecordEvent(
      RecordedDrawTargetDestruction(static_cast<DrawTarget*>(this)));
}

void DrawTargetWrapAndRecord::FillRect(const Rect& aRect,
                                       const Pattern& aPattern,
                                       const DrawOptions& aOptions) {
  EnsurePatternDependenciesStored(aPattern);

  mRecorder->RecordEvent(RecordedFillRect(this, aRect, aPattern, aOptions));
  mFinalDT->FillRect(aRect, *AdjustedPattern(aPattern), aOptions);
}

void DrawTargetWrapAndRecord::StrokeRect(const Rect& aRect,
                                         const Pattern& aPattern,
                                         const StrokeOptions& aStrokeOptions,
                                         const DrawOptions& aOptions) {
  EnsurePatternDependenciesStored(aPattern);

  mRecorder->RecordEvent(
      RecordedStrokeRect(this, aRect, aPattern, aStrokeOptions, aOptions));
  mFinalDT->StrokeRect(aRect, *AdjustedPattern(aPattern), aStrokeOptions,
                       aOptions);
}

void DrawTargetWrapAndRecord::StrokeLine(const Point& aBegin, const Point& aEnd,
                                         const Pattern& aPattern,
                                         const StrokeOptions& aStrokeOptions,
                                         const DrawOptions& aOptions) {
  EnsurePatternDependenciesStored(aPattern);

  mRecorder->RecordEvent(RecordedStrokeLine(this, aBegin, aEnd, aPattern,
                                            aStrokeOptions, aOptions));
  mFinalDT->StrokeLine(aBegin, aEnd, *AdjustedPattern(aPattern), aStrokeOptions,
                       aOptions);
}

void DrawTargetWrapAndRecord::Fill(const Path* aPath, const Pattern& aPattern,
                                   const DrawOptions& aOptions) {
  RefPtr<PathRecording> pathWrapAndRecord = EnsurePathStored(aPath);
  EnsurePatternDependenciesStored(aPattern);

  mRecorder->RecordEvent(
      RecordedFill(this, pathWrapAndRecord, aPattern, aOptions));
  mFinalDT->Fill(pathWrapAndRecord->mPath, *AdjustedPattern(aPattern),
                 aOptions);
}

struct WrapAndRecordFontUserData {
  void* refPtr;
  RefPtr<DrawEventRecorderPrivate> recorder;
};

static void WrapAndRecordFontUserDataDestroyFunc(void* aUserData) {
  WrapAndRecordFontUserData* userData =
      static_cast<WrapAndRecordFontUserData*>(aUserData);

  userData->recorder->RecordEvent(
      RecordedScaledFontDestruction(ReferencePtr(userData->refPtr)));
  userData->recorder->RemoveScaledFont((ScaledFont*)userData->refPtr);
  delete userData;
}

void DrawTargetWrapAndRecord::FillGlyphs(ScaledFont* aFont,
                                         const GlyphBuffer& aBuffer,
                                         const Pattern& aPattern,
                                         const DrawOptions& aOptions) {
  EnsurePatternDependenciesStored(aPattern);

  UserDataKey* userDataKey = reinterpret_cast<UserDataKey*>(mRecorder.get());
  if (!aFont->GetUserData(userDataKey)) {
    UnscaledFont* unscaledFont = aFont->GetUnscaledFont();
    if (!mRecorder->HasStoredUnscaledFont(unscaledFont)) {
      RecordedFontData fontData(unscaledFont);
      RecordedFontDetails fontDetails;
      if (fontData.GetFontDetails(fontDetails)) {
        // Try to serialise the whole font, just in case this is a web font that
        // is not present on the system.
        if (!mRecorder->HasStoredFontData(fontDetails.fontDataKey)) {
          mRecorder->RecordEvent(fontData);
          mRecorder->AddStoredFontData(fontDetails.fontDataKey);
        }
        mRecorder->RecordEvent(
            RecordedUnscaledFontCreation(unscaledFont, fontDetails));
      } else {
        // If that fails, record just the font description and try to load it
        // from the system on the other side.
        RecordedFontDescriptor fontDesc(unscaledFont);
        if (fontDesc.IsValid()) {
          mRecorder->RecordEvent(fontDesc);
        } else {
          gfxWarning() << "DrawTargetWrapAndRecord::FillGlyphs failed to "
                          "serialise UnscaledFont";
        }
      }
      mRecorder->AddStoredUnscaledFont(unscaledFont);
    }

    mRecorder->RecordEvent(RecordedScaledFontCreation(aFont, unscaledFont));

    WrapAndRecordFontUserData* userData = new WrapAndRecordFontUserData;
    userData->refPtr = aFont;
    userData->recorder = mRecorder;
    aFont->AddUserData(userDataKey, userData,
                       &WrapAndRecordFontUserDataDestroyFunc);
    userData->recorder->AddScaledFont(aFont);
  }

  mRecorder->RecordEvent(RecordedFillGlyphs(
      this, aFont, aPattern, aOptions, aBuffer.mGlyphs, aBuffer.mNumGlyphs));
  mFinalDT->FillGlyphs(aFont, aBuffer, *AdjustedPattern(aPattern), aOptions);
}

void DrawTargetWrapAndRecord::Mask(const Pattern& aSource, const Pattern& aMask,
                                   const DrawOptions& aOptions) {
  EnsurePatternDependenciesStored(aSource);
  EnsurePatternDependenciesStored(aMask);

  mRecorder->RecordEvent(RecordedMask(this, aSource, aMask, aOptions));
  mFinalDT->Mask(*AdjustedPattern(aSource), *AdjustedPattern(aMask), aOptions);
}

void DrawTargetWrapAndRecord::MaskSurface(const Pattern& aSource,
                                          SourceSurface* aMask, Point aOffset,
                                          const DrawOptions& aOptions) {
  EnsurePatternDependenciesStored(aSource);
  EnsureSurfaceStored(mRecorder, aMask, "MaskSurface");

  mRecorder->RecordEvent(
      RecordedMaskSurface(this, aSource, aMask, aOffset, aOptions));
  mFinalDT->MaskSurface(*AdjustedPattern(aSource), GetSourceSurface(aMask),
                        aOffset, aOptions);
}

void DrawTargetWrapAndRecord::Stroke(const Path* aPath, const Pattern& aPattern,
                                     const StrokeOptions& aStrokeOptions,
                                     const DrawOptions& aOptions) {
  RefPtr<PathRecording> pathWrapAndRecord = EnsurePathStored(aPath);
  EnsurePatternDependenciesStored(aPattern);

  mRecorder->RecordEvent(RecordedStroke(this, pathWrapAndRecord, aPattern,
                                        aStrokeOptions, aOptions));
  mFinalDT->Stroke(pathWrapAndRecord->mPath, *AdjustedPattern(aPattern),
                   aStrokeOptions, aOptions);
}

already_AddRefed<SourceSurface> DrawTargetWrapAndRecord::Snapshot() {
  RefPtr<SourceSurface> surf = mFinalDT->Snapshot();

  RefPtr<SourceSurface> retSurf =
      new SourceSurfaceWrapAndRecord(surf, mRecorder);

  mRecorder->RecordEvent(RecordedSnapshot(retSurf, this));

  return retSurf.forget();
}

already_AddRefed<SourceSurface> DrawTargetWrapAndRecord::IntoLuminanceSource(
    LuminanceType aLuminanceType, float aOpacity) {
  RefPtr<SourceSurface> surf =
      mFinalDT->IntoLuminanceSource(aLuminanceType, aOpacity);

  RefPtr<SourceSurface> retSurf =
      new SourceSurfaceWrapAndRecord(surf, mRecorder);

  mRecorder->RecordEvent(
      RecordedIntoLuminanceSource(retSurf, this, aLuminanceType, aOpacity));

  return retSurf.forget();
}

void DrawTargetWrapAndRecord::DetachAllSnapshots() {
  mFinalDT->DetachAllSnapshots();
}

void DrawTargetWrapAndRecord::DrawSurface(
    SourceSurface* aSurface, const Rect& aDest, const Rect& aSource,
    const DrawSurfaceOptions& aSurfOptions, const DrawOptions& aOptions) {
  EnsureSurfaceStored(mRecorder, aSurface, "DrawSurface");

  mRecorder->RecordEvent(RecordedDrawSurface(this, aSurface, aDest, aSource,
                                             aSurfOptions, aOptions));
  mFinalDT->DrawSurface(GetSourceSurface(aSurface), aDest, aSource,
                        aSurfOptions, aOptions);
}

void DrawTargetWrapAndRecord::DrawSurfaceWithShadow(
    SourceSurface* aSurface, const Point& aDest, const DeviceColor& aColor,
    const Point& aOffset, Float aSigma, CompositionOp aOp) {
  EnsureSurfaceStored(mRecorder, aSurface, "DrawSurfaceWithShadow");

  mRecorder->RecordEvent(RecordedDrawSurfaceWithShadow(
      this, aSurface, aDest, aColor, aOffset, aSigma, aOp));
  mFinalDT->DrawSurfaceWithShadow(GetSourceSurface(aSurface), aDest, aColor,
                                  aOffset, aSigma, aOp);
}

void DrawTargetWrapAndRecord::DrawFilter(FilterNode* aNode,
                                         const Rect& aSourceRect,
                                         const Point& aDestPoint,
                                         const DrawOptions& aOptions) {
  MOZ_ASSERT(mRecorder->HasStoredObject(aNode));

  mRecorder->RecordEvent(
      RecordedDrawFilter(this, aNode, aSourceRect, aDestPoint, aOptions));
  mFinalDT->DrawFilter(FilterNodeWrapAndRecord::GetFilterNode(aNode),
                       aSourceRect, aDestPoint, aOptions);
}

already_AddRefed<FilterNode> DrawTargetWrapAndRecord::CreateFilter(
    FilterType aType) {
  RefPtr<FilterNode> node = mFinalDT->CreateFilter(aType);

  RefPtr<FilterNode> retNode = new FilterNodeWrapAndRecord(node, mRecorder);

  mRecorder->RecordEvent(RecordedFilterNodeCreation(retNode, aType));

  return retNode.forget();
}

void DrawTargetWrapAndRecord::ClearRect(const Rect& aRect) {
  mRecorder->RecordEvent(RecordedClearRect(this, aRect));
  mFinalDT->ClearRect(aRect);
}

void DrawTargetWrapAndRecord::CopySurface(SourceSurface* aSurface,
                                          const IntRect& aSourceRect,
                                          const IntPoint& aDestination) {
  EnsureSurfaceStored(mRecorder, aSurface, "CopySurface");

  mRecorder->RecordEvent(
      RecordedCopySurface(this, aSurface, aSourceRect, aDestination));
  mFinalDT->CopySurface(GetSourceSurface(aSurface), aSourceRect, aDestination);
}

void DrawTargetWrapAndRecord::PushClip(const Path* aPath) {
  RefPtr<PathRecording> pathWrapAndRecord = EnsurePathStored(aPath);

  mRecorder->RecordEvent(RecordedPushClip(this, pathWrapAndRecord));
  mFinalDT->PushClip(pathWrapAndRecord->mPath);
}

void DrawTargetWrapAndRecord::PushClipRect(const Rect& aRect) {
  mRecorder->RecordEvent(RecordedPushClipRect(this, aRect));
  mFinalDT->PushClipRect(aRect);
}

void DrawTargetWrapAndRecord::PopClip() {
  mRecorder->RecordEvent(RecordedPopClip(static_cast<DrawTarget*>(this)));
  mFinalDT->PopClip();
}

void DrawTargetWrapAndRecord::PushLayer(bool aOpaque, Float aOpacity,
                                        SourceSurface* aMask,
                                        const Matrix& aMaskTransform,
                                        const IntRect& aBounds,
                                        bool aCopyBackground) {
  if (aMask) {
    EnsureSurfaceStored(mRecorder, aMask, "PushLayer");
  }

  mRecorder->RecordEvent(RecordedPushLayer(this, aOpaque, aOpacity, aMask,
                                           aMaskTransform, aBounds,
                                           aCopyBackground));
  mFinalDT->PushLayer(aOpaque, aOpacity, aMask, aMaskTransform, aBounds,
                      aCopyBackground);
}

void DrawTargetWrapAndRecord::PopLayer() {
  mRecorder->RecordEvent(RecordedPopLayer(static_cast<DrawTarget*>(this)));
  mFinalDT->PopLayer();
}

already_AddRefed<SourceSurface>
DrawTargetWrapAndRecord::CreateSourceSurfaceFromData(
    unsigned char* aData, const IntSize& aSize, int32_t aStride,
    SurfaceFormat aFormat) const {
  RefPtr<SourceSurface> surf =
      mFinalDT->CreateSourceSurfaceFromData(aData, aSize, aStride, aFormat);

  RefPtr<SourceSurface> retSurf =
      new SourceSurfaceWrapAndRecord(surf, mRecorder);

  mRecorder->RecordEvent(
      RecordedSourceSurfaceCreation(retSurf, aData, aStride, aSize, aFormat));

  return retSurf.forget();
}

already_AddRefed<SourceSurface> DrawTargetWrapAndRecord::OptimizeSourceSurface(
    SourceSurface* aSurface) const {
  RefPtr<SourceSurface> surf = mFinalDT->OptimizeSourceSurface(aSurface);

  RefPtr<SourceSurface> retSurf =
      new SourceSurfaceWrapAndRecord(surf, mRecorder);

  RefPtr<DataSourceSurface> dataSurf = surf->GetDataSurface();

  if (!dataSurf) {
    // Let's try get it off the original surface.
    dataSurf = aSurface->GetDataSurface();
  }

  StoreSourceSurface(mRecorder, retSurf, dataSurf, "OptimizeSourceSurface");

  return retSurf.forget();
}

already_AddRefed<SourceSurface>
DrawTargetWrapAndRecord::CreateSourceSurfaceFromNativeSurface(
    const NativeSurface& aSurface) const {
  RefPtr<SourceSurface> surf =
      mFinalDT->CreateSourceSurfaceFromNativeSurface(aSurface);

  RefPtr<SourceSurface> retSurf =
      new SourceSurfaceWrapAndRecord(surf, mRecorder);

  RefPtr<DataSourceSurface> dataSurf = surf->GetDataSurface();
  StoreSourceSurface(mRecorder, retSurf, dataSurf,
                     "CreateSourceSurfaceFromNativeSurface");

  return retSurf.forget();
}

already_AddRefed<DrawTarget> DrawTargetWrapAndRecord::CreateSimilarDrawTarget(
    const IntSize& aSize, SurfaceFormat aFormat) const {
  RefPtr<DrawTarget> similarDT =
      mFinalDT->CreateSimilarDrawTarget(aSize, aFormat);
  if (!similarDT) {
    return nullptr;
  }

  similarDT = new DrawTargetWrapAndRecord(this, similarDT);
  return similarDT.forget();
}

bool DrawTargetWrapAndRecord::CanCreateSimilarDrawTarget(
    const IntSize& aSize, SurfaceFormat aFormat) const {
  return mFinalDT->CanCreateSimilarDrawTarget(aSize, aFormat);
}

RefPtr<DrawTarget> DrawTargetWrapAndRecord::CreateClippedDrawTarget(
    const Rect& aBounds, SurfaceFormat aFormat) {
  RefPtr<DrawTarget> similarDT;
  RefPtr<DrawTarget> innerDT =
      mFinalDT->CreateClippedDrawTarget(aBounds, aFormat);
  similarDT = new DrawTargetWrapAndRecord(this->mRecorder, innerDT);
  mRecorder->RecordEvent(
      RecordedCreateClippedDrawTarget(this, similarDT.get(), aBounds, aFormat));
  return similarDT;
}

already_AddRefed<PathBuilder> DrawTargetWrapAndRecord::CreatePathBuilder(
    FillRule aFillRule) const {
  RefPtr<PathBuilder> builder = mFinalDT->CreatePathBuilder(aFillRule);
  return MakeAndAddRef<PathBuilderRecording>(builder, aFillRule);
}

already_AddRefed<GradientStops> DrawTargetWrapAndRecord::CreateGradientStops(
    GradientStop* aStops, uint32_t aNumStops, ExtendMode aExtendMode) const {
  RefPtr<GradientStops> stops =
      mFinalDT->CreateGradientStops(aStops, aNumStops, aExtendMode);

  RefPtr<GradientStops> retStops =
      new GradientStopsWrapAndRecord(stops, mRecorder);

  mRecorder->RecordEvent(
      RecordedGradientStopsCreation(retStops, aStops, aNumStops, aExtendMode));

  return retStops.forget();
}

void DrawTargetWrapAndRecord::SetTransform(const Matrix& aTransform) {
  mRecorder->RecordEvent(RecordedSetTransform(this, aTransform));
  DrawTarget::SetTransform(aTransform);
  mFinalDT->SetTransform(aTransform);
}

already_AddRefed<PathRecording> DrawTargetWrapAndRecord::EnsurePathStored(
    const Path* aPath) {
  RefPtr<PathRecording> pathWrapAndRecord;
  if (aPath->GetBackendType() == BackendType::RECORDING) {
    pathWrapAndRecord =
        const_cast<PathRecording*>(static_cast<const PathRecording*>(aPath));
    if (mRecorder->HasStoredObject(aPath)) {
      return pathWrapAndRecord.forget();
    }
  } else {
    MOZ_ASSERT(!mRecorder->HasStoredObject(aPath));
    FillRule fillRule = aPath->GetFillRule();
    RefPtr<PathBuilder> builder = mFinalDT->CreatePathBuilder(fillRule);
    RefPtr<PathBuilderRecording> builderWrapAndRecord =
        new PathBuilderRecording(builder, fillRule);
    aPath->StreamToSink(builderWrapAndRecord);
    pathWrapAndRecord =
        builderWrapAndRecord->Finish().downcast<PathRecording>();
  }

  mRecorder->RecordEvent(RecordedPathCreation(pathWrapAndRecord.get()));
  mRecorder->AddStoredObject(pathWrapAndRecord);
  pathWrapAndRecord->mStoredRecorders.push_back(mRecorder);

  return pathWrapAndRecord.forget();
}

void DrawTargetWrapAndRecord::EnsurePatternDependenciesStored(
    const Pattern& aPattern) {
  switch (aPattern.GetType()) {
    case PatternType::COLOR:
      // No dependencies here.
      return;
    case PatternType::LINEAR_GRADIENT: {
      MOZ_ASSERT(mRecorder->HasStoredObject(
          static_cast<const LinearGradientPattern*>(&aPattern)->mStops));
      return;
    }
    case PatternType::RADIAL_GRADIENT: {
      MOZ_ASSERT(mRecorder->HasStoredObject(
          static_cast<const RadialGradientPattern*>(&aPattern)->mStops));
      return;
    }
    case PatternType::CONIC_GRADIENT: {
      MOZ_ASSERT(mRecorder->HasStoredObject(
          static_cast<const ConicGradientPattern*>(&aPattern)->mStops));
      return;
    }
    case PatternType::SURFACE: {
      const SurfacePattern* pat = static_cast<const SurfacePattern*>(&aPattern);
      EnsureSurfaceStored(mRecorder, pat->mSurface,
                          "EnsurePatternDependenciesStored");
      return;
    }
  }
}

}  // namespace gfx
}  // namespace mozilla
