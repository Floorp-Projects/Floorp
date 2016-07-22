/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGCONTEXTPAINT_H_
#define MOZILLA_SVGCONTEXTPAINT_H_

#include "gfxMatrix.h"
#include "gfxPattern.h"
#include "gfxTypes.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/gfx/2D.h"
#include "nsTArray.h"

class gfxContext;
class nsIDocument;

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
 * document, as expected by nsSVGUtils::GetContextPaint.  Any pre-existing
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

} // namespace mozilla

#endif // MOZILLA_SVGCONTEXTPAINT_H_

