/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGCONTEXTPAINT_H_
#define MOZILLA_SVGCONTEXTPAINT_H_

#include "DrawMode.h"
#include "gfxMatrix.h"
#include "gfxPattern.h"
#include "gfxTypes.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/gfx/2D.h"
#include "nsStyleStruct.h"
#include "nsTArray.h"

class gfxContext;
class nsIDocument;
class nsSVGPaintServerFrame;

namespace mozilla {

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
 */
class SVGContextPaint
{
protected:
  typedef mozilla::gfx::DrawTarget DrawTarget;

  SVGContextPaint() {}

public:
  virtual ~SVGContextPaint() {}

  virtual already_AddRefed<gfxPattern> GetFillPattern(const DrawTarget* aDrawTarget,
                                                      float aOpacity,
                                                      const gfxMatrix& aCTM) = 0;
  virtual already_AddRefed<gfxPattern> GetStrokePattern(const DrawTarget* aDrawTarget,
                                                        float aOpacity,
                                                        const gfxMatrix& aCTM) = 0;
  virtual float GetFillOpacity() const = 0;
  virtual float GetStrokeOpacity() const = 0;

  already_AddRefed<gfxPattern> GetFillPattern(const DrawTarget* aDrawTarget,
                                              const gfxMatrix& aCTM) {
    return GetFillPattern(aDrawTarget, GetFillOpacity(), aCTM);
  }

  already_AddRefed<gfxPattern> GetStrokePattern(const DrawTarget* aDrawTarget,
                                                const gfxMatrix& aCTM) {
    return GetStrokePattern(aDrawTarget, GetStrokeOpacity(), aCTM);
  }

  static SVGContextPaint* GetContextPaint(nsIContent* aContent);

  // XXX This gets the geometry params from the gfxContext.  We should get that
  // information from the actual paint context!
  void InitStrokeGeometry(gfxContext *aContext,
                          float devUnitsPerSVGUnit);

  FallibleTArray<gfxFloat>& GetStrokeDashArray() {
    return mDashes;
  }

  gfxFloat GetStrokeDashOffset() {
    return mDashOffset;
  }

  gfxFloat GetStrokeWidth() {
    return mStrokeWidth;
  }

private:
  FallibleTArray<gfxFloat> mDashes;
  gfxFloat mDashOffset;
  gfxFloat mStrokeWidth;
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
  AutoSetRestoreSVGContextPaint(SVGContextPaint* aContextPaint,
                                nsIDocument* aSVGDocument);
  ~AutoSetRestoreSVGContextPaint();
private:
  nsIDocument* mSVGDocument;
  // The context paint that needs to be restored by our dtor after it removes
  // aContextPaint:
  void* mOuterContextPaint;
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
  DrawMode Init(const DrawTarget* aDrawTarget,
                const gfxMatrix& aContextMatrix,
                nsIFrame* aFrame,
                SVGContextPaint* aOuterContextPaint);

  already_AddRefed<gfxPattern> GetFillPattern(const DrawTarget* aDrawTarget,
                                              float aOpacity,
                                              const gfxMatrix& aCTM) override;
  already_AddRefed<gfxPattern> GetStrokePattern(const DrawTarget* aDrawTarget,
                                                float aOpacity,
                                                const gfxMatrix& aCTM) override;

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

    nsIFrame* mFrame;
    // CTM defining the user space for the pattern we will use.
    gfxMatrix mContextMatrix;
    nsStyleSVGPaintType mPaintType;

    // Device-space-to-pattern-space
    gfxMatrix mPatternMatrix;
    nsRefPtrHashtable<nsFloatHashKey, gfxPattern> mPatternCache;

    already_AddRefed<gfxPattern> GetPattern(const DrawTarget* aDrawTarget,
                                            float aOpacity,
                                            nsStyleSVGPaint nsStyleSVG::*aFillOrStroke,
                                            const gfxMatrix& aCTM);
  };

  Paint mFillPaint;
  Paint mStrokePaint;

  float mFillOpacity;
  float mStrokeOpacity;
};

} // namespace mozilla

#endif // MOZILLA_SVGCONTEXTPAINT_H_

