/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RecordedEvent.h"

#include "PathRecording.h"
#include "RecordingTypes.h"
#include "Tools.h"
#include "Filters.h"
#include "Logging.h"
#include "ScaledFontBase.h"
#include "SFNTData.h"

namespace mozilla {
namespace gfx {

using namespace std;

static std::string NameFromBackend(BackendType aType)
{
  switch (aType) {
  case BackendType::NONE:
    return "None";
  case BackendType::DIRECT2D:
    return "Direct2D";
  default:
    return "Unknown";
  }
}

already_AddRefed<DrawTarget>
Translator::CreateDrawTarget(ReferencePtr aRefPtr, const IntSize &aSize,
                             SurfaceFormat aFormat)
{
  RefPtr<DrawTarget> newDT =
    GetReferenceDrawTarget()->CreateSimilarDrawTarget(aSize, aFormat);
  AddDrawTarget(aRefPtr, newDT);
  return newDT.forget();
}

#define LOAD_EVENT_TYPE(_typeenum, _class) \
  case _typeenum: return new _class(aStream)

RecordedEvent *
RecordedEvent::LoadEventFromStream(std::istream &aStream, EventType aType)
{
  switch (aType) {
    LOAD_EVENT_TYPE(DRAWTARGETCREATION, RecordedDrawTargetCreation);
    LOAD_EVENT_TYPE(DRAWTARGETDESTRUCTION, RecordedDrawTargetDestruction);
    LOAD_EVENT_TYPE(FILLRECT, RecordedFillRect);
    LOAD_EVENT_TYPE(STROKERECT, RecordedStrokeRect);
    LOAD_EVENT_TYPE(STROKELINE, RecordedStrokeLine);
    LOAD_EVENT_TYPE(CLEARRECT, RecordedClearRect);
    LOAD_EVENT_TYPE(COPYSURFACE, RecordedCopySurface);
    LOAD_EVENT_TYPE(SETTRANSFORM, RecordedSetTransform);
    LOAD_EVENT_TYPE(PUSHCLIPRECT, RecordedPushClipRect);
    LOAD_EVENT_TYPE(PUSHCLIP, RecordedPushClip);
    LOAD_EVENT_TYPE(POPCLIP, RecordedPopClip);
    LOAD_EVENT_TYPE(FILL, RecordedFill);
    LOAD_EVENT_TYPE(FILLGLYPHS, RecordedFillGlyphs);
    LOAD_EVENT_TYPE(MASK, RecordedMask);
    LOAD_EVENT_TYPE(STROKE, RecordedStroke);
    LOAD_EVENT_TYPE(DRAWSURFACE, RecordedDrawSurface);
    LOAD_EVENT_TYPE(DRAWSURFACEWITHSHADOW, RecordedDrawSurfaceWithShadow);
    LOAD_EVENT_TYPE(DRAWFILTER, RecordedDrawFilter);
    LOAD_EVENT_TYPE(PATHCREATION, RecordedPathCreation);
    LOAD_EVENT_TYPE(PATHDESTRUCTION, RecordedPathDestruction);
    LOAD_EVENT_TYPE(SOURCESURFACECREATION, RecordedSourceSurfaceCreation);
    LOAD_EVENT_TYPE(SOURCESURFACEDESTRUCTION, RecordedSourceSurfaceDestruction);
    LOAD_EVENT_TYPE(FILTERNODECREATION, RecordedFilterNodeCreation);
    LOAD_EVENT_TYPE(FILTERNODEDESTRUCTION, RecordedFilterNodeDestruction);
    LOAD_EVENT_TYPE(GRADIENTSTOPSCREATION, RecordedGradientStopsCreation);
    LOAD_EVENT_TYPE(GRADIENTSTOPSDESTRUCTION, RecordedGradientStopsDestruction);
    LOAD_EVENT_TYPE(SNAPSHOT, RecordedSnapshot);
    LOAD_EVENT_TYPE(SCALEDFONTCREATION, RecordedScaledFontCreation);
    LOAD_EVENT_TYPE(SCALEDFONTDESTRUCTION, RecordedScaledFontDestruction);
    LOAD_EVENT_TYPE(MASKSURFACE, RecordedMaskSurface);
    LOAD_EVENT_TYPE(FILTERNODESETATTRIBUTE, RecordedFilterNodeSetAttribute);
    LOAD_EVENT_TYPE(FILTERNODESETINPUT, RecordedFilterNodeSetInput);
    LOAD_EVENT_TYPE(CREATESIMILARDRAWTARGET, RecordedCreateSimilarDrawTarget);
    LOAD_EVENT_TYPE(FONTDATA, RecordedFontData);
    LOAD_EVENT_TYPE(FONTDESC, RecordedFontDescriptor);
    LOAD_EVENT_TYPE(PUSHLAYER, RecordedPushLayer);
    LOAD_EVENT_TYPE(POPLAYER, RecordedPopLayer);
  default:
    return nullptr;
  }
}

string
RecordedEvent::GetEventName(EventType aType)
{
  switch (aType) {
  case DRAWTARGETCREATION:
    return "DrawTarget Creation";
  case DRAWTARGETDESTRUCTION:
    return "DrawTarget Destruction";
  case FILLRECT:
    return "FillRect";
  case STROKERECT:
    return "StrokeRect";
  case STROKELINE:
    return "StrokeLine";
  case CLEARRECT:
    return "ClearRect";
  case COPYSURFACE:
    return "CopySurface";
  case SETTRANSFORM:
    return "SetTransform";
  case PUSHCLIP:
    return "PushClip";
  case PUSHCLIPRECT:
    return "PushClipRect";
  case POPCLIP:
    return "PopClip";
  case FILL:
    return "Fill";
  case FILLGLYPHS:
    return "FillGlyphs";
  case MASK:
    return "Mask";
  case STROKE:
    return "Stroke";
  case DRAWSURFACE:
    return "DrawSurface";
  case DRAWSURFACEWITHSHADOW:
    return "DrawSurfaceWithShadow";
  case DRAWFILTER:
    return "DrawFilter";
  case PATHCREATION:
    return "PathCreation";
  case PATHDESTRUCTION:
    return "PathDestruction";
  case SOURCESURFACECREATION:
    return "SourceSurfaceCreation";
  case SOURCESURFACEDESTRUCTION:
    return "SourceSurfaceDestruction";
  case FILTERNODECREATION:
    return "FilterNodeCreation";
  case FILTERNODEDESTRUCTION:
    return "FilterNodeDestruction";
  case GRADIENTSTOPSCREATION:
    return "GradientStopsCreation";
  case GRADIENTSTOPSDESTRUCTION:
    return "GradientStopsDestruction";
  case SNAPSHOT:
    return "Snapshot";
  case SCALEDFONTCREATION:
    return "ScaledFontCreation";
  case SCALEDFONTDESTRUCTION:
    return "ScaledFontDestruction";
  case MASKSURFACE:
    return "MaskSurface";
  case FILTERNODESETATTRIBUTE:
    return "SetAttribute";
  case FILTERNODESETINPUT:
    return "SetInput";
  case CREATESIMILARDRAWTARGET:
    return "CreateSimilarDrawTarget";
  case FONTDATA:
    return "FontData";
  case FONTDESC:
    return "FontDescriptor";
  case PUSHLAYER:
    return "PushLayer";
  case POPLAYER:
    return "PopLayer";
  default:
    return "Unknown";
  }
}

void
RecordedEvent::RecordPatternData(std::ostream &aStream, const PatternStorage &aPattern) const
{
  WriteElement(aStream, aPattern.mType);

  switch (aPattern.mType) {
  case PatternType::COLOR:
    {
      WriteElement(aStream, *reinterpret_cast<const ColorPatternStorage*>(&aPattern.mStorage));
      return;
    }
  case PatternType::LINEAR_GRADIENT:
    {
      WriteElement(aStream, *reinterpret_cast<const LinearGradientPatternStorage*>(&aPattern.mStorage));
      return;
    }
  case PatternType::RADIAL_GRADIENT:
    {
      WriteElement(aStream, *reinterpret_cast<const RadialGradientPatternStorage*>(&aPattern.mStorage));
      return;
    }
  case PatternType::SURFACE:
    {
      WriteElement(aStream, *reinterpret_cast<const SurfacePatternStorage*>(&aPattern.mStorage));
      return;
    }
  default:
    return;
  }
}

void
RecordedEvent::ReadPatternData(std::istream &aStream, PatternStorage &aPattern) const
{
  ReadElement(aStream, aPattern.mType);

  switch (aPattern.mType) {
  case PatternType::COLOR:
    {
      ReadElement(aStream, *reinterpret_cast<ColorPatternStorage*>(&aPattern.mStorage));
      return;
    }
  case PatternType::LINEAR_GRADIENT:
    {
      ReadElement(aStream, *reinterpret_cast<LinearGradientPatternStorage*>(&aPattern.mStorage));
      return;
    }
  case PatternType::RADIAL_GRADIENT:
    {
      ReadElement(aStream, *reinterpret_cast<RadialGradientPatternStorage*>(&aPattern.mStorage));
      return;
    }
  case PatternType::SURFACE:
    {
      ReadElement(aStream, *reinterpret_cast<SurfacePatternStorage*>(&aPattern.mStorage));
      return;
    }
  default:
    return;
  }
}

void
RecordedEvent::StorePattern(PatternStorage &aDestination, const Pattern &aSource) const
{
  aDestination.mType = aSource.GetType();
  
  switch (aSource.GetType()) {
  case PatternType::COLOR:
    {
      reinterpret_cast<ColorPatternStorage*>(&aDestination.mStorage)->mColor =
        static_cast<const ColorPattern*>(&aSource)->mColor;
      return;
    }
  case PatternType::LINEAR_GRADIENT:
    {
      LinearGradientPatternStorage *store =
        reinterpret_cast<LinearGradientPatternStorage*>(&aDestination.mStorage);
      const LinearGradientPattern *pat =
        static_cast<const LinearGradientPattern*>(&aSource);
      store->mBegin = pat->mBegin;
      store->mEnd = pat->mEnd;
      store->mMatrix = pat->mMatrix;
      store->mStops = pat->mStops.get();
      return;
    }
  case PatternType::RADIAL_GRADIENT:
    {
      RadialGradientPatternStorage *store =
        reinterpret_cast<RadialGradientPatternStorage*>(&aDestination.mStorage);
      const RadialGradientPattern *pat =
        static_cast<const RadialGradientPattern*>(&aSource);
      store->mCenter1 = pat->mCenter1;
      store->mCenter2 = pat->mCenter2;
      store->mRadius1 = pat->mRadius1;
      store->mRadius2 = pat->mRadius2;
      store->mMatrix = pat->mMatrix;
      store->mStops = pat->mStops.get();
      return;
    }
  case PatternType::SURFACE:
    {
      SurfacePatternStorage *store =
        reinterpret_cast<SurfacePatternStorage*>(&aDestination.mStorage);
      const SurfacePattern *pat =
        static_cast<const SurfacePattern*>(&aSource);
      store->mExtend = pat->mExtendMode;
      store->mSamplingFilter = pat->mSamplingFilter;
      store->mMatrix = pat->mMatrix;
      store->mSurface = pat->mSurface;
      store->mSamplingRect = pat->mSamplingRect;
      return;
    }
  }
}

void
RecordedEvent::RecordStrokeOptions(std::ostream &aStream, const StrokeOptions &aStrokeOptions) const
{
  JoinStyle joinStyle = aStrokeOptions.mLineJoin;
  CapStyle capStyle = aStrokeOptions.mLineCap;

  WriteElement(aStream, uint64_t(aStrokeOptions.mDashLength));
  WriteElement(aStream, aStrokeOptions.mDashOffset);
  WriteElement(aStream, aStrokeOptions.mLineWidth);
  WriteElement(aStream, aStrokeOptions.mMiterLimit);
  WriteElement(aStream, joinStyle);
  WriteElement(aStream, capStyle);

  if (!aStrokeOptions.mDashPattern) {
    return;
  }

  aStream.write((char*)aStrokeOptions.mDashPattern, sizeof(Float) * aStrokeOptions.mDashLength);
}

void
RecordedEvent::ReadStrokeOptions(std::istream &aStream, StrokeOptions &aStrokeOptions)
{
  uint64_t dashLength;
  JoinStyle joinStyle;
  CapStyle capStyle;

  ReadElement(aStream, dashLength);
  ReadElement(aStream, aStrokeOptions.mDashOffset);
  ReadElement(aStream, aStrokeOptions.mLineWidth);
  ReadElement(aStream, aStrokeOptions.mMiterLimit);
  ReadElement(aStream, joinStyle);
  ReadElement(aStream, capStyle);
  // On 32 bit we truncate the value of dashLength.
  // See also bug 811850 for history.
  aStrokeOptions.mDashLength = size_t(dashLength);
  aStrokeOptions.mLineJoin = joinStyle;
  aStrokeOptions.mLineCap = capStyle;

  if (!aStrokeOptions.mDashLength) {
    return;
  }

  mDashPatternStorage.resize(aStrokeOptions.mDashLength);
  aStrokeOptions.mDashPattern = &mDashPatternStorage.front();
  aStream.read((char*)aStrokeOptions.mDashPattern, sizeof(Float) * aStrokeOptions.mDashLength);
}

void
RecordedEvent::OutputSimplePatternInfo(const PatternStorage &aStorage, std::stringstream &aOutput) const
{
  switch (aStorage.mType) {
  case PatternType::COLOR:
    {
      const Color color = reinterpret_cast<const ColorPatternStorage*>(&aStorage.mStorage)->mColor;
      aOutput << "Color: (" << color.r << ", " << color.g << ", " << color.b << ", " << color.a << ")";
      return;
    }
  case PatternType::LINEAR_GRADIENT:
    {
      const LinearGradientPatternStorage *store =
        reinterpret_cast<const LinearGradientPatternStorage*>(&aStorage.mStorage);

      aOutput << "LinearGradient (" << store->mBegin.x << ", " << store->mBegin.y <<
        ") - (" << store->mEnd.x << ", " << store->mEnd.y << ") Stops: " << store->mStops;
      return;
    }
  case PatternType::RADIAL_GRADIENT:
    {
      const RadialGradientPatternStorage *store =
        reinterpret_cast<const RadialGradientPatternStorage*>(&aStorage.mStorage);
      aOutput << "RadialGradient (Center 1: (" << store->mCenter1.x << ", " <<
        store->mCenter2.y << ") Radius 2: " << store->mRadius2;
      return;
    }
  case PatternType::SURFACE:
    {
      const SurfacePatternStorage *store =
        reinterpret_cast<const SurfacePatternStorage*>(&aStorage.mStorage);
      aOutput << "Surface (0x" << store->mSurface << ")";
      return;
    }
  }
}

RecordedDrawingEvent::RecordedDrawingEvent(EventType aType, std::istream &aStream)
  : RecordedEvent(aType)
{
  ReadElement(aStream, mDT);
}

void
RecordedDrawingEvent::RecordToStream(ostream &aStream) const
{
  WriteElement(aStream, mDT);
}

ReferencePtr
RecordedDrawingEvent::GetObjectRef() const
{
  return mDT;
}

bool
RecordedDrawTargetCreation::PlayEvent(Translator *aTranslator) const
{
  RefPtr<DrawTarget> newDT =
    aTranslator->CreateDrawTarget(mRefPtr, mSize, mFormat);

  // If we couldn't create a DrawTarget this will probably cause us to crash
  // with nullptr later in the playback, so return false to abort.
  if (!newDT) {
    return false;
  }

  if (mHasExistingData) {
    Rect dataRect(0, 0, mExistingData->GetSize().width, mExistingData->GetSize().height);
    newDT->DrawSurface(mExistingData, dataRect, dataRect);
  }

  return true;
}

void
RecordedDrawTargetCreation::RecordToStream(ostream &aStream) const
{
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mBackendType);
  WriteElement(aStream, mSize);
  WriteElement(aStream, mFormat);
  WriteElement(aStream, mHasExistingData);

  if (mHasExistingData) {
    MOZ_ASSERT(mExistingData);
    MOZ_ASSERT(mExistingData->GetSize() == mSize);
    RefPtr<DataSourceSurface> dataSurf = mExistingData->GetDataSurface();
    for (int y = 0; y < mSize.height; y++) {
      aStream.write((const char*)dataSurf->GetData() + y * dataSurf->Stride(),
                    BytesPerPixel(mFormat) * mSize.width);
    }
  }
}

RecordedDrawTargetCreation::RecordedDrawTargetCreation(istream &aStream)
  : RecordedEvent(DRAWTARGETCREATION)
  , mExistingData(nullptr)
{
  ReadElement(aStream, mRefPtr);
  ReadElement(aStream, mBackendType);
  ReadElement(aStream, mSize);
  ReadElement(aStream, mFormat);
  ReadElement(aStream, mHasExistingData);

  if (mHasExistingData) {
    RefPtr<DataSourceSurface> dataSurf = Factory::CreateDataSourceSurface(mSize, mFormat);
    if (!dataSurf) {
      gfxWarning() << "RecordedDrawTargetCreation had to reset mHasExistingData";
      mHasExistingData = false;
      return;
    }

    for (int y = 0; y < mSize.height; y++) {
      aStream.read((char*)dataSurf->GetData() + y * dataSurf->Stride(),
                    BytesPerPixel(mFormat) * mSize.width);
    }
    mExistingData = dataSurf;
  }
}

void
RecordedDrawTargetCreation::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] DrawTarget Creation (Type: " << NameFromBackend(mBackendType) << ", Size: " << mSize.width << "x" << mSize.height << ")";
}


bool
RecordedDrawTargetDestruction::PlayEvent(Translator *aTranslator) const
{
  aTranslator->RemoveDrawTarget(mRefPtr);
  return true;
}

void
RecordedDrawTargetDestruction::RecordToStream(ostream &aStream) const
{
  WriteElement(aStream, mRefPtr);
}

RecordedDrawTargetDestruction::RecordedDrawTargetDestruction(istream &aStream)
  : RecordedEvent(DRAWTARGETDESTRUCTION)
{
  ReadElement(aStream, mRefPtr);
}

void
RecordedDrawTargetDestruction::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] DrawTarget Destruction";
}

bool
RecordedCreateSimilarDrawTarget::PlayEvent(Translator *aTranslator) const
{
  RefPtr<DrawTarget> newDT =
    aTranslator->GetReferenceDrawTarget()->CreateSimilarDrawTarget(mSize, mFormat);

  // If we couldn't create a DrawTarget this will probably cause us to crash
  // with nullptr later in the playback, so return false to abort.
  if (!newDT) {
    return false;
  }

  aTranslator->AddDrawTarget(mRefPtr, newDT);
  return true;
}

void
RecordedCreateSimilarDrawTarget::RecordToStream(ostream &aStream) const
{
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mSize);
  WriteElement(aStream, mFormat);
}

RecordedCreateSimilarDrawTarget::RecordedCreateSimilarDrawTarget(istream &aStream)
  : RecordedEvent(CREATESIMILARDRAWTARGET)
{
  ReadElement(aStream, mRefPtr);
  ReadElement(aStream, mSize);
  ReadElement(aStream, mFormat);
}

void
RecordedCreateSimilarDrawTarget::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] CreateSimilarDrawTarget (Size: " << mSize.width << "x" << mSize.height << ")";
}

struct GenericPattern
{
  GenericPattern(const PatternStorage &aStorage, Translator *aTranslator)
    : mPattern(nullptr), mTranslator(aTranslator)
  {
    mStorage = const_cast<PatternStorage*>(&aStorage);
  }

  ~GenericPattern() {
    if (mPattern) {
      mPattern->~Pattern();
    }
  }

  operator Pattern*()
  {
    switch(mStorage->mType) {
    case PatternType::COLOR:
      return new (mColPat) ColorPattern(reinterpret_cast<ColorPatternStorage*>(&mStorage->mStorage)->mColor);
    case PatternType::SURFACE:
      {
        SurfacePatternStorage *storage = reinterpret_cast<SurfacePatternStorage*>(&mStorage->mStorage);
        mPattern =
          new (mSurfPat) SurfacePattern(mTranslator->LookupSourceSurface(storage->mSurface),
                                        storage->mExtend, storage->mMatrix,
                                        storage->mSamplingFilter,
                                        storage->mSamplingRect);
        return mPattern;
      }
    case PatternType::LINEAR_GRADIENT:
      {
        LinearGradientPatternStorage *storage = reinterpret_cast<LinearGradientPatternStorage*>(&mStorage->mStorage);
        mPattern =
          new (mLinGradPat) LinearGradientPattern(storage->mBegin, storage->mEnd,
                                                  mTranslator->LookupGradientStops(storage->mStops),
                                                  storage->mMatrix);
        return mPattern;
      }
    case PatternType::RADIAL_GRADIENT:
      {
        RadialGradientPatternStorage *storage = reinterpret_cast<RadialGradientPatternStorage*>(&mStorage->mStorage);
        mPattern =
          new (mRadGradPat) RadialGradientPattern(storage->mCenter1, storage->mCenter2,
                                                  storage->mRadius1, storage->mRadius2,
                                                  mTranslator->LookupGradientStops(storage->mStops),
                                                  storage->mMatrix);
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

  PatternStorage *mStorage;
  Pattern *mPattern;
  Translator *mTranslator;
};

bool
RecordedFillRect::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->FillRect(mRect, *GenericPattern(mPattern, aTranslator), mOptions);
  return true;
}

void
RecordedFillRect::RecordToStream(ostream &aStream) const
{
  RecordedDrawingEvent::RecordToStream(aStream);
  WriteElement(aStream, mRect);
  WriteElement(aStream, mOptions);
  RecordPatternData(aStream, mPattern);
}

RecordedFillRect::RecordedFillRect(istream &aStream)
  : RecordedDrawingEvent(FILLRECT, aStream)
{
  ReadElement(aStream, mRect);
  ReadElement(aStream, mOptions);
  ReadPatternData(aStream, mPattern);
}

void
RecordedFillRect::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] FillRect (" << mRect.x << ", " << mRect.y << " - " << mRect.width << " x " << mRect.height << ") ";
  OutputSimplePatternInfo(mPattern, aStringStream);
}

bool
RecordedStrokeRect::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->StrokeRect(mRect, *GenericPattern(mPattern, aTranslator), mStrokeOptions, mOptions);
  return true;
}

void
RecordedStrokeRect::RecordToStream(ostream &aStream) const
{
  RecordedDrawingEvent::RecordToStream(aStream);
  WriteElement(aStream, mRect);
  WriteElement(aStream, mOptions);
  RecordPatternData(aStream, mPattern);
  RecordStrokeOptions(aStream, mStrokeOptions);
}

RecordedStrokeRect::RecordedStrokeRect(istream &aStream)
  : RecordedDrawingEvent(STROKERECT, aStream)
{
  ReadElement(aStream, mRect);
  ReadElement(aStream, mOptions);
  ReadPatternData(aStream, mPattern);
  ReadStrokeOptions(aStream, mStrokeOptions);
}

void
RecordedStrokeRect::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] StrokeRect (" << mRect.x << ", " << mRect.y << " - " << mRect.width << " x " << mRect.height
                << ") LineWidth: " << mStrokeOptions.mLineWidth << "px ";
  OutputSimplePatternInfo(mPattern, aStringStream);
}

bool
RecordedStrokeLine::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->StrokeLine(mBegin, mEnd, *GenericPattern(mPattern, aTranslator), mStrokeOptions, mOptions);
  return true;
}

void
RecordedStrokeLine::RecordToStream(ostream &aStream) const
{
  RecordedDrawingEvent::RecordToStream(aStream);
  WriteElement(aStream, mBegin);
  WriteElement(aStream, mEnd);
  WriteElement(aStream, mOptions);
  RecordPatternData(aStream, mPattern);
  RecordStrokeOptions(aStream, mStrokeOptions);
}

RecordedStrokeLine::RecordedStrokeLine(istream &aStream)
  : RecordedDrawingEvent(STROKELINE, aStream)
{
  ReadElement(aStream, mBegin);
  ReadElement(aStream, mEnd);
  ReadElement(aStream, mOptions);
  ReadPatternData(aStream, mPattern);
  ReadStrokeOptions(aStream, mStrokeOptions);
}

void
RecordedStrokeLine::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] StrokeLine (" << mBegin.x << ", " << mBegin.y << " - " << mEnd.x << ", " << mEnd.y
                << ") LineWidth: " << mStrokeOptions.mLineWidth << "px ";
  OutputSimplePatternInfo(mPattern, aStringStream);
}

bool
RecordedFill::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->Fill(aTranslator->LookupPath(mPath), *GenericPattern(mPattern, aTranslator), mOptions);
  return true;
}

RecordedFill::RecordedFill(istream &aStream)
  : RecordedDrawingEvent(FILL, aStream)
{
  ReadElement(aStream, mPath);
  ReadElement(aStream, mOptions);
  ReadPatternData(aStream, mPattern);
}

void
RecordedFill::RecordToStream(ostream &aStream) const
{
  RecordedDrawingEvent::RecordToStream(aStream);
  WriteElement(aStream, mPath);
  WriteElement(aStream, mOptions);
  RecordPatternData(aStream, mPattern);
}

void
RecordedFill::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] Fill (" << mPath << ") ";
  OutputSimplePatternInfo(mPattern, aStringStream);
}

RecordedFillGlyphs::~RecordedFillGlyphs()
{
  delete [] mGlyphs;
}

bool
RecordedFillGlyphs::PlayEvent(Translator *aTranslator) const
{
  GlyphBuffer buffer;
  buffer.mGlyphs = mGlyphs;
  buffer.mNumGlyphs = mNumGlyphs;
  aTranslator->LookupDrawTarget(mDT)->FillGlyphs(aTranslator->LookupScaledFont(mScaledFont), buffer, *GenericPattern(mPattern, aTranslator), mOptions);
  return true;
}

RecordedFillGlyphs::RecordedFillGlyphs(istream &aStream)
  : RecordedDrawingEvent(FILLGLYPHS, aStream)
{
  ReadElement(aStream, mScaledFont);
  ReadElement(aStream, mOptions);
  ReadPatternData(aStream, mPattern);
  ReadElement(aStream, mNumGlyphs);
  mGlyphs = new Glyph[mNumGlyphs];
  aStream.read((char*)mGlyphs, sizeof(Glyph) * mNumGlyphs);
}

void
RecordedFillGlyphs::RecordToStream(ostream &aStream) const
{
  RecordedDrawingEvent::RecordToStream(aStream);
  WriteElement(aStream, mScaledFont);
  WriteElement(aStream, mOptions);
  RecordPatternData(aStream, mPattern);
  WriteElement(aStream, mNumGlyphs);
  aStream.write((char*)mGlyphs, sizeof(Glyph) * mNumGlyphs);
}

void
RecordedFillGlyphs::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] FillGlyphs (" << mScaledFont << ") ";
  OutputSimplePatternInfo(mPattern, aStringStream);
}

bool
RecordedMask::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->Mask(*GenericPattern(mSource, aTranslator), *GenericPattern(mMask, aTranslator), mOptions);
  return true;
}

RecordedMask::RecordedMask(istream &aStream)
  : RecordedDrawingEvent(MASK, aStream)
{
  ReadElement(aStream, mOptions);
  ReadPatternData(aStream, mSource);
  ReadPatternData(aStream, mMask);
}

void
RecordedMask::RecordToStream(ostream &aStream) const
{
  RecordedDrawingEvent::RecordToStream(aStream);
  WriteElement(aStream, mOptions);
  RecordPatternData(aStream, mSource);
  RecordPatternData(aStream, mMask);
}

void
RecordedMask::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] Mask (Source: ";
  OutputSimplePatternInfo(mSource, aStringStream);
  aStringStream << " Mask: ";
  OutputSimplePatternInfo(mMask, aStringStream);
}

bool
RecordedStroke::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->Stroke(aTranslator->LookupPath(mPath), *GenericPattern(mPattern, aTranslator), mStrokeOptions, mOptions);
  return true;
}

void
RecordedStroke::RecordToStream(ostream &aStream) const
{
  RecordedDrawingEvent::RecordToStream(aStream);
  WriteElement(aStream, mPath);
  WriteElement(aStream, mOptions);
  RecordPatternData(aStream, mPattern);
  RecordStrokeOptions(aStream, mStrokeOptions);
}

RecordedStroke::RecordedStroke(istream &aStream)
  : RecordedDrawingEvent(STROKE, aStream)
{
  ReadElement(aStream, mPath);
  ReadElement(aStream, mOptions);
  ReadPatternData(aStream, mPattern);
  ReadStrokeOptions(aStream, mStrokeOptions);
}

void
RecordedStroke::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] Stroke ("<< mPath << ") LineWidth: " << mStrokeOptions.mLineWidth << "px ";
  OutputSimplePatternInfo(mPattern, aStringStream);
}

bool
RecordedClearRect::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->ClearRect(mRect);
  return true;
}

void
RecordedClearRect::RecordToStream(ostream &aStream) const
{
  RecordedDrawingEvent::RecordToStream(aStream);
  WriteElement(aStream, mRect);
}

RecordedClearRect::RecordedClearRect(istream &aStream)
  : RecordedDrawingEvent(CLEARRECT, aStream)
{
    ReadElement(aStream, mRect);
}

void
RecordedClearRect::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT<< "] ClearRect (" << mRect.x << ", " << mRect.y << " - " << mRect.width << " x " << mRect.height << ") ";
}

bool
RecordedCopySurface::PlayEvent(Translator *aTranslator) const
{
	aTranslator->LookupDrawTarget(mDT)->CopySurface(aTranslator->LookupSourceSurface(mSourceSurface),
                                                  mSourceRect, mDest);
  return true;
}

void
RecordedCopySurface::RecordToStream(ostream &aStream) const
{
  RecordedDrawingEvent::RecordToStream(aStream);
  WriteElement(aStream, mSourceSurface);
  WriteElement(aStream, mSourceRect);
  WriteElement(aStream, mDest);
}

RecordedCopySurface::RecordedCopySurface(istream &aStream)
  : RecordedDrawingEvent(COPYSURFACE, aStream)
{
  ReadElement(aStream, mSourceSurface);
  ReadElement(aStream, mSourceRect);
  ReadElement(aStream, mDest);
}

void
RecordedCopySurface::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT<< "] CopySurface (" << mSourceSurface << ")";
}

bool
RecordedPushClip::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->PushClip(aTranslator->LookupPath(mPath));
  return true;
}

void
RecordedPushClip::RecordToStream(ostream &aStream) const
{
  RecordedDrawingEvent::RecordToStream(aStream);
  WriteElement(aStream, mPath);
}

RecordedPushClip::RecordedPushClip(istream &aStream)
  : RecordedDrawingEvent(PUSHCLIP, aStream)
{
  ReadElement(aStream, mPath);
}

void
RecordedPushClip::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] PushClip (" << mPath << ") ";
}

bool
RecordedPushClipRect::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->PushClipRect(mRect);
  return true;
}

void
RecordedPushClipRect::RecordToStream(ostream &aStream) const
{
  RecordedDrawingEvent::RecordToStream(aStream);
  WriteElement(aStream, mRect);
}

RecordedPushClipRect::RecordedPushClipRect(istream &aStream)
  : RecordedDrawingEvent(PUSHCLIPRECT, aStream)
{
  ReadElement(aStream, mRect);
}

void
RecordedPushClipRect::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] PushClipRect (" << mRect.x << ", " << mRect.y << " - " << mRect.width << " x " << mRect.height << ") ";
}

bool
RecordedPopClip::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->PopClip();
  return true;
}

void
RecordedPopClip::RecordToStream(ostream &aStream) const
{
  RecordedDrawingEvent::RecordToStream(aStream);
}

RecordedPopClip::RecordedPopClip(istream &aStream)
  : RecordedDrawingEvent(POPCLIP, aStream)
{
}

void
RecordedPopClip::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] PopClip";
}

bool
RecordedPushLayer::PlayEvent(Translator *aTranslator) const
{
  SourceSurface* mask = mMask ? aTranslator->LookupSourceSurface(mMask)
                              : nullptr;
  aTranslator->LookupDrawTarget(mDT)->
    PushLayer(mOpaque, mOpacity, mask, mMaskTransform, mBounds, mCopyBackground);
  return true;
}

void
RecordedPushLayer::RecordToStream(ostream &aStream) const
{
  RecordedDrawingEvent::RecordToStream(aStream);
  WriteElement(aStream, mOpaque);
  WriteElement(aStream, mOpacity);
  WriteElement(aStream, mMask);
  WriteElement(aStream, mMaskTransform);
  WriteElement(aStream, mBounds);
  WriteElement(aStream, mCopyBackground);
}

RecordedPushLayer::RecordedPushLayer(istream &aStream)
  : RecordedDrawingEvent(PUSHLAYER, aStream)
{
  ReadElement(aStream, mOpaque);
  ReadElement(aStream, mOpacity);
  ReadElement(aStream, mMask);
  ReadElement(aStream, mMaskTransform);
  ReadElement(aStream, mBounds);
  ReadElement(aStream, mCopyBackground);
}

void
RecordedPushLayer::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] PushPLayer (Opaque=" << mOpaque <<
    ", Opacity=" << mOpacity << ", Mask Ref=" << mMask << ") ";
}

bool
RecordedPopLayer::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->PopLayer();
  return true;
}

void
RecordedPopLayer::RecordToStream(ostream &aStream) const
{
  RecordedDrawingEvent::RecordToStream(aStream);
}

RecordedPopLayer::RecordedPopLayer(istream &aStream)
  : RecordedDrawingEvent(POPLAYER, aStream)
{
}

void
RecordedPopLayer::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] PopLayer";
}

bool
RecordedSetTransform::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->SetTransform(mTransform);
  return true;
}

void
RecordedSetTransform::RecordToStream(ostream &aStream) const
{
  RecordedDrawingEvent::RecordToStream(aStream);
  WriteElement(aStream, mTransform);
}

RecordedSetTransform::RecordedSetTransform(istream &aStream)
  : RecordedDrawingEvent(SETTRANSFORM, aStream)
{
  ReadElement(aStream, mTransform);
}

void
RecordedSetTransform::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] SetTransform [ " << mTransform._11 << " " << mTransform._12 << " ; " <<
    mTransform._21 << " " << mTransform._22 << " ; " << mTransform._31 << " " << mTransform._32 << " ]";
}

bool
RecordedDrawSurface::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->
    DrawSurface(aTranslator->LookupSourceSurface(mRefSource), mDest, mSource,
                mDSOptions, mOptions);
  return true;
}

void
RecordedDrawSurface::RecordToStream(ostream &aStream) const
{
  RecordedDrawingEvent::RecordToStream(aStream);
  WriteElement(aStream, mRefSource);
  WriteElement(aStream, mDest);
  WriteElement(aStream, mSource);
  WriteElement(aStream, mDSOptions);
  WriteElement(aStream, mOptions);
}

RecordedDrawSurface::RecordedDrawSurface(istream &aStream)
  : RecordedDrawingEvent(DRAWSURFACE, aStream)
{
  ReadElement(aStream, mRefSource);
  ReadElement(aStream, mDest);
  ReadElement(aStream, mSource);
  ReadElement(aStream, mDSOptions);
  ReadElement(aStream, mOptions);
}

void
RecordedDrawSurface::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] DrawSurface (" << mRefSource << ")";
}

bool
RecordedDrawFilter::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->
    DrawFilter(aTranslator->LookupFilterNode(mNode), mSourceRect,
                mDestPoint, mOptions);
  return true;
}

void
RecordedDrawFilter::RecordToStream(ostream &aStream) const
{
  RecordedDrawingEvent::RecordToStream(aStream);
  WriteElement(aStream, mNode);
  WriteElement(aStream, mSourceRect);
  WriteElement(aStream, mDestPoint);
  WriteElement(aStream, mOptions);
}

RecordedDrawFilter::RecordedDrawFilter(istream &aStream)
  : RecordedDrawingEvent(DRAWFILTER, aStream)
{
  ReadElement(aStream, mNode);
  ReadElement(aStream, mSourceRect);
  ReadElement(aStream, mDestPoint);
  ReadElement(aStream, mOptions);
}

void
RecordedDrawFilter::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] DrawFilter (" << mNode << ")";
}

bool
RecordedDrawSurfaceWithShadow::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->
    DrawSurfaceWithShadow(aTranslator->LookupSourceSurface(mRefSource),
                          mDest, mColor, mOffset, mSigma, mOp);
  return true;
}

void
RecordedDrawSurfaceWithShadow::RecordToStream(ostream &aStream) const
{
  RecordedDrawingEvent::RecordToStream(aStream);
  WriteElement(aStream, mRefSource);
  WriteElement(aStream, mDest);
  WriteElement(aStream, mColor);
  WriteElement(aStream, mOffset);
  WriteElement(aStream, mSigma);
  WriteElement(aStream, mOp);
}

RecordedDrawSurfaceWithShadow::RecordedDrawSurfaceWithShadow(istream &aStream)
  : RecordedDrawingEvent(DRAWSURFACEWITHSHADOW, aStream)
{
  ReadElement(aStream, mRefSource);
  ReadElement(aStream, mDest);
  ReadElement(aStream, mColor);
  ReadElement(aStream, mOffset);
  ReadElement(aStream, mSigma);
  ReadElement(aStream, mOp);
}

void
RecordedDrawSurfaceWithShadow::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] DrawSurfaceWithShadow (" << mRefSource << ") Color: (" <<
    mColor.r << ", " << mColor.g << ", " << mColor.b << ", " << mColor.a << ")";
}

RecordedPathCreation::RecordedPathCreation(PathRecording *aPath)
  : RecordedEvent(PATHCREATION), mRefPtr(aPath), mFillRule(aPath->mFillRule), mPathOps(aPath->mPathOps)
{
}

RecordedPathCreation::~RecordedPathCreation()
{
}

bool
RecordedPathCreation::PlayEvent(Translator *aTranslator) const
{
  RefPtr<PathBuilder> builder = 
    aTranslator->GetReferenceDrawTarget()->CreatePathBuilder(mFillRule);

  for (size_t i = 0; i < mPathOps.size(); i++) {
    const PathOp &op = mPathOps[i];
    switch (op.mType) {
    case PathOp::OP_MOVETO:
      builder->MoveTo(op.mP1);
      break;
    case PathOp::OP_LINETO:
      builder->LineTo(op.mP1);
      break;
    case PathOp::OP_BEZIERTO:
      builder->BezierTo(op.mP1, op.mP2, op.mP3);
      break;
    case PathOp::OP_QUADRATICBEZIERTO:
      builder->QuadraticBezierTo(op.mP1, op.mP2);
      break;
    case PathOp::OP_CLOSE:
      builder->Close();
      break;
    }
  }

  RefPtr<Path> path = builder->Finish();
  aTranslator->AddPath(mRefPtr, path);
  return true;
}

void
RecordedPathCreation::RecordToStream(ostream &aStream) const
{
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, uint64_t(mPathOps.size()));
  WriteElement(aStream, mFillRule);
  typedef std::vector<PathOp> pathOpVec;
  for (pathOpVec::const_iterator iter = mPathOps.begin(); iter != mPathOps.end(); iter++) {
    WriteElement(aStream, iter->mType);
    if (sPointCount[iter->mType] >= 1) {
      WriteElement(aStream, iter->mP1);
    }
    if (sPointCount[iter->mType] >= 2) {
      WriteElement(aStream, iter->mP2);
    }
    if (sPointCount[iter->mType] >= 3) {
      WriteElement(aStream, iter->mP3);
    }
  }

}

RecordedPathCreation::RecordedPathCreation(istream &aStream)
  : RecordedEvent(PATHCREATION)
{
  uint64_t size;

  ReadElement(aStream, mRefPtr);
  ReadElement(aStream, size);
  ReadElement(aStream, mFillRule);

  for (uint64_t i = 0; i < size; i++) {
    PathOp newPathOp;
    ReadElement(aStream, newPathOp.mType);
    if (sPointCount[newPathOp.mType] >= 1) {
      ReadElement(aStream, newPathOp.mP1);
    }
    if (sPointCount[newPathOp.mType] >= 2) {
      ReadElement(aStream, newPathOp.mP2);
    }
    if (sPointCount[newPathOp.mType] >= 3) {
      ReadElement(aStream, newPathOp.mP3);
    }

    mPathOps.push_back(newPathOp);
  }

}

void
RecordedPathCreation::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] Path created (OpCount: " << mPathOps.size() << ")";
}
bool
RecordedPathDestruction::PlayEvent(Translator *aTranslator) const
{
  aTranslator->RemovePath(mRefPtr);
  return true;
}

void
RecordedPathDestruction::RecordToStream(ostream &aStream) const
{
  WriteElement(aStream, mRefPtr);
}

RecordedPathDestruction::RecordedPathDestruction(istream &aStream)
  : RecordedEvent(PATHDESTRUCTION)
{
  ReadElement(aStream, mRefPtr);
}

void
RecordedPathDestruction::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] Path Destroyed";
}

RecordedSourceSurfaceCreation::~RecordedSourceSurfaceCreation()
{
  if (mDataOwned) {
    delete [] mData;
  }
}

bool
RecordedSourceSurfaceCreation::PlayEvent(Translator *aTranslator) const
{
  RefPtr<SourceSurface> src = aTranslator->GetReferenceDrawTarget()->
    CreateSourceSurfaceFromData(mData, mSize, mSize.width * BytesPerPixel(mFormat), mFormat);
  aTranslator->AddSourceSurface(mRefPtr, src);
  return true;
}

void
RecordedSourceSurfaceCreation::RecordToStream(ostream &aStream) const
{
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mSize);
  WriteElement(aStream, mFormat);
  for (int y = 0; y < mSize.height; y++) {
    aStream.write((const char*)mData + y * mStride, BytesPerPixel(mFormat) * mSize.width);
  }
}

RecordedSourceSurfaceCreation::RecordedSourceSurfaceCreation(istream &aStream)
  : RecordedEvent(SOURCESURFACECREATION), mDataOwned(true)
{
  ReadElement(aStream, mRefPtr);
  ReadElement(aStream, mSize);
  ReadElement(aStream, mFormat);
  mData = (uint8_t*)new char[mSize.width * mSize.height * BytesPerPixel(mFormat)];
  aStream.read((char*)mData, mSize.width * mSize.height * BytesPerPixel(mFormat));
}

void
RecordedSourceSurfaceCreation::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] SourceSurface created (Size: " << mSize.width << "x" << mSize.height << ")";
}

bool
RecordedSourceSurfaceDestruction::PlayEvent(Translator *aTranslator) const
{
  aTranslator->RemoveSourceSurface(mRefPtr);
  return true;
}

void
RecordedSourceSurfaceDestruction::RecordToStream(ostream &aStream) const
{
  WriteElement(aStream, mRefPtr);
}

RecordedSourceSurfaceDestruction::RecordedSourceSurfaceDestruction(istream &aStream)
  : RecordedEvent(SOURCESURFACEDESTRUCTION)
{
  ReadElement(aStream, mRefPtr);
}

void
RecordedSourceSurfaceDestruction::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] SourceSurface Destroyed";
}

RecordedFilterNodeCreation::~RecordedFilterNodeCreation()
{
}

bool
RecordedFilterNodeCreation::PlayEvent(Translator *aTranslator) const
{
  RefPtr<FilterNode> node = aTranslator->GetReferenceDrawTarget()->
    CreateFilter(mType);
  aTranslator->AddFilterNode(mRefPtr, node);
  return true;
}

void
RecordedFilterNodeCreation::RecordToStream(ostream &aStream) const
{
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mType);
}

RecordedFilterNodeCreation::RecordedFilterNodeCreation(istream &aStream)
  : RecordedEvent(FILTERNODECREATION)
{
  ReadElement(aStream, mRefPtr);
  ReadElement(aStream, mType);
}

void
RecordedFilterNodeCreation::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] FilterNode created (Type: " << int(mType) << ")";
}

bool
RecordedFilterNodeDestruction::PlayEvent(Translator *aTranslator) const
{
  aTranslator->RemoveFilterNode(mRefPtr);
  return true;
}

void
RecordedFilterNodeDestruction::RecordToStream(ostream &aStream) const
{
  WriteElement(aStream, mRefPtr);
}

RecordedFilterNodeDestruction::RecordedFilterNodeDestruction(istream &aStream)
  : RecordedEvent(FILTERNODEDESTRUCTION)
{
  ReadElement(aStream, mRefPtr);
}

void
RecordedFilterNodeDestruction::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] FilterNode Destroyed";
}

RecordedGradientStopsCreation::~RecordedGradientStopsCreation()
{
  if (mDataOwned) {
    delete [] mStops;
  }
}

bool
RecordedGradientStopsCreation::PlayEvent(Translator *aTranslator) const
{
  RefPtr<GradientStops> src = aTranslator->GetReferenceDrawTarget()->
    CreateGradientStops(mStops, mNumStops, mExtendMode);
  aTranslator->AddGradientStops(mRefPtr, src);
  return true;
}

void
RecordedGradientStopsCreation::RecordToStream(ostream &aStream) const
{
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mExtendMode);
  WriteElement(aStream, mNumStops);
  aStream.write((const char*)mStops, mNumStops * sizeof(GradientStop));
}

RecordedGradientStopsCreation::RecordedGradientStopsCreation(istream &aStream)
  : RecordedEvent(GRADIENTSTOPSCREATION), mDataOwned(true)
{
  ReadElement(aStream, mRefPtr);
  ReadElement(aStream, mExtendMode);
  ReadElement(aStream, mNumStops);
  mStops = new GradientStop[mNumStops];

  aStream.read((char*)mStops, mNumStops * sizeof(GradientStop));
}

void
RecordedGradientStopsCreation::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] GradientStops created (Stops: " << mNumStops << ")";
}

bool
RecordedGradientStopsDestruction::PlayEvent(Translator *aTranslator) const
{
  aTranslator->RemoveGradientStops(mRefPtr);
  return true;
}

void
RecordedGradientStopsDestruction::RecordToStream(ostream &aStream) const
{
  WriteElement(aStream, mRefPtr);
}

RecordedGradientStopsDestruction::RecordedGradientStopsDestruction(istream &aStream)
  : RecordedEvent(GRADIENTSTOPSDESTRUCTION)
{
  ReadElement(aStream, mRefPtr);
}

void
RecordedGradientStopsDestruction::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] GradientStops Destroyed";
}

bool
RecordedSnapshot::PlayEvent(Translator *aTranslator) const
{
  RefPtr<SourceSurface> src = aTranslator->LookupDrawTarget(mDT)->Snapshot();
  aTranslator->AddSourceSurface(mRefPtr, src);
  return true;
}

void
RecordedSnapshot::RecordToStream(ostream &aStream) const
{
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mDT);
}

RecordedSnapshot::RecordedSnapshot(istream &aStream)
  : RecordedEvent(SNAPSHOT)
{
  ReadElement(aStream, mRefPtr);
  ReadElement(aStream, mDT);
}

void
RecordedSnapshot::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] Snapshot Created (DT: " << mDT << ")";
}

RecordedFontData::~RecordedFontData()
{
  delete[] mData;
  delete[] mVariations;
}

bool
RecordedFontData::PlayEvent(Translator *aTranslator) const
{
  RefPtr<NativeFontResource> fontResource =
    Factory::CreateNativeFontResource(mData, mFontDetails.size,
                                      mFontDetails.variationCount, mVariations,
                                      aTranslator->GetDesiredFontType());
  if (!fontResource) {
    return false;
  }

  aTranslator->AddNativeFontResource(mFontDetails.fontDataKey, fontResource);
  return true;
}

void
RecordedFontData::RecordToStream(std::ostream &aStream) const
{
  MOZ_ASSERT(mGetFontFileDataSucceeded);

  WriteElement(aStream, mFontDetails.fontDataKey);
  WriteElement(aStream, mFontDetails.size);
  aStream.write((const char*)mData, mFontDetails.size);
  WriteElement(aStream, mFontDetails.variationCount);
  aStream.write((const char*)mVariations, mFontDetails.variationCount * sizeof(ScaledFont::VariationSetting));
}

void
RecordedFontData::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "Font Data of size " << mFontDetails.size;
}

void
RecordedFontData::SetFontData(const uint8_t *aData, uint32_t aSize, uint32_t aIndex,
                              Float aGlyphSize, uint32_t aVariationCount,
                              const ScaledFont::VariationSetting* aVariations)
{
  mData = new uint8_t[aSize];
  memcpy(mData, aData, aSize);
  uint32_t varDataSize = aVariationCount * sizeof(ScaledFont::VariationSetting);
  mFontDetails.fontDataKey =
    SFNTData::GetUniqueKey(aData, aSize, varDataSize, aVariations);
  mFontDetails.size = aSize;
  mFontDetails.index = aIndex;
  mFontDetails.glyphSize = aGlyphSize;
  mFontDetails.variationCount = aVariationCount;
  if (aVariationCount > 0) {
    mVariations = new ScaledFont::VariationSetting[aVariationCount];
    memcpy(mVariations, aVariations, varDataSize);
  }
}

bool
RecordedFontData::GetFontDetails(RecordedFontDetails& fontDetails)
{
  if (!mGetFontFileDataSucceeded) {
    return false;
  }

  fontDetails.fontDataKey = mFontDetails.fontDataKey;
  fontDetails.size = mFontDetails.size;
  fontDetails.glyphSize = mFontDetails.glyphSize;
  fontDetails.index = mFontDetails.index;
  fontDetails.variationCount = mFontDetails.variationCount;
  return true;
}

RecordedFontData::RecordedFontData(istream &aStream)
  : RecordedEvent(FONTDATA)
  , mData(nullptr)
  , mVariations(nullptr)
{
  ReadElement(aStream, mFontDetails.fontDataKey);
  ReadElement(aStream, mFontDetails.size);
  mData = new uint8_t[mFontDetails.size];
  aStream.read((char*)mData, mFontDetails.size);
  ReadElement(aStream, mFontDetails.variationCount);
  if (mFontDetails.variationCount > 0) {
    mVariations = new ScaledFont::VariationSetting[mFontDetails.variationCount];
    aStream.read((char*)mVariations, mFontDetails.variationCount * sizeof(ScaledFont::VariationSetting));
  } else {
    mVariations = nullptr;
  }
}

RecordedFontDescriptor::~RecordedFontDescriptor()
{
}

bool
RecordedFontDescriptor::PlayEvent(Translator *aTranslator) const
{
  RefPtr<ScaledFont> font =
    Factory::CreateScaledFontFromFontDescriptor(mType, mData.data(), mData.size(), mFontSize);
  if (!font) {
    gfxDevCrash(LogReason::InvalidFont) <<
      "Failed creating ScaledFont of type " << int(mType) << " from font descriptor";
    return false;
  }

  aTranslator->AddScaledFont(mRefPtr, font);
  return true;
}

void
RecordedFontDescriptor::RecordToStream(std::ostream &aStream) const
{
  MOZ_ASSERT(mHasDesc);
  WriteElement(aStream, mType);
  WriteElement(aStream, mFontSize);
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, (size_t)mData.size());
  aStream.write((char*)mData.data(), mData.size());
}

void
RecordedFontDescriptor::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] Font Descriptor";
}

void
RecordedFontDescriptor::SetFontDescriptor(const uint8_t* aData, uint32_t aSize, Float aFontSize)
{
  mData.assign(aData, aData + aSize);
  mFontSize = aFontSize;
}

RecordedFontDescriptor::RecordedFontDescriptor(istream &aStream)
  : RecordedEvent(FONTDATA)
{
  ReadElement(aStream, mType);
  ReadElement(aStream, mFontSize);
  ReadElement(aStream, mRefPtr);

  size_t size;
  ReadElement(aStream, size);
  mData.resize(size);
  aStream.read((char*)mData.data(), size);
}

bool
RecordedScaledFontCreation::PlayEvent(Translator *aTranslator) const
{
  NativeFontResource *fontResource = aTranslator->LookupNativeFontResource(mFontDataKey);
  if (!fontResource) {
    gfxDevCrash(LogReason::NativeFontResourceNotFound) <<
      "NativeFontResource lookup failed for key |" << hexa(mFontDataKey) << "|.";
    return false;
  }

  RefPtr<ScaledFont> scaledFont =
    fontResource->CreateScaledFont(mIndex, mGlyphSize, mInstanceData.data(), mInstanceData.size());
  aTranslator->AddScaledFont(mRefPtr, scaledFont);
  return true;
}

void
RecordedScaledFontCreation::RecordToStream(std::ostream &aStream) const
{
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mFontDataKey);
  WriteElement(aStream, mIndex);
  WriteElement(aStream, mGlyphSize);
  WriteElement(aStream, (size_t)mInstanceData.size());
  aStream.write((char*)mInstanceData.data(), mInstanceData.size());
}

void
RecordedScaledFontCreation::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] ScaledFont Created";
}

void
RecordedScaledFontCreation::SetFontInstanceData(const uint8_t *aData, uint32_t aSize)
{
  mInstanceData.assign(aData, aData + aSize);
}

RecordedScaledFontCreation::RecordedScaledFontCreation(istream &aStream)
  : RecordedEvent(SCALEDFONTCREATION)
{
  ReadElement(aStream, mRefPtr);
  ReadElement(aStream, mFontDataKey);
  ReadElement(aStream, mIndex);
  ReadElement(aStream, mGlyphSize);

  size_t size;
  ReadElement(aStream, size);
  mInstanceData.resize(size);
  aStream.read((char*)mInstanceData.data(), size);
}

bool
RecordedScaledFontDestruction::PlayEvent(Translator *aTranslator) const
{
  aTranslator->RemoveScaledFont(mRefPtr);
  return true;
}

void
RecordedScaledFontDestruction::RecordToStream(ostream &aStream) const
{
  WriteElement(aStream, mRefPtr);
}

RecordedScaledFontDestruction::RecordedScaledFontDestruction(istream &aStream)
  : RecordedEvent(SCALEDFONTDESTRUCTION)
{
  ReadElement(aStream, mRefPtr);
}

void
RecordedScaledFontDestruction::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] ScaledFont Destroyed";
}

bool
RecordedMaskSurface::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->
    MaskSurface(*GenericPattern(mPattern, aTranslator),
                aTranslator->LookupSourceSurface(mRefMask),
                mOffset, mOptions);
  return true;
}

void
RecordedMaskSurface::RecordToStream(ostream &aStream) const
{
  RecordedDrawingEvent::RecordToStream(aStream);
  RecordPatternData(aStream, mPattern);
  WriteElement(aStream, mRefMask);
  WriteElement(aStream, mOffset);
  WriteElement(aStream, mOptions);
}

RecordedMaskSurface::RecordedMaskSurface(istream &aStream)
  : RecordedDrawingEvent(MASKSURFACE, aStream)
{
  ReadPatternData(aStream, mPattern);
  ReadElement(aStream, mRefMask);
  ReadElement(aStream, mOffset);
  ReadElement(aStream, mOptions);
}

void
RecordedMaskSurface::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] MaskSurface (" << mRefMask << ")  Offset: (" << mOffset.x << "x" << mOffset.y << ") Pattern: ";
  OutputSimplePatternInfo(mPattern, aStringStream);
}

template<typename T>
void
ReplaySetAttribute(FilterNode *aNode, uint32_t aIndex, T aValue)
{
  aNode->SetAttribute(aIndex, aValue);
}

bool
RecordedFilterNodeSetAttribute::PlayEvent(Translator *aTranslator) const
{
#define REPLAY_SET_ATTRIBUTE(type, argtype) \
  case ARGTYPE_##argtype: \
  ReplaySetAttribute(aTranslator->LookupFilterNode(mNode), mIndex, *(type*)&mPayload.front()); \
  break

  switch (mArgType) {
    REPLAY_SET_ATTRIBUTE(bool, BOOL);
    REPLAY_SET_ATTRIBUTE(uint32_t, UINT32);
    REPLAY_SET_ATTRIBUTE(Float, FLOAT);
    REPLAY_SET_ATTRIBUTE(Size, SIZE);
    REPLAY_SET_ATTRIBUTE(IntSize, INTSIZE);
    REPLAY_SET_ATTRIBUTE(IntPoint, INTPOINT);
    REPLAY_SET_ATTRIBUTE(Rect, RECT);
    REPLAY_SET_ATTRIBUTE(IntRect, INTRECT);
    REPLAY_SET_ATTRIBUTE(Point, POINT);
    REPLAY_SET_ATTRIBUTE(Matrix, MATRIX);
    REPLAY_SET_ATTRIBUTE(Matrix5x4, MATRIX5X4);
    REPLAY_SET_ATTRIBUTE(Point3D, POINT3D);
    REPLAY_SET_ATTRIBUTE(Color, COLOR);
  case ARGTYPE_FLOAT_ARRAY:
    aTranslator->LookupFilterNode(mNode)->SetAttribute(
      mIndex,
      reinterpret_cast<const Float*>(&mPayload.front()),
      mPayload.size() / sizeof(Float));
    break;
  }

  return true;
}

void
RecordedFilterNodeSetAttribute::RecordToStream(ostream &aStream) const
{
  RecordedEvent::RecordToStream(aStream);
  WriteElement(aStream, mNode);
  WriteElement(aStream, mIndex);
  WriteElement(aStream, mArgType);
  WriteElement(aStream, uint64_t(mPayload.size()));
  aStream.write((const char*)&mPayload.front(), mPayload.size());
}

RecordedFilterNodeSetAttribute::RecordedFilterNodeSetAttribute(istream &aStream)
  : RecordedEvent(FILTERNODESETATTRIBUTE)
{
  ReadElement(aStream, mNode);
  ReadElement(aStream, mIndex);
  ReadElement(aStream, mArgType);
  uint64_t size;
  ReadElement(aStream, size);
  mPayload.resize(size_t(size));
  aStream.read((char*)&mPayload.front(), size);
}

void
RecordedFilterNodeSetAttribute::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mNode << "] SetAttribute (" << mIndex << ")";
}

bool
RecordedFilterNodeSetInput::PlayEvent(Translator *aTranslator) const
{
  if (mInputFilter) {
    aTranslator->LookupFilterNode(mNode)->SetInput(
      mIndex, aTranslator->LookupFilterNode(mInputFilter));
  } else {
    aTranslator->LookupFilterNode(mNode)->SetInput(
      mIndex, aTranslator->LookupSourceSurface(mInputSurface));
  }

  return true;
}

void
RecordedFilterNodeSetInput::RecordToStream(ostream &aStream) const
{
  RecordedEvent::RecordToStream(aStream);
  WriteElement(aStream, mNode);
  WriteElement(aStream, mIndex);
  WriteElement(aStream, mInputFilter);
  WriteElement(aStream, mInputSurface);
}

RecordedFilterNodeSetInput::RecordedFilterNodeSetInput(istream &aStream)
  : RecordedEvent(FILTERNODESETINPUT)
{
  ReadElement(aStream, mNode);
  ReadElement(aStream, mIndex);
  ReadElement(aStream, mInputFilter);
  ReadElement(aStream, mInputSurface);
}

void
RecordedFilterNodeSetInput::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mNode << "] SetAttribute (" << mIndex << ", ";

  if (mInputFilter) {
    aStringStream << "Filter: " << mInputFilter;
  } else {
    aStringStream << "Surface: " << mInputSurface;
  }

  aStringStream << ")";
}

} // namespace gfx
} // namespace mozilla
