/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
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

template <class Derived>
class RecordedDrawingEvent : public RecordedEventDerived<Derived> {
 public:
  ReferencePtr GetDestinedDT() override { return mDT; }

 protected:
  RecordedDrawingEvent(RecordedEvent::EventType aType, DrawTarget* aTarget)
      : RecordedEventDerived<Derived>(aType), mDT(aTarget) {}

  template <class S>
  RecordedDrawingEvent(RecordedEvent::EventType aType, S& aStream);
  template <class S>
  void Record(S& aStream) const;

  ReferencePtr mDT;
};

class RecordedDrawTargetCreation
    : public RecordedEventDerived<RecordedDrawTargetCreation> {
 public:
  RecordedDrawTargetCreation(ReferencePtr aRefPtr, BackendType aType,
                             const IntRect& aRect, SurfaceFormat aFormat,
                             bool aHasExistingData = false,
                             SourceSurface* aExistingData = nullptr)
      : RecordedEventDerived(DRAWTARGETCREATION),
        mRefPtr(aRefPtr),
        mBackendType(aType),
        mRect(aRect),
        mFormat(aFormat),
        mHasExistingData(aHasExistingData),
        mExistingData(aExistingData) {}

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  virtual void OutputSimpleEventInfo(
      std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "DrawTarget Creation"; }

  ReferencePtr mRefPtr;
  BackendType mBackendType;
  IntRect mRect;
  SurfaceFormat mFormat;
  bool mHasExistingData;
  RefPtr<SourceSurface> mExistingData;

 private:
  friend class RecordedEvent;

  template <class S>
  MOZ_IMPLICIT RecordedDrawTargetCreation(S& aStream);
};

class RecordedDrawTargetDestruction
    : public RecordedEventDerived<RecordedDrawTargetDestruction> {
 public:
  MOZ_IMPLICIT RecordedDrawTargetDestruction(ReferencePtr aRefPtr)
      : RecordedEventDerived(DRAWTARGETDESTRUCTION),
        mRefPtr(aRefPtr),
        mBackendType(BackendType::NONE) {}

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "DrawTarget Destruction"; }

  ReferencePtr mRefPtr;

  BackendType mBackendType;

 private:
  friend class RecordedEvent;

  template <class S>
  MOZ_IMPLICIT RecordedDrawTargetDestruction(S& aStream);
};

class RecordedCreateSimilarDrawTarget
    : public RecordedEventDerived<RecordedCreateSimilarDrawTarget> {
 public:
  RecordedCreateSimilarDrawTarget(ReferencePtr aRefPtr, const IntSize& aSize,
                                  SurfaceFormat aFormat)
      : RecordedEventDerived(CREATESIMILARDRAWTARGET),
        mRefPtr(aRefPtr),
        mSize(aSize),
        mFormat(aFormat) {}

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  virtual void OutputSimpleEventInfo(
      std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "CreateSimilarDrawTarget"; }

  ReferencePtr mRefPtr;
  IntSize mSize;
  SurfaceFormat mFormat;

 private:
  friend class RecordedEvent;

  template <class S>
  MOZ_IMPLICIT RecordedCreateSimilarDrawTarget(S& aStream);
};

class RecordedCreateClippedDrawTarget
    : public RecordedDrawingEvent<RecordedCreateClippedDrawTarget> {
 public:
  RecordedCreateClippedDrawTarget(DrawTarget* aDT, ReferencePtr aRefPtr,
                                  const Rect& aBounds, SurfaceFormat aFormat)
      : RecordedDrawingEvent(CREATECLIPPEDDRAWTARGET, aDT),
        mRefPtr(aRefPtr),
        mBounds(aBounds),
        mFormat(aFormat) {}

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  virtual void OutputSimpleEventInfo(
      std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "CreateClippedDrawTarget"; }

  ReferencePtr mRefPtr;
  Rect mBounds;
  SurfaceFormat mFormat;

 private:
  friend class RecordedEvent;

  template <class S>
  MOZ_IMPLICIT RecordedCreateClippedDrawTarget(S& aStream);
};

class RecordedCreateDrawTargetForFilter
    : public RecordedDrawingEvent<RecordedCreateDrawTargetForFilter> {
 public:
  RecordedCreateDrawTargetForFilter(DrawTarget* aDT, ReferencePtr aRefPtr,
                                    const IntSize& aMaxSize,
                                    SurfaceFormat aFormat, FilterNode* aFilter,
                                    FilterNode* aSource,
                                    const Rect& aSourceRect,
                                    const Point& aDestPoint)
      : RecordedDrawingEvent(CREATEDRAWTARGETFORFILTER, aDT),
        mRefPtr(aRefPtr),
        mMaxSize(aMaxSize),
        mFormat(aFormat),
        mFilter(aFilter),
        mSource(aSource),
        mSourceRect(aSourceRect),
        mDestPoint(aDestPoint) {}

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  virtual void OutputSimpleEventInfo(
      std::stringstream& aStringStream) const override;

  std::string GetName() const override {
    return "CreateSimilarDrawTargetForFilter";
  }

  ReferencePtr mRefPtr;
  IntSize mMaxSize;
  SurfaceFormat mFormat;
  ReferencePtr mFilter;
  ReferencePtr mSource;
  Rect mSourceRect;
  Point mDestPoint;

 private:
  friend class RecordedEvent;

  template <class S>
  MOZ_IMPLICIT RecordedCreateDrawTargetForFilter(S& aStream);
};

class RecordedFillRect : public RecordedDrawingEvent<RecordedFillRect> {
 public:
  RecordedFillRect(DrawTarget* aDT, const Rect& aRect, const Pattern& aPattern,
                   const DrawOptions& aOptions)
      : RecordedDrawingEvent(FILLRECT, aDT),
        mRect(aRect),
        mPattern(),
        mOptions(aOptions) {
    StorePattern(mPattern, aPattern);
  }

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "FillRect"; }

 private:
  friend class RecordedEvent;

  template <class S>
  MOZ_IMPLICIT RecordedFillRect(S& aStream);

  Rect mRect;
  PatternStorage mPattern;
  DrawOptions mOptions;
};

class RecordedStrokeRect : public RecordedDrawingEvent<RecordedStrokeRect> {
 public:
  RecordedStrokeRect(DrawTarget* aDT, const Rect& aRect,
                     const Pattern& aPattern,
                     const StrokeOptions& aStrokeOptions,
                     const DrawOptions& aOptions)
      : RecordedDrawingEvent(STROKERECT, aDT),
        mRect(aRect),
        mPattern(),
        mStrokeOptions(aStrokeOptions),
        mOptions(aOptions) {
    StorePattern(mPattern, aPattern);
  }

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "StrokeRect"; }

 private:
  friend class RecordedEvent;

  template <class S>
  MOZ_IMPLICIT RecordedStrokeRect(S& aStream);

  Rect mRect;
  PatternStorage mPattern;
  StrokeOptions mStrokeOptions;
  DrawOptions mOptions;
};

class RecordedStrokeLine : public RecordedDrawingEvent<RecordedStrokeLine> {
 public:
  RecordedStrokeLine(DrawTarget* aDT, const Point& aBegin, const Point& aEnd,
                     const Pattern& aPattern,
                     const StrokeOptions& aStrokeOptions,
                     const DrawOptions& aOptions)
      : RecordedDrawingEvent(STROKELINE, aDT),
        mBegin(aBegin),
        mEnd(aEnd),
        mPattern(),
        mStrokeOptions(aStrokeOptions),
        mOptions(aOptions) {
    StorePattern(mPattern, aPattern);
  }

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "StrokeLine"; }

 private:
  friend class RecordedEvent;

  template <class S>
  MOZ_IMPLICIT RecordedStrokeLine(S& aStream);

  Point mBegin;
  Point mEnd;
  PatternStorage mPattern;
  StrokeOptions mStrokeOptions;
  DrawOptions mOptions;
};

class RecordedFill : public RecordedDrawingEvent<RecordedFill> {
 public:
  RecordedFill(DrawTarget* aDT, ReferencePtr aPath, const Pattern& aPattern,
               const DrawOptions& aOptions)
      : RecordedDrawingEvent(FILL, aDT),
        mPath(aPath),
        mPattern(),
        mOptions(aOptions) {
    StorePattern(mPattern, aPattern);
  }

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "Fill"; }

 private:
  friend class RecordedEvent;

  template <class S>
  MOZ_IMPLICIT RecordedFill(S& aStream);

  ReferencePtr mPath;
  PatternStorage mPattern;
  DrawOptions mOptions;
};

class RecordedFillGlyphs : public RecordedDrawingEvent<RecordedFillGlyphs> {
 public:
  RecordedFillGlyphs(DrawTarget* aDT, ReferencePtr aScaledFont,
                     const Pattern& aPattern, const DrawOptions& aOptions,
                     const Glyph* aGlyphs, uint32_t aNumGlyphs)
      : RecordedDrawingEvent(FILLGLYPHS, aDT),
        mScaledFont(aScaledFont),
        mPattern(),
        mOptions(aOptions) {
    StorePattern(mPattern, aPattern);
    mNumGlyphs = aNumGlyphs;
    mGlyphs = new Glyph[aNumGlyphs];
    memcpy(mGlyphs, aGlyphs, sizeof(Glyph) * aNumGlyphs);
  }
  virtual ~RecordedFillGlyphs();

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "FillGlyphs"; }

 private:
  friend class RecordedEvent;

  template <class S>
  MOZ_IMPLICIT RecordedFillGlyphs(S& aStream);

  ReferencePtr mScaledFont;
  PatternStorage mPattern;
  DrawOptions mOptions;
  Glyph* mGlyphs;
  uint32_t mNumGlyphs;
};

class RecordedMask : public RecordedDrawingEvent<RecordedMask> {
 public:
  RecordedMask(DrawTarget* aDT, const Pattern& aSource, const Pattern& aMask,
               const DrawOptions& aOptions)
      : RecordedDrawingEvent(MASK, aDT),
        mSource(),
        mMask(),
        mOptions(aOptions) {
    StorePattern(mSource, aSource);
    StorePattern(mMask, aMask);
  }

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "Mask"; }

 private:
  friend class RecordedEvent;

  template <class S>
  MOZ_IMPLICIT RecordedMask(S& aStream);

  PatternStorage mSource;
  PatternStorage mMask;
  DrawOptions mOptions;
};

class RecordedStroke : public RecordedDrawingEvent<RecordedStroke> {
 public:
  RecordedStroke(DrawTarget* aDT, ReferencePtr aPath, const Pattern& aPattern,
                 const StrokeOptions& aStrokeOptions,
                 const DrawOptions& aOptions)
      : RecordedDrawingEvent(STROKE, aDT),
        mPath(aPath),
        mPattern(),
        mStrokeOptions(aStrokeOptions),
        mOptions(aOptions) {
    StorePattern(mPattern, aPattern);
  }

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  virtual void OutputSimpleEventInfo(
      std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "Stroke"; }

 private:
  friend class RecordedEvent;

  template <class S>
  MOZ_IMPLICIT RecordedStroke(S& aStream);

  ReferencePtr mPath;
  PatternStorage mPattern;
  StrokeOptions mStrokeOptions;
  DrawOptions mOptions;
};

class RecordedClearRect : public RecordedDrawingEvent<RecordedClearRect> {
 public:
  RecordedClearRect(DrawTarget* aDT, const Rect& aRect)
      : RecordedDrawingEvent(CLEARRECT, aDT), mRect(aRect) {}

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "ClearRect"; }

 private:
  friend class RecordedEvent;

  template <class S>
  MOZ_IMPLICIT RecordedClearRect(S& aStream);

  Rect mRect;
};

class RecordedCopySurface : public RecordedDrawingEvent<RecordedCopySurface> {
 public:
  RecordedCopySurface(DrawTarget* aDT, ReferencePtr aSourceSurface,
                      const IntRect& aSourceRect, const IntPoint& aDest)
      : RecordedDrawingEvent(COPYSURFACE, aDT),
        mSourceSurface(aSourceSurface),
        mSourceRect(aSourceRect),
        mDest(aDest) {}

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "CopySurface"; }

 private:
  friend class RecordedEvent;

  template <class S>
  MOZ_IMPLICIT RecordedCopySurface(S& aStream);

  ReferencePtr mSourceSurface;
  IntRect mSourceRect;
  IntPoint mDest;
};

class RecordedPushClip : public RecordedDrawingEvent<RecordedPushClip> {
 public:
  RecordedPushClip(DrawTarget* aDT, ReferencePtr aPath)
      : RecordedDrawingEvent(PUSHCLIP, aDT), mPath(aPath) {}

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "PushClip"; }

 private:
  friend class RecordedEvent;

  template <class S>
  MOZ_IMPLICIT RecordedPushClip(S& aStream);

  ReferencePtr mPath;
};

class RecordedPushClipRect : public RecordedDrawingEvent<RecordedPushClipRect> {
 public:
  RecordedPushClipRect(DrawTarget* aDT, const Rect& aRect)
      : RecordedDrawingEvent(PUSHCLIPRECT, aDT), mRect(aRect) {}

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "PushClipRect"; }

 private:
  friend class RecordedEvent;

  template <class S>
  MOZ_IMPLICIT RecordedPushClipRect(S& aStream);

  Rect mRect;
};

class RecordedPopClip : public RecordedDrawingEvent<RecordedPopClip> {
 public:
  MOZ_IMPLICIT RecordedPopClip(DrawTarget* aDT)
      : RecordedDrawingEvent(POPCLIP, aDT) {}

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "PopClip"; }

 private:
  friend class RecordedEvent;

  template <class S>
  MOZ_IMPLICIT RecordedPopClip(S& aStream);
};

class RecordedPushLayer : public RecordedDrawingEvent<RecordedPushLayer> {
 public:
  RecordedPushLayer(DrawTarget* aDT, bool aOpaque, Float aOpacity,
                    SourceSurface* aMask, const Matrix& aMaskTransform,
                    const IntRect& aBounds, bool aCopyBackground)
      : RecordedDrawingEvent(PUSHLAYER, aDT),
        mOpaque(aOpaque),
        mOpacity(aOpacity),
        mMask(aMask),
        mMaskTransform(aMaskTransform),
        mBounds(aBounds),
        mCopyBackground(aCopyBackground) {}

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "PushLayer"; }

 private:
  friend class RecordedEvent;

  template <class S>
  MOZ_IMPLICIT RecordedPushLayer(S& aStream);

  bool mOpaque;
  Float mOpacity;
  ReferencePtr mMask;
  Matrix mMaskTransform;
  IntRect mBounds;
  bool mCopyBackground;
};

class RecordedPushLayerWithBlend
    : public RecordedDrawingEvent<RecordedPushLayerWithBlend> {
 public:
  RecordedPushLayerWithBlend(DrawTarget* aDT, bool aOpaque, Float aOpacity,
                             SourceSurface* aMask, const Matrix& aMaskTransform,
                             const IntRect& aBounds, bool aCopyBackground,
                             CompositionOp aCompositionOp)
      : RecordedDrawingEvent(PUSHLAYERWITHBLEND, aDT),
        mOpaque(aOpaque),
        mOpacity(aOpacity),
        mMask(aMask),
        mMaskTransform(aMaskTransform),
        mBounds(aBounds),
        mCopyBackground(aCopyBackground),
        mCompositionOp(aCompositionOp) {}

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  virtual void OutputSimpleEventInfo(
      std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "PushLayerWithBlend"; }

 private:
  friend class RecordedEvent;

  template <class S>
  MOZ_IMPLICIT RecordedPushLayerWithBlend(S& aStream);

  bool mOpaque;
  Float mOpacity;
  ReferencePtr mMask;
  Matrix mMaskTransform;
  IntRect mBounds;
  bool mCopyBackground;
  CompositionOp mCompositionOp;
};

class RecordedPopLayer : public RecordedDrawingEvent<RecordedPopLayer> {
 public:
  MOZ_IMPLICIT RecordedPopLayer(DrawTarget* aDT)
      : RecordedDrawingEvent(POPLAYER, aDT) {}

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "PopLayer"; }

 private:
  friend class RecordedEvent;

  template <class S>
  MOZ_IMPLICIT RecordedPopLayer(S& aStream);
};

class RecordedSetTransform : public RecordedDrawingEvent<RecordedSetTransform> {
 public:
  RecordedSetTransform(DrawTarget* aDT, const Matrix& aTransform)
      : RecordedDrawingEvent(SETTRANSFORM, aDT), mTransform(aTransform) {}

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "SetTransform"; }

  Matrix mTransform;

 private:
  friend class RecordedEvent;

  template <class S>
  MOZ_IMPLICIT RecordedSetTransform(S& aStream);
};

class RecordedDrawSurface : public RecordedDrawingEvent<RecordedDrawSurface> {
 public:
  RecordedDrawSurface(DrawTarget* aDT, ReferencePtr aRefSource,
                      const Rect& aDest, const Rect& aSource,
                      const DrawSurfaceOptions& aDSOptions,
                      const DrawOptions& aOptions)
      : RecordedDrawingEvent(DRAWSURFACE, aDT),
        mRefSource(aRefSource),
        mDest(aDest),
        mSource(aSource),
        mDSOptions(aDSOptions),
        mOptions(aOptions) {}

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "DrawSurface"; }

 private:
  friend class RecordedEvent;

  template <class S>
  MOZ_IMPLICIT RecordedDrawSurface(S& aStream);

  ReferencePtr mRefSource;
  Rect mDest;
  Rect mSource;
  DrawSurfaceOptions mDSOptions;
  DrawOptions mOptions;
};

class RecordedDrawDependentSurface
    : public RecordedDrawingEvent<RecordedDrawDependentSurface> {
 public:
  RecordedDrawDependentSurface(DrawTarget* aDT, uint64_t aId, const Rect& aDest,
                               const DrawSurfaceOptions& aDSOptions,
                               const DrawOptions& aOptions)
      : RecordedDrawingEvent(DRAWDEPENDENTSURFACE, aDT),
        mId(aId),
        mDest(aDest),
        mDSOptions(aDSOptions),
        mOptions(aOptions) {}

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "DrawDependentSurface"; }

 private:
  friend class RecordedEvent;

  template <class S>
  MOZ_IMPLICIT RecordedDrawDependentSurface(S& aStream);

  uint64_t mId;
  Rect mDest;
  DrawSurfaceOptions mDSOptions;
  DrawOptions mOptions;
};

class RecordedDrawSurfaceWithShadow
    : public RecordedDrawingEvent<RecordedDrawSurfaceWithShadow> {
 public:
  RecordedDrawSurfaceWithShadow(DrawTarget* aDT, ReferencePtr aRefSource,
                                const Point& aDest, const DeviceColor& aColor,
                                const Point& aOffset, Float aSigma,
                                CompositionOp aOp)
      : RecordedDrawingEvent(DRAWSURFACEWITHSHADOW, aDT),
        mRefSource(aRefSource),
        mDest(aDest),
        mColor(aColor),
        mOffset(aOffset),
        mSigma(aSigma),
        mOp(aOp) {}

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "DrawSurfaceWithShadow"; }

 private:
  friend class RecordedEvent;

  template <class S>
  MOZ_IMPLICIT RecordedDrawSurfaceWithShadow(S& aStream);

  ReferencePtr mRefSource;
  Point mDest;
  DeviceColor mColor;
  Point mOffset;
  Float mSigma;
  CompositionOp mOp;
};

class RecordedDrawFilter : public RecordedDrawingEvent<RecordedDrawFilter> {
 public:
  RecordedDrawFilter(DrawTarget* aDT, ReferencePtr aNode,
                     const Rect& aSourceRect, const Point& aDestPoint,
                     const DrawOptions& aOptions)
      : RecordedDrawingEvent(DRAWFILTER, aDT),
        mNode(aNode),
        mSourceRect(aSourceRect),
        mDestPoint(aDestPoint),
        mOptions(aOptions) {}

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "DrawFilter"; }

 private:
  friend class RecordedEvent;

  template <class S>
  MOZ_IMPLICIT RecordedDrawFilter(S& aStream);

  ReferencePtr mNode;
  Rect mSourceRect;
  Point mDestPoint;
  DrawOptions mOptions;
};

class RecordedPathCreation : public RecordedEventDerived<RecordedPathCreation> {
 public:
  MOZ_IMPLICIT RecordedPathCreation(PathRecording* aPath);

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "Path Creation"; }

 private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;
  FillRule mFillRule;
  RefPtr<PathRecording> mPath;
  UniquePtr<PathOps> mPathOps;

  template <class S>
  MOZ_IMPLICIT RecordedPathCreation(S& aStream);
};

class RecordedPathDestruction
    : public RecordedEventDerived<RecordedPathDestruction> {
 public:
  MOZ_IMPLICIT RecordedPathDestruction(PathRecording* aPath)
      : RecordedEventDerived(PATHDESTRUCTION), mRefPtr(aPath) {}

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "Path Destruction"; }

 private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;

  template <class S>
  MOZ_IMPLICIT RecordedPathDestruction(S& aStream);
};

class RecordedSourceSurfaceCreation
    : public RecordedEventDerived<RecordedSourceSurfaceCreation> {
 public:
  RecordedSourceSurfaceCreation(ReferencePtr aRefPtr, uint8_t* aData,
                                int32_t aStride, const IntSize& aSize,
                                SurfaceFormat aFormat)
      : RecordedEventDerived(SOURCESURFACECREATION),
        mRefPtr(aRefPtr),
        mData(aData),
        mStride(aStride),
        mSize(aSize),
        mFormat(aFormat),
        mDataOwned(false) {}

  ~RecordedSourceSurfaceCreation();

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "SourceSurface Creation"; }

 private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;
  uint8_t* mData;
  int32_t mStride;
  IntSize mSize;
  SurfaceFormat mFormat;
  mutable bool mDataOwned;

  template <class S>
  MOZ_IMPLICIT RecordedSourceSurfaceCreation(S& aStream);
};

class RecordedSourceSurfaceDestruction
    : public RecordedEventDerived<RecordedSourceSurfaceDestruction> {
 public:
  MOZ_IMPLICIT RecordedSourceSurfaceDestruction(ReferencePtr aRefPtr)
      : RecordedEventDerived(SOURCESURFACEDESTRUCTION), mRefPtr(aRefPtr) {}

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "SourceSurface Destruction"; }

 private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;

  template <class S>
  MOZ_IMPLICIT RecordedSourceSurfaceDestruction(S& aStream);
};

class RecordedOptimizeSourceSurface
    : public RecordedEventDerived<RecordedOptimizeSourceSurface> {
 public:
  RecordedOptimizeSourceSurface(ReferencePtr aSurface, ReferencePtr aDT,
                                ReferencePtr aOptimizedSurface)
      : RecordedEventDerived(OPTIMIZESOURCESURFACE),
        mSurface(aSurface),
        mDT(aDT),
        mOptimizedSurface(aOptimizedSurface) {}

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "OptimizeSourceSurface"; }

 private:
  friend class RecordedEvent;

  ReferencePtr mSurface;
  ReferencePtr mDT;
  ReferencePtr mOptimizedSurface;

  template <class S>
  MOZ_IMPLICIT RecordedOptimizeSourceSurface(S& aStream);
};

class RecordedExternalSurfaceCreation
    : public RecordedEventDerived<RecordedExternalSurfaceCreation> {
 public:
  RecordedExternalSurfaceCreation(ReferencePtr aRefPtr, const uint64_t aKey)
      : RecordedEventDerived(EXTERNALSURFACECREATION),
        mRefPtr(aRefPtr),
        mKey(aKey) {}

  ~RecordedExternalSurfaceCreation() = default;

  virtual bool PlayEvent(Translator* aTranslator) const;

  template <class S>
  void Record(S& aStream) const;
  virtual void OutputSimpleEventInfo(std::stringstream& aStringStream) const;

  virtual std::string GetName() const {
    return "SourceSurfaceSharedData Creation";
  }

 private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;
  uint64_t mKey;

  template <class S>
  MOZ_IMPLICIT RecordedExternalSurfaceCreation(S& aStream);
};

class RecordedFilterNodeCreation
    : public RecordedEventDerived<RecordedFilterNodeCreation> {
 public:
  RecordedFilterNodeCreation(ReferencePtr aRefPtr, FilterType aType)
      : RecordedEventDerived(FILTERNODECREATION),
        mRefPtr(aRefPtr),
        mType(aType) {}

  ~RecordedFilterNodeCreation();

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "FilterNode Creation"; }

 private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;
  FilterType mType;

  template <class S>
  MOZ_IMPLICIT RecordedFilterNodeCreation(S& aStream);
};

class RecordedFilterNodeDestruction
    : public RecordedEventDerived<RecordedFilterNodeDestruction> {
 public:
  MOZ_IMPLICIT RecordedFilterNodeDestruction(ReferencePtr aRefPtr)
      : RecordedEventDerived(FILTERNODEDESTRUCTION), mRefPtr(aRefPtr) {}

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "FilterNode Destruction"; }

 private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;

  template <class S>
  MOZ_IMPLICIT RecordedFilterNodeDestruction(S& aStream);
};

class RecordedGradientStopsCreation
    : public RecordedEventDerived<RecordedGradientStopsCreation> {
 public:
  RecordedGradientStopsCreation(ReferencePtr aRefPtr, GradientStop* aStops,
                                uint32_t aNumStops, ExtendMode aExtendMode)
      : RecordedEventDerived(GRADIENTSTOPSCREATION),
        mRefPtr(aRefPtr),
        mStops(aStops),
        mNumStops(aNumStops),
        mExtendMode(aExtendMode),
        mDataOwned(false) {}

  ~RecordedGradientStopsCreation();

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "GradientStops Creation"; }

 private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;
  GradientStop* mStops;
  uint32_t mNumStops;
  ExtendMode mExtendMode;
  bool mDataOwned;

  template <class S>
  MOZ_IMPLICIT RecordedGradientStopsCreation(S& aStream);
};

class RecordedGradientStopsDestruction
    : public RecordedEventDerived<RecordedGradientStopsDestruction> {
 public:
  MOZ_IMPLICIT RecordedGradientStopsDestruction(ReferencePtr aRefPtr)
      : RecordedEventDerived(GRADIENTSTOPSDESTRUCTION), mRefPtr(aRefPtr) {}

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "GradientStops Destruction"; }

 private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;

  template <class S>
  MOZ_IMPLICIT RecordedGradientStopsDestruction(S& aStream);
};

class RecordedFlush : public RecordedDrawingEvent<RecordedFlush> {
 public:
  explicit RecordedFlush(DrawTarget* aDT) : RecordedDrawingEvent(FLUSH, aDT) {}

  bool PlayEvent(Translator* aTranslator) const final;

  template <class S>
  void Record(S& aStream) const;
  virtual void OutputSimpleEventInfo(
      std::stringstream& aStringStream) const override;

  virtual std::string GetName() const override { return "Flush"; }

 private:
  friend class RecordedEvent;

  template <class S>
  MOZ_IMPLICIT RecordedFlush(S& aStream);
};

class RecordedDetachAllSnapshots
    : public RecordedDrawingEvent<RecordedDetachAllSnapshots> {
 public:
  explicit RecordedDetachAllSnapshots(DrawTarget* aDT)
      : RecordedDrawingEvent(DETACHALLSNAPSHOTS, aDT) {}

  bool PlayEvent(Translator* aTranslator) const final;

  template <class S>
  void Record(S& aStream) const;
  virtual void OutputSimpleEventInfo(
      std::stringstream& aStringStream) const override;

  virtual std::string GetName() const override { return "DetachAllSnapshots"; }

 private:
  friend class RecordedEvent;

  template <class S>
  MOZ_IMPLICIT RecordedDetachAllSnapshots(S& aStream);
};

class RecordedSnapshot : public RecordedEventDerived<RecordedSnapshot> {
 public:
  RecordedSnapshot(ReferencePtr aRefPtr, DrawTarget* aDT)
      : RecordedEventDerived(SNAPSHOT), mRefPtr(aRefPtr), mDT(aDT) {}

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "Snapshot"; }

 private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;
  ReferencePtr mDT;

  template <class S>
  MOZ_IMPLICIT RecordedSnapshot(S& aStream);
};

class RecordedIntoLuminanceSource
    : public RecordedEventDerived<RecordedIntoLuminanceSource> {
 public:
  RecordedIntoLuminanceSource(ReferencePtr aRefPtr, DrawTarget* aDT,
                              LuminanceType aLuminanceType, float aOpacity)
      : RecordedEventDerived(INTOLUMINANCE),
        mRefPtr(aRefPtr),
        mDT(aDT),
        mLuminanceType(aLuminanceType),
        mOpacity(aOpacity) {}

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "IntoLuminanceSource"; }

 private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;
  ReferencePtr mDT;
  LuminanceType mLuminanceType;
  float mOpacity;

  template <class S>
  MOZ_IMPLICIT RecordedIntoLuminanceSource(S& aStream);
};

class RecordedFontData : public RecordedEventDerived<RecordedFontData> {
 public:
  static void FontDataProc(const uint8_t* aData, uint32_t aSize,
                           uint32_t aIndex, void* aBaton) {
    auto recordedFontData = static_cast<RecordedFontData*>(aBaton);
    recordedFontData->SetFontData(aData, aSize, aIndex);
  }

  explicit RecordedFontData(UnscaledFont* aUnscaledFont)
      : RecordedEventDerived(FONTDATA),
        mType(aUnscaledFont->GetType()),
        mData(nullptr),
        mFontDetails() {
    mGetFontFileDataSucceeded =
        aUnscaledFont->GetFontFileData(&FontDataProc, this) && mData;
  }

  virtual ~RecordedFontData();

  bool IsValid() const { return mGetFontFileDataSucceeded; }

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "Font Data"; }

  void SetFontData(const uint8_t* aData, uint32_t aSize, uint32_t aIndex);

  bool GetFontDetails(RecordedFontDetails& fontDetails);

 private:
  friend class RecordedEvent;

  FontType mType;
  uint8_t* mData;
  RecordedFontDetails mFontDetails;

  bool mGetFontFileDataSucceeded;

  template <class S>
  MOZ_IMPLICIT RecordedFontData(S& aStream);
};

class RecordedFontDescriptor
    : public RecordedEventDerived<RecordedFontDescriptor> {
 public:
  static void FontDescCb(const uint8_t* aData, uint32_t aSize, uint32_t aIndex,
                         void* aBaton) {
    auto recordedFontDesc = static_cast<RecordedFontDescriptor*>(aBaton);
    recordedFontDesc->SetFontDescriptor(aData, aSize, aIndex);
  }

  explicit RecordedFontDescriptor(UnscaledFont* aUnscaledFont)
      : RecordedEventDerived(FONTDESC),
        mType(aUnscaledFont->GetType()),
        mIndex(0),
        mRefPtr(aUnscaledFont) {
    mHasDesc = aUnscaledFont->GetFontDescriptor(FontDescCb, this);
  }

  virtual ~RecordedFontDescriptor();

  bool IsValid() const { return mHasDesc; }

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "Font Desc"; }

 private:
  friend class RecordedEvent;

  void SetFontDescriptor(const uint8_t* aData, uint32_t aSize, uint32_t aIndex);

  bool mHasDesc;

  FontType mType;
  std::vector<uint8_t> mData;
  uint32_t mIndex;
  ReferencePtr mRefPtr;

  template <class S>
  MOZ_IMPLICIT RecordedFontDescriptor(S& aStream);
};

class RecordedUnscaledFontCreation
    : public RecordedEventDerived<RecordedUnscaledFontCreation> {
 public:
  static void FontInstanceDataProc(const uint8_t* aData, uint32_t aSize,
                                   void* aBaton) {
    auto recordedUnscaledFontCreation =
        static_cast<RecordedUnscaledFontCreation*>(aBaton);
    recordedUnscaledFontCreation->SetFontInstanceData(aData, aSize);
  }

  RecordedUnscaledFontCreation(UnscaledFont* aUnscaledFont,
                               RecordedFontDetails aFontDetails)
      : RecordedEventDerived(UNSCALEDFONTCREATION),
        mRefPtr(aUnscaledFont),
        mFontDataKey(aFontDetails.fontDataKey),
        mIndex(aFontDetails.index) {
    aUnscaledFont->GetFontInstanceData(FontInstanceDataProc, this);
  }

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "UnscaledFont Creation"; }

  void SetFontInstanceData(const uint8_t* aData, uint32_t aSize);

 private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;
  uint64_t mFontDataKey;
  uint32_t mIndex;
  std::vector<uint8_t> mInstanceData;

  template <class S>
  MOZ_IMPLICIT RecordedUnscaledFontCreation(S& aStream);
};

class RecordedUnscaledFontDestruction
    : public RecordedEventDerived<RecordedUnscaledFontDestruction> {
 public:
  MOZ_IMPLICIT RecordedUnscaledFontDestruction(ReferencePtr aRefPtr)
      : RecordedEventDerived(UNSCALEDFONTDESTRUCTION), mRefPtr(aRefPtr) {}

  bool PlayEvent(Translator* aTranslator) const override;
  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "UnscaledFont Destruction"; }

 private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;

  template <class S>
  MOZ_IMPLICIT RecordedUnscaledFontDestruction(S& aStream);
};

class RecordedScaledFontCreation
    : public RecordedEventDerived<RecordedScaledFontCreation> {
 public:
  static void FontInstanceDataProc(const uint8_t* aData, uint32_t aSize,
                                   const FontVariation* aVariations,
                                   uint32_t aNumVariations, void* aBaton) {
    auto recordedScaledFontCreation =
        static_cast<RecordedScaledFontCreation*>(aBaton);
    recordedScaledFontCreation->SetFontInstanceData(aData, aSize, aVariations,
                                                    aNumVariations);
  }

  RecordedScaledFontCreation(ScaledFont* aScaledFont,
                             UnscaledFont* aUnscaledFont)
      : RecordedEventDerived(SCALEDFONTCREATION),
        mRefPtr(aScaledFont),
        mUnscaledFont(aUnscaledFont),
        mGlyphSize(aScaledFont->GetSize()) {
    aScaledFont->GetFontInstanceData(FontInstanceDataProc, this);
  }

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "ScaledFont Creation"; }

  void SetFontInstanceData(const uint8_t* aData, uint32_t aSize,
                           const FontVariation* aVariations,
                           uint32_t aNumVariations);

 private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;
  ReferencePtr mUnscaledFont;
  Float mGlyphSize;
  std::vector<uint8_t> mInstanceData;
  std::vector<FontVariation> mVariations;

  template <class S>
  MOZ_IMPLICIT RecordedScaledFontCreation(S& aStream);
};

class RecordedScaledFontDestruction
    : public RecordedEventDerived<RecordedScaledFontDestruction> {
 public:
  MOZ_IMPLICIT RecordedScaledFontDestruction(ReferencePtr aRefPtr)
      : RecordedEventDerived(SCALEDFONTDESTRUCTION), mRefPtr(aRefPtr) {}

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "ScaledFont Destruction"; }

 private:
  friend class RecordedEvent;

  ReferencePtr mRefPtr;

  template <class S>
  MOZ_IMPLICIT RecordedScaledFontDestruction(S& aStream);
};

class RecordedMaskSurface : public RecordedDrawingEvent<RecordedMaskSurface> {
 public:
  RecordedMaskSurface(DrawTarget* aDT, const Pattern& aPattern,
                      ReferencePtr aRefMask, const Point& aOffset,
                      const DrawOptions& aOptions)
      : RecordedDrawingEvent(MASKSURFACE, aDT),
        mPattern(),
        mRefMask(aRefMask),
        mOffset(aOffset),
        mOptions(aOptions) {
    StorePattern(mPattern, aPattern);
  }

  bool PlayEvent(Translator* aTranslator) const override;

  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "MaskSurface"; }

 private:
  friend class RecordedEvent;

  template <class S>
  MOZ_IMPLICIT RecordedMaskSurface(S& aStream);

  PatternStorage mPattern;
  ReferencePtr mRefMask;
  Point mOffset;
  DrawOptions mOptions;
};

class RecordedFilterNodeSetAttribute
    : public RecordedEventDerived<RecordedFilterNodeSetAttribute> {
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

  template <typename T>
  RecordedFilterNodeSetAttribute(FilterNode* aNode, uint32_t aIndex,
                                 T aArgument, ArgType aArgType)
      : RecordedEventDerived(FILTERNODESETATTRIBUTE),
        mNode(aNode),
        mIndex(aIndex),
        mArgType(aArgType) {
    mPayload.resize(sizeof(T));
    memcpy(&mPayload.front(), &aArgument, sizeof(T));
  }

  RecordedFilterNodeSetAttribute(FilterNode* aNode, uint32_t aIndex,
                                 const Float* aFloat, uint32_t aSize)
      : RecordedEventDerived(FILTERNODESETATTRIBUTE),
        mNode(aNode),
        mIndex(aIndex),
        mArgType(ARGTYPE_FLOAT_ARRAY) {
    mPayload.resize(sizeof(Float) * aSize);
    memcpy(&mPayload.front(), aFloat, sizeof(Float) * aSize);
  }

  bool PlayEvent(Translator* aTranslator) const override;
  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "SetAttribute"; }

 private:
  friend class RecordedEvent;

  ReferencePtr mNode;

  uint32_t mIndex;
  ArgType mArgType;
  std::vector<uint8_t> mPayload;

  template <class S>
  MOZ_IMPLICIT RecordedFilterNodeSetAttribute(S& aStream);
};

class RecordedFilterNodeSetInput
    : public RecordedEventDerived<RecordedFilterNodeSetInput> {
 public:
  RecordedFilterNodeSetInput(FilterNode* aNode, uint32_t aIndex,
                             FilterNode* aInputNode)
      : RecordedEventDerived(FILTERNODESETINPUT),
        mNode(aNode),
        mIndex(aIndex),
        mInputFilter(aInputNode),
        mInputSurface(nullptr) {}

  RecordedFilterNodeSetInput(FilterNode* aNode, uint32_t aIndex,
                             SourceSurface* aInputSurface)
      : RecordedEventDerived(FILTERNODESETINPUT),
        mNode(aNode),
        mIndex(aIndex),
        mInputFilter(nullptr),
        mInputSurface(aInputSurface) {}

  bool PlayEvent(Translator* aTranslator) const override;
  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "SetInput"; }

 private:
  friend class RecordedEvent;

  ReferencePtr mNode;
  uint32_t mIndex;
  ReferencePtr mInputFilter;
  ReferencePtr mInputSurface;

  template <class S>
  MOZ_IMPLICIT RecordedFilterNodeSetInput(S& aStream);
};

class RecordedLink : public RecordedDrawingEvent<RecordedLink> {
 public:
  RecordedLink(DrawTarget* aDT, const char* aDestination, const Rect& aRect)
      : RecordedDrawingEvent(LINK, aDT),
        mDestination(aDestination),
        mRect(aRect) {}

  bool PlayEvent(Translator* aTranslator) const override;
  template <class S>
  void Record(S& aStream) const;
  void OutputSimpleEventInfo(std::stringstream& aStringStream) const override;

  std::string GetName() const override { return "Link"; }

 private:
  friend class RecordedEvent;

  std::string mDestination;
  Rect mRect;

  template <class S>
  MOZ_IMPLICIT RecordedLink(S& aStream);
};

static std::string NameFromBackend(BackendType aType) {
  switch (aType) {
    case BackendType::NONE:
      return "None";
    case BackendType::DIRECT2D:
      return "Direct2D";
    default:
      return "Unknown";
  }
}

template <class S>
void RecordedEvent::RecordPatternData(S& aStream,
                                      const PatternStorage& aPattern) const {
  WriteElement(aStream, aPattern.mType);

  switch (aPattern.mType) {
    case PatternType::COLOR: {
      WriteElement(aStream, *reinterpret_cast<const ColorPatternStorage*>(
                                &aPattern.mStorage));
      return;
    }
    case PatternType::LINEAR_GRADIENT: {
      WriteElement(aStream,
                   *reinterpret_cast<const LinearGradientPatternStorage*>(
                       &aPattern.mStorage));
      return;
    }
    case PatternType::RADIAL_GRADIENT: {
      WriteElement(aStream,
                   *reinterpret_cast<const RadialGradientPatternStorage*>(
                       &aPattern.mStorage));
      return;
    }
    case PatternType::CONIC_GRADIENT: {
      WriteElement(aStream,
                   *reinterpret_cast<const ConicGradientPatternStorage*>(
                       &aPattern.mStorage));
      return;
    }
    case PatternType::SURFACE: {
      WriteElement(aStream, *reinterpret_cast<const SurfacePatternStorage*>(
                                &aPattern.mStorage));
      return;
    }
    default:
      return;
  }
}

template <class S>
void RecordedEvent::ReadPatternData(S& aStream,
                                    PatternStorage& aPattern) const {
  ReadElementConstrained(aStream, aPattern.mType, PatternType::COLOR,
                         kHighestPatternType);

  switch (aPattern.mType) {
    case PatternType::COLOR: {
      ReadElement(aStream,
                  *reinterpret_cast<ColorPatternStorage*>(&aPattern.mStorage));
      return;
    }
    case PatternType::LINEAR_GRADIENT: {
      ReadElement(aStream, *reinterpret_cast<LinearGradientPatternStorage*>(
                               &aPattern.mStorage));
      return;
    }
    case PatternType::RADIAL_GRADIENT: {
      ReadElement(aStream, *reinterpret_cast<RadialGradientPatternStorage*>(
                               &aPattern.mStorage));
      return;
    }
    case PatternType::CONIC_GRADIENT: {
      ReadElement(aStream, *reinterpret_cast<ConicGradientPatternStorage*>(
                               &aPattern.mStorage));
      return;
    }
    case PatternType::SURFACE: {
      SurfacePatternStorage* sps =
          reinterpret_cast<SurfacePatternStorage*>(&aPattern.mStorage);
      ReadElement(aStream, *sps);
      if (sps->mExtend < ExtendMode::CLAMP ||
          sps->mExtend > ExtendMode::REFLECT) {
        aStream.SetIsBad();
        return;
      }

      if (sps->mSamplingFilter < SamplingFilter::GOOD ||
          sps->mSamplingFilter >= SamplingFilter::SENTINEL) {
        aStream.SetIsBad();
      }
      return;
    }
    default:
      return;
  }
}

inline void RecordedEvent::StorePattern(PatternStorage& aDestination,
                                        const Pattern& aSource) const {
  aDestination.mType = aSource.GetType();

  switch (aSource.GetType()) {
    case PatternType::COLOR: {
      reinterpret_cast<ColorPatternStorage*>(&aDestination.mStorage)->mColor =
          static_cast<const ColorPattern*>(&aSource)->mColor;
      return;
    }
    case PatternType::LINEAR_GRADIENT: {
      LinearGradientPatternStorage* store =
          reinterpret_cast<LinearGradientPatternStorage*>(
              &aDestination.mStorage);
      const LinearGradientPattern* pat =
          static_cast<const LinearGradientPattern*>(&aSource);
      store->mBegin = pat->mBegin;
      store->mEnd = pat->mEnd;
      store->mMatrix = pat->mMatrix;
      store->mStops = pat->mStops.get();
      return;
    }
    case PatternType::RADIAL_GRADIENT: {
      RadialGradientPatternStorage* store =
          reinterpret_cast<RadialGradientPatternStorage*>(
              &aDestination.mStorage);
      const RadialGradientPattern* pat =
          static_cast<const RadialGradientPattern*>(&aSource);
      store->mCenter1 = pat->mCenter1;
      store->mCenter2 = pat->mCenter2;
      store->mRadius1 = pat->mRadius1;
      store->mRadius2 = pat->mRadius2;
      store->mMatrix = pat->mMatrix;
      store->mStops = pat->mStops.get();
      return;
    }
    case PatternType::CONIC_GRADIENT: {
      ConicGradientPatternStorage* store =
          reinterpret_cast<ConicGradientPatternStorage*>(
              &aDestination.mStorage);
      const ConicGradientPattern* pat =
          static_cast<const ConicGradientPattern*>(&aSource);
      store->mCenter = pat->mCenter;
      store->mAngle = pat->mAngle;
      store->mStartOffset = pat->mStartOffset;
      store->mEndOffset = pat->mEndOffset;
      store->mMatrix = pat->mMatrix;
      store->mStops = pat->mStops.get();
      return;
    }
    case PatternType::SURFACE: {
      SurfacePatternStorage* store =
          reinterpret_cast<SurfacePatternStorage*>(&aDestination.mStorage);
      const SurfacePattern* pat = static_cast<const SurfacePattern*>(&aSource);
      store->mExtend = pat->mExtendMode;
      store->mSamplingFilter = pat->mSamplingFilter;
      store->mMatrix = pat->mMatrix;
      store->mSurface = pat->mSurface;
      store->mSamplingRect = pat->mSamplingRect;
      return;
    }
  }
}

template <class S>
void RecordedEvent::RecordStrokeOptions(
    S& aStream, const StrokeOptions& aStrokeOptions) const {
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

  aStream.write((char*)aStrokeOptions.mDashPattern,
                sizeof(Float) * aStrokeOptions.mDashLength);
}

template <class S>
void RecordedEvent::ReadStrokeOptions(S& aStream,
                                      StrokeOptions& aStrokeOptions) {
  uint64_t dashLength;
  JoinStyle joinStyle;
  CapStyle capStyle;

  ReadElement(aStream, dashLength);
  ReadElement(aStream, aStrokeOptions.mDashOffset);
  ReadElement(aStream, aStrokeOptions.mLineWidth);
  ReadElement(aStream, aStrokeOptions.mMiterLimit);
  ReadElementConstrained(aStream, joinStyle, JoinStyle::BEVEL,
                         JoinStyle::MITER_OR_BEVEL);
  ReadElementConstrained(aStream, capStyle, CapStyle::BUTT, CapStyle::SQUARE);
  // On 32 bit we truncate the value of dashLength.
  // See also bug 811850 for history.
  aStrokeOptions.mDashLength = size_t(dashLength);
  aStrokeOptions.mLineJoin = joinStyle;
  aStrokeOptions.mLineCap = capStyle;

  if (!aStrokeOptions.mDashLength || !aStream.good()) {
    return;
  }

  mDashPatternStorage.resize(aStrokeOptions.mDashLength);
  aStrokeOptions.mDashPattern = &mDashPatternStorage.front();
  aStream.read((char*)aStrokeOptions.mDashPattern,
               sizeof(Float) * aStrokeOptions.mDashLength);
}

template <class S>
static void ReadDrawOptions(S& aStream, DrawOptions& aDrawOptions) {
  ReadElement(aStream, aDrawOptions);
  if (aDrawOptions.mAntialiasMode < AntialiasMode::NONE ||
      aDrawOptions.mAntialiasMode > AntialiasMode::DEFAULT) {
    aStream.SetIsBad();
    return;
  }

  if (aDrawOptions.mCompositionOp < CompositionOp::OP_OVER ||
      aDrawOptions.mCompositionOp > CompositionOp::OP_COUNT) {
    aStream.SetIsBad();
  }
}

template <class S>
static void ReadDrawSurfaceOptions(S& aStream,
                                   DrawSurfaceOptions& aDrawSurfaceOptions) {
  ReadElement(aStream, aDrawSurfaceOptions);
  if (aDrawSurfaceOptions.mSamplingFilter < SamplingFilter::GOOD ||
      aDrawSurfaceOptions.mSamplingFilter >= SamplingFilter::SENTINEL) {
    aStream.SetIsBad();
    return;
  }

  if (aDrawSurfaceOptions.mSamplingBounds < SamplingBounds::UNBOUNDED ||
      aDrawSurfaceOptions.mSamplingBounds > SamplingBounds::BOUNDED) {
    aStream.SetIsBad();
  }
}

inline void RecordedEvent::OutputSimplePatternInfo(
    const PatternStorage& aStorage, std::stringstream& aOutput) const {
  switch (aStorage.mType) {
    case PatternType::COLOR: {
      const DeviceColor color =
          reinterpret_cast<const ColorPatternStorage*>(&aStorage.mStorage)
              ->mColor;
      aOutput << "DeviceColor: (" << color.r << ", " << color.g << ", "
              << color.b << ", " << color.a << ")";
      return;
    }
    case PatternType::LINEAR_GRADIENT: {
      const LinearGradientPatternStorage* store =
          reinterpret_cast<const LinearGradientPatternStorage*>(
              &aStorage.mStorage);

      aOutput << "LinearGradient (" << store->mBegin.x << ", "
              << store->mBegin.y << ") - (" << store->mEnd.x << ", "
              << store->mEnd.y << ") Stops: " << store->mStops;
      return;
    }
    case PatternType::RADIAL_GRADIENT: {
      const RadialGradientPatternStorage* store =
          reinterpret_cast<const RadialGradientPatternStorage*>(
              &aStorage.mStorage);
      aOutput << "RadialGradient (Center 1: (" << store->mCenter1.x << ", "
              << store->mCenter2.y << ") Radius 2: " << store->mRadius2;
      return;
    }
    case PatternType::CONIC_GRADIENT: {
      const ConicGradientPatternStorage* store =
          reinterpret_cast<const ConicGradientPatternStorage*>(
              &aStorage.mStorage);
      aOutput << "ConicGradient (Center: (" << store->mCenter.x << ", "
              << store->mCenter.y << ") Angle: " << store->mAngle
              << " Range:" << store->mStartOffset << " - " << store->mEndOffset;
      return;
    }
    case PatternType::SURFACE: {
      const SurfacePatternStorage* store =
          reinterpret_cast<const SurfacePatternStorage*>(&aStorage.mStorage);
      aOutput << "Surface (0x" << store->mSurface << ")";
      return;
    }
  }
}

template <class T>
template <class S>
RecordedDrawingEvent<T>::RecordedDrawingEvent(RecordedEvent::EventType aType,
                                              S& aStream)
    : RecordedEventDerived<T>(aType) {
  ReadElement(aStream, mDT);
}

template <class T>
template <class S>
void RecordedDrawingEvent<T>::Record(S& aStream) const {
  WriteElement(aStream, mDT);
}

inline bool RecordedDrawTargetCreation::PlayEvent(
    Translator* aTranslator) const {
  RefPtr<DrawTarget> newDT =
      aTranslator->CreateDrawTarget(mRefPtr, mRect.Size(), mFormat);

  // If we couldn't create a DrawTarget this will probably cause us to crash
  // with nullptr later in the playback, so return false to abort.
  if (!newDT) {
    return false;
  }

  if (mHasExistingData) {
    Rect dataRect(0, 0, mExistingData->GetSize().width,
                  mExistingData->GetSize().height);
    newDT->DrawSurface(mExistingData, dataRect, dataRect);
  }

  return true;
}

template <class S>
void RecordedDrawTargetCreation::Record(S& aStream) const {
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mBackendType);
  WriteElement(aStream, mRect);
  WriteElement(aStream, mFormat);
  WriteElement(aStream, mHasExistingData);

  if (mHasExistingData) {
    MOZ_ASSERT(mExistingData);
    MOZ_ASSERT(mExistingData->GetSize() == mRect.Size());
    RefPtr<DataSourceSurface> dataSurf = mExistingData->GetDataSurface();

    DataSourceSurface::ScopedMap map(dataSurf, DataSourceSurface::READ);
    for (int y = 0; y < mRect.height; y++) {
      aStream.write((const char*)map.GetData() + y * map.GetStride(),
                    BytesPerPixel(mFormat) * mRect.width);
    }
  }
}

template <class S>
RecordedDrawTargetCreation::RecordedDrawTargetCreation(S& aStream)
    : RecordedEventDerived(DRAWTARGETCREATION), mExistingData(nullptr) {
  ReadElement(aStream, mRefPtr);
  ReadElementConstrained(aStream, mBackendType, BackendType::NONE,
                         BackendType::CAPTURE);
  ReadElement(aStream, mRect);
  ReadElementConstrained(aStream, mFormat, SurfaceFormat::A8R8G8B8_UINT32,
                         SurfaceFormat::UNKNOWN);
  ReadElement(aStream, mHasExistingData);

  if (mHasExistingData) {
    RefPtr<DataSourceSurface> dataSurf =
        Factory::CreateDataSourceSurface(mRect.Size(), mFormat);
    if (!dataSurf) {
      gfxWarning()
          << "RecordedDrawTargetCreation had to reset mHasExistingData";
      mHasExistingData = false;
      return;
    }

    DataSourceSurface::ScopedMap map(dataSurf, DataSourceSurface::READ);
    for (int y = 0; y < mRect.height; y++) {
      aStream.read((char*)map.GetData() + y * map.GetStride(),
                   BytesPerPixel(mFormat) * mRect.width);
    }
    mExistingData = dataSurf;
  }
}

inline void RecordedDrawTargetCreation::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mRefPtr << "] DrawTarget Creation (Type: "
                << NameFromBackend(mBackendType) << ", Size: " << mRect.width
                << "x" << mRect.height << ")";
}

inline bool RecordedDrawTargetDestruction::PlayEvent(
    Translator* aTranslator) const {
  aTranslator->RemoveDrawTarget(mRefPtr);
  return true;
}

template <class S>
void RecordedDrawTargetDestruction::Record(S& aStream) const {
  WriteElement(aStream, mRefPtr);
}

template <class S>
RecordedDrawTargetDestruction::RecordedDrawTargetDestruction(S& aStream)
    : RecordedEventDerived(DRAWTARGETDESTRUCTION) {
  ReadElement(aStream, mRefPtr);
}

inline void RecordedDrawTargetDestruction::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mRefPtr << "] DrawTarget Destruction";
}

inline bool RecordedCreateSimilarDrawTarget::PlayEvent(
    Translator* aTranslator) const {
  RefPtr<DrawTarget> drawTarget = aTranslator->GetReferenceDrawTarget();
  if (!drawTarget) {
    // We might end up with a null reference draw target due to a device
    // failure, just return false so that we can recover.
    return false;
  }

  RefPtr<DrawTarget> newDT =
      drawTarget->CreateSimilarDrawTarget(mSize, mFormat);

  // If we couldn't create a DrawTarget this will probably cause us to crash
  // with nullptr later in the playback, so return false to abort.
  if (!newDT) {
    return false;
  }

  aTranslator->AddDrawTarget(mRefPtr, newDT);
  return true;
}

template <class S>
void RecordedCreateSimilarDrawTarget::Record(S& aStream) const {
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mSize);
  WriteElement(aStream, mFormat);
}

template <class S>
RecordedCreateSimilarDrawTarget::RecordedCreateSimilarDrawTarget(S& aStream)
    : RecordedEventDerived(CREATESIMILARDRAWTARGET) {
  ReadElement(aStream, mRefPtr);
  ReadElement(aStream, mSize);
  ReadElementConstrained(aStream, mFormat, SurfaceFormat::A8R8G8B8_UINT32,
                         SurfaceFormat::UNKNOWN);
}

inline void RecordedCreateSimilarDrawTarget::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mRefPtr
                << "] CreateSimilarDrawTarget (Size: " << mSize.width << "x"
                << mSize.height << ")";
}

inline bool RecordedCreateDrawTargetForFilter::PlayEvent(
    Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }

  IntRect baseRect = dt->GetRect();

  auto maxRect = IntRect(IntPoint(0, 0), mMaxSize);

  auto clone = dt->GetTransform();
  bool invertible = clone.Invert();
  // mSourceRect is in filter space. The filter outputs from mSourceRect need
  // to be drawn at mDestPoint in user space.
  Rect userSpaceSource = Rect(mDestPoint, mSourceRect.Size());
  if (invertible) {
    // Try to reduce the source rect so that it's not much bigger
    // than the draw target. The result is not minimal. Examples
    // are left as an exercise for the reader.
    auto destRect = IntRectToRect(baseRect);
    Rect userSpaceBounds = clone.TransformBounds(destRect);
    userSpaceSource = userSpaceSource.Intersect(userSpaceBounds);
  }

  // Compute how much we moved the top-left of the source rect by, and use that
  // to compute the new dest point, and move our intersected source rect back
  // into the (new) filter space.
  Point shift = userSpaceSource.TopLeft() - mDestPoint;
  Rect filterSpaceSource =
      Rect(mSourceRect.TopLeft() + shift, userSpaceSource.Size());

  baseRect = RoundedOut(filterSpaceSource);
  FilterNode* filter = aTranslator->LookupFilterNode(mFilter);
  if (!filter) {
    return false;
  }

  IntRect transformedRect = filter->MapRectToSource(
      baseRect, maxRect, aTranslator->LookupFilterNode(mSource));

  // Intersect with maxRect to make sure we didn't end up with something bigger
  transformedRect = transformedRect.Intersect(maxRect);

  // If we end up with an empty rect make it 1x1 so that things don't break.
  if (transformedRect.IsEmpty()) {
    transformedRect = IntRect(0, 0, 1, 1);
  }

  RefPtr<DrawTarget> newDT =
      dt->CreateSimilarDrawTarget(transformedRect.Size(), mFormat);
  newDT =
      gfx::Factory::CreateOffsetDrawTarget(newDT, transformedRect.TopLeft());

  // If we couldn't create a DrawTarget this will probably cause us to crash
  // with nullptr later in the playback, so return false to abort.
  if (!newDT) {
    return false;
  }

  aTranslator->AddDrawTarget(mRefPtr, newDT);
  return true;
}

inline bool RecordedCreateClippedDrawTarget::PlayEvent(
    Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }

  RefPtr<DrawTarget> newDT = dt->CreateClippedDrawTarget(mBounds, mFormat);

  // If we couldn't create a DrawTarget this will probably cause us to crash
  // with nullptr later in the playback, so return false to abort.
  if (!newDT) {
    return false;
  }

  aTranslator->AddDrawTarget(mRefPtr, newDT);
  return true;
}

template <class S>
void RecordedCreateClippedDrawTarget::Record(S& aStream) const {
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mBounds);
  WriteElement(aStream, mFormat);
}

template <class S>
RecordedCreateClippedDrawTarget::RecordedCreateClippedDrawTarget(S& aStream)
    : RecordedDrawingEvent(CREATECLIPPEDDRAWTARGET, aStream) {
  ReadElement(aStream, mRefPtr);
  ReadElement(aStream, mBounds);
  ReadElementConstrained(aStream, mFormat, SurfaceFormat::A8R8G8B8_UINT32,
                         SurfaceFormat::UNKNOWN);
}

inline void RecordedCreateClippedDrawTarget::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mRefPtr << "] CreateClippedDrawTarget ()";
}

template <class S>
void RecordedCreateDrawTargetForFilter::Record(S& aStream) const {
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mMaxSize);
  WriteElement(aStream, mFormat);
  WriteElement(aStream, mFilter);
  WriteElement(aStream, mSource);
  WriteElement(aStream, mSourceRect);
  WriteElement(aStream, mDestPoint);
}

template <class S>
RecordedCreateDrawTargetForFilter::RecordedCreateDrawTargetForFilter(S& aStream)
    : RecordedDrawingEvent(CREATEDRAWTARGETFORFILTER, aStream) {
  ReadElement(aStream, mRefPtr);
  ReadElement(aStream, mMaxSize);
  ReadElementConstrained(aStream, mFormat, SurfaceFormat::A8R8G8B8_UINT32,
                         SurfaceFormat::UNKNOWN);
  ReadElement(aStream, mFilter);
  ReadElement(aStream, mSource);
  ReadElement(aStream, mSourceRect);
  ReadElement(aStream, mDestPoint);
}

inline void RecordedCreateDrawTargetForFilter::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mRefPtr << "] CreateDrawTargetForFilter ()";
}

struct GenericPattern {
  GenericPattern(const PatternStorage& aStorage, Translator* aTranslator)
      : mPattern(nullptr), mTranslator(aTranslator) {
    mStorage = const_cast<PatternStorage*>(&aStorage);
  }

  ~GenericPattern() {
    if (mPattern) {
      mPattern->~Pattern();
    }
  }

  operator Pattern*() {
    switch (mStorage->mType) {
      case PatternType::COLOR:
        return new (mColPat) ColorPattern(
            reinterpret_cast<ColorPatternStorage*>(&mStorage->mStorage)
                ->mColor);
      case PatternType::SURFACE: {
        SurfacePatternStorage* storage =
            reinterpret_cast<SurfacePatternStorage*>(&mStorage->mStorage);
        mPattern = new (mSurfPat)
            SurfacePattern(mTranslator->LookupSourceSurface(storage->mSurface),
                           storage->mExtend, storage->mMatrix,
                           storage->mSamplingFilter, storage->mSamplingRect);
        return mPattern;
      }
      case PatternType::LINEAR_GRADIENT: {
        LinearGradientPatternStorage* storage =
            reinterpret_cast<LinearGradientPatternStorage*>(
                &mStorage->mStorage);
        mPattern = new (mLinGradPat) LinearGradientPattern(
            storage->mBegin, storage->mEnd,
            storage->mStops ? mTranslator->LookupGradientStops(storage->mStops)
                            : nullptr,
            storage->mMatrix);
        return mPattern;
      }
      case PatternType::RADIAL_GRADIENT: {
        RadialGradientPatternStorage* storage =
            reinterpret_cast<RadialGradientPatternStorage*>(
                &mStorage->mStorage);
        mPattern = new (mRadGradPat) RadialGradientPattern(
            storage->mCenter1, storage->mCenter2, storage->mRadius1,
            storage->mRadius2,
            storage->mStops ? mTranslator->LookupGradientStops(storage->mStops)
                            : nullptr,
            storage->mMatrix);
        return mPattern;
      }
      case PatternType::CONIC_GRADIENT: {
        ConicGradientPatternStorage* storage =
            reinterpret_cast<ConicGradientPatternStorage*>(&mStorage->mStorage);
        mPattern = new (mConGradPat) ConicGradientPattern(
            storage->mCenter, storage->mAngle, storage->mStartOffset,
            storage->mEndOffset,
            storage->mStops ? mTranslator->LookupGradientStops(storage->mStops)
                            : nullptr,
            storage->mMatrix);
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

  PatternStorage* mStorage;
  Pattern* mPattern;
  Translator* mTranslator;
};

inline bool RecordedFillRect::PlayEvent(Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }

  dt->FillRect(mRect, *GenericPattern(mPattern, aTranslator), mOptions);
  return true;
}

template <class S>
void RecordedFillRect::Record(S& aStream) const {
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mRect);
  WriteElement(aStream, mOptions);
  RecordPatternData(aStream, mPattern);
}

template <class S>
RecordedFillRect::RecordedFillRect(S& aStream)
    : RecordedDrawingEvent(FILLRECT, aStream) {
  ReadElement(aStream, mRect);
  ReadDrawOptions(aStream, mOptions);
  ReadPatternData(aStream, mPattern);
}

inline void RecordedFillRect::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mDT << "] FillRect (" << mRect.X() << ", "
                << mRect.Y() << " - " << mRect.Width() << " x "
                << mRect.Height() << ") ";
  OutputSimplePatternInfo(mPattern, aStringStream);
}

inline bool RecordedStrokeRect::PlayEvent(Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }

  dt->StrokeRect(mRect, *GenericPattern(mPattern, aTranslator), mStrokeOptions,
                 mOptions);
  return true;
}

template <class S>
void RecordedStrokeRect::Record(S& aStream) const {
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mRect);
  WriteElement(aStream, mOptions);
  RecordPatternData(aStream, mPattern);
  RecordStrokeOptions(aStream, mStrokeOptions);
}

template <class S>
RecordedStrokeRect::RecordedStrokeRect(S& aStream)
    : RecordedDrawingEvent(STROKERECT, aStream) {
  ReadElement(aStream, mRect);
  ReadDrawOptions(aStream, mOptions);
  ReadPatternData(aStream, mPattern);
  ReadStrokeOptions(aStream, mStrokeOptions);
}

inline void RecordedStrokeRect::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mDT << "] StrokeRect (" << mRect.X() << ", "
                << mRect.Y() << " - " << mRect.Width() << " x "
                << mRect.Height()
                << ") LineWidth: " << mStrokeOptions.mLineWidth << "px ";
  OutputSimplePatternInfo(mPattern, aStringStream);
}

inline bool RecordedStrokeLine::PlayEvent(Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }

  dt->StrokeLine(mBegin, mEnd, *GenericPattern(mPattern, aTranslator),
                 mStrokeOptions, mOptions);
  return true;
}

template <class S>
void RecordedStrokeLine::Record(S& aStream) const {
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mBegin);
  WriteElement(aStream, mEnd);
  WriteElement(aStream, mOptions);
  RecordPatternData(aStream, mPattern);
  RecordStrokeOptions(aStream, mStrokeOptions);
}

template <class S>
RecordedStrokeLine::RecordedStrokeLine(S& aStream)
    : RecordedDrawingEvent(STROKELINE, aStream) {
  ReadElement(aStream, mBegin);
  ReadElement(aStream, mEnd);
  ReadDrawOptions(aStream, mOptions);
  ReadPatternData(aStream, mPattern);
  ReadStrokeOptions(aStream, mStrokeOptions);
}

inline void RecordedStrokeLine::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mDT << "] StrokeLine (" << mBegin.x << ", "
                << mBegin.y << " - " << mEnd.x << ", " << mEnd.y
                << ") LineWidth: " << mStrokeOptions.mLineWidth << "px ";
  OutputSimplePatternInfo(mPattern, aStringStream);
}

inline bool RecordedFill::PlayEvent(Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }

  dt->Fill(aTranslator->LookupPath(mPath),
           *GenericPattern(mPattern, aTranslator), mOptions);
  return true;
}

template <class S>
RecordedFill::RecordedFill(S& aStream) : RecordedDrawingEvent(FILL, aStream) {
  ReadElement(aStream, mPath);
  ReadDrawOptions(aStream, mOptions);
  ReadPatternData(aStream, mPattern);
}

template <class S>
void RecordedFill::Record(S& aStream) const {
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mPath);
  WriteElement(aStream, mOptions);
  RecordPatternData(aStream, mPattern);
}

inline void RecordedFill::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mDT << "] Fill (" << mPath << ") ";
  OutputSimplePatternInfo(mPattern, aStringStream);
}

inline RecordedFillGlyphs::~RecordedFillGlyphs() { delete[] mGlyphs; }

inline bool RecordedFillGlyphs::PlayEvent(Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }

  ScaledFont* scaledFont = aTranslator->LookupScaledFont(mScaledFont);
  if (!scaledFont) {
    return false;
  }

  GlyphBuffer buffer;
  buffer.mGlyphs = mGlyphs;
  buffer.mNumGlyphs = mNumGlyphs;
  dt->FillGlyphs(scaledFont, buffer, *GenericPattern(mPattern, aTranslator),
                 mOptions);
  return true;
}

template <class S>
RecordedFillGlyphs::RecordedFillGlyphs(S& aStream)
    : RecordedDrawingEvent(FILLGLYPHS, aStream) {
  ReadElement(aStream, mScaledFont);
  ReadDrawOptions(aStream, mOptions);
  ReadPatternData(aStream, mPattern);
  ReadElement(aStream, mNumGlyphs);
  if (!aStream.good()) {
    return;
  }

  mGlyphs = new Glyph[mNumGlyphs];
  aStream.read((char*)mGlyphs, sizeof(Glyph) * mNumGlyphs);
}

template <class S>
void RecordedFillGlyphs::Record(S& aStream) const {
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mScaledFont);
  WriteElement(aStream, mOptions);
  RecordPatternData(aStream, mPattern);
  WriteElement(aStream, mNumGlyphs);
  aStream.write((char*)mGlyphs, sizeof(Glyph) * mNumGlyphs);
}

inline void RecordedFillGlyphs::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mDT << "] FillGlyphs (" << mScaledFont << ") ";
  OutputSimplePatternInfo(mPattern, aStringStream);
}

inline bool RecordedMask::PlayEvent(Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }

  dt->Mask(*GenericPattern(mSource, aTranslator),
           *GenericPattern(mMask, aTranslator), mOptions);
  return true;
}

template <class S>
RecordedMask::RecordedMask(S& aStream) : RecordedDrawingEvent(MASK, aStream) {
  ReadDrawOptions(aStream, mOptions);
  ReadPatternData(aStream, mSource);
  ReadPatternData(aStream, mMask);
}

template <class S>
void RecordedMask::Record(S& aStream) const {
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mOptions);
  RecordPatternData(aStream, mSource);
  RecordPatternData(aStream, mMask);
}

inline void RecordedMask::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mDT << "] Mask (Source: ";
  OutputSimplePatternInfo(mSource, aStringStream);
  aStringStream << " Mask: ";
  OutputSimplePatternInfo(mMask, aStringStream);
}

inline bool RecordedStroke::PlayEvent(Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }

  Path* path = aTranslator->LookupPath(mPath);
  if (!path) {
    return false;
  }

  dt->Stroke(path, *GenericPattern(mPattern, aTranslator), mStrokeOptions,
             mOptions);
  return true;
}

template <class S>
void RecordedStroke::Record(S& aStream) const {
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mPath);
  WriteElement(aStream, mOptions);
  RecordPatternData(aStream, mPattern);
  RecordStrokeOptions(aStream, mStrokeOptions);
}

template <class S>
RecordedStroke::RecordedStroke(S& aStream)
    : RecordedDrawingEvent(STROKE, aStream) {
  ReadElement(aStream, mPath);
  ReadDrawOptions(aStream, mOptions);
  ReadPatternData(aStream, mPattern);
  ReadStrokeOptions(aStream, mStrokeOptions);
}

inline void RecordedStroke::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mDT << "] Stroke (" << mPath
                << ") LineWidth: " << mStrokeOptions.mLineWidth << "px ";
  OutputSimplePatternInfo(mPattern, aStringStream);
}

inline bool RecordedClearRect::PlayEvent(Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }

  dt->ClearRect(mRect);
  return true;
}

template <class S>
void RecordedClearRect::Record(S& aStream) const {
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mRect);
}

template <class S>
RecordedClearRect::RecordedClearRect(S& aStream)
    : RecordedDrawingEvent(CLEARRECT, aStream) {
  ReadElement(aStream, mRect);
}

inline void RecordedClearRect::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mDT << "] ClearRect (" << mRect.X() << ", "
                << mRect.Y() << " - " << mRect.Width() << " x "
                << mRect.Height() << ") ";
}

inline bool RecordedCopySurface::PlayEvent(Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }

  SourceSurface* surface = aTranslator->LookupSourceSurface(mSourceSurface);
  if (!surface) {
    return false;
  }

  dt->CopySurface(surface, mSourceRect, mDest);
  return true;
}

template <class S>
void RecordedCopySurface::Record(S& aStream) const {
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mSourceSurface);
  WriteElement(aStream, mSourceRect);
  WriteElement(aStream, mDest);
}

template <class S>
RecordedCopySurface::RecordedCopySurface(S& aStream)
    : RecordedDrawingEvent(COPYSURFACE, aStream) {
  ReadElement(aStream, mSourceSurface);
  ReadElement(aStream, mSourceRect);
  ReadElement(aStream, mDest);
}

inline void RecordedCopySurface::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mDT << "] CopySurface (" << mSourceSurface << ")";
}

inline bool RecordedPushClip::PlayEvent(Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }

  Path* path = aTranslator->LookupPath(mPath);
  if (!path) {
    return false;
  }

  dt->PushClip(path);
  return true;
}

template <class S>
void RecordedPushClip::Record(S& aStream) const {
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mPath);
}

template <class S>
RecordedPushClip::RecordedPushClip(S& aStream)
    : RecordedDrawingEvent(PUSHCLIP, aStream) {
  ReadElement(aStream, mPath);
}

inline void RecordedPushClip::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mDT << "] PushClip (" << mPath << ") ";
}

inline bool RecordedPushClipRect::PlayEvent(Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }

  dt->PushClipRect(mRect);
  return true;
}

template <class S>
void RecordedPushClipRect::Record(S& aStream) const {
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mRect);
}

template <class S>
RecordedPushClipRect::RecordedPushClipRect(S& aStream)
    : RecordedDrawingEvent(PUSHCLIPRECT, aStream) {
  ReadElement(aStream, mRect);
}

inline void RecordedPushClipRect::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mDT << "] PushClipRect (" << mRect.X() << ", "
                << mRect.Y() << " - " << mRect.Width() << " x "
                << mRect.Height() << ") ";
}

inline bool RecordedPopClip::PlayEvent(Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }

  dt->PopClip();
  return true;
}

template <class S>
void RecordedPopClip::Record(S& aStream) const {
  RecordedDrawingEvent::Record(aStream);
}

template <class S>
RecordedPopClip::RecordedPopClip(S& aStream)
    : RecordedDrawingEvent(POPCLIP, aStream) {}

inline void RecordedPopClip::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mDT << "] PopClip";
}

inline bool RecordedPushLayer::PlayEvent(Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }

  SourceSurface* mask =
      mMask ? aTranslator->LookupSourceSurface(mMask) : nullptr;
  dt->PushLayer(mOpaque, mOpacity, mask, mMaskTransform, mBounds,
                mCopyBackground);
  return true;
}

template <class S>
void RecordedPushLayer::Record(S& aStream) const {
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mOpaque);
  WriteElement(aStream, mOpacity);
  WriteElement(aStream, mMask);
  WriteElement(aStream, mMaskTransform);
  WriteElement(aStream, mBounds);
  WriteElement(aStream, mCopyBackground);
}

template <class S>
RecordedPushLayer::RecordedPushLayer(S& aStream)
    : RecordedDrawingEvent(PUSHLAYER, aStream) {
  ReadElement(aStream, mOpaque);
  ReadElement(aStream, mOpacity);
  ReadElement(aStream, mMask);
  ReadElement(aStream, mMaskTransform);
  ReadElement(aStream, mBounds);
  ReadElement(aStream, mCopyBackground);
}

inline void RecordedPushLayer::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mDT << "] PushPLayer (Opaque=" << mOpaque
                << ", Opacity=" << mOpacity << ", Mask Ref=" << mMask << ") ";
}

inline bool RecordedPushLayerWithBlend::PlayEvent(
    Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }

  SourceSurface* mask =
      mMask ? aTranslator->LookupSourceSurface(mMask) : nullptr;
  dt->PushLayerWithBlend(mOpaque, mOpacity, mask, mMaskTransform, mBounds,
                         mCopyBackground, mCompositionOp);
  return true;
}

template <class S>
void RecordedPushLayerWithBlend::Record(S& aStream) const {
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mOpaque);
  WriteElement(aStream, mOpacity);
  WriteElement(aStream, mMask);
  WriteElement(aStream, mMaskTransform);
  WriteElement(aStream, mBounds);
  WriteElement(aStream, mCopyBackground);
  WriteElement(aStream, mCompositionOp);
}

template <class S>
RecordedPushLayerWithBlend::RecordedPushLayerWithBlend(S& aStream)
    : RecordedDrawingEvent(PUSHLAYERWITHBLEND, aStream) {
  ReadElement(aStream, mOpaque);
  ReadElement(aStream, mOpacity);
  ReadElement(aStream, mMask);
  ReadElement(aStream, mMaskTransform);
  ReadElement(aStream, mBounds);
  ReadElement(aStream, mCopyBackground);
  ReadElementConstrained(aStream, mCompositionOp, CompositionOp::OP_OVER,
                         CompositionOp::OP_COUNT);
}

inline void RecordedPushLayerWithBlend::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mDT << "] PushLayerWithBlend (Opaque=" << mOpaque
                << ", Opacity=" << mOpacity << ", Mask Ref=" << mMask << ") ";
}

inline bool RecordedPopLayer::PlayEvent(Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }

  dt->PopLayer();
  return true;
}

template <class S>
void RecordedPopLayer::Record(S& aStream) const {
  RecordedDrawingEvent::Record(aStream);
}

template <class S>
RecordedPopLayer::RecordedPopLayer(S& aStream)
    : RecordedDrawingEvent(POPLAYER, aStream) {}

inline void RecordedPopLayer::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mDT << "] PopLayer";
}

inline bool RecordedSetTransform::PlayEvent(Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }

  dt->SetTransform(mTransform);
  return true;
}

template <class S>
void RecordedSetTransform::Record(S& aStream) const {
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mTransform);
}

template <class S>
RecordedSetTransform::RecordedSetTransform(S& aStream)
    : RecordedDrawingEvent(SETTRANSFORM, aStream) {
  ReadElement(aStream, mTransform);
}

inline void RecordedSetTransform::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mDT << "] SetTransform [ " << mTransform._11 << " "
                << mTransform._12 << " ; " << mTransform._21 << " "
                << mTransform._22 << " ; " << mTransform._31 << " "
                << mTransform._32 << " ]";
}

inline bool RecordedDrawSurface::PlayEvent(Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }

  SourceSurface* surface = aTranslator->LookupSourceSurface(mRefSource);
  if (!surface) {
    return false;
  }

  dt->DrawSurface(surface, mDest, mSource, mDSOptions, mOptions);
  return true;
}

template <class S>
void RecordedDrawSurface::Record(S& aStream) const {
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mRefSource);
  WriteElement(aStream, mDest);
  WriteElement(aStream, mSource);
  WriteElement(aStream, mDSOptions);
  WriteElement(aStream, mOptions);
}

template <class S>
RecordedDrawSurface::RecordedDrawSurface(S& aStream)
    : RecordedDrawingEvent(DRAWSURFACE, aStream) {
  ReadElement(aStream, mRefSource);
  ReadElement(aStream, mDest);
  ReadElement(aStream, mSource);
  ReadDrawSurfaceOptions(aStream, mDSOptions);
  ReadDrawOptions(aStream, mOptions);
}

inline void RecordedDrawSurface::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mDT << "] DrawSurface (" << mRefSource << ")";
}

inline bool RecordedDrawDependentSurface::PlayEvent(
    Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }

  // We still return true even if this fails, since dependent surfaces are
  // used for cross-origin iframe drawing and can fail.
  RefPtr<SourceSurface> surface = aTranslator->LookupExternalSurface(mId);
  if (!surface) {
    return true;
  }

  dt->DrawSurface(surface, mDest, Rect(Point(), Size(surface->GetSize())),
                  mDSOptions, mOptions);
  return true;
}

template <class S>
void RecordedDrawDependentSurface::Record(S& aStream) const {
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mId);
  WriteElement(aStream, mDest);
  WriteElement(aStream, mDSOptions);
  WriteElement(aStream, mOptions);
}

template <class S>
RecordedDrawDependentSurface::RecordedDrawDependentSurface(S& aStream)
    : RecordedDrawingEvent(DRAWDEPENDENTSURFACE, aStream) {
  ReadElement(aStream, mId);
  ReadElement(aStream, mDest);
  ReadDrawSurfaceOptions(aStream, mDSOptions);
  ReadDrawOptions(aStream, mOptions);
}

inline void RecordedDrawDependentSurface::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mDT << "] DrawDependentSurface (" << mId << ")";
}

inline bool RecordedDrawFilter::PlayEvent(Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }

  FilterNode* filter = aTranslator->LookupFilterNode(mNode);
  if (!filter) {
    return false;
  }

  dt->DrawFilter(filter, mSourceRect, mDestPoint, mOptions);
  return true;
}

template <class S>
void RecordedDrawFilter::Record(S& aStream) const {
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mNode);
  WriteElement(aStream, mSourceRect);
  WriteElement(aStream, mDestPoint);
  WriteElement(aStream, mOptions);
}

template <class S>
RecordedDrawFilter::RecordedDrawFilter(S& aStream)
    : RecordedDrawingEvent(DRAWFILTER, aStream) {
  ReadElement(aStream, mNode);
  ReadElement(aStream, mSourceRect);
  ReadElement(aStream, mDestPoint);
  ReadDrawOptions(aStream, mOptions);
}

inline void RecordedDrawFilter::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mDT << "] DrawFilter (" << mNode << ")";
}

inline bool RecordedDrawSurfaceWithShadow::PlayEvent(
    Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }

  SourceSurface* surface = aTranslator->LookupSourceSurface(mRefSource);
  if (!surface) {
    return false;
  }

  dt->DrawSurfaceWithShadow(surface, mDest, mColor, mOffset, mSigma, mOp);
  return true;
}

template <class S>
void RecordedDrawSurfaceWithShadow::Record(S& aStream) const {
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mRefSource);
  WriteElement(aStream, mDest);
  WriteElement(aStream, mColor);
  WriteElement(aStream, mOffset);
  WriteElement(aStream, mSigma);
  WriteElement(aStream, mOp);
}

template <class S>
RecordedDrawSurfaceWithShadow::RecordedDrawSurfaceWithShadow(S& aStream)
    : RecordedDrawingEvent(DRAWSURFACEWITHSHADOW, aStream) {
  ReadElement(aStream, mRefSource);
  ReadElement(aStream, mDest);
  ReadElement(aStream, mColor);
  ReadElement(aStream, mOffset);
  ReadElement(aStream, mSigma);
  ReadElementConstrained(aStream, mOp, CompositionOp::OP_OVER,
                         CompositionOp::OP_COUNT);
}

inline void RecordedDrawSurfaceWithShadow::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mDT << "] DrawSurfaceWithShadow (" << mRefSource
                << ") DeviceColor: (" << mColor.r << ", " << mColor.g << ", "
                << mColor.b << ", " << mColor.a << ")";
}

inline RecordedPathCreation::RecordedPathCreation(PathRecording* aPath)
    : RecordedEventDerived(PATHCREATION),
      mRefPtr(aPath),
      mFillRule(aPath->mFillRule),
      mPath(aPath) {}

inline bool RecordedPathCreation::PlayEvent(Translator* aTranslator) const {
  RefPtr<DrawTarget> drawTarget = aTranslator->GetReferenceDrawTarget();
  if (!drawTarget) {
    // We might end up with a null reference draw target due to a device
    // failure, just return false so that we can recover.
    return false;
  }

  RefPtr<PathBuilder> builder = drawTarget->CreatePathBuilder(mFillRule);
  if (!mPathOps->StreamToSink(*builder)) {
    return false;
  }

  RefPtr<Path> path = builder->Finish();
  aTranslator->AddPath(mRefPtr, path);
  return true;
}

template <class S>
void RecordedPathCreation::Record(S& aStream) const {
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mFillRule);
  mPath->mPathOps.Record(aStream);
}

template <class S>
RecordedPathCreation::RecordedPathCreation(S& aStream)
    : RecordedEventDerived(PATHCREATION) {
  ReadElement(aStream, mRefPtr);
  ReadElementConstrained(aStream, mFillRule, FillRule::FILL_WINDING,
                         FillRule::FILL_EVEN_ODD);
  mPathOps = MakeUnique<PathOps>(aStream);
}

inline void RecordedPathCreation::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  size_t numberOfOps =
      mPath ? mPath->mPathOps.NumberOfOps() : mPathOps->NumberOfOps();
  aStringStream << "[" << mRefPtr << "] Path created (OpCount: " << numberOfOps
                << ")";
}
inline bool RecordedPathDestruction::PlayEvent(Translator* aTranslator) const {
  aTranslator->RemovePath(mRefPtr);
  return true;
}

template <class S>
void RecordedPathDestruction::Record(S& aStream) const {
  WriteElement(aStream, mRefPtr);
}

template <class S>
RecordedPathDestruction::RecordedPathDestruction(S& aStream)
    : RecordedEventDerived(PATHDESTRUCTION) {
  ReadElement(aStream, mRefPtr);
}

inline void RecordedPathDestruction::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mRefPtr << "] Path Destroyed";
}

inline RecordedSourceSurfaceCreation::~RecordedSourceSurfaceCreation() {
  if (mDataOwned) {
    delete[] mData;
  }
}

inline bool RecordedSourceSurfaceCreation::PlayEvent(
    Translator* aTranslator) const {
  if (!mData) {
    return false;
  }

  RefPtr<SourceSurface> src = Factory::CreateWrappingDataSourceSurface(
      mData, mSize.width * BytesPerPixel(mFormat), mSize, mFormat,
      [](void* aClosure) { delete[] static_cast<uint8_t*>(aClosure); }, mData);
  if (src) {
    mDataOwned = false;
  }

  aTranslator->AddSourceSurface(mRefPtr, src);
  return true;
}

template <class S>
void RecordedSourceSurfaceCreation::Record(S& aStream) const {
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mSize);
  WriteElement(aStream, mFormat);
  MOZ_ASSERT(mData);
  size_t dataFormatWidth = BytesPerPixel(mFormat) * mSize.width;
  const char* endSrc = (const char*)(mData + (mSize.height * mStride));
  for (const char* src = (const char*)mData; src < endSrc; src += mStride) {
    aStream.write(src, dataFormatWidth);
  }
}

template <class S>
RecordedSourceSurfaceCreation::RecordedSourceSurfaceCreation(S& aStream)
    : RecordedEventDerived(SOURCESURFACECREATION), mDataOwned(true) {
  ReadElement(aStream, mRefPtr);
  ReadElement(aStream, mSize);
  ReadElementConstrained(aStream, mFormat, SurfaceFormat::A8R8G8B8_UINT32,
                         SurfaceFormat::UNKNOWN);
  if (!aStream.good()) {
    return;
  }

  size_t size = mSize.width * mSize.height * BytesPerPixel(mFormat);
  mData = new (fallible) uint8_t[size];
  if (!mData) {
    gfxCriticalNote
        << "RecordedSourceSurfaceCreation failed to allocate data of size "
        << size;
    aStream.SetIsBad();
  } else {
    aStream.read((char*)mData, size);
  }
}

inline void RecordedSourceSurfaceCreation::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mRefPtr
                << "] SourceSurface created (Size: " << mSize.width << "x"
                << mSize.height << ")";
}

inline bool RecordedSourceSurfaceDestruction::PlayEvent(
    Translator* aTranslator) const {
  aTranslator->RemoveSourceSurface(mRefPtr);
  return true;
}

template <class S>
void RecordedSourceSurfaceDestruction::Record(S& aStream) const {
  WriteElement(aStream, mRefPtr);
}

template <class S>
RecordedSourceSurfaceDestruction::RecordedSourceSurfaceDestruction(S& aStream)
    : RecordedEventDerived(SOURCESURFACEDESTRUCTION) {
  ReadElement(aStream, mRefPtr);
}

inline void RecordedSourceSurfaceDestruction::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mRefPtr << "] SourceSurface Destroyed";
}

inline bool RecordedOptimizeSourceSurface::PlayEvent(
    Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }

  SourceSurface* surface = aTranslator->LookupSourceSurface(mSurface);
  if (!surface) {
    return false;
  }

  RefPtr<SourceSurface> optimizedSurface = dt->OptimizeSourceSurface(surface);
  aTranslator->AddSourceSurface(mOptimizedSurface, optimizedSurface);
  return true;
}

template <class S>
void RecordedOptimizeSourceSurface::Record(S& aStream) const {
  WriteElement(aStream, mSurface);
  WriteElement(aStream, mDT);
  WriteElement(aStream, mOptimizedSurface);
}

template <class S>
RecordedOptimizeSourceSurface::RecordedOptimizeSourceSurface(S& aStream)
    : RecordedEventDerived(OPTIMIZESOURCESURFACE) {
  ReadElement(aStream, mSurface);
  ReadElement(aStream, mDT);
  ReadElement(aStream, mOptimizedSurface);
}

inline void RecordedOptimizeSourceSurface::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mSurface << "] Surface Optimized (DT: " << mDT << ")";
}

inline bool RecordedExternalSurfaceCreation::PlayEvent(
    Translator* aTranslator) const {
  RefPtr<SourceSurface> surface = aTranslator->LookupExternalSurface(mKey);
  if (!surface) {
    return false;
  }

  aTranslator->AddSourceSurface(mRefPtr, surface);
  return true;
}

template <class S>
void RecordedExternalSurfaceCreation::Record(S& aStream) const {
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mKey);
}

template <class S>
RecordedExternalSurfaceCreation::RecordedExternalSurfaceCreation(S& aStream)
    : RecordedEventDerived(EXTERNALSURFACECREATION) {
  ReadElement(aStream, mRefPtr);
  ReadElement(aStream, mKey);
}

inline void RecordedExternalSurfaceCreation::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mRefPtr
                << "] SourceSurfaceSharedData created (Key: " << mKey << ")";
}

inline RecordedFilterNodeCreation::~RecordedFilterNodeCreation() = default;

inline bool RecordedFilterNodeCreation::PlayEvent(
    Translator* aTranslator) const {
  RefPtr<DrawTarget> drawTarget = aTranslator->GetReferenceDrawTarget();
  if (!drawTarget) {
    // We might end up with a null reference draw target due to a device
    // failure, just return false so that we can recover.
    return false;
  }

  RefPtr<FilterNode> node = drawTarget->CreateFilter(mType);
  aTranslator->AddFilterNode(mRefPtr, node);
  return true;
}

template <class S>
void RecordedFilterNodeCreation::Record(S& aStream) const {
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mType);
}

template <class S>
RecordedFilterNodeCreation::RecordedFilterNodeCreation(S& aStream)
    : RecordedEventDerived(FILTERNODECREATION) {
  ReadElement(aStream, mRefPtr);
  ReadElementConstrained(aStream, mType, FilterType::BLEND,
                         FilterType::OPACITY);
}

inline void RecordedFilterNodeCreation::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mRefPtr
                << "] FilterNode created (Type: " << int(mType) << ")";
}

inline bool RecordedFilterNodeDestruction::PlayEvent(
    Translator* aTranslator) const {
  aTranslator->RemoveFilterNode(mRefPtr);
  return true;
}

template <class S>
void RecordedFilterNodeDestruction::Record(S& aStream) const {
  WriteElement(aStream, mRefPtr);
}

template <class S>
RecordedFilterNodeDestruction::RecordedFilterNodeDestruction(S& aStream)
    : RecordedEventDerived(FILTERNODEDESTRUCTION) {
  ReadElement(aStream, mRefPtr);
}

inline void RecordedFilterNodeDestruction::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mRefPtr << "] FilterNode Destroyed";
}

inline RecordedGradientStopsCreation::~RecordedGradientStopsCreation() {
  if (mDataOwned) {
    delete[] mStops;
  }
}

inline bool RecordedGradientStopsCreation::PlayEvent(
    Translator* aTranslator) const {
  RefPtr<GradientStops> src =
      aTranslator->GetOrCreateGradientStops(mStops, mNumStops, mExtendMode);
  aTranslator->AddGradientStops(mRefPtr, src);
  return true;
}

template <class S>
void RecordedGradientStopsCreation::Record(S& aStream) const {
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mExtendMode);
  WriteElement(aStream, mNumStops);
  aStream.write((const char*)mStops, mNumStops * sizeof(GradientStop));
}

template <class S>
RecordedGradientStopsCreation::RecordedGradientStopsCreation(S& aStream)
    : RecordedEventDerived(GRADIENTSTOPSCREATION), mDataOwned(true) {
  ReadElement(aStream, mRefPtr);
  ReadElementConstrained(aStream, mExtendMode, ExtendMode::CLAMP,
                         ExtendMode::REFLECT);
  ReadElement(aStream, mNumStops);
  if (!aStream.good()) {
    return;
  }

  mStops = new GradientStop[mNumStops];

  aStream.read((char*)mStops, mNumStops * sizeof(GradientStop));
}

inline void RecordedGradientStopsCreation::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mRefPtr
                << "] GradientStops created (Stops: " << mNumStops << ")";
}

inline bool RecordedGradientStopsDestruction::PlayEvent(
    Translator* aTranslator) const {
  aTranslator->RemoveGradientStops(mRefPtr);
  return true;
}

template <class S>
void RecordedGradientStopsDestruction::Record(S& aStream) const {
  WriteElement(aStream, mRefPtr);
}

template <class S>
RecordedGradientStopsDestruction::RecordedGradientStopsDestruction(S& aStream)
    : RecordedEventDerived(GRADIENTSTOPSDESTRUCTION) {
  ReadElement(aStream, mRefPtr);
}

inline void RecordedGradientStopsDestruction::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mRefPtr << "] GradientStops Destroyed";
}

inline bool RecordedIntoLuminanceSource::PlayEvent(
    Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }

  RefPtr<SourceSurface> src = dt->IntoLuminanceSource(mLuminanceType, mOpacity);
  aTranslator->AddSourceSurface(mRefPtr, src);
  return true;
}

template <class S>
void RecordedIntoLuminanceSource::Record(S& aStream) const {
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mDT);
  WriteElement(aStream, mLuminanceType);
  WriteElement(aStream, mOpacity);
}

template <class S>
RecordedIntoLuminanceSource::RecordedIntoLuminanceSource(S& aStream)
    : RecordedEventDerived(INTOLUMINANCE) {
  ReadElement(aStream, mRefPtr);
  ReadElement(aStream, mDT);
  ReadElementConstrained(aStream, mLuminanceType, LuminanceType::LUMINANCE,
                         LuminanceType::LINEARRGB);
  ReadElement(aStream, mOpacity);
}

inline void RecordedIntoLuminanceSource::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mRefPtr << "] Into Luminance Source (DT: " << mDT
                << ")";
}

inline bool RecordedFlush::PlayEvent(Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }

  dt->Flush();
  return true;
}

template <class S>
void RecordedFlush::Record(S& aStream) const {
  RecordedDrawingEvent::Record(aStream);
}

template <class S>
RecordedFlush::RecordedFlush(S& aStream)
    : RecordedDrawingEvent(FLUSH, aStream) {}

inline void RecordedFlush::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mDT << "] Flush";
}

inline bool RecordedDetachAllSnapshots::PlayEvent(
    Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }

  dt->DetachAllSnapshots();
  return true;
}

template <class S>
void RecordedDetachAllSnapshots::Record(S& aStream) const {
  RecordedDrawingEvent::Record(aStream);
}

template <class S>
RecordedDetachAllSnapshots::RecordedDetachAllSnapshots(S& aStream)
    : RecordedDrawingEvent(DETACHALLSNAPSHOTS, aStream) {}

inline void RecordedDetachAllSnapshots::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mDT << "] DetachAllSnapshots";
}

inline bool RecordedSnapshot::PlayEvent(Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }

  RefPtr<SourceSurface> src = aTranslator->LookupDrawTarget(mDT)->Snapshot();
  aTranslator->AddSourceSurface(mRefPtr, src);
  return true;
}

template <class S>
void RecordedSnapshot::Record(S& aStream) const {
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mDT);
}

template <class S>
RecordedSnapshot::RecordedSnapshot(S& aStream)
    : RecordedEventDerived(SNAPSHOT) {
  ReadElement(aStream, mRefPtr);
  ReadElement(aStream, mDT);
}

inline void RecordedSnapshot::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mRefPtr << "] Snapshot Created (DT: " << mDT << ")";
}

inline RecordedFontData::~RecordedFontData() { delete[] mData; }

inline bool RecordedFontData::PlayEvent(Translator* aTranslator) const {
  if (!mData) {
    return false;
  }

  RefPtr<NativeFontResource> fontResource = Factory::CreateNativeFontResource(
      mData, mFontDetails.size, mType, aTranslator->GetFontContext());
  if (!fontResource) {
    return false;
  }

  aTranslator->AddNativeFontResource(mFontDetails.fontDataKey, fontResource);
  return true;
}

template <class S>
void RecordedFontData::Record(S& aStream) const {
  MOZ_ASSERT(mGetFontFileDataSucceeded);

  WriteElement(aStream, mType);
  WriteElement(aStream, mFontDetails.fontDataKey);
  if (!mData) {
    WriteElement(aStream, 0);
  } else {
    WriteElement(aStream, mFontDetails.size);
    aStream.write((const char*)mData, mFontDetails.size);
  }
}

inline void RecordedFontData::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "Font Data of size " << mFontDetails.size;
}

inline void RecordedFontData::SetFontData(const uint8_t* aData, uint32_t aSize,
                                          uint32_t aIndex) {
  mData = new (fallible) uint8_t[aSize];
  if (!mData) {
    gfxCriticalNote
        << "RecordedFontData failed to allocate data for recording of size "
        << aSize;
  } else {
    memcpy(mData, aData, aSize);
  }
  mFontDetails.fontDataKey = SFNTData::GetUniqueKey(aData, aSize, 0, nullptr);
  mFontDetails.size = aSize;
  mFontDetails.index = aIndex;
}

inline bool RecordedFontData::GetFontDetails(RecordedFontDetails& fontDetails) {
  if (!mGetFontFileDataSucceeded) {
    return false;
  }

  fontDetails.fontDataKey = mFontDetails.fontDataKey;
  fontDetails.size = mFontDetails.size;
  fontDetails.index = mFontDetails.index;
  return true;
}

template <class S>
RecordedFontData::RecordedFontData(S& aStream)
    : RecordedEventDerived(FONTDATA), mType(FontType::UNKNOWN), mData(nullptr) {
  ReadElementConstrained(aStream, mType, FontType::DWRITE, FontType::UNKNOWN);
  ReadElement(aStream, mFontDetails.fontDataKey);
  ReadElement(aStream, mFontDetails.size);
  if (!mFontDetails.size || !aStream.good()) {
    return;
  }

  mData = new (fallible) uint8_t[mFontDetails.size];
  if (!mData) {
    gfxCriticalNote
        << "RecordedFontData failed to allocate data for playback of size "
        << mFontDetails.size;
    aStream.SetIsBad();
  } else {
    aStream.read((char*)mData, mFontDetails.size);
  }
}

inline RecordedFontDescriptor::~RecordedFontDescriptor() = default;

inline bool RecordedFontDescriptor::PlayEvent(Translator* aTranslator) const {
  RefPtr<UnscaledFont> font = Factory::CreateUnscaledFontFromFontDescriptor(
      mType, mData.data(), mData.size(), mIndex);
  if (!font) {
    gfxDevCrash(LogReason::InvalidFont)
        << "Failed creating UnscaledFont of type " << int(mType)
        << " from font descriptor";
    return false;
  }

  aTranslator->AddUnscaledFont(mRefPtr, font);
  return true;
}

template <class S>
void RecordedFontDescriptor::Record(S& aStream) const {
  MOZ_ASSERT(mHasDesc);
  WriteElement(aStream, mType);
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mIndex);
  WriteElement(aStream, (size_t)mData.size());
  if (mData.size()) {
    aStream.write((char*)mData.data(), mData.size());
  }
}

inline void RecordedFontDescriptor::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mRefPtr << "] Font Descriptor";
}

inline void RecordedFontDescriptor::SetFontDescriptor(const uint8_t* aData,
                                                      uint32_t aSize,
                                                      uint32_t aIndex) {
  mData.assign(aData, aData + aSize);
  mIndex = aIndex;
}

template <class S>
RecordedFontDescriptor::RecordedFontDescriptor(S& aStream)
    : RecordedEventDerived(FONTDESC) {
  ReadElementConstrained(aStream, mType, FontType::DWRITE, FontType::UNKNOWN);
  ReadElement(aStream, mRefPtr);
  ReadElement(aStream, mIndex);

  size_t size;
  ReadElement(aStream, size);
  if (!aStream.good()) {
    return;
  }
  if (size) {
    mData.resize(size);
    aStream.read((char*)mData.data(), size);
  }
}

inline bool RecordedUnscaledFontCreation::PlayEvent(
    Translator* aTranslator) const {
  NativeFontResource* fontResource =
      aTranslator->LookupNativeFontResource(mFontDataKey);
  if (!fontResource) {
    gfxDevCrash(LogReason::NativeFontResourceNotFound)
        << "NativeFontResource lookup failed for key |" << hexa(mFontDataKey)
        << "|.";
    return false;
  }

  RefPtr<UnscaledFont> unscaledFont = fontResource->CreateUnscaledFont(
      mIndex, mInstanceData.data(), mInstanceData.size());
  aTranslator->AddUnscaledFont(mRefPtr, unscaledFont);
  return true;
}

template <class S>
void RecordedUnscaledFontCreation::Record(S& aStream) const {
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mFontDataKey);
  WriteElement(aStream, mIndex);
  WriteElement(aStream, (size_t)mInstanceData.size());
  if (mInstanceData.size()) {
    aStream.write((char*)mInstanceData.data(), mInstanceData.size());
  }
}

inline void RecordedUnscaledFontCreation::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mRefPtr << "] UnscaledFont Created";
}

inline void RecordedUnscaledFontCreation::SetFontInstanceData(
    const uint8_t* aData, uint32_t aSize) {
  if (aSize) {
    mInstanceData.assign(aData, aData + aSize);
  }
}

template <class S>
RecordedUnscaledFontCreation::RecordedUnscaledFontCreation(S& aStream)
    : RecordedEventDerived(UNSCALEDFONTCREATION) {
  ReadElement(aStream, mRefPtr);
  ReadElement(aStream, mFontDataKey);
  ReadElement(aStream, mIndex);

  size_t size;
  ReadElement(aStream, size);
  if (!aStream.good()) {
    return;
  }
  if (size) {
    mInstanceData.resize(size);
    aStream.read((char*)mInstanceData.data(), size);
  }
}

inline bool RecordedUnscaledFontDestruction::PlayEvent(
    Translator* aTranslator) const {
  aTranslator->RemoveUnscaledFont(mRefPtr);
  return true;
}

template <class S>
void RecordedUnscaledFontDestruction::Record(S& aStream) const {
  WriteElement(aStream, mRefPtr);
}

template <class S>
RecordedUnscaledFontDestruction::RecordedUnscaledFontDestruction(S& aStream)
    : RecordedEventDerived(UNSCALEDFONTDESTRUCTION) {
  ReadElement(aStream, mRefPtr);
}

inline void RecordedUnscaledFontDestruction::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mRefPtr << "] UnscaledFont Destroyed";
}

inline bool RecordedScaledFontCreation::PlayEvent(
    Translator* aTranslator) const {
  UnscaledFont* unscaledFont = aTranslator->LookupUnscaledFont(mUnscaledFont);
  if (!unscaledFont) {
    gfxDevCrash(LogReason::UnscaledFontNotFound)
        << "UnscaledFont lookup failed for key |" << hexa(mUnscaledFont)
        << "|.";
    return false;
  }

  RefPtr<ScaledFont> scaledFont = unscaledFont->CreateScaledFont(
      mGlyphSize, mInstanceData.data(), mInstanceData.size(),
      mVariations.data(), mVariations.size());

  aTranslator->AddScaledFont(mRefPtr, scaledFont);
  return true;
}

template <class S>
void RecordedScaledFontCreation::Record(S& aStream) const {
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mUnscaledFont);
  WriteElement(aStream, mGlyphSize);
  WriteElement(aStream, (size_t)mInstanceData.size());
  if (mInstanceData.size()) {
    aStream.write((char*)mInstanceData.data(), mInstanceData.size());
  }
  WriteElement(aStream, (size_t)mVariations.size());
  if (mVariations.size()) {
    aStream.write((char*)mVariations.data(),
                  sizeof(FontVariation) * mVariations.size());
  }
}

inline void RecordedScaledFontCreation::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mRefPtr << "] ScaledFont Created";
}

inline void RecordedScaledFontCreation::SetFontInstanceData(
    const uint8_t* aData, uint32_t aSize, const FontVariation* aVariations,
    uint32_t aNumVariations) {
  if (aSize) {
    mInstanceData.assign(aData, aData + aSize);
  }
  if (aNumVariations) {
    mVariations.assign(aVariations, aVariations + aNumVariations);
  }
}

template <class S>
RecordedScaledFontCreation::RecordedScaledFontCreation(S& aStream)
    : RecordedEventDerived(SCALEDFONTCREATION) {
  ReadElement(aStream, mRefPtr);
  ReadElement(aStream, mUnscaledFont);
  ReadElement(aStream, mGlyphSize);

  size_t size;
  ReadElement(aStream, size);
  if (!aStream.good()) {
    return;
  }
  if (size) {
    mInstanceData.resize(size);
    aStream.read((char*)mInstanceData.data(), size);
  }

  size_t numVariations;
  ReadElement(aStream, numVariations);
  if (!aStream.good()) {
    return;
  }
  if (numVariations) {
    mVariations.resize(numVariations);
    aStream.read((char*)mVariations.data(),
                 sizeof(FontVariation) * numVariations);
  }
}

inline bool RecordedScaledFontDestruction::PlayEvent(
    Translator* aTranslator) const {
  aTranslator->RemoveScaledFont(mRefPtr);
  return true;
}

template <class S>
void RecordedScaledFontDestruction::Record(S& aStream) const {
  WriteElement(aStream, mRefPtr);
}

template <class S>
RecordedScaledFontDestruction::RecordedScaledFontDestruction(S& aStream)
    : RecordedEventDerived(SCALEDFONTDESTRUCTION) {
  ReadElement(aStream, mRefPtr);
}

inline void RecordedScaledFontDestruction::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mRefPtr << "] ScaledFont Destroyed";
}

inline bool RecordedMaskSurface::PlayEvent(Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }

  SourceSurface* surface = aTranslator->LookupSourceSurface(mRefMask);
  if (!surface) {
    return false;
  }

  dt->MaskSurface(*GenericPattern(mPattern, aTranslator), surface, mOffset,
                  mOptions);
  return true;
}

template <class S>
void RecordedMaskSurface::Record(S& aStream) const {
  RecordedDrawingEvent::Record(aStream);
  RecordPatternData(aStream, mPattern);
  WriteElement(aStream, mRefMask);
  WriteElement(aStream, mOffset);
  WriteElement(aStream, mOptions);
}

template <class S>
RecordedMaskSurface::RecordedMaskSurface(S& aStream)
    : RecordedDrawingEvent(MASKSURFACE, aStream) {
  ReadPatternData(aStream, mPattern);
  ReadElement(aStream, mRefMask);
  ReadElement(aStream, mOffset);
  ReadDrawOptions(aStream, mOptions);
}

inline void RecordedMaskSurface::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mDT << "] MaskSurface (" << mRefMask << ")  Offset: ("
                << mOffset.x << "x" << mOffset.y << ") Pattern: ";
  OutputSimplePatternInfo(mPattern, aStringStream);
}

template <typename T>
void ReplaySetAttribute(FilterNode* aNode, uint32_t aIndex, T aValue) {
  aNode->SetAttribute(aIndex, aValue);
}

inline bool RecordedFilterNodeSetAttribute::PlayEvent(
    Translator* aTranslator) const {
  FilterNode* node = aTranslator->LookupFilterNode(mNode);
  if (!node) {
    return false;
  }

#define REPLAY_SET_ATTRIBUTE(type, argtype)                      \
  case ARGTYPE_##argtype:                                        \
    ReplaySetAttribute(node, mIndex, *(type*)&mPayload.front()); \
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
    REPLAY_SET_ATTRIBUTE(DeviceColor, COLOR);
    case ARGTYPE_FLOAT_ARRAY:
      node->SetAttribute(mIndex,
                         reinterpret_cast<const Float*>(&mPayload.front()),
                         mPayload.size() / sizeof(Float));
      break;
  }

  return true;
}

template <class S>
void RecordedFilterNodeSetAttribute::Record(S& aStream) const {
  WriteElement(aStream, mNode);
  WriteElement(aStream, mIndex);
  WriteElement(aStream, mArgType);
  WriteElement(aStream, uint64_t(mPayload.size()));
  aStream.write((const char*)&mPayload.front(), mPayload.size());
}

template <class S>
RecordedFilterNodeSetAttribute::RecordedFilterNodeSetAttribute(S& aStream)
    : RecordedEventDerived(FILTERNODESETATTRIBUTE) {
  ReadElement(aStream, mNode);
  ReadElement(aStream, mIndex);
  ReadElementConstrained(aStream, mArgType, ArgType::ARGTYPE_UINT32,
                         ArgType::ARGTYPE_FLOAT_ARRAY);
  uint64_t size;
  ReadElement(aStream, size);
  if (!aStream.good()) {
    return;
  }

  mPayload.resize(size_t(size));
  aStream.read((char*)&mPayload.front(), size);
}

inline void RecordedFilterNodeSetAttribute::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mNode << "] SetAttribute (" << mIndex << ")";
}

inline bool RecordedFilterNodeSetInput::PlayEvent(
    Translator* aTranslator) const {
  FilterNode* node = aTranslator->LookupFilterNode(mNode);
  if (!node) {
    return false;
  }

  if (mInputFilter) {
    node->SetInput(mIndex, aTranslator->LookupFilterNode(mInputFilter));
  } else {
    node->SetInput(mIndex, aTranslator->LookupSourceSurface(mInputSurface));
  }

  return true;
}

template <class S>
void RecordedFilterNodeSetInput::Record(S& aStream) const {
  WriteElement(aStream, mNode);
  WriteElement(aStream, mIndex);
  WriteElement(aStream, mInputFilter);
  WriteElement(aStream, mInputSurface);
}

template <class S>
RecordedFilterNodeSetInput::RecordedFilterNodeSetInput(S& aStream)
    : RecordedEventDerived(FILTERNODESETINPUT) {
  ReadElement(aStream, mNode);
  ReadElement(aStream, mIndex);
  ReadElement(aStream, mInputFilter);
  ReadElement(aStream, mInputSurface);
}

inline void RecordedFilterNodeSetInput::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "[" << mNode << "] SetAttribute (" << mIndex << ", ";

  if (mInputFilter) {
    aStringStream << "Filter: " << mInputFilter;
  } else {
    aStringStream << "Surface: " << mInputSurface;
  }

  aStringStream << ")";
}

inline bool RecordedLink::PlayEvent(Translator* aTranslator) const {
  DrawTarget* dt = aTranslator->LookupDrawTarget(mDT);
  if (!dt) {
    return false;
  }
  dt->Link(mDestination.c_str(), mRect);
  return true;
}

template <class S>
void RecordedLink::Record(S& aStream) const {
  RecordedDrawingEvent::Record(aStream);
  WriteElement(aStream, mRect);
  uint32_t len = mDestination.length();
  WriteElement(aStream, len);
  if (len) {
    aStream.write(mDestination.data(), len);
  }
}

template <class S>
RecordedLink::RecordedLink(S& aStream) : RecordedDrawingEvent(LINK, aStream) {
  ReadElement(aStream, mRect);
  uint32_t len;
  ReadElement(aStream, len);
  mDestination.resize(size_t(len));
  if (len && aStream.good()) {
    aStream.read(&mDestination.front(), len);
  }
}

inline void RecordedLink::OutputSimpleEventInfo(
    std::stringstream& aStringStream) const {
  aStringStream << "Link [" << mDestination << " @ " << mRect << "]";
}

#define FOR_EACH_EVENT(f)                                          \
  f(DRAWTARGETCREATION, RecordedDrawTargetCreation);               \
  f(DRAWTARGETDESTRUCTION, RecordedDrawTargetDestruction);         \
  f(FILLRECT, RecordedFillRect);                                   \
  f(STROKERECT, RecordedStrokeRect);                               \
  f(STROKELINE, RecordedStrokeLine);                               \
  f(CLEARRECT, RecordedClearRect);                                 \
  f(COPYSURFACE, RecordedCopySurface);                             \
  f(SETTRANSFORM, RecordedSetTransform);                           \
  f(PUSHCLIPRECT, RecordedPushClipRect);                           \
  f(PUSHCLIP, RecordedPushClip);                                   \
  f(POPCLIP, RecordedPopClip);                                     \
  f(FILL, RecordedFill);                                           \
  f(FILLGLYPHS, RecordedFillGlyphs);                               \
  f(MASK, RecordedMask);                                           \
  f(STROKE, RecordedStroke);                                       \
  f(DRAWSURFACE, RecordedDrawSurface);                             \
  f(DRAWDEPENDENTSURFACE, RecordedDrawDependentSurface);           \
  f(DRAWSURFACEWITHSHADOW, RecordedDrawSurfaceWithShadow);         \
  f(DRAWFILTER, RecordedDrawFilter);                               \
  f(PATHCREATION, RecordedPathCreation);                           \
  f(PATHDESTRUCTION, RecordedPathDestruction);                     \
  f(SOURCESURFACECREATION, RecordedSourceSurfaceCreation);         \
  f(SOURCESURFACEDESTRUCTION, RecordedSourceSurfaceDestruction);   \
  f(FILTERNODECREATION, RecordedFilterNodeCreation);               \
  f(FILTERNODEDESTRUCTION, RecordedFilterNodeDestruction);         \
  f(GRADIENTSTOPSCREATION, RecordedGradientStopsCreation);         \
  f(GRADIENTSTOPSDESTRUCTION, RecordedGradientStopsDestruction);   \
  f(SNAPSHOT, RecordedSnapshot);                                   \
  f(SCALEDFONTCREATION, RecordedScaledFontCreation);               \
  f(SCALEDFONTDESTRUCTION, RecordedScaledFontDestruction);         \
  f(MASKSURFACE, RecordedMaskSurface);                             \
  f(FILTERNODESETATTRIBUTE, RecordedFilterNodeSetAttribute);       \
  f(FILTERNODESETINPUT, RecordedFilterNodeSetInput);               \
  f(CREATESIMILARDRAWTARGET, RecordedCreateSimilarDrawTarget);     \
  f(CREATECLIPPEDDRAWTARGET, RecordedCreateClippedDrawTarget);     \
  f(CREATEDRAWTARGETFORFILTER, RecordedCreateDrawTargetForFilter); \
  f(FONTDATA, RecordedFontData);                                   \
  f(FONTDESC, RecordedFontDescriptor);                             \
  f(PUSHLAYER, RecordedPushLayer);                                 \
  f(PUSHLAYERWITHBLEND, RecordedPushLayerWithBlend);               \
  f(POPLAYER, RecordedPopLayer);                                   \
  f(UNSCALEDFONTCREATION, RecordedUnscaledFontCreation);           \
  f(UNSCALEDFONTDESTRUCTION, RecordedUnscaledFontDestruction);     \
  f(INTOLUMINANCE, RecordedIntoLuminanceSource);                   \
  f(EXTERNALSURFACECREATION, RecordedExternalSurfaceCreation);     \
  f(FLUSH, RecordedFlush);                                         \
  f(DETACHALLSNAPSHOTS, RecordedDetachAllSnapshots);               \
  f(OPTIMIZESOURCESURFACE, RecordedOptimizeSourceSurface);         \
  f(LINK, RecordedLink);

#define DO_WITH_EVENT_TYPE(_typeenum, _class) \
  case _typeenum: {                           \
    auto e = _class(aStream);                 \
    return aAction(&e);                       \
  }

template <class S>
bool RecordedEvent::DoWithEvent(
    S& aStream, EventType aType,
    const std::function<bool(RecordedEvent*)>& aAction) {
  switch (aType) {
    FOR_EACH_EVENT(DO_WITH_EVENT_TYPE)
    default:
      return false;
  }
}

}  // namespace gfx
}  // namespace mozilla

#endif
