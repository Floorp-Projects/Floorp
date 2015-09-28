/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_DRAWCOMMAND_H_
#define MOZILLA_GFX_DRAWCOMMAND_H_


#define _USE_MATH_DEFINES
#include <math.h>

#include "2D.h"
#include "Filters.h"
#include <vector>

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
  MASK,
  MASKSURFACE,
  PUSHCLIP,
  PUSHCLIPRECT,
  POPCLIP,
  SETTRANSFORM,
  FLUSH
};

class DrawingCommand
{
public:
  virtual ~DrawingCommand() {}

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix* aTransform = nullptr) const = 0;

  virtual bool GetAffectedRect(Rect& aDeviceRect, const Matrix& aTransform) const { return false; }

protected:
  explicit DrawingCommand(CommandType aType)
    : mType(aType)
  {
  }

  CommandType GetType() { return mType; }

private:
  CommandType mType;
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
      SurfacePattern* surfPat = new (mColor)SurfacePattern(*static_cast<const SurfacePattern*>(&aPattern));
      surfPat->mSurface->GuaranteePersistance();
      return;
    }
    case PatternType::LINEAR_GRADIENT:
      new (mColor)LinearGradientPattern(*static_cast<const LinearGradientPattern*>(&aPattern));
      return;
    case PatternType::RADIAL_GRADIENT:
      new (mColor)RadialGradientPattern(*static_cast<const RadialGradientPattern*>(&aPattern));
      return;
    }
  }

  ~StoredPattern()
  {
    reinterpret_cast<Pattern*>(mColor)->~Pattern();
  }

  operator Pattern&()
  {
    return *reinterpret_cast<Pattern*>(mColor);
  }

  operator const Pattern&() const
  {
    return *reinterpret_cast<const Pattern*>(mColor);
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

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const
  {
    aDT->DrawSurface(mSurface, mDest, mSource, mSurfOptions, mOptions);
  }

private:
  RefPtr<SourceSurface> mSurface;
  Rect mDest;
  Rect mSource;
  DrawSurfaceOptions mSurfOptions;
  DrawOptions mOptions;
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

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const
  {
    aDT->DrawFilter(mFilter, mSourceRect, mDestPoint, mOptions);
  }

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

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const
  {
    aDT->ClearRect(mRect);
  }

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

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix* aTransform) const
  {
    MOZ_ASSERT(!aTransform || !aTransform->HasNonIntegerTranslation());
    Point dest(Float(mDestination.x), Float(mDestination.y));
    if (aTransform) {
      dest = (*aTransform) * dest;
    }
    aDT->CopySurface(mSurface, mSourceRect, IntPoint(uint32_t(dest.x), uint32_t(dest.y)));
  }

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

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const
  {
    aDT->FillRect(mRect, mPattern, mOptions);
  }

  bool GetAffectedRect(Rect& aDeviceRect, const Matrix& aTransform) const
  {
    aDeviceRect = aTransform.TransformBounds(mRect);
    return true;
  }

private:
  Rect mRect;
  StoredPattern mPattern;
  DrawOptions mOptions;
};

class StrokeRectCommand : public DrawingCommand
{
public:
  StrokeRectCommand(const Rect& aRect,
                    const Pattern& aPattern,
                    const StrokeOptions& aStrokeOptions,
                    const DrawOptions& aOptions)
    : DrawingCommand(CommandType::STROKERECT)
    , mRect(aRect)
    , mPattern(aPattern)
    , mStrokeOptions(aStrokeOptions)
    , mOptions(aOptions)
  {
    if (aStrokeOptions.mDashLength) {
      mDashes.resize(aStrokeOptions.mDashLength);
      mStrokeOptions.mDashPattern = &mDashes.front();
      memcpy(&mDashes.front(), aStrokeOptions.mDashPattern, mStrokeOptions.mDashLength * sizeof(Float));
    }
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const
  {
    aDT->StrokeRect(mRect, mPattern, mStrokeOptions, mOptions);
  }

private:
  Rect mRect;
  StoredPattern mPattern;
  StrokeOptions mStrokeOptions;
  DrawOptions mOptions;
  std::vector<Float> mDashes;
};

class StrokeLineCommand : public DrawingCommand
{
public:
  StrokeLineCommand(const Point& aStart,
                    const Point& aEnd,
                    const Pattern& aPattern,
                    const StrokeOptions& aStrokeOptions,
                    const DrawOptions& aOptions)
    : DrawingCommand(CommandType::STROKELINE)
    , mStart(aStart)
    , mEnd(aEnd)
    , mPattern(aPattern)
    , mStrokeOptions(aStrokeOptions)
    , mOptions(aOptions)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const
  {
    aDT->StrokeLine(mStart, mEnd, mPattern, mStrokeOptions, mOptions);
  }

private:
  Point mStart;
  Point mEnd;
  StoredPattern mPattern;
  StrokeOptions mStrokeOptions;
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

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const
  {
    aDT->Fill(mPath, mPattern, mOptions);
  }

  bool GetAffectedRect(Rect& aDeviceRect, const Matrix& aTransform) const
  {
    aDeviceRect = mPath->GetBounds(aTransform);
    return true;
  }

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

  Rect result = aRect;
  result.Inflate(dx, dy);
  return result;
}

class StrokeCommand : public DrawingCommand
{
public:
  StrokeCommand(const Path* aPath,
                const Pattern& aPattern,
                const StrokeOptions& aStrokeOptions,
                const DrawOptions& aOptions)
    : DrawingCommand(CommandType::STROKE)
    , mPath(const_cast<Path*>(aPath))
    , mPattern(aPattern)
    , mStrokeOptions(aStrokeOptions)
    , mOptions(aOptions)
  {
    if (aStrokeOptions.mDashLength) {
      mDashes.resize(aStrokeOptions.mDashLength);
      mStrokeOptions.mDashPattern = &mDashes.front();
      memcpy(&mDashes.front(), aStrokeOptions.mDashPattern, mStrokeOptions.mDashLength * sizeof(Float));
    }
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const
  {
    aDT->Stroke(mPath, mPattern, mStrokeOptions, mOptions);
  }

  bool GetAffectedRect(Rect& aDeviceRect, const Matrix& aTransform) const
  {
    aDeviceRect = PathExtentsToMaxStrokeExtents(mStrokeOptions, mPath->GetBounds(aTransform), aTransform);
    return true;
  }

private:
  RefPtr<Path> mPath;
  StoredPattern mPattern;
  StrokeOptions mStrokeOptions;
  DrawOptions mOptions;
  std::vector<Float> mDashes;
};

class FillGlyphsCommand : public DrawingCommand
{
public:
  FillGlyphsCommand(ScaledFont* aFont,
                    const GlyphBuffer& aBuffer,
                    const Pattern& aPattern,
                    const DrawOptions& aOptions,
                    const GlyphRenderingOptions* aRenderingOptions)
    : DrawingCommand(CommandType::FILLGLYPHS)
    , mFont(aFont)
    , mPattern(aPattern)
    , mOptions(aOptions)
    , mRenderingOptions(const_cast<GlyphRenderingOptions*>(aRenderingOptions))
  {
    mGlyphs.resize(aBuffer.mNumGlyphs);
    memcpy(&mGlyphs.front(), aBuffer.mGlyphs, sizeof(Glyph) * aBuffer.mNumGlyphs);
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const
  {
    GlyphBuffer buf;
    buf.mNumGlyphs = mGlyphs.size();
    buf.mGlyphs = &mGlyphs.front();
    aDT->FillGlyphs(mFont, buf, mPattern, mOptions, mRenderingOptions);
  }

private:
  RefPtr<ScaledFont> mFont;
  std::vector<Glyph> mGlyphs;
  StoredPattern mPattern;
  DrawOptions mOptions;
  RefPtr<GlyphRenderingOptions> mRenderingOptions;
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

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const
  {
    aDT->Mask(mSource, mMask, mOptions);
  }

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

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const
  {
    aDT->MaskSurface(mSource, mMask, mOffset, mOptions);
  }

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

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const
  {
    aDT->PushClip(mPath);
  }

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

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const
  {
    aDT->PushClipRect(mRect);
  }

private:
  Rect mRect;
};

class PopClipCommand : public DrawingCommand
{
public:
  PopClipCommand()
    : DrawingCommand(CommandType::POPCLIP)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const
  {
    aDT->PopClip();
  }
};

class SetTransformCommand : public DrawingCommand
{
public:
  explicit SetTransformCommand(const Matrix& aTransform)
    : DrawingCommand(CommandType::SETTRANSFORM)
    , mTransform(aTransform)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix* aMatrix) const
  {
    if (aMatrix) {
      aDT->SetTransform(mTransform * (*aMatrix));
    } else {
      aDT->SetTransform(mTransform);
    }
  }

private:
  Matrix mTransform;
};

class FlushCommand : public DrawingCommand
{
public:
  explicit FlushCommand()
    : DrawingCommand(CommandType::FLUSH)
  {
  }

  virtual void ExecuteOnDT(DrawTarget* aDT, const Matrix*) const
  {
    aDT->Flush();
  }
};

} // namespace gfx

} // namespace mozilla


#endif /* MOZILLA_GFX_DRAWCOMMAND_H_ */
