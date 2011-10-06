/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla MathML Project.
 *
 * The Initial Developer of the Original Code is
 * The University Of Queensland.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsMathMLFrame_h___
#define nsMathMLFrame_h___

#include "nsCOMPtr.h"
#include "nsPresContext.h"
#include "nsFontMetrics.h"
#include "nsStyleContext.h"
#include "nsMathMLAtoms.h"
#include "nsMathMLOperators.h"
#include "nsIMathMLFrame.h"
#include "nsFrame.h"
#include "nsCSSValue.h"
#include "nsMathMLElement.h"
#include "nsLayoutUtils.h"

class nsMathMLChar;

// Concrete base class with default methods that derived MathML frames can override
class nsMathMLFrame : public nsIMathMLFrame {
public:

  // nsIMathMLFrame ---

  virtual PRBool
  IsSpaceLike() {
    return NS_MATHML_IS_SPACE_LIKE(mPresentationData.flags);
  }

  NS_IMETHOD
  GetBoundingMetrics(nsBoundingMetrics& aBoundingMetrics) {
    aBoundingMetrics = mBoundingMetrics;
    return NS_OK;
  }

  NS_IMETHOD
  SetBoundingMetrics(const nsBoundingMetrics& aBoundingMetrics) {
    mBoundingMetrics = aBoundingMetrics;
    return NS_OK;
  }

  NS_IMETHOD
  SetReference(const nsPoint& aReference) {
    mReference = aReference;
    return NS_OK;
  }

  virtual eMathMLFrameType GetMathMLFrameType();

  NS_IMETHOD
  Stretch(nsRenderingContext& aRenderingContext,
          nsStretchDirection   aStretchDirection,
          nsBoundingMetrics&   aContainerSize,
          nsHTMLReflowMetrics& aDesiredStretchSize)
  {
    return NS_OK;
  }

  NS_IMETHOD
  GetEmbellishData(nsEmbellishData& aEmbellishData) {
    aEmbellishData = mEmbellishData;
    return NS_OK;
  }

  NS_IMETHOD
  GetPresentationData(nsPresentationData& aPresentationData) {
    aPresentationData = mPresentationData;
    return NS_OK;
  }

  NS_IMETHOD
  InheritAutomaticData(nsIFrame* aParent);

  NS_IMETHOD
  TransmitAutomaticData()
  {
    return NS_OK;
  }

  NS_IMETHOD
  UpdatePresentationData(PRUint32        aFlagsValues,
                         PRUint32        aFlagsToUpdate);

  NS_IMETHOD
  UpdatePresentationDataFromChildAt(PRInt32         aFirstIndex,
                                    PRInt32         aLastIndex,
                                    PRUint32        aFlagsValues,
                                    PRUint32        aFlagsToUpdate)
  {
    return NS_OK;
  }

  // helper to give a style context suitable for doing the stretching to the
  // MathMLChar. Frame classes that use this should make the extra style contexts
  // accessible to the Style System via Get/Set AdditionalStyleContext.
  static void
  ResolveMathMLCharStyle(nsPresContext*  aPresContext,
                         nsIContent*      aContent,
                         nsStyleContext*  aParenStyleContext,
                         nsMathMLChar*    aMathMLChar,
                         PRBool           aIsMutableChar);

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
                          PRBool              aClimbTree = PR_TRUE);

  // helper used by <mstyle> and <mtable> to see if they have a displaystyle attribute 
  static void
  FindAttrDisplaystyle(nsIContent*         aContent,
                       nsPresentationData& aPresentationData);

  // helper to check if a content has an attribute. If content is nsnull or if
  // the attribute is not there, check if the attribute is on the mstyle hierarchy
  // @return PR_TRUE  --if attribute exists
  //         PR_FALSE --if attribute doesn't exist
  static PRBool
  GetAttribute(nsIContent* aContent,
               nsIFrame*   aMathMLmstyleFrame,          
               nsIAtom*    aAttributeAtom,
               nsString&   aValue);

  // utilities to parse and retrieve numeric values in CSS units
  // All values are stored in twips.
  static PRBool
  ParseNumericValue(const nsString& aString,
                    nsCSSValue&     aCSSValue) {
    return nsMathMLElement::ParseNumericValue(aString, aCSSValue,
            nsMathMLElement::PARSE_ALLOW_NEGATIVE |
            nsMathMLElement::PARSE_ALLOW_UNITLESS);
  }

  static nscoord 
  CalcLength(nsPresContext*   aPresContext,
             nsStyleContext*   aStyleContext,
             const nsCSSValue& aCSSValue);

  static PRBool
  ParseNamedSpaceValue(nsIFrame*   aMathMLmstyleFrame,
                       nsString&   aString,
                       nsCSSValue& aCSSValue);

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
                      nscoord&        aSubDrop) 
  {
    nsRefPtr<nsFontMetrics> fm;
    nsLayoutUtils::GetFontMetricsForFrame(aChild, getter_AddRefs(fm));
    GetSubDrop(fm, aSubDrop);
  }

  static void 
  GetSupDropFromChild(nsIFrame*       aChild,
                      nscoord&        aSupDrop) 
  {
    nsRefPtr<nsFontMetrics> fm;
    nsLayoutUtils::GetFontMetricsForFrame(aChild, getter_AddRefs(fm));
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
  GetRuleThickness(nsRenderingContext& aRenderingContext, 
                   nsFontMetrics*      aFontMetrics,
                   nscoord&             aRuleThickness);

  static void
  GetAxisHeight(nsRenderingContext& aRenderingContext, 
                nsFontMetrics*      aFontMetrics,
                nscoord&             aAxisHeight);

protected:
#if defined(NS_DEBUG) && defined(SHOW_BOUNDING_BOX)
  nsresult DisplayBoundingMetrics(nsDisplayListBuilder* aBuilder,
                                  nsIFrame* aFrame, const nsPoint& aPt,
                                  const nsBoundingMetrics& aMetrics,
                                  const nsDisplayListSet& aLists);
#endif

  /**
   * Display a solid rectangle in the frame's text color. Used for drawing
   * fraction separators and root/sqrt overbars.
   */
  nsresult DisplayBar(nsDisplayListBuilder* aBuilder,
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
