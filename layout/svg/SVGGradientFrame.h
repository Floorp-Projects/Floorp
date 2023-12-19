/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYOUT_SVG_SVGGRADIENTFRAME_H_
#define LAYOUT_SVG_SVGGRADIENTFRAME_H_

#include "mozilla/Attributes.h"
#include "mozilla/SVGPaintServerFrame.h"
#include "gfxMatrix.h"
#include "gfxRect.h"
#include "nsCOMPtr.h"
#include "nsCSSRenderingGradients.h"
#include "nsIFrame.h"
#include "nsLiteralString.h"

class gfxPattern;
class nsAtom;
class nsIContent;

namespace mozilla {
class PresShell;
class SVGAnimatedTransformList;

namespace dom {
class SVGLinearGradientElement;
class SVGRadialGradientElement;
}  // namespace dom
}  // namespace mozilla

nsIFrame* NS_NewSVGLinearGradientFrame(mozilla::PresShell* aPresShell,
                                       mozilla::ComputedStyle* aStyle);
nsIFrame* NS_NewSVGRadialGradientFrame(mozilla::PresShell* aPresShell,
                                       mozilla::ComputedStyle* aStyle);

namespace mozilla {

class SVGGradientFrame : public SVGPaintServerFrame {
  using ExtendMode = gfx::ExtendMode;

 protected:
  SVGGradientFrame(ComputedStyle* aStyle, nsPresContext* aPresContext,
                   ClassID aID);

 public:
  NS_DECL_ABSTRACT_FRAME(SVGGradientFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_QUERYFRAME_TARGET(SVGGradientFrame)

  // SVGPaintServerFrame methods:
  already_AddRefed<gfxPattern> GetPaintServerPattern(
      nsIFrame* aSource, const DrawTarget* aDrawTarget,
      const gfxMatrix& aContextMatrix, StyleSVGPaint nsStyleSVG::*aFillOrStroke,
      float aGraphicOpacity, imgDrawingParams& aImgParams,
      const gfxRect* aOverrideBounds) override;

  // nsIFrame interface:
  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) override;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"SVGGradient"_ns, aResult);
  }
#endif  // DEBUG

 private:
  /**
   * Parses this frame's href and - if it references another gradient - returns
   * it.  It also makes this frame a rendering observer of the specified ID.
   */
  SVGGradientFrame* GetReferencedGradient();

  void GetStops(nsTArray<ColorStop>* aStops, float aGraphicOpacity);

  const SVGAnimatedTransformList* GetGradientTransformList(
      nsIContent* aDefault);
  // Will be singular for gradientUnits="objectBoundingBox" with an empty bbox.
  gfxMatrix GetGradientTransform(nsIFrame* aSource,
                                 const gfxRect* aOverrideBounds);

 protected:
  virtual bool GradientVectorLengthIsZero() = 0;
  virtual already_AddRefed<gfxPattern> CreateGradient() = 0;

  // Accessors to lookup gradient attributes
  uint16_t GetEnumValue(uint32_t aIndex, nsIContent* aDefault);
  uint16_t GetEnumValue(uint32_t aIndex) {
    return GetEnumValue(aIndex, mContent);
  }
  uint16_t GetGradientUnits();
  uint16_t GetSpreadMethod();

  // Gradient-type-specific lookups since the length values differ between
  // linear and radial gradients
  virtual dom::SVGLinearGradientElement* GetLinearGradientWithLength(
      uint32_t aIndex, dom::SVGLinearGradientElement* aDefault);
  virtual dom::SVGRadialGradientElement* GetRadialGradientWithLength(
      uint32_t aIndex, dom::SVGRadialGradientElement* aDefault);

  // The frame our gradient is (currently) being applied to
  nsIFrame* mSource;

 private:
  // Flag to mark this frame as "in use" during recursive calls along our
  // gradient's reference chain so we can detect reference loops. See:
  // http://www.w3.org/TR/SVG11/pservers.html#LinearGradientElementHrefAttribute
  bool mLoopFlag;
  // Gradients often don't reference other gradients, so here we cache
  // the fact that that isn't happening.
  bool mNoHRefURI;
};

// -------------------------------------------------------------------------
// Linear Gradients
// -------------------------------------------------------------------------

class SVGLinearGradientFrame final : public SVGGradientFrame {
  friend nsIFrame* ::NS_NewSVGLinearGradientFrame(
      mozilla::PresShell* aPresShell, ComputedStyle* aStyle);

 protected:
  explicit SVGLinearGradientFrame(ComputedStyle* aStyle,
                                  nsPresContext* aPresContext)
      : SVGGradientFrame(aStyle, aPresContext, kClassID) {}

 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(SVGLinearGradientFrame)

  // nsIFrame interface:
#ifdef DEBUG
  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;
#endif

  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) override;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"SVGLinearGradient"_ns, aResult);
  }
#endif  // DEBUG

 protected:
  float GetLengthValue(uint32_t aIndex);
  mozilla::dom::SVGLinearGradientElement* GetLinearGradientWithLength(
      uint32_t aIndex,
      mozilla::dom::SVGLinearGradientElement* aDefault) override;
  bool GradientVectorLengthIsZero() override;
  already_AddRefed<gfxPattern> CreateGradient() override;
};

// -------------------------------------------------------------------------
// Radial Gradients
// -------------------------------------------------------------------------

class SVGRadialGradientFrame final : public SVGGradientFrame {
  friend nsIFrame* ::NS_NewSVGRadialGradientFrame(
      mozilla::PresShell* aPresShell, ComputedStyle* aStyle);

 protected:
  explicit SVGRadialGradientFrame(ComputedStyle* aStyle,
                                  nsPresContext* aPresContext)
      : SVGGradientFrame(aStyle, aPresContext, kClassID) {}

 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(SVGRadialGradientFrame)

  // nsIFrame interface:
#ifdef DEBUG
  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;
#endif

  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) override;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"SVGRadialGradient"_ns, aResult);
  }
#endif  // DEBUG

 protected:
  float GetLengthValue(uint32_t aIndex);
  float GetLengthValue(uint32_t aIndex, float aDefaultValue);
  float GetLengthValueFromElement(
      uint32_t aIndex, mozilla::dom::SVGRadialGradientElement& aElement);
  mozilla::dom::SVGRadialGradientElement* GetRadialGradientWithLength(
      uint32_t aIndex,
      mozilla::dom::SVGRadialGradientElement* aDefault) override;
  bool GradientVectorLengthIsZero() override;
  already_AddRefed<gfxPattern> CreateGradient() override;
};

}  // namespace mozilla

#endif  // LAYOUT_SVG_SVGGRADIENTFRAME_H_
