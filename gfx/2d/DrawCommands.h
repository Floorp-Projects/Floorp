/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_DRAWCOMMANDS_H_
#define MOZILLA_GFX_DRAWCOMMANDS_H_

#include <math.h>

#include "2D.h"
#include "Blur.h"
#include "Filters.h"
#include <vector>
#include "CaptureCommandList.h"
#include "DrawCommand.h"
#include "FilterNodeCapture.h"
#include "Logging.h"

namespace mozilla {
namespace gfx {

#define CLONE_INTO(Type) new (aList->Append<Type>()) Type

class StrokeOptionsCommand : public DrawingCommand
{
public:
  StrokeOptionsCommand(CommandType aType,
                       const StrokeOptions& aStrokeOptions)
    : DrawingCommand(aType)
    , mStrokeOptions(aStrokeOptions)
  {
    // Stroke Options dashes are owned by the caller.
    // Have to copy them here so they don't get freed
    // between now and replay.
    if (aStrokeOptions.mDashLength) {
      mDashes.resize(aStrokeOptions.mDashLength);
      mStrokeOptions.mDashPattern = &mDashes.front();
      PodCopy(&mDashes.front(), aStrokeOptions.mDashPattern, mStrokeOptions.mDashLength);
    }
  }

  virtual ~StrokeOptionsCommand() {}

protected:
  StrokeOptions mStrokeOptions;
  std::vector<Float> mDashes;
};

class StoredPattern
{
public:
  explicit StoredPattern(const Pattern& aPattern)
  {
    Assign(aPattern);
  }

  void Assign(const Pattern& aPattern)
  {
    switch (aPattern.GetType()) {
    case PatternType::COLOR:
      new (mColor)ColorPattern(*static_cast<const ColorPattern*>(&aPattern));
      return;
    case PatternType::SURFACE:
    {
      SurfacePattern* surfPat = new (mSurface)SurfacePattern(*static_cast<const SurfacePattern*>(&aPattern));
      surfPat->mSurface->GuaranteePersistance();
      return;
    }
    case PatternType::LINEAR_GRADIENT:
      new (mLinear)LinearGradientPattern(*static_cast<const LinearGradientPattern*>(&aPattern));
      return;
    case PatternType::RADIAL_GRADIENT:
      new (mRadial)RadialGradientPattern(*static_cast<const RadialGradientPattern*>(&aPattern));
      return;
    }
  }

  ~StoredPattern()
  {
    reinterpret_cast<Pattern*>(mPattern)->~Pattern();
  }

  Pattern* Get()
  {
    return reinterpret_cast<Pattern*>(mPattern);
  }

  const Pattern* Get() const
  {
    return reinterpret_cast<const Pattern*>(mPattern);
  }

  operator Pattern&()
  {
    return *reinterpret_cast<Pattern*>(mPattern);
  }

  operator const Pattern&() const
  {
    return *reinterpret_cast<const Pattern*>(mPattern);
  }

  StoredPattern(const StoredPattern& aPattern)
  {
    Assign(aPattern);
  }

private:
  StoredPattern operator=(const StoredPattern& aOther)
  {
    // Block this so that we notice if someone's doing excessive assigning.
    return *this;
  }

  union {
    char mPattern[sizeof(Pattern)];
    char mColor[sizeof(ColorPattern)];
    char mLinear[sizeof(LinearGradientPattern)];
    char mRadial[sizeof(RadialGradientPattern)];
    char mSurface[sizeof(SurfacePattern)];
  };
};

class DrawSurfaceCommand : public DrawingCommand
{
public:
  DrawSurfaceCommand(SourceSurface *aSurface, const Rect& aDest,
                     const Rect& aSource, const DrawSurfaceOptions& aSurfOptions,
                     const DrawOptions& aOptions)
    : DrawingCommand(DrawSurfaceCommand::Type)
    , mSurface(aSurface), mDest(aDest)
    , mSource(aSource), mSurfOptions(aSurfOptions)
    , mOptions(aOptions)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(DrawSurfaceCommand)(mSurface, mDest, mSource, mSurfOptions, mOptions);
  }

  void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->DrawSurface(mSurface, mDest, mSource, mSurfOptions, mOptions);
  }

  void Log(TreeLog& aStream) const override
  {
    aStream << "[DrawSurface surf=" << mSurface;
    aStream << " dest=" << mDest;
    aStream << " src=" << mSource;
    aStream << " surfOpt=" << mSurfOptions;
    aStream << " opt=" << mOptions;
    aStream << "]";
  }

  static const bool AffectsSnapshot = true;
  static const CommandType Type = CommandType::DRAWSURFACE;

private:
  RefPtr<SourceSurface> mSurface;
  Rect mDest;
  Rect mSource;
  DrawSurfaceOptions mSurfOptions;
  DrawOptions mOptions;
};

class DrawSurfaceWithShadowCommand : public DrawingCommand
{
public:
  DrawSurfaceWithShadowCommand(SourceSurface *aSurface,
                               const Point &aDest,
                               const Color &aColor,
                               const Point &aOffset,
                               Float aSigma,
                               CompositionOp aOperator)
    : DrawingCommand(DrawSurfaceWithShadowCommand::Type),
      mSurface(aSurface),
      mDest(aDest),
      mColor(aColor),
      mOffset(aOffset),
      mSigma(aSigma),
      mOperator(aOperator)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(DrawSurfaceWithShadowCommand)(mSurface, mDest, mColor, mOffset, mSigma, mOperator);
  }

  void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->DrawSurfaceWithShadow(mSurface, mDest, mColor, mOffset, mSigma, mOperator);
  }

  void Log(TreeLog& aStream) const override
  {
    aStream << "[DrawSurfaceWithShadow surf=" << mSurface;
    aStream << " dest=" << mDest;
    aStream << " color=" << mColor;
    aStream << " offset=" << mOffset;
    aStream << " sigma=" << mSigma;
    aStream << " op=" << mOperator;
    aStream << "]";
  }

  static const bool AffectsSnapshot = true;
  static const CommandType Type = CommandType::DRAWSURFACEWITHSHADOW;

private:
  RefPtr<SourceSurface> mSurface;
  Point mDest;
  Color mColor;
  Point mOffset;
  Float mSigma;
  CompositionOp mOperator;
};

class DrawFilterCommand : public DrawingCommand
{
public:
  DrawFilterCommand(FilterNode* aFilter, const Rect& aSourceRect,
                    const Point& aDestPoint, const DrawOptions& aOptions)
    : DrawingCommand(DrawFilterCommand::Type)
    , mFilter(aFilter), mSourceRect(aSourceRect)
    , mDestPoint(aDestPoint), mOptions(aOptions)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(DrawFilterCommand)(mFilter, mSourceRect, mDestPoint, mOptions);
  }

  void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    RefPtr<FilterNode> filter = mFilter;
    if (mFilter->GetBackendType() == FilterBackend::FILTER_BACKEND_CAPTURE) {
      filter = static_cast<FilterNodeCapture*>(filter.get())->Validate(aDT);

      // This can happen if the FilterNodeCapture is unable to create a
      // backing FilterNode on the target backend. Normally this would be
      // handled by the painting code, but here there's not much we can do.
      if (!filter) {
        return;
      }
    }
    aDT->DrawFilter(filter, mSourceRect, mDestPoint, mOptions);
  }

  void Log(TreeLog& aStream) const override
  {
    aStream << "[DrawFilter surf=" << mFilter;
    aStream << " src=" << mSourceRect;
    aStream << " dest=" << mDestPoint;
    aStream << " opt=" << mOptions;
    aStream << "]";
  }

  static const bool AffectsSnapshot = true;
  static const CommandType Type = CommandType::DRAWFILTER;

private:
  RefPtr<FilterNode> mFilter;
  Rect mSourceRect;
  Point mDestPoint;
  DrawOptions mOptions;
};

class ClearRectCommand : public DrawingCommand
{
public:
  explicit ClearRectCommand(const Rect& aRect)
    : DrawingCommand(ClearRectCommand::Type)
    , mRect(aRect)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(ClearRectCommand)(mRect);
  }

  void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->ClearRect(mRect);
  }

  void Log(TreeLog& aStream) const override
  {
    aStream << "[ClearRect rect=" << mRect << "]";
  }

  static const bool AffectsSnapshot = true;
  static const CommandType Type = CommandType::CLEARRECT;

private:
  Rect mRect;
};

class CopySurfaceCommand : public DrawingCommand
{
public:
  CopySurfaceCommand(SourceSurface* aSurface,
                     const IntRect& aSourceRect,
                     const IntPoint& aDestination)
    : DrawingCommand(CopySurfaceCommand::Type)
    , mSurface(aSurface)
    , mSourceRect(aSourceRect)
    , mDestination(aDestination)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(CopySurfaceCommand)(mSurface, mSourceRect, mDestination);
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix* aTransform) const override
  {
    MOZ_ASSERT(!aTransform || !aTransform->HasNonIntegerTranslation());
    Point dest(Float(mDestination.x), Float(mDestination.y));
    if (aTransform) {
      dest = aTransform->TransformPoint(dest);
    }
    aDT->CopySurface(mSurface, mSourceRect, IntPoint(uint32_t(dest.x), uint32_t(dest.y)));
  }

  void Log(TreeLog& aStream) const override
  {
    aStream << "[CopySurface surf=" << mSurface;
    aStream << " src=" << mSourceRect;
    aStream << " dest=" << mDestination;
    aStream << "]";
  }

  static const bool AffectsSnapshot = true;
  static const CommandType Type = CommandType::COPYSURFACE;

private:
  RefPtr<SourceSurface> mSurface;
  IntRect mSourceRect;
  IntPoint mDestination;
};

class FillRectCommand : public DrawingCommand
{
public:
  FillRectCommand(const Rect& aRect,
                  const Pattern& aPattern,
                  const DrawOptions& aOptions)
    : DrawingCommand(FillRectCommand::Type)
    , mRect(aRect)
    , mPattern(aPattern)
    , mOptions(aOptions)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(FillRectCommand)(mRect, mPattern, mOptions);
  }

  void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->FillRect(mRect, mPattern, mOptions);
  }

  void Log(TreeLog& aStream) const override
  {
    aStream << "[FillRect rect=" << mRect;
    aStream << " pattern=" << mPattern.Get();
    aStream << " opt=" << mOptions;
    aStream << "]";
  }

  static const bool AffectsSnapshot = true;
  static const CommandType Type = CommandType::FILLRECT;

private:
  Rect mRect;
  StoredPattern mPattern;
  DrawOptions mOptions;
};

class StrokeRectCommand : public StrokeOptionsCommand
{
public:
  StrokeRectCommand(const Rect& aRect,
                    const Pattern& aPattern,
                    const StrokeOptions& aStrokeOptions,
                    const DrawOptions& aOptions)
    : StrokeOptionsCommand(StrokeRectCommand::Type, aStrokeOptions)
    , mRect(aRect)
    , mPattern(aPattern)
    , mOptions(aOptions)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(StrokeRectCommand)(mRect, mPattern, mStrokeOptions, mOptions);
  }

  void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->StrokeRect(mRect, mPattern, mStrokeOptions, mOptions);
  }

  void Log(TreeLog& aStream) const override
  {
    aStream << "[StrokeRect rect=" << mRect;
    aStream << " pattern=" << mPattern.Get();
    aStream << " opt=" << mOptions;
    aStream << "]";
  }

  static const bool AffectsSnapshot = true;
  static const CommandType Type = CommandType::STROKERECT;

private:
  Rect mRect;
  StoredPattern mPattern;
  DrawOptions mOptions;
};

class StrokeLineCommand : public StrokeOptionsCommand
{
public:
  StrokeLineCommand(const Point& aStart,
                    const Point& aEnd,
                    const Pattern& aPattern,
                    const StrokeOptions& aStrokeOptions,
                    const DrawOptions& aOptions)
    : StrokeOptionsCommand(StrokeLineCommand::Type, aStrokeOptions)
    , mStart(aStart)
    , mEnd(aEnd)
    , mPattern(aPattern)
    , mOptions(aOptions)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(StrokeLineCommand)(mStart, mEnd, mPattern, mStrokeOptions, mOptions);
  }

  void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->StrokeLine(mStart, mEnd, mPattern, mStrokeOptions, mOptions);
  }

  void Log(TreeLog& aStream) const override
  {
    aStream << "[StrokeLine start=" << mStart;
    aStream << " end=" << mEnd;
    aStream << " pattern=" << mPattern.Get();
    aStream << " opt=" << mOptions;
    aStream << "]";
  }

  static const bool AffectsSnapshot = true;
  static const CommandType Type = CommandType::STROKELINE;

private:
  Point mStart;
  Point mEnd;
  StoredPattern mPattern;
  DrawOptions mOptions;
};

class FillCommand : public DrawingCommand
{
public:
  FillCommand(const Path* aPath,
              const Pattern& aPattern,
              const DrawOptions& aOptions)
    : DrawingCommand(FillCommand::Type)
    , mPath(const_cast<Path*>(aPath))
    , mPattern(aPattern)
    , mOptions(aOptions)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(FillCommand)(mPath, mPattern, mOptions);
  }

  void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->Fill(mPath, mPattern, mOptions);
  }

  void Log(TreeLog& aStream) const override
  {
    aStream << "[FillCommand path=" << mPath;
    aStream << " pattern=" << mPattern.Get();
    aStream << " opt=" << mOptions;
    aStream << "]";
  }

  static const bool AffectsSnapshot = true;
  static const CommandType Type = CommandType::FILL;

private:
  RefPtr<Path> mPath;
  StoredPattern mPattern;
  DrawOptions mOptions;
};

class StrokeCommand : public StrokeOptionsCommand
{
public:
  StrokeCommand(const Path* aPath,
                const Pattern& aPattern,
                const StrokeOptions& aStrokeOptions,
                const DrawOptions& aOptions)
    : StrokeOptionsCommand(StrokeCommand::Type, aStrokeOptions)
    , mPath(const_cast<Path*>(aPath))
    , mPattern(aPattern)
    , mOptions(aOptions)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(StrokeCommand)(mPath, mPattern, mStrokeOptions, mOptions);
  }

  void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->Stroke(mPath, mPattern, mStrokeOptions, mOptions);
  }

  void Log(TreeLog& aStream) const override
  {
    aStream << "[Stroke path=" << mPath;
    aStream << " pattern=" << mPattern.Get();
    aStream << " opt=" << mOptions;
    aStream << "]";
  }

  static const bool AffectsSnapshot = true;
  static const CommandType Type = CommandType::STROKE;

private:
  RefPtr<Path> mPath;
  StoredPattern mPattern;
  DrawOptions mOptions;
};

class FillGlyphsCommand : public DrawingCommand
{
  friend class DrawTargetCaptureImpl;
public:
  FillGlyphsCommand(ScaledFont* aFont,
                    const GlyphBuffer& aBuffer,
                    const Pattern& aPattern,
                    const DrawOptions& aOptions)
    : DrawingCommand(FillGlyphsCommand::Type)
    , mFont(aFont)
    , mPattern(aPattern)
    , mOptions(aOptions)
  {
    mGlyphs.resize(aBuffer.mNumGlyphs);
    memcpy(&mGlyphs.front(), aBuffer.mGlyphs, sizeof(Glyph) * aBuffer.mNumGlyphs);
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    GlyphBuffer glyphs = {
      mGlyphs.data(),
      (uint32_t)mGlyphs.size(),
    };
    CLONE_INTO(FillGlyphsCommand)(mFont, glyphs, mPattern, mOptions);
  }

  void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    GlyphBuffer buf;
    buf.mNumGlyphs = mGlyphs.size();
    buf.mGlyphs = &mGlyphs.front();
    aDT->FillGlyphs(mFont, buf, mPattern, mOptions);
  }

  void Log(TreeLog& aStream) const override
  {
    aStream << "[FillGlyphs font=" << mFont;
    aStream << " glyphCount=" << mGlyphs.size();
    aStream << " pattern=" << mPattern.Get();
    aStream << " opt=" << mOptions;
    aStream << "]";
  }

  static const bool AffectsSnapshot = true;
  static const CommandType Type = CommandType::FILLGLYPHS;

private:
  RefPtr<ScaledFont> mFont;
  std::vector<Glyph> mGlyphs;
  StoredPattern mPattern;
  DrawOptions mOptions;
};

class StrokeGlyphsCommand : public StrokeOptionsCommand
{
  friend class DrawTargetCaptureImpl;
public:
  StrokeGlyphsCommand(ScaledFont* aFont,
                      const GlyphBuffer& aBuffer,
                      const Pattern& aPattern,
                      const StrokeOptions& aStrokeOptions,
                      const DrawOptions& aOptions)
    : StrokeOptionsCommand(StrokeGlyphsCommand::Type, aStrokeOptions)
    , mFont(aFont)
    , mPattern(aPattern)
    , mOptions(aOptions)
  {
    mGlyphs.resize(aBuffer.mNumGlyphs);
    memcpy(&mGlyphs.front(), aBuffer.mGlyphs, sizeof(Glyph) * aBuffer.mNumGlyphs);
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    GlyphBuffer glyphs = {
      mGlyphs.data(),
      (uint32_t)mGlyphs.size(),
    };
    CLONE_INTO(StrokeGlyphsCommand)(mFont, glyphs, mPattern, mStrokeOptions, mOptions);
  }

  void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    GlyphBuffer buf;
    buf.mNumGlyphs = mGlyphs.size();
    buf.mGlyphs = &mGlyphs.front();
    aDT->StrokeGlyphs(mFont, buf, mPattern, mStrokeOptions, mOptions);
  }

  void Log(TreeLog& aStream) const override
  {
    aStream << "[StrokeGlyphs font=" << mFont;
    aStream << " glyphCount=" << mGlyphs.size();
    aStream << " pattern=" << mPattern.Get();
    aStream << " opt=" << mOptions;
    aStream << "]";
  }

  static const bool AffectsSnapshot = true;
  static const CommandType Type = CommandType::STROKEGLYPHS;

private:
  RefPtr<ScaledFont> mFont;
  std::vector<Glyph> mGlyphs;
  StoredPattern mPattern;
  DrawOptions mOptions;
};

class MaskCommand : public DrawingCommand
{
public:
  MaskCommand(const Pattern& aSource,
              const Pattern& aMask,
              const DrawOptions& aOptions)
    : DrawingCommand(MaskCommand::Type)
    , mSource(aSource)
    , mMask(aMask)
    , mOptions(aOptions)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(MaskCommand)(mSource, mMask, mOptions);
  }

  void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->Mask(mSource, mMask, mOptions);
  }

  void Log(TreeLog& aStream) const override
  {
    aStream << "[Mask source=" << mSource.Get();
    aStream << " mask=" << mMask.Get();
    aStream << " opt=" << mOptions;
    aStream << "]";
  }

  static const bool AffectsSnapshot = true;
  static const CommandType Type = CommandType::MASK;

private:
  StoredPattern mSource;
  StoredPattern mMask;
  DrawOptions mOptions;
};

class MaskSurfaceCommand : public DrawingCommand
{
public:
  MaskSurfaceCommand(const Pattern& aSource,
                     const SourceSurface* aMask,
                     const Point& aOffset,
                     const DrawOptions& aOptions)
    : DrawingCommand(MaskSurfaceCommand::Type)
    , mSource(aSource)
    , mMask(const_cast<SourceSurface*>(aMask))
    , mOffset(aOffset)
    , mOptions(aOptions)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(MaskSurfaceCommand)(mSource, mMask, mOffset, mOptions);
  }

  void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->MaskSurface(mSource, mMask, mOffset, mOptions);
  }

  void Log(TreeLog& aStream) const override
  {
    aStream << "[Mask source=" << mSource.Get();
    aStream << " mask=" << mMask;
    aStream << " offset=" << &mOffset;
    aStream << " opt=" << mOptions;
    aStream << "]";
  }

  static const bool AffectsSnapshot = true;
  static const CommandType Type = CommandType::MASKSURFACE;

private:
  StoredPattern mSource;
  RefPtr<SourceSurface> mMask;
  Point mOffset;
  DrawOptions mOptions;
};

class PushClipCommand : public DrawingCommand
{
public:
  explicit PushClipCommand(const Path* aPath)
    : DrawingCommand(PushClipCommand::Type)
    , mPath(const_cast<Path*>(aPath))
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(PushClipCommand)(mPath);
  }

  void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->PushClip(mPath);
  }

  void Log(TreeLog& aStream) const override
  {
    aStream << "[PushClip path=" << mPath << "]";
  }

  static const bool AffectsSnapshot = false;
  static const CommandType Type = CommandType::PUSHCLIP;

private:
  RefPtr<Path> mPath;
};

class PushClipRectCommand : public DrawingCommand
{
public:
  explicit PushClipRectCommand(const Rect& aRect)
    : DrawingCommand(PushClipRectCommand::Type)
    , mRect(aRect)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(PushClipRectCommand)(mRect);
  }

  void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->PushClipRect(mRect);
  }

  void Log(TreeLog& aStream) const override
  {
    aStream << "[PushClipRect rect=" << mRect << "]";
  }

  static const bool AffectsSnapshot = false;
  static const CommandType Type = CommandType::PUSHCLIPRECT;

private:
  Rect mRect;
};

class PushLayerCommand : public DrawingCommand
{
public:
  PushLayerCommand(const bool aOpaque,
                   const Float aOpacity,
                   SourceSurface* aMask,
                   const Matrix& aMaskTransform,
                   const IntRect& aBounds,
                   bool aCopyBackground)
    : DrawingCommand(PushLayerCommand::Type)
    , mOpaque(aOpaque)
    , mOpacity(aOpacity)
    , mMask(aMask)
    , mMaskTransform(aMaskTransform)
    , mBounds(aBounds)
    , mCopyBackground(aCopyBackground)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(PushLayerCommand)(mOpaque, mOpacity, mMask, mMaskTransform, mBounds, mCopyBackground);
  }

  void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->PushLayer(mOpaque, mOpacity, mMask,
                   mMaskTransform, mBounds, mCopyBackground);
  }

  void Log(TreeLog& aStream) const override
  {
    aStream << "[PushLayer opaque=" << mOpaque;
    aStream << " opacity=" << mOpacity;
    aStream << " mask=" << mMask;
    aStream << " maskTransform=" << mMaskTransform;
    aStream << " bounds=" << mBounds;
    aStream << " copyBackground=" << mCopyBackground;
    aStream << "]";
  }

  static const bool AffectsSnapshot = false;
  static const CommandType Type = CommandType::PUSHLAYER;

private:
  bool mOpaque;
  float mOpacity;
  RefPtr<SourceSurface> mMask;
  Matrix mMaskTransform;
  IntRect mBounds;
  bool mCopyBackground;
};

class PopClipCommand : public DrawingCommand
{
public:
  PopClipCommand()
    : DrawingCommand(PopClipCommand::Type)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(PopClipCommand)();
  }

  void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->PopClip();
  }

  void Log(TreeLog& aStream) const override
  {
    aStream << "[PopClip]";
  }

  static const bool AffectsSnapshot = false;
  static const CommandType Type = CommandType::POPCLIP;
};

class PopLayerCommand : public DrawingCommand
{
public:
  PopLayerCommand()
    : DrawingCommand(PopLayerCommand::Type)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(PopLayerCommand)();
  }

  void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->PopLayer();
  }

  void Log(TreeLog& aStream) const override
  {
    aStream << "[PopLayer]";
  }

  static const bool AffectsSnapshot = true;
  static const CommandType Type = CommandType::POPLAYER;
};

class SetTransformCommand : public DrawingCommand
{
  friend class DrawTargetCaptureImpl;
public:
  explicit SetTransformCommand(const Matrix& aTransform)
    : DrawingCommand(SetTransformCommand::Type)
    , mTransform(aTransform)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(SetTransformCommand)(mTransform);
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix* aMatrix) const override
  {
    if (aMatrix) {
      aDT->SetTransform(mTransform * (*aMatrix));
    } else {
      aDT->SetTransform(mTransform);
    }
  }

  void Log(TreeLog& aStream) const override
  {
    aStream << "[SetTransform transform=" << mTransform << "]";
  }

  static const bool AffectsSnapshot = false;
  static const CommandType Type = CommandType::SETTRANSFORM;

private:
  Matrix mTransform;
};

class SetPermitSubpixelAACommand : public DrawingCommand
{
  friend class DrawTargetCaptureImpl;
public:
  explicit SetPermitSubpixelAACommand(bool aPermitSubpixelAA)
    : DrawingCommand(SetPermitSubpixelAACommand::Type)
    , mPermitSubpixelAA(aPermitSubpixelAA)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(SetPermitSubpixelAACommand)(mPermitSubpixelAA);
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix* aMatrix) const override
  {
    aDT->SetPermitSubpixelAA(mPermitSubpixelAA);
  }

  void Log(TreeLog& aStream) const override
  {
    aStream << "[SetPermitSubpixelAA permitSubpixelAA=" << mPermitSubpixelAA << "]";
  }

  static const bool AffectsSnapshot = false;
  static const CommandType Type = CommandType::SETPERMITSUBPIXELAA;

private:
  bool mPermitSubpixelAA;
};

class FlushCommand : public DrawingCommand
{
public:
  explicit FlushCommand()
    : DrawingCommand(FlushCommand::Type)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(FlushCommand)();
  }

  void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->Flush();
  }

  void Log(TreeLog& aStream) const override
  {
    aStream << "[Flush]";
  }

  static const bool AffectsSnapshot = false;
  static const CommandType Type = CommandType::FLUSH;
};

class BlurCommand : public DrawingCommand
{
public:
  explicit BlurCommand(const AlphaBoxBlur& aBlur)
   : DrawingCommand(BlurCommand::Type)
   , mBlur(aBlur)
  {}

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(BlurCommand)(mBlur);
  }

  void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->Blur(mBlur);
  }

  void Log(TreeLog& aStream) const override
  {
    aStream << "[Blur]";
  }

  static const bool AffectsSnapshot = true;
  static const CommandType Type = CommandType::BLUR;

private:
  AlphaBoxBlur mBlur;
};

#undef CLONE_INTO

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_DRAWCOMMANDS_H_ */
