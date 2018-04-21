/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGCONTEXTPAINT_H_
#define MOZILLA_SVGCONTEXTPAINT_H_

#include "DrawMode.h"
#include "gfxMatrix.h"
#include "gfxPattern.h"
#include "gfxTypes.h"
#include "gfxUtils.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/gfx/2D.h"
#include "nsColor.h"
#include "nsStyleStruct.h"
#include "nsTArray.h"
#include "ImgDrawResult.h"

class gfxContext;
class nsIDocument;
class nsSVGPaintServerFrame;

namespace mozilla {

namespace dom {
class SVGDocument;
}

/**
 * This class is used to pass information about a context element through to
 * SVG painting code in order to resolve the 'context-fill' and related
 * keywords. See:
 *
 *   https://www.w3.org/TR/SVG2/painting.html#context-paint
 *
 * This feature allows the color in an SVG-in-OpenType glyph to come from the
 * computed style for the text that is being drawn, for example, or for color
 * in an SVG embedded by an <img> element to come from the embedding <img>
 * element.
 *
 * This class is reference counted so that it can be shared among many similar
 * SVGImageContext objects. (SVGImageContext objects are frequently
 * copy-constructed with small modifications, and we'd like for those copies to
 * be able to share their context-paint data cheaply.)  However, in most cases,
 * SVGContextPaint instances are stored in a local RefPtr and only last for the
 * duration of a function call.
 * XXX Note: SVGImageContext doesn't actually have a SVGContextPaint member yet,
 * but it will in a later patch in the patch series that added this comment.
 */
class SVGContextPaint : public RefCounted<SVGContextPaint>
{
protected:
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::gfx::Float Float;
  typedef mozilla::image::imgDrawingParams imgDrawingParams;

  SVGContextPaint()
    : mDashOffset(0.0f)
    , mStrokeWidth(0.0f)
  {}

public:

  MOZ_DECLARE_REFCOUNTED_TYPENAME(SVGContextPaint)

  virtual ~SVGContextPaint() {}

  virtual already_AddRefed<gfxPattern>
  GetFillPattern(const DrawTarget* aDrawTarget,
                 float aOpacity,
                 const gfxMatrix& aCTM,
                 imgDrawingParams& aImgParams) = 0;
  virtual already_AddRefed<gfxPattern>
  GetStrokePattern(const DrawTarget* aDrawTarget,
                   float aOpacity,
                   const gfxMatrix& aCTM,
                   imgDrawingParams& aImgParams) = 0;
  virtual float GetFillOpacity() const = 0;
  virtual float GetStrokeOpacity() const = 0;

  already_AddRefed<gfxPattern>
  GetFillPattern(const DrawTarget* aDrawTarget, const gfxMatrix& aCTM,
                 imgDrawingParams& aImgParams) {
    return GetFillPattern(aDrawTarget, GetFillOpacity(), aCTM, aImgParams);
  }

  already_AddRefed<gfxPattern>
  GetStrokePattern(const DrawTarget* aDrawTarget, const gfxMatrix& aCTM,
                   imgDrawingParams& aImgParams) {
    return GetStrokePattern(aDrawTarget, GetStrokeOpacity(), aCTM, aImgParams);
  }

  static SVGContextPaint* GetContextPaint(nsIContent* aContent);

  // XXX This gets the geometry params from the gfxContext.  We should get that
  // information from the actual paint context!
  void InitStrokeGeometry(gfxContext *aContext,
                          float devUnitsPerSVGUnit);

  const FallibleTArray<Float>& GetStrokeDashArray() const {
    return mDashes;
  }

  Float GetStrokeDashOffset() const {
    return mDashOffset;
  }

  Float GetStrokeWidth() const {
    return mStrokeWidth;
  }

  virtual uint32_t Hash() const {
    MOZ_ASSERT_UNREACHABLE("Only VectorImage needs to hash, and that should "
                           "only be operating on our SVGEmbeddingContextPaint "
                           "subclass");
    return 0;
  }

  /**
   * Returns true if image context paint is allowed to be used in an image that
   * has the given URI, else returns false.
   */
  static bool IsAllowedForImageFromURI(nsIURI* aURI);

private:
  // Member-vars are initialized in InitStrokeGeometry.
  FallibleTArray<Float> mDashes;
  MOZ_INIT_OUTSIDE_CTOR Float mDashOffset;
  MOZ_INIT_OUTSIDE_CTOR Float mStrokeWidth;
};

/**
 * RAII class used to temporarily set and remove an SVGContextPaint while a
 * piece of SVG is being painted.  The context paint is set on the SVG's owner
 * document, as expected by SVGContextPaint::GetContextPaint.  Any pre-existing
 * context paint is restored after this class removes the context paint that it
 * set.
 */
class MOZ_RAII AutoSetRestoreSVGContextPaint
{
public:
  AutoSetRestoreSVGContextPaint(const SVGContextPaint& aContextPaint,
                                dom::SVGDocument& aSVGDocument);
  ~AutoSetRestoreSVGContextPaint();
private:
  dom::SVGDocument& mSVGDocument;
  // The context paint that needs to be restored by our dtor after it removes
  // aContextPaint:
  const SVGContextPaint* mOuterContextPaint;
};


/**
 * This class should be flattened into SVGContextPaint once we get rid of the
 * other sub-class (SimpleTextContextPaint).
 */
struct SVGContextPaintImpl : public SVGContextPaint
{
protected:
  typedef mozilla::gfx::DrawTarget DrawTarget;

public:

  DrawMode
  Init(const DrawTarget* aDrawTarget,
       const gfxMatrix& aContextMatrix,
       nsIFrame* aFrame,
       SVGContextPaint* aOuterContextPaint,
       imgDrawingParams& aImgParams);

  already_AddRefed<gfxPattern>
  GetFillPattern(const DrawTarget* aDrawTarget,
                 float aOpacity,
                 const gfxMatrix& aCTM,
                 imgDrawingParams& aImgParams) override;
  already_AddRefed<gfxPattern>
  GetStrokePattern(const DrawTarget* aDrawTarget,
                   float aOpacity,
                   const gfxMatrix& aCTM,
                   imgDrawingParams& aImgParams) override;

  void SetFillOpacity(float aOpacity) { mFillOpacity = aOpacity; }
  float GetFillOpacity() const override { return mFillOpacity; }

  void SetStrokeOpacity(float aOpacity) { mStrokeOpacity = aOpacity; }
  float GetStrokeOpacity() const override { return mStrokeOpacity; }

  struct Paint {
    Paint() : mPaintType(eStyleSVGPaintType_None) {}

    void SetPaintServer(nsIFrame* aFrame,
                        const gfxMatrix& aContextMatrix,
                        nsSVGPaintServerFrame* aPaintServerFrame) {
      mPaintType = eStyleSVGPaintType_Server;
      mPaintDefinition.mPaintServerFrame = aPaintServerFrame;
      mFrame = aFrame;
      mContextMatrix = aContextMatrix;
    }

    void SetColor(const nscolor &aColor) {
      mPaintType = eStyleSVGPaintType_Color;
      mPaintDefinition.mColor = aColor;
    }

    void SetContextPaint(SVGContextPaint* aContextPaint,
                         nsStyleSVGPaintType aPaintType) {
      NS_ASSERTION(aPaintType == eStyleSVGPaintType_ContextFill ||
                   aPaintType == eStyleSVGPaintType_ContextStroke,
                   "Invalid context paint type");
      mPaintType = aPaintType;
      mPaintDefinition.mContextPaint = aContextPaint;
    }

    union {
      nsSVGPaintServerFrame* mPaintServerFrame;
      SVGContextPaint* mContextPaint;
      nscolor mColor;
    } mPaintDefinition;

    // Initialized (if needed) in SetPaintServer():
    MOZ_INIT_OUTSIDE_CTOR nsIFrame* mFrame;
    // CTM defining the user space for the pattern we will use.
    gfxMatrix mContextMatrix;
    nsStyleSVGPaintType mPaintType;

    // Device-space-to-pattern-space
    gfxMatrix mPatternMatrix;
    nsRefPtrHashtable<nsFloatHashKey, gfxPattern> mPatternCache;

    already_AddRefed<gfxPattern>
    GetPattern(const DrawTarget* aDrawTarget,
               float aOpacity,
               nsStyleSVGPaint nsStyleSVG::*aFillOrStroke,
               const gfxMatrix& aCTM,
               imgDrawingParams& aImgParams);
  };

  Paint mFillPaint;
  Paint mStrokePaint;

  float mFillOpacity;
  float mStrokeOpacity;
};

/**
 * This class is used to pass context paint to an SVG image when an element
 * references that image (e.g. via HTML <img> or SVG <image>, or by referencing
 * it from a CSS property such as 'background-image').  In this case we only
 * support context colors and not paint servers.
 */
class SVGEmbeddingContextPaint : public SVGContextPaint
{
  typedef gfx::Color Color;

public:
  SVGEmbeddingContextPaint()
    : mFillOpacity(1.0f)
    , mStrokeOpacity(1.0f)
  {}

  bool operator==(const SVGEmbeddingContextPaint& aOther) const {
    MOZ_ASSERT(GetStrokeWidth() == aOther.GetStrokeWidth() &&
               GetStrokeDashOffset() == aOther.GetStrokeDashOffset() &&
               GetStrokeDashArray() == aOther.GetStrokeDashArray(),
               "We don't currently include these in the context information "
               "from an embedding element");
    return mFill == aOther.mFill &&
           mStroke == aOther.mStroke &&
           mFillOpacity == aOther.mFillOpacity &&
           mStrokeOpacity == aOther.mStrokeOpacity;
  }

  void SetFill(nscolor aFill) {
    mFill.emplace(gfx::ToDeviceColor(aFill));
  }
  const Maybe<Color>& GetFill() const {
    return mFill;
  }
  void SetStroke(nscolor aStroke) {
    mStroke.emplace(gfx::ToDeviceColor(aStroke));
  }
  const Maybe<Color>& GetStroke() const {
    return mStroke;
  }

  /**
   * Returns a pattern of type PatternType::COLOR, or else nullptr.
   */
  already_AddRefed<gfxPattern>
  GetFillPattern(const DrawTarget* aDrawTarget, float aFillOpacity,
                 const gfxMatrix& aCTM, imgDrawingParams& aImgParams) override;

  /**
   * Returns a pattern of type PatternType::COLOR, or else nullptr.
   */
  already_AddRefed<gfxPattern>
  GetStrokePattern(const DrawTarget* aDrawTarget, float aStrokeOpacity,
                   const gfxMatrix& aCTM, imgDrawingParams& aImgParams) override;

  void SetFillOpacity(float aOpacity) {
    mFillOpacity = aOpacity;
  }
  float GetFillOpacity() const override {
    return mFillOpacity;
  };

  void SetStrokeOpacity(float aOpacity) {
    mStrokeOpacity = aOpacity;
  }
  float GetStrokeOpacity() const override {
    return mStrokeOpacity;
  };

  uint32_t Hash() const override;

private:
  Maybe<Color> mFill;
  Maybe<Color> mStroke;
  float mFillOpacity;
  float mStrokeOpacity;
};

} // namespace mozilla

#endif // MOZILLA_SVGCONTEXTPAINT_H_

