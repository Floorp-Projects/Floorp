/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RECORDEDEVENT_H_
#define MOZILLA_GFX_RECORDEDEVENT_H_

#include "2D.h"
#include <ostream>
#include <sstream>
#include <cstring>
#include "RecordingTypes.h"
#include "PathRecording.h"

namespace mozilla {
namespace gfx {

// A change in major revision means a change in event binary format, causing
// loss of backwards compatibility. Old streams will not work in a player
// using a newer major revision. And new streams will not work in a player
// using an older major revision.
const uint16_t kMajorRevision = 2;
// A change in minor revision means additions of new events. New streams will
// not play in older players.
const uint16_t kMinorRevision = 1;

struct ReferencePtr
{
  ReferencePtr()
    : mLongPtr(0)
  {}

  ReferencePtr(const void* aLongPtr)
    : mLongPtr(uint64_t(aLongPtr))
  {}

  template <typename T>
  ReferencePtr(const RefPtr<T>& aPtr)
    : mLongPtr(uint64_t(aPtr.get()))
  {}

  ReferencePtr &operator =(const void* aLongPtr) {
    mLongPtr = uint64_t(aLongPtr);
    return *this;
  }

  template <typename T>
  ReferencePtr &operator =(const RefPtr<T>& aPtr) {
    mLongPtr = uint64_t(aPtr.get());
    return *this;
  }

  operator void*() const {
    return (void*)mLongPtr;
  }

  uint64_t mLongPtr;
};

// Used by the Azure drawing debugger (player2d)
inline std::string StringFromPtr(ReferencePtr aPtr)
{
  std::stringstream stream;
  stream << aPtr;
  return stream.str();
}

class Translator
{
public:
  virtual DrawTarget *LookupDrawTarget(ReferencePtr aRefPtr) = 0;
  virtual Path *LookupPath(ReferencePtr aRefPtr) = 0;
  virtual SourceSurface *LookupSourceSurface(ReferencePtr aRefPtr) = 0;
  virtual GradientStops *LookupGradientStops(ReferencePtr aRefPtr) = 0;
  virtual ScaledFont *LookupScaledFont(ReferencePtr aRefPtr) = 0;
  virtual void AddDrawTarget(ReferencePtr aRefPtr, DrawTarget *aDT) = 0;
  virtual void RemoveDrawTarget(ReferencePtr aRefPtr) = 0;
  virtual void AddPath(ReferencePtr aRefPtr, Path *aPath) = 0;
  virtual void RemovePath(ReferencePtr aRefPtr) = 0;
  virtual void AddSourceSurface(ReferencePtr aRefPtr, SourceSurface *aPath) = 0;
  virtual void RemoveSourceSurface(ReferencePtr aRefPtr) = 0;
  virtual void AddGradientStops(ReferencePtr aRefPtr, GradientStops *aPath) = 0;
  virtual void RemoveGradientStops(ReferencePtr aRefPtr) = 0;
  virtual void AddScaledFont(ReferencePtr aRefPtr, ScaledFont *aScaledFont) = 0;
  virtual void RemoveScaledFont(ReferencePtr aRefPtr) = 0;

  virtual DrawTarget *GetReferenceDrawTarget() = 0;
  virtual FontType GetDesiredFontType() = 0;
};

struct ColorPatternStorage
{
  Color mColor;
};

struct LinearGradientPatternStorage
{
  Point mBegin;
  Point mEnd;
  ReferencePtr mStops;
  Matrix mMatrix;
};

struct RadialGradientPatternStorage
{
  Point mCenter1;
  Point mCenter2;
  Float mRadius1;
  Float mRadius2;
  ReferencePtr mStops;
  Matrix mMatrix;
};

struct SurfacePatternStorage
{
  ExtendMode mExtend;
  Filter mFilter;
  ReferencePtr mSurface;
  Matrix mMatrix;
};

struct PatternStorage
{
  PatternType mType;
  union {
    char *mStorage;
    char mColor[sizeof(ColorPatternStorage)];
    char mLinear[sizeof(LinearGradientPatternStorage)];
    char mRadial[sizeof(RadialGradientPatternStorage)];
    char mSurface[sizeof(SurfacePatternStorage)];
  };
};

class RecordedEvent {
public:
  enum EventType {
    DRAWTARGETCREATION = 0,
    DRAWTARGETDESTRUCTION,
    FILLRECT,
    STROKERECT,
    STROKELINE,
    CLEARRECT,
    COPYSURFACE,
    SETTRANSFORM,
    PUSHCLIP,
    PUSHCLIPRECT,
    POPCLIP,
    FILL,
    FILLGLYPHS,
    MASK,
    STROKE,
    DRAWSURFACE,
    DRAWSURFACEWITHSHADOW,
    PATHCREATION,
    PATHDESTRUCTION,
    SOURCESURFACECREATION,
    SOURCESURFACEDESTRUCTION,
    GRADIENTSTOPSCREATION,
    GRADIENTSTOPSDESTRUCTION,
    SNAPSHOT,
    SCALEDFONTCREATION,
    SCALEDFONTDESTRUCTION,
    MASKSURFACE
  };

  virtual void PlayEvent(Translator *aTranslator) const {}

  virtual void RecordToStream(std::ostream &aStream) const {}

  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const { }

  void RecordPatternData(std::ostream &aStream, const PatternStorage &aPatternStorage) const;
  void ReadPatternData(std::istream &aStream, PatternStorage &aPatternStorage) const;
  void StorePattern(PatternStorage &aDestination, const Pattern &aSource) const;
  void RecordStrokeOptions(std::ostream &aStream, const StrokeOptions &aStrokeOptions) const;
  void ReadStrokeOptions(std::istream &aStream, StrokeOptions &aStrokeOptions);

  virtual std::string GetName() const = 0;

  virtual ReferencePtr GetObject() const = 0;

  virtual ReferencePtr GetDestinedDT() { return nullptr; }

  void OutputSimplePatternInfo(const PatternStorage &aStorage, std::stringstream &aOutput) const;

  static RecordedEvent *LoadEventFromStream(std::istream &aStream, EventType aType);

  EventType GetType() { return (EventType)mType; }
protected:
  friend class DrawEventRecorderPrivate;

  RecordedEvent(int32_t aType) : mType(aType)
  {}

  int32_t mType;
  std::vector<Float> mDashPatternStorage;
};

class RecordedDrawingEvent : public RecordedEvent
{
public:
   virtual ReferencePtr GetDestinedDT() { return mDT; }

protected:
  RecordedDrawingEvent(EventType aType, DrawTarget *aTarget)
    : RecordedEvent(aType), mDT(aTarget)
  {
  }

  RecordedDrawingEvent(EventType aType, std::istream &aStream);
  virtual void RecordToStream(std::ostream &aStream) const;

  virtual ReferencePtr GetObject() const;

  ReferencePtr mDT;
};

class RecordedDrawTargetCreation : public RecordedEvent {
public:
  RecordedDrawTargetCreation(ReferencePtr aRefPtr, BackendType aType, const IntSize &aSize, SurfaceFormat aFormat)
    : RecordedEvent(DRAWTARGETCREATION), mRefPtr(aRefPtr), mBackendType(aType), mSize(aSize), mFormat(aFormat)
  {}

  virtual void PlayEvent(Translator *aTranslator) const;

  virtual void RecordToStream(std::ostream &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "DrawTarget Creation"; }
  virtual ReferencePtr GetObject() const { return mRefPtr; }

  ReferencePtr mRefPtr;
  BackendType mBackendType;
  IntSize mSize;
  SurfaceFormat mFormat;
  
private:
  friend class RecordedEvent;

  RecordedDrawTargetCreation(std::istream &aStream);
};

class RecordedDrawTargetDestruction : public RecordedEvent {
public:
  RecordedDrawTargetDestruction(ReferencePtr aRefPtr)
    : RecordedEvent(DRAWTARGETDESTRUCTION), mRefPtr(aRefPtr)
  {}

  virtual void PlayEvent(Translator *aTranslator) const;

  virtual void RecordToStream(std::ostream &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "DrawTarget Destruction"; }
  virtual ReferencePtr GetObject() const { return mRefPtr; }

  ReferencePtr mRefPtr;

  BackendType mBackendType;
private:
  friend class RecordedEvent;

  RecordedDrawTargetDestruction(std::istream &aStream);
};

class RecordedFillRect : public RecordedDrawingEvent {
public:
  RecordedFillRect(DrawTarget *aDT, const Rect &aRect, const Pattern &aPattern, const DrawOptions &aOptions)
    : RecordedDrawingEvent(FILLRECT, aDT), mRect(aRect), mOptions(aOptions)
  {
    StorePattern(mPattern, aPattern);
  }

  virtual void PlayEvent(Translator *aTranslator) const;

  virtual void RecordToStream(std::ostream &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "FillRect"; }
private:
  friend class RecordedEvent;

  RecordedFillRect(std::istream &aStream);

  Rect mRect;
  PatternStorage mPattern;
  DrawOptions mOptions;
};

class RecordedStrokeRect : public RecordedDrawingEvent {
public:
  RecordedStrokeRect(DrawTarget *aDT, const Rect &aRect, const Pattern &aPattern,
                     const StrokeOptions &aStrokeOptions, const DrawOptions &aOptions)
    : RecordedDrawingEvent(STROKERECT, aDT), mRect(aRect),
      mStrokeOptions(aStrokeOptions), mOptions(aOptions)
  {
    StorePattern(mPattern, aPattern);
  }

  virtual void PlayEvent(Translator *aTranslator) const;

  virtual void RecordToStream(std::ostream &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "StrokeRect"; }
private:
  friend class RecordedEvent;

  RecordedStrokeRect(std::istream &aStream);

  Rect mRect;
  PatternStorage mPattern;
  StrokeOptions mStrokeOptions;
  DrawOptions mOptions;
};

class RecordedStrokeLine : public RecordedDrawingEvent {
public:
  RecordedStrokeLine(DrawTarget *aDT, const Point &aBegin, const Point &aEnd,
                     const Pattern &aPattern, const StrokeOptions &aStrokeOptions,
                     const DrawOptions &aOptions)
    : RecordedDrawingEvent(STROKELINE, aDT), mBegin(aBegin), mEnd(aEnd),
      mStrokeOptions(aStrokeOptions), mOptions(aOptions)
  {
    StorePattern(mPattern, aPattern);
  }

  virtual void PlayEvent(Translator *aTranslator) const;

  virtual void RecordToStream(std::ostream &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "StrokeLine"; }
private:
  friend class RecordedEvent;

  RecordedStrokeLine(std::istream &aStream);

  Point mBegin;
  Point mEnd;
  PatternStorage mPattern;
  StrokeOptions mStrokeOptions;
  DrawOptions mOptions;
};

class RecordedFill : public RecordedDrawingEvent {
public:
  RecordedFill(DrawTarget *aDT, ReferencePtr aPath, const Pattern &aPattern, const DrawOptions &aOptions)
    : RecordedDrawingEvent(FILL, aDT), mPath(aPath), mOptions(aOptions)
  {
    StorePattern(mPattern, aPattern);
  }

  virtual void PlayEvent(Translator *aTranslator) const;

  virtual void RecordToStream(std::ostream &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "Fill"; }
private:
  friend class RecordedEvent;

  RecordedFill(std::istream &aStream);

  ReferencePtr mPath;
  PatternStorage mPattern;
  DrawOptions mOptions;
};

class RecordedFillGlyphs : public RecordedDrawingEvent {
public:
  RecordedFillGlyphs(DrawTarget *aDT, ReferencePtr aScaledFont, const Pattern &aPattern, const DrawOptions &aOptions,
                     const Glyph *aGlyphs, uint32_t aNumGlyphs)
    : RecordedDrawingEvent(FILLGLYPHS, aDT), mScaledFont(aScaledFont), mOptions(aOptions)
  {
    StorePattern(mPattern, aPattern);
    mNumGlyphs = aNumGlyphs;
    mGlyphs = new Glyph[aNumGlyphs];
    memcpy(mGlyphs, aGlyphs, sizeof(Glyph) * aNumGlyphs);
  }
  virtual ~RecordedFillGlyphs();

  virtual void PlayEvent(Translator *aTranslator) const;

  virtual void RecordToStream(std::ostream &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "FillGlyphs"; }
private:
  friend class RecordedEvent;

  RecordedFillGlyphs(std::istream &aStream);

  ReferencePtr mScaledFont;
  PatternStorage mPattern;
  DrawOptions mOptions;
  Glyph *mGlyphs;
  uint32_t mNumGlyphs;
};

class RecordedMask : public RecordedDrawingEvent {
public:
  RecordedMask(DrawTarget *aDT, const Pattern &aSource, const Pattern &aMask, const DrawOptions &aOptions)
    : RecordedDrawingEvent(MASK, aDT), mOptions(aOptions)
  {
    StorePattern(mSource, aSource);
    StorePattern(mMask, aMask);
  }

  virtual void PlayEvent(Translator *aTranslator) const;

  virtual void RecordToStream(std::ostream &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "Mask"; }
private:
  friend class RecordedEvent;

  RecordedMask(std::istream &aStream);

  PatternStorage mSource;
  PatternStorage mMask;
  DrawOptions mOptions;
};

class RecordedStroke : public RecordedDrawingEvent {
public:
  RecordedStroke(DrawTarget *aDT, ReferencePtr aPath, const Pattern &aPattern,
                     const StrokeOptions &aStrokeOptions, const DrawOptions &aOptions)
    : RecordedDrawingEvent(STROKE, aDT), mPath(aPath),
      mStrokeOptions(aStrokeOptions), mOptions(aOptions)
  {
    StorePattern(mPattern, aPattern);
  }

  virtual void PlayEvent(Translator *aTranslator) const;

  virtual void RecordToStream(std::ostream &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "Stroke"; }
private:
  friend class RecordedEvent;

  RecordedStroke(std::istream &aStream);

  ReferencePtr mPath;
  PatternStorage mPattern;
  StrokeOptions mStrokeOptions;
  DrawOptions mOptions;
};

class RecordedClearRect : public RecordedDrawingEvent {
public:
  RecordedClearRect(DrawTarget *aDT, const Rect &aRect)
    : RecordedDrawingEvent(CLEARRECT, aDT), mRect(aRect)
  {
  }

  virtual void PlayEvent(Translator *aTranslator) const;

  virtual void RecordToStream(std::ostream &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "ClearRect"; }
private:
  friend class RecordedEvent;

  RecordedClearRect(std::istream &aStream);

  Rect mRect;
};

class RecordedCopySurface : public RecordedDrawingEvent {
public:
  RecordedCopySurface(DrawTarget *aDT, ReferencePtr aSourceSurface,
                      const IntRect &aSourceRect, const IntPoint &aDest)
    : RecordedDrawingEvent(COPYSURFACE, aDT), mSourceSurface(aSourceSurface),
	  mSourceRect(aSourceRect), mDest(aDest)
  {
  }

  virtual void PlayEvent(Translator *aTranslator) const;

  virtual void RecordToStream(std::ostream &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "CopySurface"; }
private:
  friend class RecordedEvent;

  RecordedCopySurface(std::istream &aStream);

  ReferencePtr mSourceSurface;
  IntRect mSourceRect;
  IntPoint mDest;
};

class RecordedPushClip : public RecordedDrawingEvent {
public:
  RecordedPushClip(DrawTarget *aDT, ReferencePtr aPath)
    : RecordedDrawingEvent(PUSHCLIP, aDT), mPath(aPath)
  {
  }

  virtual void PlayEvent(Translator *aTranslator) const;

  virtual void RecordToStream(std::ostream &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "PushClip"; }
private:
  friend class RecordedEvent;

  RecordedPushClip(std::istream &aStream);

  ReferencePtr mPath;
};

class RecordedPushClipRect : public RecordedDrawingEvent {
public:
  RecordedPushClipRect(DrawTarget *aDT, const Rect &aRect)
    : RecordedDrawingEvent(PUSHCLIPRECT, aDT), mRect(aRect)
  {
  }

  virtual void PlayEvent(Translator *aTranslator) const;

  virtual void RecordToStream(std::ostream &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "PushClipRect"; }
private:
  friend class RecordedEvent;

  RecordedPushClipRect(std::istream &aStream);

  Rect mRect;
};

class RecordedPopClip : public RecordedDrawingEvent {
public:
  RecordedPopClip(DrawTarget *aDT)
    : RecordedDrawingEvent(POPCLIP, aDT)
  {}

  virtual void PlayEvent(Translator *aTranslator) const;

  virtual void RecordToStream(std::ostream &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "PopClip"; }
private:
  friend class RecordedEvent;

  RecordedPopClip(std::istream &aStream);
};

class RecordedSetTransform : public RecordedDrawingEvent {
public:
  RecordedSetTransform(DrawTarget *aDT, const Matrix &aTransform)
    : RecordedDrawingEvent(SETTRANSFORM, aDT), mTransform(aTransform)
  {
  }

  virtual void PlayEvent(Translator *aTranslator) const;

  virtual void RecordToStream(std::ostream &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;
  
  virtual std::string GetName() const { return "SetTransform"; }
private:
  friend class RecordedEvent;

  RecordedSetTransform(std::istream &aStream);

  Matrix mTransform;
};

class RecordedDrawSurface : public RecordedDrawingEvent {
public:
  RecordedDrawSurface(DrawTarget *aDT, ReferencePtr aRefSource, const Rect &aDest,
                      const Rect &aSource, const DrawSurfaceOptions &aDSOptions,
                      const DrawOptions &aOptions)
    : RecordedDrawingEvent(DRAWSURFACE, aDT), mRefSource(aRefSource), mDest(aDest)
    , mSource(aSource), mDSOptions(aDSOptions), mOptions(aOptions)
  {
  }

  virtual void PlayEvent(Translator *aTranslator) const;

  virtual void RecordToStream(std::ostream &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;
  
  virtual std::string GetName() const { return "DrawSurface"; }
private:
  friend class RecordedEvent;

  RecordedDrawSurface(std::istream &aStream);

  ReferencePtr mRefSource;
  Rect mDest;
  Rect mSource;
  DrawSurfaceOptions mDSOptions;
  DrawOptions mOptions;
};

class RecordedDrawSurfaceWithShadow : public RecordedDrawingEvent {
public:
  RecordedDrawSurfaceWithShadow(DrawTarget *aDT, ReferencePtr aRefSource, const Point &aDest,
                                const Color &aColor, const Point &aOffset,
                                Float aSigma, CompositionOp aOp)
    : RecordedDrawingEvent(DRAWSURFACEWITHSHADOW, aDT), mRefSource(aRefSource), mDest(aDest)
    , mColor(aColor), mOffset(aOffset), mSigma(aSigma), mOp(aOp)
  {
  }

  virtual void PlayEvent(Translator *aTranslator) const;

  virtual void RecordToStream(std::ostream &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;
  
  virtual std::string GetName() const { return "DrawSurfaceWithShadow"; }
private:
  friend class RecordedEvent;

  RecordedDrawSurfaceWithShadow(std::istream &aStream);

  ReferencePtr mRefSource;
  Point mDest;
  Color mColor;
  Point mOffset;
  Float mSigma;
  CompositionOp mOp;
};

class RecordedPathCreation : public RecordedEvent {
public:
  RecordedPathCreation(PathRecording *aPath);
  ~RecordedPathCreation();
  
  virtual void PlayEvent(Translator *aTranslator) const;

  virtual void RecordToStream(std::ostream &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;
  
  virtual std::string GetName() const { return "Path Creation"; }
  virtual ReferencePtr GetObject() const { return mRefPtr; }
private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;
  FillRule mFillRule;
  std::vector<PathOp> mPathOps;

  RecordedPathCreation(std::istream &aStream);
};

class RecordedPathDestruction : public RecordedEvent {
public:
  RecordedPathDestruction(PathRecording *aPath)
    : RecordedEvent(PATHDESTRUCTION), mRefPtr(aPath)
  {
  }
  
  virtual void PlayEvent(Translator *aTranslator) const;

  virtual void RecordToStream(std::ostream &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;
  
  virtual std::string GetName() const { return "Path Destruction"; }
  virtual ReferencePtr GetObject() const { return mRefPtr; }
private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;

  RecordedPathDestruction(std::istream &aStream);
};

class RecordedSourceSurfaceCreation : public RecordedEvent {
public:
  RecordedSourceSurfaceCreation(ReferencePtr aRefPtr, uint8_t *aData, int32_t aStride,
                                const IntSize &aSize, SurfaceFormat aFormat)
    : RecordedEvent(SOURCESURFACECREATION), mRefPtr(aRefPtr), mData(aData)
    , mStride(aStride), mSize(aSize), mFormat(aFormat), mDataOwned(false)
  {
  }

  ~RecordedSourceSurfaceCreation();

  virtual void PlayEvent(Translator *aTranslator) const;

  virtual void RecordToStream(std::ostream &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;
  
  virtual std::string GetName() const { return "SourceSurface Creation"; }
  virtual ReferencePtr GetObject() const { return mRefPtr; }
private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;
  uint8_t *mData;
  int32_t mStride;
  IntSize mSize;
  SurfaceFormat mFormat;
  bool mDataOwned;

  RecordedSourceSurfaceCreation(std::istream &aStream);
};

class RecordedSourceSurfaceDestruction : public RecordedEvent {
public:
  RecordedSourceSurfaceDestruction(ReferencePtr aRefPtr)
    : RecordedEvent(SOURCESURFACEDESTRUCTION), mRefPtr(aRefPtr)
  {
  }

  virtual void PlayEvent(Translator *aTranslator) const;

  virtual void RecordToStream(std::ostream &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;
  
  virtual std::string GetName() const { return "SourceSurface Destruction"; }
  virtual ReferencePtr GetObject() const { return mRefPtr; }
private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;

  RecordedSourceSurfaceDestruction(std::istream &aStream);
};

class RecordedGradientStopsCreation : public RecordedEvent {
public:
  RecordedGradientStopsCreation(ReferencePtr aRefPtr, GradientStop *aStops,
                                uint32_t aNumStops, ExtendMode aExtendMode)
    : RecordedEvent(GRADIENTSTOPSCREATION), mRefPtr(aRefPtr), mStops(aStops)
    , mNumStops(aNumStops), mExtendMode(aExtendMode), mDataOwned(false)
  {
  }

  ~RecordedGradientStopsCreation();

  virtual void PlayEvent(Translator *aTranslator) const;

  virtual void RecordToStream(std::ostream &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;
  
  virtual std::string GetName() const { return "GradientStops Creation"; }
  virtual ReferencePtr GetObject() const { return mRefPtr; }
private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;
  GradientStop *mStops;
  uint32_t mNumStops;
  ExtendMode mExtendMode;
  bool mDataOwned;

  RecordedGradientStopsCreation(std::istream &aStream);
};

class RecordedGradientStopsDestruction : public RecordedEvent {
public:
  RecordedGradientStopsDestruction(ReferencePtr aRefPtr)
    : RecordedEvent(GRADIENTSTOPSDESTRUCTION), mRefPtr(aRefPtr)
  {
  }

  virtual void PlayEvent(Translator *aTranslator) const;

  virtual void RecordToStream(std::ostream &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;
  
  virtual std::string GetName() const { return "GradientStops Destruction"; }
  virtual ReferencePtr GetObject() const { return mRefPtr; }
private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;

  RecordedGradientStopsDestruction(std::istream &aStream);
};

class RecordedSnapshot : public RecordedEvent {
public:
  RecordedSnapshot(ReferencePtr aRefPtr, DrawTarget *aDT)
    : RecordedEvent(SNAPSHOT), mRefPtr(aRefPtr), mDT(aDT)
  {
  }

  virtual void PlayEvent(Translator *aTranslator) const;

  virtual void RecordToStream(std::ostream &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;
  
  virtual std::string GetName() const { return "Snapshot"; }
  virtual ReferencePtr GetObject() const { return mRefPtr; }
private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;
  ReferencePtr mDT;

  RecordedSnapshot(std::istream &aStream);
};

class RecordedScaledFontCreation : public RecordedEvent {
public:
  static void FontDataProc(const uint8_t *aData, uint32_t aSize, uint32_t aIndex, Float aGlyphSize, void* aBaton)
  {
    static_cast<RecordedScaledFontCreation*>(aBaton)->SetFontData(aData, aSize, aIndex, aGlyphSize);
  }

  RecordedScaledFontCreation(ReferencePtr aRefPtr, ScaledFont *aScaledFont)
    : RecordedEvent(SCALEDFONTCREATION), mRefPtr(aRefPtr), mData(NULL)
  {
    aScaledFont->GetFontFileData(&FontDataProc, this);
  }

  ~RecordedScaledFontCreation();

  virtual void PlayEvent(Translator *aTranslator) const;

  virtual void RecordToStream(std::ostream &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;
  
  virtual std::string GetName() const { return "ScaledFont Creation"; }
  virtual ReferencePtr GetObject() const { return mRefPtr; }

  void SetFontData(const uint8_t *aData, uint32_t aSize, uint32_t aIndex, Float aGlyphSize);

private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;
  uint8_t *mData;
  uint32_t mSize;
  Float mGlyphSize;
  uint32_t mIndex;

  RecordedScaledFontCreation(std::istream &aStream);
};

class RecordedScaledFontDestruction : public RecordedEvent {
public:
  RecordedScaledFontDestruction(ReferencePtr aRefPtr)
    : RecordedEvent(SCALEDFONTDESTRUCTION), mRefPtr(aRefPtr)
  {
  }

  virtual void PlayEvent(Translator *aTranslator) const;

  virtual void RecordToStream(std::ostream &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;
  
  virtual std::string GetName() const { return "ScaledFont Destruction"; }
  virtual ReferencePtr GetObject() const { return mRefPtr; }
private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;

  RecordedScaledFontDestruction(std::istream &aStream);
};

class RecordedMaskSurface : public RecordedDrawingEvent {
public:
  RecordedMaskSurface(DrawTarget *aDT, const Pattern &aPattern, ReferencePtr aRefMask,
                      const Point &aOffset, const DrawOptions &aOptions)
    : RecordedDrawingEvent(MASKSURFACE, aDT), mRefMask(aRefMask), mOffset(aOffset)
    , mOptions(aOptions)
  {
    StorePattern(mPattern, aPattern);
  }

  virtual void PlayEvent(Translator *aTranslator) const;

  virtual void RecordToStream(std::ostream &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;
  
  virtual std::string GetName() const { return "MaskSurface"; }
private:
  friend class RecordedEvent;

  RecordedMaskSurface(std::istream &aStream);

  PatternStorage mPattern;
  ReferencePtr mRefMask;
  Point mOffset;
  DrawOptions mOptions;
};

}
}

#endif
