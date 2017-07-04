/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RECORDEDEVENTIMPL_H_
#define MOZILLA_GFX_RECORDEDEVENTIMPL_H_

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

template<class Derived>
class RecordedEventDerived : public RecordedEvent {
  using RecordedEvent::RecordedEvent;
  void RecordToStream(std::ostream &aStream) const {
    static_cast<const Derived*>(this)->Record(aStream);
  }
  void RecordToStream(MemStream &aStream) const {
    SizeCollector size;
    static_cast<const Derived*>(this)->Record(size);
    aStream.Resize(aStream.mLength + size.mTotalSize);
    MemWriter writer(aStream.mData + aStream.mLength - size.mTotalSize);
    static_cast<const Derived*>(this)->Record(writer);
  }
};

template<class Derived>
class RecordedDrawingEvent : public RecordedEventDerived<Derived>
{
public:
   virtual ReferencePtr GetDestinedDT() { return mDT; }

protected:
  RecordedDrawingEvent(RecordedEvent::EventType aType, DrawTarget *aTarget)
    : RecordedEventDerived<Derived>(aType), mDT(aTarget)
  {
  }

  template<class S>
  RecordedDrawingEvent(RecordedEvent::EventType aType, S &aStream);
  template<class S>
  void Record(S &aStream) const;

  virtual ReferencePtr GetObjectRef() const;

  ReferencePtr mDT;
};

class RecordedDrawTargetCreation : public RecordedEventDerived<RecordedDrawTargetCreation> {
public:
  RecordedDrawTargetCreation(ReferencePtr aRefPtr, BackendType aType, const IntSize &aSize, SurfaceFormat aFormat,
                             bool aHasExistingData = false, SourceSurface *aExistingData = nullptr)
    : RecordedEventDerived(DRAWTARGETCREATION), mRefPtr(aRefPtr), mBackendType(aType), mSize(aSize), mFormat(aFormat)
    , mHasExistingData(aHasExistingData), mExistingData(aExistingData)
  {}

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S>
  void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "DrawTarget Creation"; }
  virtual ReferencePtr GetObjectRef() const { return mRefPtr; }

  ReferencePtr mRefPtr;
  BackendType mBackendType;
  IntSize mSize;
  SurfaceFormat mFormat;
  bool mHasExistingData;
  RefPtr<SourceSurface> mExistingData;
  
private:
  friend class RecordedEvent;

  template<class S>
  MOZ_IMPLICIT RecordedDrawTargetCreation(S &aStream);
};

class RecordedDrawTargetDestruction : public RecordedEventDerived<RecordedDrawTargetDestruction> {
public:
  MOZ_IMPLICIT RecordedDrawTargetDestruction(ReferencePtr aRefPtr)
    : RecordedEventDerived(DRAWTARGETDESTRUCTION), mRefPtr(aRefPtr)
  {}

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S>
  void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "DrawTarget Destruction"; }
  virtual ReferencePtr GetObjectRef() const { return mRefPtr; }

  ReferencePtr mRefPtr;

  BackendType mBackendType;
private:
  friend class RecordedEvent;

  template<class S>
  MOZ_IMPLICIT RecordedDrawTargetDestruction(S &aStream);
};

class RecordedCreateSimilarDrawTarget : public RecordedEventDerived<RecordedCreateSimilarDrawTarget> {
public:
  RecordedCreateSimilarDrawTarget(ReferencePtr aRefPtr, const IntSize &aSize,
                                  SurfaceFormat aFormat)
    : RecordedEventDerived(CREATESIMILARDRAWTARGET)
    , mRefPtr(aRefPtr) , mSize(aSize), mFormat(aFormat)
  {
  }

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S>
  void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "CreateSimilarDrawTarget"; }
  virtual ReferencePtr GetObjectRef() const { return mRefPtr; }

  ReferencePtr mRefPtr;
  IntSize mSize;
  SurfaceFormat mFormat;

private:
  friend class RecordedEvent;

  template<class S>
  MOZ_IMPLICIT RecordedCreateSimilarDrawTarget(S &aStream);
};

class RecordedFillRect : public RecordedDrawingEvent<RecordedFillRect> {
public:
  RecordedFillRect(DrawTarget *aDT, const Rect &aRect, const Pattern &aPattern, const DrawOptions &aOptions)
    : RecordedDrawingEvent(FILLRECT, aDT), mRect(aRect), mOptions(aOptions)
  {
    StorePattern(mPattern, aPattern);
  }

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S>
  void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "FillRect"; }
private:
  friend class RecordedEvent;

  template<class S>
  MOZ_IMPLICIT RecordedFillRect(S &aStream);

  Rect mRect;
  PatternStorage mPattern;
  DrawOptions mOptions;
};

class RecordedStrokeRect : public RecordedDrawingEvent<RecordedStrokeRect> {
public:
  RecordedStrokeRect(DrawTarget *aDT, const Rect &aRect, const Pattern &aPattern,
                     const StrokeOptions &aStrokeOptions, const DrawOptions &aOptions)
    : RecordedDrawingEvent(STROKERECT, aDT), mRect(aRect),
      mStrokeOptions(aStrokeOptions), mOptions(aOptions)
  {
    StorePattern(mPattern, aPattern);
  }

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S>
  void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "StrokeRect"; }
private:
  friend class RecordedEvent;

  template<class S>
  MOZ_IMPLICIT RecordedStrokeRect(S &aStream);

  Rect mRect;
  PatternStorage mPattern;
  StrokeOptions mStrokeOptions;
  DrawOptions mOptions;
};

class RecordedStrokeLine : public RecordedDrawingEvent<RecordedStrokeLine> {
public:
  RecordedStrokeLine(DrawTarget *aDT, const Point &aBegin, const Point &aEnd,
                     const Pattern &aPattern, const StrokeOptions &aStrokeOptions,
                     const DrawOptions &aOptions)
    : RecordedDrawingEvent(STROKELINE, aDT), mBegin(aBegin), mEnd(aEnd),
      mStrokeOptions(aStrokeOptions), mOptions(aOptions)
  {
    StorePattern(mPattern, aPattern);
  }

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S>
  void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "StrokeLine"; }
private:
  friend class RecordedEvent;

  template<class S>
  MOZ_IMPLICIT RecordedStrokeLine(S &aStream);

  Point mBegin;
  Point mEnd;
  PatternStorage mPattern;
  StrokeOptions mStrokeOptions;
  DrawOptions mOptions;
};

class RecordedFill : public RecordedDrawingEvent<RecordedFill> {
public:
  RecordedFill(DrawTarget *aDT, ReferencePtr aPath, const Pattern &aPattern, const DrawOptions &aOptions)
    : RecordedDrawingEvent(FILL, aDT), mPath(aPath), mOptions(aOptions)
  {
    StorePattern(mPattern, aPattern);
  }

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "Fill"; }
private:
  friend class RecordedEvent;

  template<class S>
  MOZ_IMPLICIT RecordedFill(S &aStream);

  ReferencePtr mPath;
  PatternStorage mPattern;
  DrawOptions mOptions;
};

class RecordedFillGlyphs : public RecordedDrawingEvent<RecordedFillGlyphs> {
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

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "FillGlyphs"; }
private:
  friend class RecordedEvent;

  template<class S>
  MOZ_IMPLICIT RecordedFillGlyphs(S &aStream);

  ReferencePtr mScaledFont;
  PatternStorage mPattern;
  DrawOptions mOptions;
  Glyph *mGlyphs;
  uint32_t mNumGlyphs;
};

class RecordedMask : public RecordedDrawingEvent<RecordedMask> {
public:
  RecordedMask(DrawTarget *aDT, const Pattern &aSource, const Pattern &aMask, const DrawOptions &aOptions)
    : RecordedDrawingEvent(MASK, aDT), mOptions(aOptions)
  {
    StorePattern(mSource, aSource);
    StorePattern(mMask, aMask);
  }

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "Mask"; }
private:
  friend class RecordedEvent;

  template<class S>
  MOZ_IMPLICIT RecordedMask(S &aStream);

  PatternStorage mSource;
  PatternStorage mMask;
  DrawOptions mOptions;
};

class RecordedStroke : public RecordedDrawingEvent<RecordedStroke> {
public:
  RecordedStroke(DrawTarget *aDT, ReferencePtr aPath, const Pattern &aPattern,
                     const StrokeOptions &aStrokeOptions, const DrawOptions &aOptions)
    : RecordedDrawingEvent(STROKE, aDT), mPath(aPath),
      mStrokeOptions(aStrokeOptions), mOptions(aOptions)
  {
    StorePattern(mPattern, aPattern);
  }

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "Stroke"; }
private:
  friend class RecordedEvent;

  template<class S>
  MOZ_IMPLICIT RecordedStroke(S &aStream);

  ReferencePtr mPath;
  PatternStorage mPattern;
  StrokeOptions mStrokeOptions;
  DrawOptions mOptions;
};

class RecordedClearRect : public RecordedDrawingEvent<RecordedClearRect> {
public:
  RecordedClearRect(DrawTarget *aDT, const Rect &aRect)
    : RecordedDrawingEvent(CLEARRECT, aDT), mRect(aRect)
  {
  }

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "ClearRect"; }
private:
  friend class RecordedEvent;

  template<class S>
  MOZ_IMPLICIT RecordedClearRect(S &aStream);

  Rect mRect;
};

class RecordedCopySurface : public RecordedDrawingEvent<RecordedCopySurface> {
public:
  RecordedCopySurface(DrawTarget *aDT, ReferencePtr aSourceSurface,
                      const IntRect &aSourceRect, const IntPoint &aDest)
    : RecordedDrawingEvent(COPYSURFACE, aDT), mSourceSurface(aSourceSurface),
	  mSourceRect(aSourceRect), mDest(aDest)
  {
  }

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "CopySurface"; }
private:
  friend class RecordedEvent;

  template<class S>
  MOZ_IMPLICIT RecordedCopySurface(S &aStream);

  ReferencePtr mSourceSurface;
  IntRect mSourceRect;
  IntPoint mDest;
};

class RecordedPushClip : public RecordedDrawingEvent<RecordedPushClip> {
public:
  RecordedPushClip(DrawTarget *aDT, ReferencePtr aPath)
    : RecordedDrawingEvent(PUSHCLIP, aDT), mPath(aPath)
  {
  }

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "PushClip"; }
private:
  friend class RecordedEvent;

  template<class S>
  MOZ_IMPLICIT RecordedPushClip(S &aStream);

  ReferencePtr mPath;
};

class RecordedPushClipRect : public RecordedDrawingEvent<RecordedPushClipRect> {
public:
  RecordedPushClipRect(DrawTarget *aDT, const Rect &aRect)
    : RecordedDrawingEvent(PUSHCLIPRECT, aDT), mRect(aRect)
  {
  }

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "PushClipRect"; }
private:
  friend class RecordedEvent;

  template<class S>
  MOZ_IMPLICIT RecordedPushClipRect(S &aStream);

  Rect mRect;
};

class RecordedPopClip : public RecordedDrawingEvent<RecordedPopClip> {
public:
  MOZ_IMPLICIT RecordedPopClip(DrawTarget *aDT)
    : RecordedDrawingEvent(POPCLIP, aDT)
  {}

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "PopClip"; }
private:
  friend class RecordedEvent;

  template<class S>
  MOZ_IMPLICIT RecordedPopClip(S &aStream);
};

class RecordedPushLayer : public RecordedDrawingEvent<RecordedPushLayer> {
public:
  RecordedPushLayer(DrawTarget* aDT, bool aOpaque, Float aOpacity,
                    SourceSurface* aMask, const Matrix& aMaskTransform,
                    const IntRect& aBounds, bool aCopyBackground)
    : RecordedDrawingEvent(PUSHLAYER, aDT), mOpaque(aOpaque)
    , mOpacity(aOpacity), mMask(aMask), mMaskTransform(aMaskTransform)
    , mBounds(aBounds), mCopyBackground(aCopyBackground)
  {
  }

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "PushLayer"; }
private:
  friend class RecordedEvent;

  template<class S>
  MOZ_IMPLICIT RecordedPushLayer(S &aStream);

  bool mOpaque;
  Float mOpacity;
  ReferencePtr mMask;
  Matrix mMaskTransform;
  IntRect mBounds;
  bool mCopyBackground;
};

class RecordedPopLayer : public RecordedDrawingEvent<RecordedPopLayer> {
public:
  MOZ_IMPLICIT RecordedPopLayer(DrawTarget* aDT)
    : RecordedDrawingEvent(POPLAYER, aDT)
  {
  }

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "PopLayer"; }
private:
  friend class RecordedEvent;

  template<class S>
  MOZ_IMPLICIT RecordedPopLayer(S &aStream);
};

class RecordedSetTransform : public RecordedDrawingEvent<RecordedSetTransform> {
public:
  RecordedSetTransform(DrawTarget *aDT, const Matrix &aTransform)
    : RecordedDrawingEvent(SETTRANSFORM, aDT), mTransform(aTransform)
  {
  }

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;
  
  virtual std::string GetName() const { return "SetTransform"; }

  Matrix mTransform;
private:
  friend class RecordedEvent;

   template<class S>
  MOZ_IMPLICIT RecordedSetTransform(S &aStream);
};

class RecordedDrawSurface : public RecordedDrawingEvent<RecordedDrawSurface> {
public:
  RecordedDrawSurface(DrawTarget *aDT, ReferencePtr aRefSource, const Rect &aDest,
                      const Rect &aSource, const DrawSurfaceOptions &aDSOptions,
                      const DrawOptions &aOptions)
    : RecordedDrawingEvent(DRAWSURFACE, aDT), mRefSource(aRefSource), mDest(aDest)
    , mSource(aSource), mDSOptions(aDSOptions), mOptions(aOptions)
  {
  }

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;
  
  virtual std::string GetName() const { return "DrawSurface"; }
private:
  friend class RecordedEvent;

   template<class S>
  MOZ_IMPLICIT RecordedDrawSurface(S &aStream);

  ReferencePtr mRefSource;
  Rect mDest;
  Rect mSource;
  DrawSurfaceOptions mDSOptions;
  DrawOptions mOptions;
};

class RecordedDrawSurfaceWithShadow : public RecordedDrawingEvent<RecordedDrawSurfaceWithShadow> {
public:
  RecordedDrawSurfaceWithShadow(DrawTarget *aDT, ReferencePtr aRefSource, const Point &aDest,
                                const Color &aColor, const Point &aOffset,
                                Float aSigma, CompositionOp aOp)
    : RecordedDrawingEvent(DRAWSURFACEWITHSHADOW, aDT), mRefSource(aRefSource), mDest(aDest)
    , mColor(aColor), mOffset(aOffset), mSigma(aSigma), mOp(aOp)
  {
  }

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;
  
  virtual std::string GetName() const { return "DrawSurfaceWithShadow"; }
private:
  friend class RecordedEvent;

  template<class S>
  MOZ_IMPLICIT RecordedDrawSurfaceWithShadow(S &aStream);

  ReferencePtr mRefSource;
  Point mDest;
  Color mColor;
  Point mOffset;
  Float mSigma;
  CompositionOp mOp;
};

class RecordedDrawFilter : public RecordedDrawingEvent<RecordedDrawFilter> {
public:
  RecordedDrawFilter(DrawTarget *aDT, ReferencePtr aNode,
                     const Rect &aSourceRect,
                     const Point &aDestPoint,
                     const DrawOptions &aOptions)
    : RecordedDrawingEvent(DRAWFILTER, aDT), mNode(aNode), mSourceRect(aSourceRect)
    , mDestPoint(aDestPoint), mOptions(aOptions)
  {
  }

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "DrawFilter"; }
private:
  friend class RecordedEvent;

  template<class S>
  MOZ_IMPLICIT RecordedDrawFilter(S &aStream);

  ReferencePtr mNode;
  Rect mSourceRect;
  Point mDestPoint;
  DrawOptions mOptions;
};

class RecordedPathCreation : public RecordedEventDerived<RecordedPathCreation> {
public:
  MOZ_IMPLICIT RecordedPathCreation(PathRecording *aPath);
  ~RecordedPathCreation();
  
  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;
  
  virtual std::string GetName() const { return "Path Creation"; }
  virtual ReferencePtr GetObjectRef() const { return mRefPtr; }
private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;
  FillRule mFillRule;
  std::vector<PathOp> mPathOps;

  template<class S>
  MOZ_IMPLICIT RecordedPathCreation(S &aStream);
};

class RecordedPathDestruction : public RecordedEventDerived<RecordedPathDestruction> {
public:
  MOZ_IMPLICIT RecordedPathDestruction(PathRecording *aPath)
    : RecordedEventDerived(PATHDESTRUCTION), mRefPtr(aPath)
  {
  }
  
  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;
  
  virtual std::string GetName() const { return "Path Destruction"; }
  virtual ReferencePtr GetObjectRef() const { return mRefPtr; }
private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;

  template<class S>
  MOZ_IMPLICIT RecordedPathDestruction(S &aStream);
};

class RecordedSourceSurfaceCreation : public RecordedEventDerived<RecordedSourceSurfaceCreation> {
public:
  RecordedSourceSurfaceCreation(ReferencePtr aRefPtr, uint8_t *aData, int32_t aStride,
                                const IntSize &aSize, SurfaceFormat aFormat)
    : RecordedEventDerived(SOURCESURFACECREATION), mRefPtr(aRefPtr), mData(aData)
    , mStride(aStride), mSize(aSize), mFormat(aFormat), mDataOwned(false)
  {
  }

  ~RecordedSourceSurfaceCreation();

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;
  
  virtual std::string GetName() const { return "SourceSurface Creation"; }
  virtual ReferencePtr GetObjectRef() const { return mRefPtr; }
private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;
  uint8_t *mData;
  int32_t mStride;
  IntSize mSize;
  SurfaceFormat mFormat;
  bool mDataOwned;

  template<class S>
  MOZ_IMPLICIT RecordedSourceSurfaceCreation(S &aStream);
};

class RecordedSourceSurfaceDestruction : public RecordedEventDerived<RecordedSourceSurfaceDestruction> {
public:
  MOZ_IMPLICIT RecordedSourceSurfaceDestruction(ReferencePtr aRefPtr)
    : RecordedEventDerived(SOURCESURFACEDESTRUCTION), mRefPtr(aRefPtr)
  {
  }

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;
  
  virtual std::string GetName() const { return "SourceSurface Destruction"; }
  virtual ReferencePtr GetObjectRef() const { return mRefPtr; }
private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;

  template<class S>
  MOZ_IMPLICIT RecordedSourceSurfaceDestruction(S &aStream);
};

class RecordedFilterNodeCreation : public RecordedEventDerived<RecordedFilterNodeCreation> {
public:
  RecordedFilterNodeCreation(ReferencePtr aRefPtr, FilterType aType)
    : RecordedEventDerived(FILTERNODECREATION), mRefPtr(aRefPtr), mType(aType)
  {
  }

  ~RecordedFilterNodeCreation();

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "FilterNode Creation"; }
  virtual ReferencePtr GetObjectRef() const { return mRefPtr; }
private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;
  FilterType mType;

  template<class S>
  MOZ_IMPLICIT RecordedFilterNodeCreation(S &aStream);
};

class RecordedFilterNodeDestruction : public RecordedEventDerived<RecordedFilterNodeDestruction> {
public:
  MOZ_IMPLICIT RecordedFilterNodeDestruction(ReferencePtr aRefPtr)
    : RecordedEventDerived(FILTERNODEDESTRUCTION), mRefPtr(aRefPtr)
  {
  }

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "FilterNode Destruction"; }
  virtual ReferencePtr GetObjectRef() const { return mRefPtr; }
private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;

  template<class S>
  MOZ_IMPLICIT RecordedFilterNodeDestruction(S &aStream);
};

class RecordedGradientStopsCreation : public RecordedEventDerived<RecordedGradientStopsCreation> {
public:
  RecordedGradientStopsCreation(ReferencePtr aRefPtr, GradientStop *aStops,
                                uint32_t aNumStops, ExtendMode aExtendMode)
    : RecordedEventDerived(GRADIENTSTOPSCREATION), mRefPtr(aRefPtr), mStops(aStops)
    , mNumStops(aNumStops), mExtendMode(aExtendMode), mDataOwned(false)
  {
  }

  ~RecordedGradientStopsCreation();

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;
  
  virtual std::string GetName() const { return "GradientStops Creation"; }
  virtual ReferencePtr GetObjectRef() const { return mRefPtr; }
private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;
  GradientStop *mStops;
  uint32_t mNumStops;
  ExtendMode mExtendMode;
  bool mDataOwned;

  template<class S>
  MOZ_IMPLICIT RecordedGradientStopsCreation(S &aStream);
};

class RecordedGradientStopsDestruction : public RecordedEventDerived<RecordedGradientStopsDestruction> {
public:
  MOZ_IMPLICIT RecordedGradientStopsDestruction(ReferencePtr aRefPtr)
    : RecordedEventDerived(GRADIENTSTOPSDESTRUCTION), mRefPtr(aRefPtr)
  {
  }

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;
  
  virtual std::string GetName() const { return "GradientStops Destruction"; }
  virtual ReferencePtr GetObjectRef() const { return mRefPtr; }
private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;

  template<class S>
  MOZ_IMPLICIT RecordedGradientStopsDestruction(S &aStream);
};

class RecordedSnapshot : public RecordedEventDerived<RecordedSnapshot> {
public:
  RecordedSnapshot(ReferencePtr aRefPtr, DrawTarget *aDT)
    : RecordedEventDerived(SNAPSHOT), mRefPtr(aRefPtr), mDT(aDT)
  {
  }

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;
  
  virtual std::string GetName() const { return "Snapshot"; }
  virtual ReferencePtr GetObjectRef() const { return mRefPtr; }
private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;
  ReferencePtr mDT;

  template<class S>
  MOZ_IMPLICIT RecordedSnapshot(S &aStream);
};

class RecordedIntoLuminanceSource : public RecordedEventDerived<RecordedIntoLuminanceSource> {
public:
  RecordedIntoLuminanceSource(ReferencePtr aRefPtr, DrawTarget *aDT,
                              LuminanceType aLuminanceType, float aOpacity)
    : RecordedEventDerived(SNAPSHOT), mRefPtr(aRefPtr), mDT(aDT),
      mLuminanceType(aLuminanceType), mOpacity(aOpacity)
  {
  }

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "IntoLuminanceSource"; }
  virtual ReferencePtr GetObjectRef() const { return mRefPtr; }
private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;
  ReferencePtr mDT;
  LuminanceType mLuminanceType;
  float mOpacity;

  template<class S>
  MOZ_IMPLICIT RecordedIntoLuminanceSource(S &aStream);
};

class RecordedFontData : public RecordedEventDerived<RecordedFontData> {
public:

  static void FontDataProc(const uint8_t *aData, uint32_t aSize,
                           uint32_t aIndex, void* aBaton)
  {
    auto recordedFontData = static_cast<RecordedFontData*>(aBaton);
    recordedFontData->SetFontData(aData, aSize, aIndex);
  }

  explicit RecordedFontData(UnscaledFont *aUnscaledFont)
    : RecordedEventDerived(FONTDATA), mData(nullptr)
  {
    mGetFontFileDataSucceeded = aUnscaledFont->GetFontFileData(&FontDataProc, this);
  }

  ~RecordedFontData();

  bool IsValid() const { return mGetFontFileDataSucceeded; }

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "Font Data"; }
  virtual ReferencePtr GetObjectRef() const { return nullptr; };

  void SetFontData(const uint8_t *aData, uint32_t aSize, uint32_t aIndex);

  bool GetFontDetails(RecordedFontDetails& fontDetails);

private:
  friend class RecordedEvent;

  uint8_t* mData;
  RecordedFontDetails mFontDetails;

  bool mGetFontFileDataSucceeded;

  template<class S>
  MOZ_IMPLICIT RecordedFontData(S &aStream);
};

class RecordedFontDescriptor : public RecordedEventDerived<RecordedFontDescriptor> {
public:

  static void FontDescCb(const uint8_t* aData, uint32_t aSize,
                         void* aBaton)
  {
    auto recordedFontDesc = static_cast<RecordedFontDescriptor*>(aBaton);
    recordedFontDesc->SetFontDescriptor(aData, aSize);
  }

  explicit RecordedFontDescriptor(UnscaledFont* aUnscaledFont)
    : RecordedEventDerived(FONTDESC)
    , mType(aUnscaledFont->GetType())
    , mRefPtr(aUnscaledFont)
  {
    mHasDesc = aUnscaledFont->GetFontDescriptor(FontDescCb, this);
  }

  ~RecordedFontDescriptor();

  bool IsValid() const { return mHasDesc; }

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "Font Desc"; }
  virtual ReferencePtr GetObjectRef() const { return mRefPtr; }

private:
  friend class RecordedEvent;

  void SetFontDescriptor(const uint8_t* aData, uint32_t aSize);

  bool mHasDesc;

  FontType mType;
  std::vector<uint8_t> mData;
  ReferencePtr mRefPtr;

  template<class S>
  MOZ_IMPLICIT RecordedFontDescriptor(S &aStream);
};

class RecordedUnscaledFontCreation : public RecordedEventDerived<RecordedUnscaledFontCreation> {
public:
  static void FontInstanceDataProc(const uint8_t* aData, uint32_t aSize, void* aBaton)
  {
    auto recordedUnscaledFontCreation = static_cast<RecordedUnscaledFontCreation*>(aBaton);
    recordedUnscaledFontCreation->SetFontInstanceData(aData, aSize);
  }

  RecordedUnscaledFontCreation(UnscaledFont* aUnscaledFont,
                               RecordedFontDetails aFontDetails)
    : RecordedEventDerived(UNSCALEDFONTCREATION)
    , mRefPtr(aUnscaledFont)
    , mFontDataKey(aFontDetails.fontDataKey)
    , mIndex(aFontDetails.index)
  {
    aUnscaledFont->GetFontInstanceData(FontInstanceDataProc, this);
  }

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "UnscaledFont Creation"; }
  virtual ReferencePtr GetObjectRef() const { return mRefPtr; }

  void SetFontInstanceData(const uint8_t *aData, uint32_t aSize);

private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;
  uint64_t mFontDataKey;
  uint32_t mIndex;
  std::vector<uint8_t> mInstanceData;

  template<class S>
  MOZ_IMPLICIT RecordedUnscaledFontCreation(S &aStream);
};

class RecordedUnscaledFontDestruction : public RecordedEventDerived<RecordedUnscaledFontDestruction> {
public:
  MOZ_IMPLICIT RecordedUnscaledFontDestruction(ReferencePtr aRefPtr)
    : RecordedEventDerived(UNSCALEDFONTDESTRUCTION), mRefPtr(aRefPtr)
  {}

  virtual bool PlayEvent(Translator *aTranslator) const;
  template<class S>
  void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "UnscaledFont Destruction"; }
  virtual ReferencePtr GetObjectRef() const { return mRefPtr; }
private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;

  template<class S>
  MOZ_IMPLICIT RecordedUnscaledFontDestruction(S &aStream);
};

class RecordedScaledFontCreation : public RecordedEventDerived<RecordedScaledFontCreation> {
public:

  static void FontInstanceDataProc(const uint8_t* aData, uint32_t aSize, void* aBaton)
  {
    auto recordedScaledFontCreation = static_cast<RecordedScaledFontCreation*>(aBaton);
    recordedScaledFontCreation->SetFontInstanceData(aData, aSize);
  }

  RecordedScaledFontCreation(ScaledFont* aScaledFont,
                             UnscaledFont* aUnscaledFont)
    : RecordedEventDerived(SCALEDFONTCREATION)
    , mRefPtr(aScaledFont)
    , mUnscaledFont(aUnscaledFont)
    , mGlyphSize(aScaledFont->GetSize())
  {
    aScaledFont->GetFontInstanceData(FontInstanceDataProc, this);
  }

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;
  
  virtual std::string GetName() const { return "ScaledFont Creation"; }
  virtual ReferencePtr GetObjectRef() const { return mRefPtr; }

  void SetFontInstanceData(const uint8_t *aData, uint32_t aSize);

private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;
  ReferencePtr mUnscaledFont;
  Float mGlyphSize;
  std::vector<uint8_t> mInstanceData;

  template<class S>
  MOZ_IMPLICIT RecordedScaledFontCreation(S &aStream);
};

class RecordedScaledFontDestruction : public RecordedEventDerived<RecordedScaledFontDestruction> {
public:
  MOZ_IMPLICIT RecordedScaledFontDestruction(ReferencePtr aRefPtr)
    : RecordedEventDerived(SCALEDFONTDESTRUCTION), mRefPtr(aRefPtr)
  {
  }

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S>
  void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "ScaledFont Destruction"; }
  virtual ReferencePtr GetObjectRef() const { return mRefPtr; }
private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;

  template<class S>
  MOZ_IMPLICIT RecordedScaledFontDestruction(S &aStream);
};

class RecordedMaskSurface : public RecordedDrawingEvent<RecordedMaskSurface> {
public:
  RecordedMaskSurface(DrawTarget *aDT, const Pattern &aPattern, ReferencePtr aRefMask,
                      const Point &aOffset, const DrawOptions &aOptions)
    : RecordedDrawingEvent(MASKSURFACE, aDT), mRefMask(aRefMask), mOffset(aOffset)
    , mOptions(aOptions)
  {
    StorePattern(mPattern, aPattern);
  }

  virtual bool PlayEvent(Translator *aTranslator) const;

  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;
  
  virtual std::string GetName() const { return "MaskSurface"; }
private:
  friend class RecordedEvent;

  template<class S>
  MOZ_IMPLICIT RecordedMaskSurface(S &aStream);

  PatternStorage mPattern;
  ReferencePtr mRefMask;
  Point mOffset;
  DrawOptions mOptions;
};

class RecordedFilterNodeSetAttribute : public RecordedEventDerived<RecordedFilterNodeSetAttribute>
{
public:
  enum ArgType {
    ARGTYPE_UINT32,
    ARGTYPE_BOOL,
    ARGTYPE_FLOAT,
    ARGTYPE_SIZE,
    ARGTYPE_INTSIZE,
    ARGTYPE_INTPOINT,
    ARGTYPE_RECT,
    ARGTYPE_INTRECT,
    ARGTYPE_POINT,
    ARGTYPE_MATRIX,
    ARGTYPE_MATRIX5X4,
    ARGTYPE_POINT3D,
    ARGTYPE_COLOR,
    ARGTYPE_FLOAT_ARRAY
  };

  template<typename T>
  RecordedFilterNodeSetAttribute(FilterNode *aNode, uint32_t aIndex, T aArgument, ArgType aArgType)
    : RecordedEventDerived(FILTERNODESETATTRIBUTE), mNode(aNode), mIndex(aIndex), mArgType(aArgType)
  {
    mPayload.resize(sizeof(T));
    memcpy(&mPayload.front(), &aArgument, sizeof(T));
  }

  RecordedFilterNodeSetAttribute(FilterNode *aNode, uint32_t aIndex, const Float *aFloat, uint32_t aSize)
    : RecordedEventDerived(FILTERNODESETATTRIBUTE), mNode(aNode), mIndex(aIndex), mArgType(ARGTYPE_FLOAT_ARRAY)
  {
    mPayload.resize(sizeof(Float) * aSize);
    memcpy(&mPayload.front(), aFloat, sizeof(Float) * aSize);
  }

  virtual bool PlayEvent(Translator *aTranslator) const;
  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "SetAttribute"; }

  virtual ReferencePtr GetObjectRef() const { return mNode; }

private:
  friend class RecordedEvent;

  ReferencePtr mNode;

  uint32_t mIndex;
  ArgType mArgType;
  std::vector<uint8_t> mPayload;

  template<class S>
  MOZ_IMPLICIT RecordedFilterNodeSetAttribute(S &aStream);
};

class RecordedFilterNodeSetInput : public RecordedEventDerived<RecordedFilterNodeSetInput>
{
public:
  RecordedFilterNodeSetInput(FilterNode* aNode, uint32_t aIndex, FilterNode* aInputNode)
    : RecordedEventDerived(FILTERNODESETINPUT), mNode(aNode), mIndex(aIndex)
    , mInputFilter(aInputNode), mInputSurface(nullptr)
  {
  }

  RecordedFilterNodeSetInput(FilterNode *aNode, uint32_t aIndex, SourceSurface *aInputSurface)
    : RecordedEventDerived(FILTERNODESETINPUT), mNode(aNode), mIndex(aIndex)
    , mInputFilter(nullptr), mInputSurface(aInputSurface)
  {
  }

  virtual bool PlayEvent(Translator *aTranslator) const;
  template<class S> void Record(S &aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream &aStringStream) const;

  virtual std::string GetName() const { return "SetInput"; }

  virtual ReferencePtr GetObjectRef() const { return mNode; }

private:
  friend class RecordedEvent;

  ReferencePtr mNode;
  uint32_t mIndex;
  ReferencePtr mInputFilter;
  ReferencePtr mInputSurface;

  template<class S>
  MOZ_IMPLICIT RecordedFilterNodeSetInput(S &aStream);
};


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

template<class S>
void
RecordedEvent::RecordPatternData(S &aStream, const PatternStorage &aPattern) const
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

inline void
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

inline void
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

template<class S>
void
RecordedEvent::RecordStrokeOptions(S &aStream, const StrokeOptions &aStrokeOptions) const
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

inline void
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

inline void
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

template<class T>
template<class S>
RecordedDrawingEvent<T>::RecordedDrawingEvent(RecordedEvent::EventType aType, S &aStream)
  : RecordedEventDerived<T>(aType)
{
  ReadElement(aStream, mDT);
}

template<class T>
template<class S>
void
RecordedDrawingEvent<T>::Record(S &aStream) const
{
  WriteElement(aStream, mDT);
}

template<class T>
ReferencePtr
RecordedDrawingEvent<T>::GetObjectRef() const
{
  return mDT;
}

inline bool
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

template<class S>
void
RecordedDrawTargetCreation::Record(S &aStream) const
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

template<class S>
RecordedDrawTargetCreation::RecordedDrawTargetCreation(S &aStream)
  : RecordedEventDerived(DRAWTARGETCREATION)
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

inline void
RecordedDrawTargetCreation::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] DrawTarget Creation (Type: " << NameFromBackend(mBackendType) << ", Size: " << mSize.width << "x" << mSize.height << ")";
}


inline bool
RecordedDrawTargetDestruction::PlayEvent(Translator *aTranslator) const
{
  aTranslator->RemoveDrawTarget(mRefPtr);
  return true;
}

template<class S>
void
RecordedDrawTargetDestruction::Record(S &aStream) const
{
  WriteElement(aStream, mRefPtr);
}

template<class S>
RecordedDrawTargetDestruction::RecordedDrawTargetDestruction(S &aStream)
  : RecordedEventDerived(DRAWTARGETDESTRUCTION)
{
  ReadElement(aStream, mRefPtr);
}

inline void
RecordedDrawTargetDestruction::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] DrawTarget Destruction";
}

inline bool
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

template<class S>
void
RecordedCreateSimilarDrawTarget::Record(S &aStream) const
{
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mSize);
  WriteElement(aStream, mFormat);
}

template<class S>
RecordedCreateSimilarDrawTarget::RecordedCreateSimilarDrawTarget(S &aStream)
  : RecordedEventDerived(CREATESIMILARDRAWTARGET)
{
  ReadElement(aStream, mRefPtr);
  ReadElement(aStream, mSize);
  ReadElement(aStream, mFormat);
}

inline void
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

inline bool
RecordedFillRect::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->FillRect(mRect, *GenericPattern(mPattern, aTranslator), mOptions);
  return true;
}

template<class S>
void
RecordedFillRect::Record(S &aStream) const
{
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mRect);
  WriteElement(aStream, mOptions);
  RecordPatternData(aStream, mPattern);
}

template<class S>
RecordedFillRect::RecordedFillRect(S &aStream)
  : RecordedDrawingEvent(FILLRECT, aStream)
{
  ReadElement(aStream, mRect);
  ReadElement(aStream, mOptions);
  ReadPatternData(aStream, mPattern);
}

inline void
RecordedFillRect::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] FillRect (" << mRect.x << ", " << mRect.y << " - " << mRect.width << " x " << mRect.height << ") ";
  OutputSimplePatternInfo(mPattern, aStringStream);
}

inline bool
RecordedStrokeRect::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->StrokeRect(mRect, *GenericPattern(mPattern, aTranslator), mStrokeOptions, mOptions);
  return true;
}

template<class S>
void
RecordedStrokeRect::Record(S &aStream) const
{
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mRect);
  WriteElement(aStream, mOptions);
  RecordPatternData(aStream, mPattern);
  RecordStrokeOptions(aStream, mStrokeOptions);
}

template<class S>
RecordedStrokeRect::RecordedStrokeRect(S &aStream)
  : RecordedDrawingEvent(STROKERECT, aStream)
{
  ReadElement(aStream, mRect);
  ReadElement(aStream, mOptions);
  ReadPatternData(aStream, mPattern);
  ReadStrokeOptions(aStream, mStrokeOptions);
}

inline void
RecordedStrokeRect::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] StrokeRect (" << mRect.x << ", " << mRect.y << " - " << mRect.width << " x " << mRect.height
                << ") LineWidth: " << mStrokeOptions.mLineWidth << "px ";
  OutputSimplePatternInfo(mPattern, aStringStream);
}

inline bool
RecordedStrokeLine::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->StrokeLine(mBegin, mEnd, *GenericPattern(mPattern, aTranslator), mStrokeOptions, mOptions);
  return true;
}

template<class S>
void
RecordedStrokeLine::Record(S &aStream) const
{
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mBegin);
  WriteElement(aStream, mEnd);
  WriteElement(aStream, mOptions);
  RecordPatternData(aStream, mPattern);
  RecordStrokeOptions(aStream, mStrokeOptions);
}

template<class S>
RecordedStrokeLine::RecordedStrokeLine(S &aStream)
  : RecordedDrawingEvent(STROKELINE, aStream)
{
  ReadElement(aStream, mBegin);
  ReadElement(aStream, mEnd);
  ReadElement(aStream, mOptions);
  ReadPatternData(aStream, mPattern);
  ReadStrokeOptions(aStream, mStrokeOptions);
}

inline void
RecordedStrokeLine::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] StrokeLine (" << mBegin.x << ", " << mBegin.y << " - " << mEnd.x << ", " << mEnd.y
                << ") LineWidth: " << mStrokeOptions.mLineWidth << "px ";
  OutputSimplePatternInfo(mPattern, aStringStream);
}

inline bool
RecordedFill::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->Fill(aTranslator->LookupPath(mPath), *GenericPattern(mPattern, aTranslator), mOptions);
  return true;
}

template<class S>
RecordedFill::RecordedFill(S &aStream)
  : RecordedDrawingEvent(FILL, aStream)
{
  ReadElement(aStream, mPath);
  ReadElement(aStream, mOptions);
  ReadPatternData(aStream, mPattern);
}

template<class S>
void
RecordedFill::Record(S &aStream) const
{
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mPath);
  WriteElement(aStream, mOptions);
  RecordPatternData(aStream, mPattern);
}

inline void
RecordedFill::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] Fill (" << mPath << ") ";
  OutputSimplePatternInfo(mPattern, aStringStream);
}

inline
RecordedFillGlyphs::~RecordedFillGlyphs()
{
  delete [] mGlyphs;
}

inline bool
RecordedFillGlyphs::PlayEvent(Translator *aTranslator) const
{
  GlyphBuffer buffer;
  buffer.mGlyphs = mGlyphs;
  buffer.mNumGlyphs = mNumGlyphs;
  aTranslator->LookupDrawTarget(mDT)->FillGlyphs(aTranslator->LookupScaledFont(mScaledFont), buffer, *GenericPattern(mPattern, aTranslator), mOptions);
  return true;
}

template<class S>
RecordedFillGlyphs::RecordedFillGlyphs(S &aStream)
  : RecordedDrawingEvent(FILLGLYPHS, aStream)
{
  ReadElement(aStream, mScaledFont);
  ReadElement(aStream, mOptions);
  ReadPatternData(aStream, mPattern);
  ReadElement(aStream, mNumGlyphs);
  mGlyphs = new Glyph[mNumGlyphs];
  aStream.read((char*)mGlyphs, sizeof(Glyph) * mNumGlyphs);
}

template<class S>
void
RecordedFillGlyphs::Record(S &aStream) const
{
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mScaledFont);
  WriteElement(aStream, mOptions);
  RecordPatternData(aStream, mPattern);
  WriteElement(aStream, mNumGlyphs);
  aStream.write((char*)mGlyphs, sizeof(Glyph) * mNumGlyphs);
}

inline void
RecordedFillGlyphs::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] FillGlyphs (" << mScaledFont << ") ";
  OutputSimplePatternInfo(mPattern, aStringStream);
}

inline bool
RecordedMask::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->Mask(*GenericPattern(mSource, aTranslator), *GenericPattern(mMask, aTranslator), mOptions);
  return true;
}

template<class S>
RecordedMask::RecordedMask(S &aStream)
  : RecordedDrawingEvent(MASK, aStream)
{
  ReadElement(aStream, mOptions);
  ReadPatternData(aStream, mSource);
  ReadPatternData(aStream, mMask);
}

template<class S>
void
RecordedMask::Record(S &aStream) const
{
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mOptions);
  RecordPatternData(aStream, mSource);
  RecordPatternData(aStream, mMask);
}

inline void
RecordedMask::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] Mask (Source: ";
  OutputSimplePatternInfo(mSource, aStringStream);
  aStringStream << " Mask: ";
  OutputSimplePatternInfo(mMask, aStringStream);
}

inline bool
RecordedStroke::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->Stroke(aTranslator->LookupPath(mPath), *GenericPattern(mPattern, aTranslator), mStrokeOptions, mOptions);
  return true;
}

template<class S>
void
RecordedStroke::Record(S &aStream) const
{
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mPath);
  WriteElement(aStream, mOptions);
  RecordPatternData(aStream, mPattern);
  RecordStrokeOptions(aStream, mStrokeOptions);
}

template<class S>
RecordedStroke::RecordedStroke(S &aStream)
  : RecordedDrawingEvent(STROKE, aStream)
{
  ReadElement(aStream, mPath);
  ReadElement(aStream, mOptions);
  ReadPatternData(aStream, mPattern);
  ReadStrokeOptions(aStream, mStrokeOptions);
}

inline void
RecordedStroke::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] Stroke ("<< mPath << ") LineWidth: " << mStrokeOptions.mLineWidth << "px ";
  OutputSimplePatternInfo(mPattern, aStringStream);
}

inline bool
RecordedClearRect::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->ClearRect(mRect);
  return true;
}

template<class S>
void
RecordedClearRect::Record(S &aStream) const
{
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mRect);
}

template<class S>
RecordedClearRect::RecordedClearRect(S &aStream)
  : RecordedDrawingEvent(CLEARRECT, aStream)
{
    ReadElement(aStream, mRect);
}

inline void
RecordedClearRect::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT<< "] ClearRect (" << mRect.x << ", " << mRect.y << " - " << mRect.width << " x " << mRect.height << ") ";
}

inline bool
RecordedCopySurface::PlayEvent(Translator *aTranslator) const
{
	aTranslator->LookupDrawTarget(mDT)->CopySurface(aTranslator->LookupSourceSurface(mSourceSurface),
                                                  mSourceRect, mDest);
  return true;
}

template<class S>
void
RecordedCopySurface::Record(S &aStream) const
{
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mSourceSurface);
  WriteElement(aStream, mSourceRect);
  WriteElement(aStream, mDest);
}

template<class S>
RecordedCopySurface::RecordedCopySurface(S &aStream)
  : RecordedDrawingEvent(COPYSURFACE, aStream)
{
  ReadElement(aStream, mSourceSurface);
  ReadElement(aStream, mSourceRect);
  ReadElement(aStream, mDest);
}

inline void
RecordedCopySurface::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT<< "] CopySurface (" << mSourceSurface << ")";
}

inline bool
RecordedPushClip::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->PushClip(aTranslator->LookupPath(mPath));
  return true;
}

template<class S>
void
RecordedPushClip::Record(S &aStream) const
{
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mPath);
}

template<class S>
RecordedPushClip::RecordedPushClip(S &aStream)
  : RecordedDrawingEvent(PUSHCLIP, aStream)
{
  ReadElement(aStream, mPath);
}

inline void
RecordedPushClip::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] PushClip (" << mPath << ") ";
}

inline bool
RecordedPushClipRect::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->PushClipRect(mRect);
  return true;
}

template<class S>
void
RecordedPushClipRect::Record(S &aStream) const
{
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mRect);
}

template<class S>
RecordedPushClipRect::RecordedPushClipRect(S &aStream)
  : RecordedDrawingEvent(PUSHCLIPRECT, aStream)
{
  ReadElement(aStream, mRect);
}

inline void
RecordedPushClipRect::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] PushClipRect (" << mRect.x << ", " << mRect.y << " - " << mRect.width << " x " << mRect.height << ") ";
}

inline bool
RecordedPopClip::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->PopClip();
  return true;
}

template<class S>
void
RecordedPopClip::Record(S &aStream) const
{
  RecordedDrawingEvent::Record(aStream);
}

template<class S>
RecordedPopClip::RecordedPopClip(S &aStream)
  : RecordedDrawingEvent(POPCLIP, aStream)
{
}

inline void
RecordedPopClip::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] PopClip";
}

inline bool
RecordedPushLayer::PlayEvent(Translator *aTranslator) const
{
  SourceSurface* mask = mMask ? aTranslator->LookupSourceSurface(mMask)
                              : nullptr;
  aTranslator->LookupDrawTarget(mDT)->
    PushLayer(mOpaque, mOpacity, mask, mMaskTransform, mBounds, mCopyBackground);
  return true;
}

template<class S>
void
RecordedPushLayer::Record(S &aStream) const
{
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mOpaque);
  WriteElement(aStream, mOpacity);
  WriteElement(aStream, mMask);
  WriteElement(aStream, mMaskTransform);
  WriteElement(aStream, mBounds);
  WriteElement(aStream, mCopyBackground);
}

template<class S>
RecordedPushLayer::RecordedPushLayer(S &aStream)
  : RecordedDrawingEvent(PUSHLAYER, aStream)
{
  ReadElement(aStream, mOpaque);
  ReadElement(aStream, mOpacity);
  ReadElement(aStream, mMask);
  ReadElement(aStream, mMaskTransform);
  ReadElement(aStream, mBounds);
  ReadElement(aStream, mCopyBackground);
}

inline void
RecordedPushLayer::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] PushPLayer (Opaque=" << mOpaque <<
    ", Opacity=" << mOpacity << ", Mask Ref=" << mMask << ") ";
}

inline bool
RecordedPopLayer::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->PopLayer();
  return true;
}

template<class S>
void
RecordedPopLayer::Record(S &aStream) const
{
  RecordedDrawingEvent::Record(aStream);
}

template<class S>
RecordedPopLayer::RecordedPopLayer(S &aStream)
  : RecordedDrawingEvent(POPLAYER, aStream)
{
}

inline void
RecordedPopLayer::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] PopLayer";
}

inline bool
RecordedSetTransform::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->SetTransform(mTransform);
  return true;
}

template<class S>
void
RecordedSetTransform::Record(S &aStream) const
{
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mTransform);
}

template<class S>
RecordedSetTransform::RecordedSetTransform(S &aStream)
  : RecordedDrawingEvent(SETTRANSFORM, aStream)
{
  ReadElement(aStream, mTransform);
}

inline void
RecordedSetTransform::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] SetTransform [ " << mTransform._11 << " " << mTransform._12 << " ; " <<
    mTransform._21 << " " << mTransform._22 << " ; " << mTransform._31 << " " << mTransform._32 << " ]";
}

inline bool
RecordedDrawSurface::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->
    DrawSurface(aTranslator->LookupSourceSurface(mRefSource), mDest, mSource,
                mDSOptions, mOptions);
  return true;
}

template<class S>
void
RecordedDrawSurface::Record(S &aStream) const
{
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mRefSource);
  WriteElement(aStream, mDest);
  WriteElement(aStream, mSource);
  WriteElement(aStream, mDSOptions);
  WriteElement(aStream, mOptions);
}

template<class S>
RecordedDrawSurface::RecordedDrawSurface(S &aStream)
  : RecordedDrawingEvent(DRAWSURFACE, aStream)
{
  ReadElement(aStream, mRefSource);
  ReadElement(aStream, mDest);
  ReadElement(aStream, mSource);
  ReadElement(aStream, mDSOptions);
  ReadElement(aStream, mOptions);
}

inline void
RecordedDrawSurface::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] DrawSurface (" << mRefSource << ")";
}

inline bool
RecordedDrawFilter::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->
    DrawFilter(aTranslator->LookupFilterNode(mNode), mSourceRect,
                mDestPoint, mOptions);
  return true;
}

template<class S>
void
RecordedDrawFilter::Record(S &aStream) const
{
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mNode);
  WriteElement(aStream, mSourceRect);
  WriteElement(aStream, mDestPoint);
  WriteElement(aStream, mOptions);
}

template<class S>
RecordedDrawFilter::RecordedDrawFilter(S &aStream)
  : RecordedDrawingEvent(DRAWFILTER, aStream)
{
  ReadElement(aStream, mNode);
  ReadElement(aStream, mSourceRect);
  ReadElement(aStream, mDestPoint);
  ReadElement(aStream, mOptions);
}

inline void
RecordedDrawFilter::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] DrawFilter (" << mNode << ")";
}

inline bool
RecordedDrawSurfaceWithShadow::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->
    DrawSurfaceWithShadow(aTranslator->LookupSourceSurface(mRefSource),
                          mDest, mColor, mOffset, mSigma, mOp);
  return true;
}

template<class S>
void
RecordedDrawSurfaceWithShadow::Record(S &aStream) const
{
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mRefSource);
  WriteElement(aStream, mDest);
  WriteElement(aStream, mColor);
  WriteElement(aStream, mOffset);
  WriteElement(aStream, mSigma);
  WriteElement(aStream, mOp);
}

template<class S>
RecordedDrawSurfaceWithShadow::RecordedDrawSurfaceWithShadow(S &aStream)
  : RecordedDrawingEvent(DRAWSURFACEWITHSHADOW, aStream)
{
  ReadElement(aStream, mRefSource);
  ReadElement(aStream, mDest);
  ReadElement(aStream, mColor);
  ReadElement(aStream, mOffset);
  ReadElement(aStream, mSigma);
  ReadElement(aStream, mOp);
}

inline void
RecordedDrawSurfaceWithShadow::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mDT << "] DrawSurfaceWithShadow (" << mRefSource << ") Color: (" <<
    mColor.r << ", " << mColor.g << ", " << mColor.b << ", " << mColor.a << ")";
}

inline
RecordedPathCreation::RecordedPathCreation(PathRecording *aPath)
  : RecordedEventDerived(PATHCREATION), mRefPtr(aPath), mFillRule(aPath->mFillRule), mPathOps(aPath->mPathOps)
{
}

inline
RecordedPathCreation::~RecordedPathCreation()
{
}

inline bool
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

template<class S>
void
RecordedPathCreation::Record(S &aStream) const
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

template<class S>
RecordedPathCreation::RecordedPathCreation(S &aStream)
  : RecordedEventDerived(PATHCREATION)
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

inline void
RecordedPathCreation::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] Path created (OpCount: " << mPathOps.size() << ")";
}
inline bool
RecordedPathDestruction::PlayEvent(Translator *aTranslator) const
{
  aTranslator->RemovePath(mRefPtr);
  return true;
}

template<class S>
void
RecordedPathDestruction::Record(S &aStream) const
{
  WriteElement(aStream, mRefPtr);
}

template<class S>
RecordedPathDestruction::RecordedPathDestruction(S &aStream)
  : RecordedEventDerived(PATHDESTRUCTION)
{
  ReadElement(aStream, mRefPtr);
}

inline void
RecordedPathDestruction::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] Path Destroyed";
}

inline
RecordedSourceSurfaceCreation::~RecordedSourceSurfaceCreation()
{
  if (mDataOwned) {
    delete [] mData;
  }
}

inline bool
RecordedSourceSurfaceCreation::PlayEvent(Translator *aTranslator) const
{
  if (!mData) {
    return false;
  }

  RefPtr<SourceSurface> src = aTranslator->GetReferenceDrawTarget()->
    CreateSourceSurfaceFromData(mData, mSize, mSize.width * BytesPerPixel(mFormat), mFormat);
  aTranslator->AddSourceSurface(mRefPtr, src);
  return true;
}

template<class S>
void
RecordedSourceSurfaceCreation::Record(S &aStream) const
{
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mSize);
  WriteElement(aStream, mFormat);
  MOZ_ASSERT(mData);
  for (int y = 0; y < mSize.height; y++) {
    aStream.write((const char*)mData + y * mStride, BytesPerPixel(mFormat) * mSize.width);
  }
}

template<class S>
RecordedSourceSurfaceCreation::RecordedSourceSurfaceCreation(S &aStream)
  : RecordedEventDerived(SOURCESURFACECREATION), mDataOwned(true)
{
  ReadElement(aStream, mRefPtr);
  ReadElement(aStream, mSize);
  ReadElement(aStream, mFormat);
  mData = (uint8_t*)new (fallible) char[mSize.width * mSize.height * BytesPerPixel(mFormat)];
  if (!mData) {
    gfxWarning() << "RecordedSourceSurfaceCreation failed to allocate data";
  } else {
    aStream.read((char*)mData, mSize.width * mSize.height * BytesPerPixel(mFormat));
  }
}

inline void
RecordedSourceSurfaceCreation::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] SourceSurface created (Size: " << mSize.width << "x" << mSize.height << ")";
}

inline bool
RecordedSourceSurfaceDestruction::PlayEvent(Translator *aTranslator) const
{
  aTranslator->RemoveSourceSurface(mRefPtr);
  return true;
}

template<class S>
void
RecordedSourceSurfaceDestruction::Record(S &aStream) const
{
  WriteElement(aStream, mRefPtr);
}

template<class S>
RecordedSourceSurfaceDestruction::RecordedSourceSurfaceDestruction(S &aStream)
  : RecordedEventDerived(SOURCESURFACEDESTRUCTION)
{
  ReadElement(aStream, mRefPtr);
}

inline void
RecordedSourceSurfaceDestruction::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] SourceSurface Destroyed";
}

inline
RecordedFilterNodeCreation::~RecordedFilterNodeCreation()
{
}

inline bool
RecordedFilterNodeCreation::PlayEvent(Translator *aTranslator) const
{
  RefPtr<FilterNode> node = aTranslator->GetReferenceDrawTarget()->
    CreateFilter(mType);
  aTranslator->AddFilterNode(mRefPtr, node);
  return true;
}

template<class S>
void
RecordedFilterNodeCreation::Record(S &aStream) const
{
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mType);
}

template<class S>
RecordedFilterNodeCreation::RecordedFilterNodeCreation(S &aStream)
  : RecordedEventDerived(FILTERNODECREATION)
{
  ReadElement(aStream, mRefPtr);
  ReadElement(aStream, mType);
}

inline void
RecordedFilterNodeCreation::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] FilterNode created (Type: " << int(mType) << ")";
}

inline bool
RecordedFilterNodeDestruction::PlayEvent(Translator *aTranslator) const
{
  aTranslator->RemoveFilterNode(mRefPtr);
  return true;
}

template<class S>
void
RecordedFilterNodeDestruction::Record(S &aStream) const
{
  WriteElement(aStream, mRefPtr);
}

template<class S>
RecordedFilterNodeDestruction::RecordedFilterNodeDestruction(S &aStream)
  : RecordedEventDerived(FILTERNODEDESTRUCTION)
{
  ReadElement(aStream, mRefPtr);
}

inline void
RecordedFilterNodeDestruction::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] FilterNode Destroyed";
}

inline
RecordedGradientStopsCreation::~RecordedGradientStopsCreation()
{
  if (mDataOwned) {
    delete [] mStops;
  }
}

inline bool
RecordedGradientStopsCreation::PlayEvent(Translator *aTranslator) const
{
  RefPtr<GradientStops> src = aTranslator->GetReferenceDrawTarget()->
    CreateGradientStops(mStops, mNumStops, mExtendMode);
  aTranslator->AddGradientStops(mRefPtr, src);
  return true;
}

template<class S>
void
RecordedGradientStopsCreation::Record(S &aStream) const
{
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mExtendMode);
  WriteElement(aStream, mNumStops);
  aStream.write((const char*)mStops, mNumStops * sizeof(GradientStop));
}

template<class S>
RecordedGradientStopsCreation::RecordedGradientStopsCreation(S &aStream)
  : RecordedEventDerived(GRADIENTSTOPSCREATION), mDataOwned(true)
{
  ReadElement(aStream, mRefPtr);
  ReadElement(aStream, mExtendMode);
  ReadElement(aStream, mNumStops);
  mStops = new GradientStop[mNumStops];

  aStream.read((char*)mStops, mNumStops * sizeof(GradientStop));
}

inline void
RecordedGradientStopsCreation::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] GradientStops created (Stops: " << mNumStops << ")";
}

inline bool
RecordedGradientStopsDestruction::PlayEvent(Translator *aTranslator) const
{
  aTranslator->RemoveGradientStops(mRefPtr);
  return true;
}

template<class S>
void
RecordedGradientStopsDestruction::Record(S &aStream) const
{
  WriteElement(aStream, mRefPtr);
}

template<class S>
RecordedGradientStopsDestruction::RecordedGradientStopsDestruction(S &aStream)
  : RecordedEventDerived(GRADIENTSTOPSDESTRUCTION)
{
  ReadElement(aStream, mRefPtr);
}

inline void
RecordedGradientStopsDestruction::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] GradientStops Destroyed";
}

inline bool
RecordedIntoLuminanceSource::PlayEvent(Translator *aTranslator) const
{
  RefPtr<SourceSurface> src = aTranslator->LookupDrawTarget(mDT)->IntoLuminanceSource(mLuminanceType, mOpacity);
  aTranslator->AddSourceSurface(mRefPtr, src);
  return true;
}

template<class S>
void
RecordedIntoLuminanceSource::Record(S &aStream) const
{
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mDT);
  WriteElement(aStream, mLuminanceType);
  WriteElement(aStream, mOpacity);
}

template<class S>
RecordedIntoLuminanceSource::RecordedIntoLuminanceSource(S &aStream)
  : RecordedEventDerived(SNAPSHOT)
{
  ReadElement(aStream, mRefPtr);
  ReadElement(aStream, mDT);
  ReadElement(aStream, mLuminanceType);
  ReadElement(aStream, mOpacity);
}

inline void
RecordedIntoLuminanceSource::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] Into Luminance Source (DT: " << mDT << ")";
}

inline bool
RecordedSnapshot::PlayEvent(Translator *aTranslator) const
{
  RefPtr<SourceSurface> src = aTranslator->LookupDrawTarget(mDT)->Snapshot();
  aTranslator->AddSourceSurface(mRefPtr, src);
  return true;
}

template<class S>
void
RecordedSnapshot::Record(S &aStream) const
{
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mDT);
}

template<class S>
RecordedSnapshot::RecordedSnapshot(S &aStream)
  : RecordedEventDerived(SNAPSHOT)
{
  ReadElement(aStream, mRefPtr);
  ReadElement(aStream, mDT);
}

inline void
RecordedSnapshot::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] Snapshot Created (DT: " << mDT << ")";
}

inline
RecordedFontData::~RecordedFontData()
{
  delete[] mData;
}

inline bool
RecordedFontData::PlayEvent(Translator *aTranslator) const
{
  RefPtr<NativeFontResource> fontResource =
    Factory::CreateNativeFontResource(mData, mFontDetails.size,
                                      aTranslator->GetDesiredFontType(),
                                      aTranslator->GetFontContext());
  if (!fontResource) {
    return false;
  }

  aTranslator->AddNativeFontResource(mFontDetails.fontDataKey, fontResource);
  return true;
}

template<class S>
void
RecordedFontData::Record(S &aStream) const
{
  MOZ_ASSERT(mGetFontFileDataSucceeded);

  WriteElement(aStream, mFontDetails.fontDataKey);
  WriteElement(aStream, mFontDetails.size);
  aStream.write((const char*)mData, mFontDetails.size);
}

inline void
RecordedFontData::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "Font Data of size " << mFontDetails.size;
}

inline void
RecordedFontData::SetFontData(const uint8_t *aData, uint32_t aSize, uint32_t aIndex)
{
  mData = new uint8_t[aSize];
  memcpy(mData, aData, aSize);
  mFontDetails.fontDataKey =
    SFNTData::GetUniqueKey(aData, aSize, 0, nullptr);
  mFontDetails.size = aSize;
  mFontDetails.index = aIndex;
}

inline bool
RecordedFontData::GetFontDetails(RecordedFontDetails& fontDetails)
{
  if (!mGetFontFileDataSucceeded) {
    return false;
  }

  fontDetails.fontDataKey = mFontDetails.fontDataKey;
  fontDetails.size = mFontDetails.size;
  fontDetails.index = mFontDetails.index;
  return true;
}

template<class S>
RecordedFontData::RecordedFontData(S &aStream)
  : RecordedEventDerived(FONTDATA)
  , mData(nullptr)
{
  ReadElement(aStream, mFontDetails.fontDataKey);
  ReadElement(aStream, mFontDetails.size);
  mData = new uint8_t[mFontDetails.size];
  aStream.read((char*)mData, mFontDetails.size);
}

inline
RecordedFontDescriptor::~RecordedFontDescriptor()
{
}

inline bool
RecordedFontDescriptor::PlayEvent(Translator *aTranslator) const
{
  RefPtr<UnscaledFont> font =
    Factory::CreateUnscaledFontFromFontDescriptor(mType, mData.data(), mData.size());
  if (!font) {
    gfxDevCrash(LogReason::InvalidFont) <<
      "Failed creating UnscaledFont of type " << int(mType) << " from font descriptor";
    return false;
  }

  aTranslator->AddUnscaledFont(mRefPtr, font);
  return true;
}

template<class S>
void
RecordedFontDescriptor::Record(S &aStream) const
{
  MOZ_ASSERT(mHasDesc);
  WriteElement(aStream, mType);
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, (size_t)mData.size());
  aStream.write((char*)mData.data(), mData.size());
}

inline void
RecordedFontDescriptor::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] Font Descriptor";
}

inline void
RecordedFontDescriptor::SetFontDescriptor(const uint8_t* aData, uint32_t aSize)
{
  mData.assign(aData, aData + aSize);
}

template<class S>
RecordedFontDescriptor::RecordedFontDescriptor(S &aStream)
  : RecordedEventDerived(FONTDESC)
{
  ReadElement(aStream, mType);
  ReadElement(aStream, mRefPtr);

  size_t size;
  ReadElement(aStream, size);
  mData.resize(size);
  aStream.read((char*)mData.data(), size);
}

inline bool
RecordedUnscaledFontCreation::PlayEvent(Translator *aTranslator) const
{
  NativeFontResource *fontResource = aTranslator->LookupNativeFontResource(mFontDataKey);
  if (!fontResource) {
    gfxDevCrash(LogReason::NativeFontResourceNotFound) <<
      "NativeFontResource lookup failed for key |" << hexa(mFontDataKey) << "|.";
    return false;
  }

  RefPtr<UnscaledFont> unscaledFont =
    fontResource->CreateUnscaledFont(mIndex, mInstanceData.data(), mInstanceData.size());
  aTranslator->AddUnscaledFont(mRefPtr, unscaledFont);
  return true;
}

template<class S>
void
RecordedUnscaledFontCreation::Record(S &aStream) const
{
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mFontDataKey);
  WriteElement(aStream, mIndex);
  WriteElement(aStream, (size_t)mInstanceData.size());
  aStream.write((char*)mInstanceData.data(), mInstanceData.size());
}

inline void
RecordedUnscaledFontCreation::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] UnscaledFont Created";
}

inline void
RecordedUnscaledFontCreation::SetFontInstanceData(const uint8_t *aData, uint32_t aSize)
{
  mInstanceData.assign(aData, aData + aSize);
}

template<class S>
RecordedUnscaledFontCreation::RecordedUnscaledFontCreation(S &aStream)
  : RecordedEventDerived(UNSCALEDFONTCREATION)
{
  ReadElement(aStream, mRefPtr);
  ReadElement(aStream, mFontDataKey);
  ReadElement(aStream, mIndex);

  size_t size;
  ReadElement(aStream, size);
  mInstanceData.resize(size);
  aStream.read((char*)mInstanceData.data(), size);
}

inline bool
RecordedUnscaledFontDestruction::PlayEvent(Translator *aTranslator) const
{
  aTranslator->RemoveUnscaledFont(mRefPtr);
  return true;
}

template<class S>
void
RecordedUnscaledFontDestruction::Record(S &aStream) const
{
  WriteElement(aStream, mRefPtr);
}

template<class S>
RecordedUnscaledFontDestruction::RecordedUnscaledFontDestruction(S &aStream)
  : RecordedEventDerived(UNSCALEDFONTDESTRUCTION)
{
  ReadElement(aStream, mRefPtr);
}

inline void
RecordedUnscaledFontDestruction::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] UnscaledFont Destroyed";
}

inline bool
RecordedScaledFontCreation::PlayEvent(Translator *aTranslator) const
{
  UnscaledFont* unscaledFont = aTranslator->LookupUnscaledFont(mUnscaledFont);
  if (!unscaledFont) {
    gfxDevCrash(LogReason::UnscaledFontNotFound) <<
      "UnscaledFont lookup failed for key |" << hexa(mUnscaledFont) << "|.";
    return false;
  }

  RefPtr<ScaledFont> scaledFont =
    unscaledFont->CreateScaledFont(mGlyphSize, mInstanceData.data(), mInstanceData.size());
  aTranslator->AddScaledFont(mRefPtr, scaledFont);
  return true;
}

template<class S>
void
RecordedScaledFontCreation::Record(S &aStream) const
{
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mUnscaledFont);
  WriteElement(aStream, mGlyphSize);
  WriteElement(aStream, (size_t)mInstanceData.size());
  aStream.write((char*)mInstanceData.data(), mInstanceData.size());
}

inline void
RecordedScaledFontCreation::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] ScaledFont Created";
}

inline void
RecordedScaledFontCreation::SetFontInstanceData(const uint8_t *aData, uint32_t aSize)
{
  mInstanceData.assign(aData, aData + aSize);
}

template<class S>
RecordedScaledFontCreation::RecordedScaledFontCreation(S &aStream)
  : RecordedEventDerived(SCALEDFONTCREATION)
{
  ReadElement(aStream, mRefPtr);
  ReadElement(aStream, mUnscaledFont);
  ReadElement(aStream, mGlyphSize);

  size_t size;
  ReadElement(aStream, size);
  mInstanceData.resize(size);
  aStream.read((char*)mInstanceData.data(), size);
}

inline bool
RecordedScaledFontDestruction::PlayEvent(Translator *aTranslator) const
{
  aTranslator->RemoveScaledFont(mRefPtr);
  return true;
}

template<class S>
void
RecordedScaledFontDestruction::Record(S &aStream) const
{
  WriteElement(aStream, mRefPtr);
}

template<class S>
RecordedScaledFontDestruction::RecordedScaledFontDestruction(S &aStream)
  : RecordedEventDerived(SCALEDFONTDESTRUCTION)
{
  ReadElement(aStream, mRefPtr);
}

inline void
RecordedScaledFontDestruction::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mRefPtr << "] ScaledFont Destroyed";
}

inline bool
RecordedMaskSurface::PlayEvent(Translator *aTranslator) const
{
  aTranslator->LookupDrawTarget(mDT)->
    MaskSurface(*GenericPattern(mPattern, aTranslator),
                aTranslator->LookupSourceSurface(mRefMask),
                mOffset, mOptions);
  return true;
}

template<class S>
void
RecordedMaskSurface::Record(S &aStream) const
{
  RecordedDrawingEvent::Record(aStream);
  RecordPatternData(aStream, mPattern);
  WriteElement(aStream, mRefMask);
  WriteElement(aStream, mOffset);
  WriteElement(aStream, mOptions);
}

template<class S>
RecordedMaskSurface::RecordedMaskSurface(S &aStream)
  : RecordedDrawingEvent(MASKSURFACE, aStream)
{
  ReadPatternData(aStream, mPattern);
  ReadElement(aStream, mRefMask);
  ReadElement(aStream, mOffset);
  ReadElement(aStream, mOptions);
}

inline void
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

inline bool
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

template<class S>
void
RecordedFilterNodeSetAttribute::Record(S &aStream) const
{
  WriteElement(aStream, mNode);
  WriteElement(aStream, mIndex);
  WriteElement(aStream, mArgType);
  WriteElement(aStream, uint64_t(mPayload.size()));
  aStream.write((const char*)&mPayload.front(), mPayload.size());
}

template<class S>
RecordedFilterNodeSetAttribute::RecordedFilterNodeSetAttribute(S &aStream)
  : RecordedEventDerived(FILTERNODESETATTRIBUTE)
{
  ReadElement(aStream, mNode);
  ReadElement(aStream, mIndex);
  ReadElement(aStream, mArgType);
  uint64_t size;
  ReadElement(aStream, size);
  mPayload.resize(size_t(size));
  aStream.read((char*)&mPayload.front(), size);
}

inline void
RecordedFilterNodeSetAttribute::OutputSimpleEventInfo(stringstream &aStringStream) const
{
  aStringStream << "[" << mNode << "] SetAttribute (" << mIndex << ")";
}

inline bool
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

template<class S>
void
RecordedFilterNodeSetInput::Record(S &aStream) const
{
  WriteElement(aStream, mNode);
  WriteElement(aStream, mIndex);
  WriteElement(aStream, mInputFilter);
  WriteElement(aStream, mInputSurface);
}

template<class S>
RecordedFilterNodeSetInput::RecordedFilterNodeSetInput(S &aStream)
  : RecordedEventDerived(FILTERNODESETINPUT)
{
  ReadElement(aStream, mNode);
  ReadElement(aStream, mIndex);
  ReadElement(aStream, mInputFilter);
  ReadElement(aStream, mInputSurface);
}

inline void
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

#define LOAD_EVENT_TYPE(_typeenum, _class) \
  case _typeenum: return new _class(aStream)

template<class S>
RecordedEvent *
RecordedEvent::LoadEvent(S &aStream, EventType aType)
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
    LOAD_EVENT_TYPE(UNSCALEDFONTCREATION, RecordedUnscaledFontCreation);
    LOAD_EVENT_TYPE(UNSCALEDFONTDESTRUCTION, RecordedUnscaledFontDestruction);
  default:
    return nullptr;
  }
}


} // namespace gfx
} // namespace mozilla

#endif
