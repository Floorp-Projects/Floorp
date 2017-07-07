/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLFrame_h___
#define nsMathMLFrame_h___

#include "mozilla/Attributes.h"
#include "nsFontMetrics.h"
#include "nsMathMLOperators.h"
#include "nsIMathMLFrame.h"
#include "nsLayoutUtils.h"
#include "nsBoundingMetrics.h"
#include "nsIFrame.h"

class nsMathMLChar;
class nsCSSValue;
class nsDisplayListSet;

// Concrete base class with default methods that derived MathML frames can override
class nsMathMLFrame : public nsIMathMLFrame {
public:
  // nsIMathMLFrame ---

  virtual bool
  IsSpaceLike() override {
    return NS_MATHML_IS_SPACE_LIKE(mPresentationData.flags);
  }

  NS_IMETHOD
  GetBoundingMetrics(nsBoundingMetrics& aBoundingMetrics) override {
    aBoundingMetrics = mBoundingMetrics;
    return NS_OK;
  }

  NS_IMETHOD
  SetBoundingMetrics(const nsBoundingMetrics& aBoundingMetrics) override {
    mBoundingMetrics = aBoundingMetrics;
    return NS_OK;
  }

  NS_IMETHOD
  SetReference(const nsPoint& aReference) override {
    mReference = aReference;
    return NS_OK;
  }

  virtual eMathMLFrameType GetMathMLFrameType() override;

  NS_IMETHOD
  Stretch(mozilla::gfx::DrawTarget* aDrawTarget,
          nsStretchDirection   aStretchDirection,
          nsBoundingMetrics&   aContainerSize,
          mozilla::ReflowOutput& aDesiredStretchSize) override
  {
    return NS_OK;
  }

  NS_IMETHOD
  GetEmbellishData(nsEmbellishData& aEmbellishData) override {
    aEmbellishData = mEmbellishData;
    return NS_OK;
  }

  NS_IMETHOD
  GetPresentationData(nsPresentationData& aPresentationData) override {
    aPresentationData = mPresentationData;
    return NS_OK;
  }

  NS_IMETHOD
  InheritAutomaticData(nsIFrame* aParent) override;

  NS_IMETHOD
  TransmitAutomaticData() override
  {
    return NS_OK;
  }

  NS_IMETHOD
  UpdatePresentationData(uint32_t        aFlagsValues,
                         uint32_t        aFlagsToUpdate) override;

  NS_IMETHOD
  UpdatePresentationDataFromChildAt(int32_t         aFirstIndex,
                                    int32_t         aLastIndex,
                                    uint32_t        aFlagsValues,
                                    uint32_t        aFlagsToUpdate) override
  {
    return NS_OK;
  }

  uint8_t
  ScriptIncrement(nsIFrame* aFrame) override
  {
    return 0;
  }

  bool
  IsMrowLike() override
  {
    return false;
  }

  // helper to give a style context suitable for doing the stretching to the
  // MathMLChar. Frame classes that use this should make the extra style contexts
  // accessible to the Style System via Get/Set AdditionalStyleContext.
  static void
  ResolveMathMLCharStyle(nsPresContext*  aPresContext,
                         nsIContent*      aContent,
                         nsStyleContext*  aParenStyleContext,
                         nsMathMLChar*    aMathMLChar);

  // helper to get the mEmbellishData of a frame
  // The MathML REC precisely defines an "embellished operator" as:
  // - an <mo> element;
  // - or one of the elements <msub>, <msup>, <msubsup>, <munder>, <mover>,
  //   <munderover>, <mmultiscripts>, <mfrac>, or <semantics>, whose first
  //   argument exists and is an embellished operator;
  //- or one of the elements <mstyle>, <mphantom>, or <mpadded>, such that
  //   an <mrow> containing the same arguments would be an embellished
  //   operator;
  // - or an <maction> element whose selected subexpression exists and is an
  //   embellished operator;
  // - or an <mrow> whose arguments consist (in any order) of one embellished
  //   operator and zero or more spacelike elements.
  static void
  GetEmbellishDataFrom(nsIFrame*        aFrame,
                       nsEmbellishData& aEmbellishData);

  // helper to get the presentation data of a frame. If aClimbTree is
  // set to true and the frame happens to be surrounded by non-MathML
  // helper frames needed for its support, we walk up the frame hierarchy
  // until we reach a MathML ancestor or the <root> math element.
  static void
  GetPresentationDataFrom(nsIFrame*           aFrame,
                          nsPresentationData& aPresentationData,
                          bool                aClimbTree = true);

  // utilities to parse and retrieve numeric values in CSS units
  // All values are stored in twips.
  // @pre  aLengthValue is the default length value of the attribute.
  // @post aLengthValue is the length value computed from the attribute.
  static void ParseNumericValue(const nsString&   aString,
                                nscoord*          aLengthValue,
                                uint32_t          aFlags,
                                nsPresContext*    aPresContext,
                                nsStyleContext*   aStyleContext,
                                float             aFontSizeInflation);

  static nscoord
  CalcLength(nsPresContext*   aPresContext,
             nsStyleContext*   aStyleContext,
             const nsCSSValue& aCSSValue,
             float             aFontSizeInflation);

  static eMathMLFrameType
  GetMathMLFrameTypeFor(nsIFrame* aFrame)
  {
    if (aFrame->IsFrameOfType(nsIFrame::eMathML)) {
      nsIMathMLFrame* mathMLFrame = do_QueryFrame(aFrame);
      if (mathMLFrame)
        return mathMLFrame->GetMathMLFrameType();
    }
    return eMathMLFrameType_UNKNOWN;
  }

  // estimate of the italic correction
  static void
  GetItalicCorrection(nsBoundingMetrics& aBoundingMetrics,
                      nscoord&           aItalicCorrection)
  {
    aItalicCorrection = aBoundingMetrics.rightBearing - aBoundingMetrics.width;
    if (0 > aItalicCorrection) {
      aItalicCorrection = 0;
    }
  }

  static void
  GetItalicCorrection(nsBoundingMetrics& aBoundingMetrics,
                      nscoord&           aLeftItalicCorrection,
                      nscoord&           aRightItalicCorrection)
  {
    aRightItalicCorrection = aBoundingMetrics.rightBearing - aBoundingMetrics.width;
    if (0 > aRightItalicCorrection) {
      aRightItalicCorrection = 0;
    }
    aLeftItalicCorrection = -aBoundingMetrics.leftBearing;
    if (0 > aLeftItalicCorrection) {
      aLeftItalicCorrection = 0;
    }
  }

  // helper methods for getting sup/subdrop's from a child
  static void
  GetSubDropFromChild(nsIFrame*       aChild,
                      nscoord&        aSubDrop,
                      float           aFontSizeInflation)
  {
    RefPtr<nsFontMetrics> fm =
      nsLayoutUtils::GetFontMetricsForFrame(aChild, aFontSizeInflation);
    GetSubDrop(fm, aSubDrop);
  }

  static void
  GetSupDropFromChild(nsIFrame*       aChild,
                      nscoord&        aSupDrop,
                      float           aFontSizeInflation)
  {
    RefPtr<nsFontMetrics> fm =
      nsLayoutUtils::GetFontMetricsForFrame(aChild, aFontSizeInflation);
    GetSupDrop(fm, aSupDrop);
  }

  static void
  GetSkewCorrectionFromChild(nsIFrame*       aChild,
                             nscoord&        aSkewCorrection)
  {
    // default is 0
    // individual classes should over-ride this method if necessary
    aSkewCorrection = 0;
  }

  // 2 levels of subscript shifts
  static void
  GetSubScriptShifts(nsFontMetrics* fm,
                     nscoord&        aSubScriptShift1,
                     nscoord&        aSubScriptShift2)
  {
    nscoord xHeight = fm->XHeight();
    aSubScriptShift1 = NSToCoordRound(150.000f/430.556f * xHeight);
    aSubScriptShift2 = NSToCoordRound(247.217f/430.556f * xHeight);
  }

  // 3 levels of superscript shifts
  static void
  GetSupScriptShifts(nsFontMetrics* fm,
                     nscoord&        aSupScriptShift1,
                     nscoord&        aSupScriptShift2,
                     nscoord&        aSupScriptShift3)
  {
    nscoord xHeight = fm->XHeight();
    aSupScriptShift1 = NSToCoordRound(412.892f/430.556f * xHeight);
    aSupScriptShift2 = NSToCoordRound(362.892f/430.556f * xHeight);
    aSupScriptShift3 = NSToCoordRound(288.889f/430.556f * xHeight);
  }

  // these are TeX specific params not found in ordinary fonts

  static void
  GetSubDrop(nsFontMetrics* fm,
             nscoord&        aSubDrop)
  {
    nscoord xHeight = fm->XHeight();
    aSubDrop = NSToCoordRound(50.000f/430.556f * xHeight);
  }

  static void
  GetSupDrop(nsFontMetrics* fm,
             nscoord&        aSupDrop)
  {
    nscoord xHeight = fm->XHeight();
    aSupDrop = NSToCoordRound(386.108f/430.556f * xHeight);
  }

  static void
  GetNumeratorShifts(nsFontMetrics* fm,
                     nscoord&        numShift1,
                     nscoord&        numShift2,
                     nscoord&        numShift3)
  {
    nscoord xHeight = fm->XHeight();
    numShift1 = NSToCoordRound(676.508f/430.556f * xHeight);
    numShift2 = NSToCoordRound(393.732f/430.556f * xHeight);
    numShift3 = NSToCoordRound(443.731f/430.556f * xHeight);
  }

  static void
  GetDenominatorShifts(nsFontMetrics* fm,
                       nscoord&        denShift1,
                       nscoord&        denShift2)
  {
    nscoord xHeight = fm->XHeight();
    denShift1 = NSToCoordRound(685.951f/430.556f * xHeight);
    denShift2 = NSToCoordRound(344.841f/430.556f * xHeight);
  }

  static void
  GetEmHeight(nsFontMetrics* fm,
              nscoord&        emHeight)
  {
#if 0
    // should switch to this API in order to scale with changes of TextZoom
    emHeight = fm->EmHeight();
#else
    emHeight = NSToCoordRound(float(fm->Font().size));
#endif
  }

  static void
  GetAxisHeight (nsFontMetrics* fm,
                 nscoord&        axisHeight)
  {
    axisHeight = NSToCoordRound(250.000f/430.556f * fm->XHeight());
  }

  static void
  GetBigOpSpacings(nsFontMetrics* fm,
                   nscoord&        bigOpSpacing1,
                   nscoord&        bigOpSpacing2,
                   nscoord&        bigOpSpacing3,
                   nscoord&        bigOpSpacing4,
                   nscoord&        bigOpSpacing5)
  {
    nscoord xHeight = fm->XHeight();
    bigOpSpacing1 = NSToCoordRound(111.111f/430.556f * xHeight);
    bigOpSpacing2 = NSToCoordRound(166.667f/430.556f * xHeight);
    bigOpSpacing3 = NSToCoordRound(200.000f/430.556f * xHeight);
    bigOpSpacing4 = NSToCoordRound(600.000f/430.556f * xHeight);
    bigOpSpacing5 = NSToCoordRound(100.000f/430.556f * xHeight);
  }

  static void
  GetRuleThickness(nsFontMetrics* fm,
                   nscoord&        ruleThickness)
  {
    nscoord xHeight = fm->XHeight();
    ruleThickness = NSToCoordRound(40.000f/430.556f * xHeight);
  }

  // Some parameters are not accurately obtained using the x-height.
  // Here are some slower variants to obtain the desired metrics
  // by actually measuring some characters
  static void
  GetRuleThickness(mozilla::gfx::DrawTarget* aDrawTarget,
                   nsFontMetrics* aFontMetrics,
                   nscoord& aRuleThickness);

  static void
  GetAxisHeight(mozilla::gfx::DrawTarget* aDrawTarget,
                nsFontMetrics* aFontMetrics,
                nscoord& aAxisHeight);

  static void
  GetRadicalParameters(nsFontMetrics* aFontMetrics,
                       bool aDisplayStyle,
                       nscoord& aRadicalRuleThickness,
                       nscoord& aRadicalExtraAscender,
                       nscoord& aRadicalVerticalGap);

protected:
#if defined(DEBUG) && defined(SHOW_BOUNDING_BOX)
  void DisplayBoundingMetrics(nsDisplayListBuilder* aBuilder,
                              nsIFrame* aFrame, const nsPoint& aPt,
                              const nsBoundingMetrics& aMetrics,
                              const nsDisplayListSet& aLists);
#endif

  /**
   * Display a solid rectangle in the frame's text color. Used for drawing
   * fraction separators and root/sqrt overbars.
   */
  void DisplayBar(nsDisplayListBuilder* aBuilder,
                  nsIFrame* aFrame, const nsRect& aRect,
                  const nsDisplayListSet& aLists);

  // information about the presentation policy of the frame
  nsPresentationData mPresentationData;

  // information about a container that is an embellished operator
  nsEmbellishData mEmbellishData;

  // Metrics that _exactly_ enclose the text of the frame
  nsBoundingMetrics mBoundingMetrics;

  // Reference point of the frame: mReference.y is the baseline
  nsPoint mReference;
};

#endif /* nsMathMLFrame_h___ */
