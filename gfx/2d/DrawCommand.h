/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_DRAWCOMMAND_H_
#define MOZILLA_GFX_DRAWCOMMAND_H_

#include <math.h>

#include "2D.h"
#include "Blur.h"
#include "Filters.h"
#include <vector>
#include "CaptureCommandList.h"
#include "FilterNodeCapture.h"

namespace mozilla {
namespace gfx {

enum class CommandType : int8_t {
  DRAWSURFACE = 0,
  DRAWFILTER,
  DRAWSURFACEWITHSHADOW,
  CLEARRECT,
  COPYSURFACE,
  COPYRECT,
  FILLRECT,
  STROKERECT,
  STROKELINE,
  STROKE,
  FILL,
  FILLGLYPHS,
  STROKEGLYPHS,
  MASK,
  MASKSURFACE,
  PUSHCLIP,
  PUSHCLIPRECT,
  PUSHLAYER,
  POPCLIP,
  POPLAYER,
  SETTRANSFORM,
  SETPERMITSUBPIXELAA,
  FLUSH,
  BLUR
};

class DrawingCommand
{
public:
  virtual ~DrawingCommand() {}

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix* aTransform = nullptr) const = 0;
  virtual bool GetAffectedRect(Rect& aDeviceRect, const Matrix& aTransform) const { return false; }
  virtual void CloneInto(CaptureCommandList* aList) = 0;

  CommandType GetType() { return mType; }

protected:
  explicit DrawingCommand(CommandType aType)
    : mType(aType)
  {
  }

private:
  CommandType mType;
};

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
    : DrawingCommand(CommandType::DRAWSURFACE)
    , mSurface(aSurface), mDest(aDest)
    , mSource(aSource), mSurfOptions(aSurfOptions)
    , mOptions(aOptions)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(DrawSurfaceCommand)(mSurface, mDest, mSource, mSurfOptions, mOptions);
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->DrawSurface(mSurface, mDest, mSource, mSurfOptions, mOptions);
  }

  static const bool AffectsSnapshot = true;

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
    : DrawingCommand(CommandType::DRAWSURFACEWITHSHADOW),
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

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->DrawSurfaceWithShadow(mSurface, mDest, mColor, mOffset, mSigma, mOperator);
  }

  static const bool AffectsSnapshot = true;

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
    : DrawingCommand(CommandType::DRAWSURFACE)
    , mFilter(aFilter), mSourceRect(aSourceRect)
    , mDestPoint(aDestPoint), mOptions(aOptions)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(DrawFilterCommand)(mFilter, mSourceRect, mDestPoint, mOptions);
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    RefPtr<FilterNode> filter = mFilter;
    if (mFilter->GetBackendType() == FilterBackend::FILTER_BACKEND_CAPTURE) {
      filter = static_cast<FilterNodeCapture*>(filter.get())->Validate(aDT);
    }
    aDT->DrawFilter(filter, mSourceRect, mDestPoint, mOptions);
  }

  static const bool AffectsSnapshot = true;

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
    : DrawingCommand(CommandType::CLEARRECT)
    , mRect(aRect)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(ClearRectCommand)(mRect);
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->ClearRect(mRect);
  }

  static const bool AffectsSnapshot = true;

private:
  Rect mRect;
};

class CopySurfaceCommand : public DrawingCommand
{
public:
  CopySurfaceCommand(SourceSurface* aSurface,
                     const IntRect& aSourceRect,
                     const IntPoint& aDestination)
    : DrawingCommand(CommandType::COPYSURFACE)
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

  static const bool AffectsSnapshot = true;

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
    : DrawingCommand(CommandType::FILLRECT)
    , mRect(aRect)
    , mPattern(aPattern)
    , mOptions(aOptions)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(FillRectCommand)(mRect, mPattern, mOptions);
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->FillRect(mRect, mPattern, mOptions);
  }

  bool GetAffectedRect(Rect& aDeviceRect, const Matrix& aTransform) const override
  {
    aDeviceRect = aTransform.TransformBounds(mRect);
    return true;
  }

  static const bool AffectsSnapshot = true;

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
    : StrokeOptionsCommand(CommandType::STROKERECT, aStrokeOptions)
    , mRect(aRect)
    , mPattern(aPattern)
    , mOptions(aOptions)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(StrokeRectCommand)(mRect, mPattern, mStrokeOptions, mOptions);
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->StrokeRect(mRect, mPattern, mStrokeOptions, mOptions);
  }

  static const bool AffectsSnapshot = true;

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
    : StrokeOptionsCommand(CommandType::STROKELINE, aStrokeOptions)
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

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->StrokeLine(mStart, mEnd, mPattern, mStrokeOptions, mOptions);
  }

  static const bool AffectsSnapshot = true;

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
    : DrawingCommand(CommandType::FILL)
    , mPath(const_cast<Path*>(aPath))
    , mPattern(aPattern)
    , mOptions(aOptions)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(FillCommand)(mPath, mPattern, mOptions);
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->Fill(mPath, mPattern, mOptions);
  }

  bool GetAffectedRect(Rect& aDeviceRect, const Matrix& aTransform) const override
  {
    aDeviceRect = mPath->GetBounds(aTransform);
    return true;
  }

  static const bool AffectsSnapshot = true;

private:
  RefPtr<Path> mPath;
  StoredPattern mPattern;
  DrawOptions mOptions;
};

#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880
#endif

#ifndef M_SQRT1_2
#define M_SQRT1_2 0.707106781186547524400844362104849039
#endif

// The logic for this comes from _cairo_stroke_style_max_distance_from_path
static Rect
PathExtentsToMaxStrokeExtents(const StrokeOptions &aStrokeOptions,
                              const Rect &aRect,
                              const Matrix &aTransform)
{
  double styleExpansionFactor = 0.5f;

  if (aStrokeOptions.mLineCap == CapStyle::SQUARE) {
    styleExpansionFactor = M_SQRT1_2;
  }

  if (aStrokeOptions.mLineJoin == JoinStyle::MITER &&
      styleExpansionFactor < M_SQRT2 * aStrokeOptions.mMiterLimit) {
    styleExpansionFactor = M_SQRT2 * aStrokeOptions.mMiterLimit;
  }

  styleExpansionFactor *= aStrokeOptions.mLineWidth;

  double dx = styleExpansionFactor * hypot(aTransform._11, aTransform._21);
  double dy = styleExpansionFactor * hypot(aTransform._22, aTransform._12);

  // Even if the stroke only partially covers a pixel, it must still render to
  // full pixels. Round up to compensate for this.
  dx = ceil(dx);
  dy = ceil(dy);

  Rect result = aRect;
  result.Inflate(dx, dy);
  return result;
}


class StrokeCommand : public StrokeOptionsCommand
{
public:
  StrokeCommand(const Path* aPath,
                const Pattern& aPattern,
                const StrokeOptions& aStrokeOptions,
                const DrawOptions& aOptions)
    : StrokeOptionsCommand(CommandType::STROKE, aStrokeOptions)
    , mPath(const_cast<Path*>(aPath))
    , mPattern(aPattern)
    , mOptions(aOptions)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(StrokeCommand)(mPath, mPattern, mStrokeOptions, mOptions);
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->Stroke(mPath, mPattern, mStrokeOptions, mOptions);
  }

  bool GetAffectedRect(Rect& aDeviceRect, const Matrix& aTransform) const override
  {
    aDeviceRect = PathExtentsToMaxStrokeExtents(mStrokeOptions, mPath->GetBounds(aTransform), aTransform);
    return true;
  }

  static const bool AffectsSnapshot = true;

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
    : DrawingCommand(CommandType::FILLGLYPHS)
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

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    GlyphBuffer buf;
    buf.mNumGlyphs = mGlyphs.size();
    buf.mGlyphs = &mGlyphs.front();
    aDT->FillGlyphs(mFont, buf, mPattern, mOptions);
  }

  static const bool AffectsSnapshot = true;

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
    : StrokeOptionsCommand(CommandType::STROKEGLYPHS, aStrokeOptions)
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

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    GlyphBuffer buf;
    buf.mNumGlyphs = mGlyphs.size();
    buf.mGlyphs = &mGlyphs.front();
    aDT->StrokeGlyphs(mFont, buf, mPattern, mStrokeOptions, mOptions);
  }

  static const bool AffectsSnapshot = true;

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
    : DrawingCommand(CommandType::MASK)
    , mSource(aSource)
    , mMask(aMask)
    , mOptions(aOptions)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(MaskCommand)(mSource, mMask, mOptions);
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->Mask(mSource, mMask, mOptions);
  }

  static const bool AffectsSnapshot = true;

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
    : DrawingCommand(CommandType::MASKSURFACE)
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

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->MaskSurface(mSource, mMask, mOffset, mOptions);
  }

  static const bool AffectsSnapshot = true;

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
    : DrawingCommand(CommandType::PUSHCLIP)
    , mPath(const_cast<Path*>(aPath))
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(PushClipCommand)(mPath);
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->PushClip(mPath);
  }

  static const bool AffectsSnapshot = false;

private:
  RefPtr<Path> mPath;
};

class PushClipRectCommand : public DrawingCommand
{
public:
  explicit PushClipRectCommand(const Rect& aRect)
    : DrawingCommand(CommandType::PUSHCLIPRECT)
    , mRect(aRect)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(PushClipRectCommand)(mRect);
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->PushClipRect(mRect);
  }

  static const bool AffectsSnapshot = false;

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
    : DrawingCommand(CommandType::PUSHLAYER)
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

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->PushLayer(mOpaque, mOpacity, mMask,
                   mMaskTransform, mBounds, mCopyBackground);
  }

  static const bool AffectsSnapshot = false;

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
    : DrawingCommand(CommandType::POPCLIP)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(PopClipCommand)();
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->PopClip();
  }

  static const bool AffectsSnapshot = false;
};

class PopLayerCommand : public DrawingCommand
{
public:
  PopLayerCommand()
    : DrawingCommand(CommandType::POPLAYER)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(PopLayerCommand)();
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->PopLayer();
  }

  static const bool AffectsSnapshot = true;
};

class SetTransformCommand : public DrawingCommand
{
  friend class DrawTargetCaptureImpl;
public:
  explicit SetTransformCommand(const Matrix& aTransform)
    : DrawingCommand(CommandType::SETTRANSFORM)
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

  static const bool AffectsSnapshot = false;

private:
  Matrix mTransform;
};

class SetPermitSubpixelAACommand : public DrawingCommand
{
  friend class DrawTargetCaptureImpl;
public:
  explicit SetPermitSubpixelAACommand(bool aPermitSubpixelAA)
    : DrawingCommand(CommandType::SETPERMITSUBPIXELAA)
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

  static const bool AffectsSnapshot = false;

private:
  bool mPermitSubpixelAA;
};

class FlushCommand : public DrawingCommand
{
public:
  explicit FlushCommand()
    : DrawingCommand(CommandType::FLUSH)
  {
  }

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(FlushCommand)();
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->Flush();
  }

  static const bool AffectsSnapshot = false;
};

class BlurCommand : public DrawingCommand
{
public:
  explicit BlurCommand(const AlphaBoxBlur& aBlur)
   : DrawingCommand(CommandType::BLUR)
   , mBlur(aBlur)
  {}

  void CloneInto(CaptureCommandList* aList) override
  {
    CLONE_INTO(BlurCommand)(mBlur);
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const override
  {
    aDT->Blur(mBlur);
  }

  static const bool AffectsSnapshot = true;

private:
  AlphaBoxBlur mBlur;
};

#undef CLONE_INTO

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_DRAWCOMMAND_H_ */
