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
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsStyleContext.h"
#include "nsMathMLAtoms.h"
#include "nsMathMLOperators.h"
#include "nsIMathMLFrame.h"
#include "nsFrame.h"
#include "nsCSSValue.h"

class nsMathMLChar;

// Concrete base class with default methods that derived MathML frames can override
class nsMathMLFrame : public nsIMathMLFrame {
public:

  // nsISupports ------

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_IMETHOD_(nsrefcnt) AddRef() {
    // not meaningfull for frames
    return 1;
  }

  NS_IMETHOD_(nsrefcnt) Release() {
    // not meaningfull for frames
    return 1;
  }

  // nsIMathMLFrame ---

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
  GetReference(nsPoint& aReference) {
    aReference = mReference;
    return NS_OK;
  }

  NS_IMETHOD
  SetReference(const nsPoint& aReference) {
    mReference = aReference;
    return NS_OK;
  }

  NS_IMETHOD
  Stretch(nsIPresContext*      aPresContext,
          nsIRenderingContext& aRenderingContext,
          nsStretchDirection   aStretchDirection,
          nsBoundingMetrics&   aContainerSize,
          nsHTMLReflowMetrics& aDesiredStretchSize)
  {
    return NS_OK;
  }

  NS_IMETHOD
  Place(nsIPresContext*      aPresContext,
        nsIRenderingContext& aRenderingContext,
        PRBool               aPlaceOrigin,
        nsHTMLReflowMetrics& aDesiredSize)
  {
    return NS_OK;
  }

  NS_IMETHOD
  GetEmbellishData(nsEmbellishData& aEmbellishData) {
    aEmbellishData = mEmbellishData;
    return NS_OK;
  }
 
  NS_IMETHOD
  SetEmbellishData(const nsEmbellishData& aEmbellishData) {
    mEmbellishData = aEmbellishData;
    return NS_OK;
  }

  NS_IMETHOD
  GetPresentationData(nsPresentationData& aPresentationData) {
    aPresentationData = mPresentationData;
    return NS_OK;
  }

  NS_IMETHOD
  SetPresentationData(const nsPresentationData& aPresentationData) {
    mPresentationData = aPresentationData;
    return NS_OK;
  }

  NS_IMETHOD
  InheritAutomaticData(nsIPresContext* aPresContext,
                       nsIFrame*       aParent);

  NS_IMETHOD
  TransmitAutomaticData(nsIPresContext* aPresContext)
  {
    return NS_OK;
  }

  NS_IMETHOD
  UpdatePresentationData(nsIPresContext* aPresContext,
                         PRInt32         aScriptLevelIncrement,
                         PRUint32        aFlagsValues,
                         PRUint32        aFlagsToUpdate);

  NS_IMETHOD
  UpdatePresentationDataFromChildAt(nsIPresContext* aPresContext,
                                    PRInt32         aFirstIndex,
                                    PRInt32         aLastIndex,
                                    PRInt32         aScriptLevelIncrement,
                                    PRUint32        aFlagsValues,
                                    PRUint32        aFlagsToUpdate)
  {
    return NS_OK;
  }

  NS_IMETHOD
  ReResolveScriptStyle(nsIPresContext* aPresContext,
                       PRInt32         aParentScriptLevel)
  {
    return NS_OK;
  }

  // helper to give a style context suitable for doing the stretching to the
  // MathMLChar. Frame classes that use this should make the extra style contexts
  // accessible to the Style System via Get/Set AdditionalStyleContext.
  static void
  ResolveMathMLCharStyle(nsIPresContext*  aPresContext,
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

  // helper to check if a content has an attribute. If content is nsnull or if
  // the attribute is not there, check if the attribute is on the mstyle hierarchy
  // @return NS_CONTENT_ATTR_HAS_VALUE --if attribute has non-empty value, attr="value"
  //         NS_CONTENT_ATTR_NO_VALUE  --if attribute has empty value, attr=""
  //         NS_CONTENT_ATTR_NOT_THERE --if attribute is not there
  static nsresult
  GetAttribute(nsIContent* aContent,
               nsIFrame*   aMathMLmstyleFrame,          
               nsIAtom*    aAttributeAtom,
               nsString&   aValue);

  // utilities to parse and retrieve numeric values in CSS units
  // All values are stored in twips.
  static PRBool
  ParseNumericValue(nsString&   aString,
                    nsCSSValue& aCSSValue);

  static nscoord 
  CalcLength(nsIPresContext*   aPresContext,
             nsStyleContext*   aStyleContext,
             const nsCSSValue& aCSSValue);

  static PRBool
  ParseNamedSpaceValue(nsIFrame*   aMathMLmstyleFrame,
                       nsString&   aString,
                       nsCSSValue& aCSSValue);

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
  GetSubDropFromChild(nsIPresContext* aPresContext,
                      nsIFrame*       aChild, 
                      nscoord&        aSubDrop) 
  {
    const nsStyleFont* font = aChild->GetStyleFont();
    nsCOMPtr<nsIFontMetrics> fm = aPresContext->GetMetricsFor(font->mFont);
    GetSubDrop(fm, aSubDrop);
  }

  static void 
  GetSupDropFromChild(nsIPresContext* aPresContext,
                      nsIFrame*       aChild, 
                      nscoord&        aSupDrop) 
  {
    const nsStyleFont* font = aChild->GetStyleFont();
    nsCOMPtr<nsIFontMetrics> fm = aPresContext->GetMetricsFor(font->mFont);
    GetSupDrop(fm, aSupDrop);
  }

  static void
  GetSkewCorrectionFromChild(nsIPresContext* aPresContext,
                             nsIFrame*       aChild, 
                             nscoord&        aSkewCorrection) 
  {
    // default is 0
    // individual classes should over-ride this method if necessary
    aSkewCorrection = 0;
  }

  // 2 levels of subscript shifts
  static void
  GetSubScriptShifts(nsIFontMetrics* fm, 
                     nscoord&        aSubScriptShift1, 
                     nscoord&        aSubScriptShift2)
  {
    nscoord xHeight;
    fm->GetXHeight(xHeight);
    aSubScriptShift1 = NSToCoordRound(150.000f/430.556f * xHeight);
    aSubScriptShift2 = NSToCoordRound(247.217f/430.556f * xHeight);
  }

  // 3 levels of superscript shifts
  static void
  GetSupScriptShifts(nsIFontMetrics* fm, 
                     nscoord&        aSupScriptShift1, 
                     nscoord&        aSupScriptShift2, 
                     nscoord&        aSupScriptShift3)
  {
    nscoord xHeight;
    fm->GetXHeight(xHeight);
    aSupScriptShift1 = NSToCoordRound(412.892f/430.556f * xHeight);
    aSupScriptShift2 = NSToCoordRound(362.892f/430.556f * xHeight);
    aSupScriptShift3 = NSToCoordRound(288.889f/430.556f * xHeight);
  }

  // these are TeX specific params not found in ordinary fonts

  static void
  GetSubDrop(nsIFontMetrics* fm,
             nscoord&        aSubDrop)
  {
    nscoord xHeight;
    fm->GetXHeight(xHeight);
    aSubDrop = NSToCoordRound(50.000f/430.556f * xHeight);
  }

  static void
  GetSupDrop(nsIFontMetrics* fm,
             nscoord&        aSupDrop)
  {
    nscoord xHeight;
    fm->GetXHeight(xHeight);
    aSupDrop = NSToCoordRound(386.108f/430.556f * xHeight);
  }

  static void
  GetNumeratorShifts(nsIFontMetrics* fm, 
                     nscoord&        numShift1, 
                     nscoord&        numShift2, 
                     nscoord&        numShift3)
  {
    nscoord xHeight;
    fm->GetXHeight(xHeight);
    numShift1 = NSToCoordRound(676.508f/430.556f * xHeight);
    numShift2 = NSToCoordRound(393.732f/430.556f * xHeight);
    numShift3 = NSToCoordRound(443.731f/430.556f * xHeight);
  }

  static void
  GetDenominatorShifts(nsIFontMetrics* fm, 
                       nscoord&        denShift1, 
                       nscoord&        denShift2)
  {
    nscoord xHeight;
    fm->GetXHeight(xHeight);
    denShift1 = NSToCoordRound(685.951f/430.556f * xHeight);
    denShift2 = NSToCoordRound(344.841f/430.556f * xHeight);
  }

  static void
  GetEmHeight(nsIFontMetrics* fm,
              nscoord&        emHeight)
  {
#if 0 
    // should switch to this API in order to scale with changes of TextZoom
    fm->GetEmHeight(emHeight);
#else
    const nsFont* font;
    fm->GetFont(font);
    emHeight = NSToCoordRound(float(font->size));
#endif
  }

  static void
  GetAxisHeight (nsIFontMetrics* fm,
                 nscoord&        axisHeight)
  {
    fm->GetXHeight (axisHeight);
    axisHeight = NSToCoordRound(250.000f/430.556f * axisHeight);
  }

  static void
  GetBigOpSpacings(nsIFontMetrics* fm, 
                   nscoord&        bigOpSpacing1,
                   nscoord&        bigOpSpacing2,
                   nscoord&        bigOpSpacing3,
                   nscoord&        bigOpSpacing4,
                   nscoord&        bigOpSpacing5)
  {
    nscoord xHeight;
    fm->GetXHeight(xHeight);
    bigOpSpacing1 = NSToCoordRound(111.111f/430.556f * xHeight);
    bigOpSpacing2 = NSToCoordRound(166.667f/430.556f * xHeight);
    bigOpSpacing3 = NSToCoordRound(200.000f/430.556f * xHeight);
    bigOpSpacing4 = NSToCoordRound(600.000f/430.556f * xHeight);
    bigOpSpacing5 = NSToCoordRound(100.000f/430.556f * xHeight);
  }

  static void
  GetRuleThickness(nsIFontMetrics* fm,
                   nscoord&        ruleThickness)
  {
    nscoord xHeight;
    fm->GetXHeight(xHeight);
    ruleThickness = NSToCoordRound(40.000f/430.556f * xHeight);
  }

  // Some parameters are not accurately obtained using the x-height.
  // Here are some slower variants to obtain the desired metrics
  // by actually measuring some characters
  static void
  GetRuleThickness(nsIRenderingContext& aRenderingContext, 
                   nsIFontMetrics*      aFontMetrics,
                   nscoord&             aRuleThickness);

  static void
  GetAxisHeight(nsIRenderingContext& aRenderingContext, 
                nsIFontMetrics*      aFontMetrics,
                nscoord&             aAxisHeight);

  // ================
  // helpers to map attributes into CSS rules (work-around to bug 69409 which
  // is not scheduled to be fixed anytime soon)
  static PRInt32
  MapAttributesIntoCSS(nsIPresContext* aPresContext,
                       nsIContent*     aContent);
  static PRInt32
  MapAttributesIntoCSS(nsIPresContext* aPresContext,
                       nsIFrame*       aFrame);

protected:
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
