/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla MathML Project.
 * 
 * The Initial Developer of the Original Code is The University Of 
 * Queensland.  Portions created by The University Of Queensland are
 * Copyright (C) 1999 The University Of Queensland.  All Rights Reserved.
 * 
 * Contributor(s): 
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 *   David J. Fiddes <D.J.Fiddes@hw.ac.uk>
 *   Shyjan Mahamud <mahamud@cs.cmu.edu> (added TeX rendering rules)
 */

#ifndef nsMathMLContainerFrame_h___
#define nsMathMLContainerFrame_h___

#include "nsCOMPtr.h"
#include "nsIViewManager.h"
#include "nsHTMLContainerFrame.h"
#include "nsBlockFrame.h"
#include "nsInlineFrame.h"
#include "nsMathMLAtoms.h"
#include "nsMathMLOperators.h"
#include "nsMathMLChar.h"
#include "nsMathMLFrame.h"
#include "nsMathMLParts.h"

#include "nsCSSValue.h"

/*
 * Base class for MathML container frames. It acts like an inferred 
 * mrow. By default, this frame uses its Reflow() method to lay its 
 * children horizontally and ensure that their baselines are aligned.
 * The Reflow() method relies upon Place() to position children.
 * By overloading Place() in derived classes, it is therefore possible
 * to position children in various customized ways.
 */

// Parameters to handle the change of font-size induced by changing the
// scriptlevel. These are hard-coded values that match with the rules in
// mathml.css. Note that mScriptLevel can exceed these bounds, but the
// scaling effect on the font-size will be bounded. The following bounds can
// be expanded provided the new corresponding rules are added in mathml.css.
#define NS_MATHML_CSS_POSITIVE_SCRIPTLEVEL_LIMIT  +5
#define NS_MATHML_CSS_NEGATIVE_SCRIPTLEVEL_LIMIT  -5
#define NS_MATHML_SCRIPTSIZEMULTIPLIER             0.71f
#define NS_MATHML_SCRIPTMINSIZE                    8

class nsMathMLContainerFrame : public nsHTMLContainerFrame,
                               public nsMathMLFrame {
public:

  NS_DECL_ISUPPORTS_INHERITED

  // --------------------------------------------------------------------------
  // Overloaded nsMathMLFrame methods -- see documentation in nsIMathMLFrame.h

  NS_IMETHOD
  Stretch(nsIPresContext*      aPresContext,
          nsIRenderingContext& aRenderingContext,
          nsStretchDirection   aStretchDirection,
          nsBoundingMetrics&   aContainerSize,
          nsHTMLReflowMetrics& aDesiredStretchSize);

  NS_IMETHOD
  Place(nsIPresContext*      aPresContext,
        nsIRenderingContext& aRenderingContext,
        PRBool               aPlaceOrigin,
        nsHTMLReflowMetrics& aDesiredSize);

  NS_IMETHOD
  EmbellishOperator();

  NS_IMETHOD
  UpdatePresentationDataFromChildAt(nsIPresContext* aPresContext,
                                    PRInt32         aFirstIndex,
                                    PRInt32         aLastIndex,
                                    PRInt32         aScriptLevelIncrement,
                                    PRUint32        aFlagsValues,
                                    PRUint32        aFlagsToUpdate);

  NS_IMETHOD
  ReResolveScriptStyle(nsIPresContext*  aPresContext,
                       nsIStyleContext* aParentContext,
                       PRInt32          aParentScriptLevel);

  // --------------------------------------------------------------------------
  // Overloaded nsHTMLContainerFrame methods -- see documentation in nsIFrame.h
 
  NS_IMETHOD
  GetFrameType(nsIAtom** aType) const;

  NS_IMETHOD
  Init(nsIPresContext*  aPresContext,
       nsIContent*      aContent,
       nsIFrame*        aParent,
       nsIStyleContext* aContext,
       nsIFrame*        aPrevInFlow);

  NS_IMETHOD
  SetInitialChildList(nsIPresContext* aPresContext,
                      nsIAtom*        aListName,
                      nsIFrame*       aChildList);

  NS_IMETHODIMP
  ReflowDirtyChild(nsIPresShell* aPresShell, 
                   nsIFrame*     aChild);

  NS_IMETHOD
  Reflow(nsIPresContext*          aPresContext,
         nsHTMLReflowMetrics&     aDesiredSize,
         const nsHTMLReflowState& aReflowState,
         nsReflowStatus&          aStatus);

  NS_IMETHOD
  DidReflow(nsIPresContext*   aPresContext,
            nsDidReflowStatus aStatus)

  {
    mEmbellishData.flags &= ~NS_MATHML_STRETCH_DONE;
    return nsHTMLContainerFrame::DidReflow(aPresContext, aStatus);
  }

  NS_IMETHOD
  AttributeChanged(nsIPresContext* aPresContext,
                   nsIContent*     aChild,
                   PRInt32         aNameSpaceID,
                   nsIAtom*        aAttribute,
                   PRInt32         aModType, 
                   PRInt32         aHint);

  NS_IMETHOD 
  Paint(nsIPresContext*      aPresContext,
        nsIRenderingContext& aRenderingContext,
        const nsRect&        aDirtyRect,
        nsFramePaintLayer    aWhichLayer,
        PRUint32             aFlags = 0);

  // --------------------------------------------------------------------------
  // Additional methods 

  // error handlers to report than an error (typically invalid markup)
  // was encountered during reflow. By default the user will see the
  // Unicode REPLACEMENT CHAR U+FFFD at the spot where the error was
  // encountered.
  virtual nsresult
  ReflowError(nsIPresContext*      aPresContext,
              nsIRenderingContext& aRenderingContext,
              nsHTMLReflowMetrics& aDesiredSize);
  virtual nsresult
  PaintError(nsIPresContext*      aPresContext,
             nsIRenderingContext& aRenderingContext,
             const nsRect&        aDirtyRect,
             nsFramePaintLayer    aWhichLayer);

  // helper function to reflow token elements
  static nsresult
  ReflowTokenFor(nsIFrame*                aFrame,
                 nsIPresContext*          aPresContext,
                 nsHTMLReflowMetrics&     aDesiredSize,
                 const nsHTMLReflowState& aReflowState,
                 nsReflowStatus&          aStatus);

  // helper function to place token elements
  static nsresult
  PlaceTokenFor(nsIFrame*            aFrame,
                nsIPresContext*      aPresContext,
                nsIRenderingContext& aRenderingContext,
                PRBool               aPlaceOrigin,  
                nsHTMLReflowMetrics& aDesiredSize);

  // helper method to reflow a child frame. We are inline frames, and we don't
  // know our positions until reflow is finished. That's why we ask the
  // base method not to worry about our position.
  nsresult 
  ReflowChild(nsIFrame*                aKidFrame,
              nsIPresContext*          aPresContext,
              nsHTMLReflowMetrics&     aDesiredSize,
              const nsHTMLReflowState& aReflowState,
              nsReflowStatus&          aStatus)
  {
    return nsHTMLContainerFrame::ReflowChild(aKidFrame, aPresContext, aDesiredSize, aReflowState,
                                             0, 0, NS_FRAME_NO_MOVE_FRAME, aStatus);
  }

  // helper to add the inter-spacing when <math> is the immediate parent.
  // Since we don't (yet) handle the root <math> element ourselves, we need to
  // take special care of the inter-frame spacing on elements for which <math>
  // is the direct xml parent. This function will be repeatedly called from
  // left to right on the childframes of <math>, and by so doing it will
  // emulate the spacing that would have been done by a <mrow> container.
  // e.g., it fixes <math> <mi>f</mi> <mo>q</mo> <mi>f</mi> <mo>I</mo> </math>
  virtual nsresult
  FixInterFrameSpacing(nsIPresContext*      aPresContext,
                       nsHTMLReflowMetrics& aDesiredSize);

  // helper method to complete the post-reflow hook and ensure that embellished
  // operators don't terminate their Reflow without receiving a Stretch command.
  virtual nsresult
  FinalizeReflow(nsIPresContext*      aPresContext,
                 nsIRenderingContext& aRenderingContext,
                 nsHTMLReflowMetrics& aDesiredSize);

  // helper to give a style context suitable for doing the stretching to the
  // MathMLChar. Frame classes that use this should make the extra style contexts
  // accessible to the Style System via Get/Set AdditionalStyleContext.
  // return true if the char is a mutable char
  static PRBool
  ResolveMathMLCharStyle(nsIPresContext*  aPresContext,
                         nsIContent*      aContent,
                         nsIStyleContext* aParenStyleContext,
                         nsMathMLChar*    aMathMLChar);

  // helper to check if a frame is an embellished container
  static PRBool
  IsEmbellishOperator(nsIFrame* aFrame);

  // helper method to facilitate getting the reflow and bounding metrics
  // IMPORTANT: This function is only meant to be called in Place() methods 
  // where it is assumed that the frame's rect is still acting as place holder
  // for the frame's ascent and descent information
  static void
  GetReflowAndBoundingMetricsFor(nsIFrame*            aFrame,
                                 nsHTMLReflowMetrics& aReflowMetrics,
                                 nsBoundingMetrics&   aBoundingMetrics);

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
             nsIStyleContext*  aStyleContext,
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
    GetSubDropFromChild (nsIPresContext* aPresContext,
                         nsIFrame*       aChild, 
                         nscoord&        aSubDrop) 
    {
      const nsStyleFont *font;
      aChild->GetStyleData(eStyleStruct_Font, (const nsStyleStruct *&)font);
      nsCOMPtr<nsIFontMetrics> fm;
      aPresContext->GetMetricsFor(font->mFont, getter_AddRefs(fm));

      GetSubDrop (fm, aSubDrop);
    }

  static void 
    GetSupDropFromChild (nsIPresContext* aPresContext,
                         nsIFrame*       aChild, 
                         nscoord&        aSupDrop) 
    {
      const nsStyleFont *font;
      aChild->GetStyleData(eStyleStruct_Font, (const nsStyleStruct *&)font);
      nsCOMPtr<nsIFontMetrics> fm;
      aPresContext->GetMetricsFor(font->mFont, getter_AddRefs(fm));

      GetSupDrop (fm, aSupDrop);
    }

  static void 
  GetSkewCorrectionFromChild (nsIPresContext* aPresContext,
                              nsIFrame*       aChild, 
                              nscoord&        aSkewCorrection) 
    {
      // default is 0
      // individual classes should over-ride this method if necessary
      aSkewCorrection = 0;
    }

  // 2 levels of subscript shifts
  static void
    GetSubScriptShifts (nsIFontMetrics *fm, 
                        nscoord& aSubScriptShift1, 
                        nscoord& aSubScriptShift2)
    {
      GetSubShifts(fm, aSubScriptShift1, aSubScriptShift2);
#if 0
      // XXX for now an alias for GetSubscriptOffset
      fm->GetSubscriptOffset (aSubScriptShift1);
      aSubScriptShift2 = aSubScriptShift1 
        = NSToCoordRound(0.5f * aSubScriptShift1);
#endif
    }

  // 3 levels of superscript shifts
  static void
    GetSupScriptShifts (nsIFontMetrics *fm, 
                        nscoord& aSupScriptShift1, 
                        nscoord& aSupScriptShift2, 
                        nscoord& aSupScriptShift3)
    {
      GetSupShifts(fm, aSupScriptShift1, aSupScriptShift2, aSupScriptShift3);
#if 0
      // XXX for now an alias for GetSupscriptOffset
      fm->GetSuperscriptOffset (aSupScriptShift1);
      aSupScriptShift2 = aSupScriptShift3 = aSupScriptShift1
        = NSToCoordRound(0.75f * aSupScriptShift1); 
#endif
    }

  // these are TeX specific params not found in ordinary fonts

  static void
  GetSubDrop (nsIFontMetrics *fm, nscoord& aSubDrop)
    {
      nscoord xHeight;
      fm->GetXHeight (xHeight);
      aSubDrop = NSToCoordRound(50.000f/430.556f * xHeight);
    }

  static void
  GetSupDrop (nsIFontMetrics *fm, nscoord& aSupDrop)
    {
      nscoord xHeight;
      fm->GetXHeight (xHeight);
      aSupDrop = NSToCoordRound(386.108f/430.556f * xHeight);
    }

  static void
  GetSubShifts (nsIFontMetrics *fm, 
                nscoord& aSubShift1, 
                nscoord& aSubShift2)
    {
      nscoord xHeight = 0;
      fm->GetXHeight (xHeight);
      aSubShift1 = NSToCoordRound (150.000f/430.556f * xHeight);
      aSubShift2 = NSToCoordRound (247.217f/430.556f * xHeight);
    }

  static void
  GetSupShifts (nsIFontMetrics *fm, 
                nscoord& aSupShift1, 
                nscoord& aSupShift2, 
                nscoord& aSupShift3)
    {
      nscoord xHeight = 0;
      fm->GetXHeight (xHeight);
      aSupShift1 = NSToCoordRound (412.892f/430.556f * xHeight);
      aSupShift2 = NSToCoordRound (362.892f/430.556f * xHeight);
      aSupShift3 = NSToCoordRound (288.889f/430.556f * xHeight);
    }

  static void
  GetNumeratorShifts (nsIFontMetrics *fm, 
                      nscoord& numShift1, 
                      nscoord& numShift2, 
                      nscoord& numShift3)
    {
      nscoord xHeight = 0;
      fm->GetXHeight (xHeight);
      numShift1 = NSToCoordRound (676.508f/430.556f * xHeight);
      numShift2 = NSToCoordRound (393.732f/430.556f * xHeight);
      numShift3 = NSToCoordRound (443.731f/430.556f * xHeight);
    }

  static void
  GetDenominatorShifts (nsIFontMetrics *fm, 
                        nscoord& denShift1, 
                        nscoord& denShift2)
    {
      nscoord xHeight = 0;
      fm->GetXHeight (xHeight);
      denShift1 = NSToCoordRound (685.951f/430.556f * xHeight);
      denShift2 = NSToCoordRound (344.841f/430.556f * xHeight);
    }

  static void
  GetAxisHeight (nsIFontMetrics *fm,
                 nscoord& axisHeight)
    {
      fm->GetXHeight (axisHeight);
      axisHeight = NSToCoordRound (250.000f/430.556f * axisHeight);
    }

  static void
  GetBigOpSpacings (nsIFontMetrics *fm, 
                    nscoord& bigOpSpacing1,
                    nscoord& bigOpSpacing2,
                    nscoord& bigOpSpacing3,
                    nscoord& bigOpSpacing4,
                    nscoord& bigOpSpacing5)
    {
      nscoord xHeight = 0;
      fm->GetXHeight (xHeight);
      bigOpSpacing1 = NSToCoordRound (111.111f/430.556f * xHeight);
      bigOpSpacing2 = NSToCoordRound (166.667f/430.556f * xHeight);
      bigOpSpacing3 = NSToCoordRound (200.000f/430.556f * xHeight);
      bigOpSpacing4 = NSToCoordRound (600.000f/430.556f * xHeight);
      bigOpSpacing5 = NSToCoordRound (100.000f/430.556f * xHeight);
    }

  static void
  GetRuleThickness(nsIFontMetrics* fm,
                   nscoord& ruleThickness)
    {
      nscoord xHeight;
      fm->GetXHeight (xHeight);
      ruleThickness = NSToCoordRound (40.000f/430.556f * xHeight);
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

protected:
  virtual PRIntn GetSkipSides() const { return 0; }
};


// --------------------------------------------------------------------------
// Currently, to benefit from line-breaking inside the <math> element, <math> is
// simply mapping to nsBlockFrame or nsInlineFrame.
// A separate implemention needs to provide:
// 1) line-breaking
// 2) proper inter-frame spacing
// 3) firing of Stretch() (in which case FinalizeReflow() would have to be cleaned)
// Issues: If/when mathml becomes a pluggable component, the separation will be needed.
class nsMathMLmathBlockFrame : public nsBlockFrame {
public:
  friend nsresult NS_NewMathMLmathBlockFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  NS_IMETHOD
  SetInitialChildList(nsIPresContext* aPresContext,
                      nsIAtom*        aListName,
                      nsIFrame*       aChildList)
  {
    nsresult rv;

    rv = nsBlockFrame::SetInitialChildList(aPresContext, aListName, aChildList);

    // re-resolve our subtree to set any mathml-expected scriptsizes
    nsIFrame* childFrame = aChildList; // mFrames is not used by nsBlockFrame
    while (childFrame) {
      nsIMathMLFrame* mathMLFrame;
      nsresult res = childFrame->QueryInterface(
        NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
      if (NS_SUCCEEDED(res) && mathMLFrame) {
        mathMLFrame->ReResolveScriptStyle(aPresContext, mStyleContext, 0);
      }
      childFrame->GetNextSibling(&childFrame);
    }
    return rv;
  }

protected:
  nsMathMLmathBlockFrame() {}
  virtual ~nsMathMLmathBlockFrame() {}
};

class nsMathMLmathInlineFrame : public nsInlineFrame {
public:
  friend nsresult NS_NewMathMLmathInlineFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  NS_IMETHOD
  SetInitialChildList(nsIPresContext* aPresContext,
                      nsIAtom*        aListName,
                      nsIFrame*       aChildList)
  {
    nsresult rv;
    rv = nsInlineFrame::SetInitialChildList(aPresContext, aListName, aChildList);

    // re-resolve our subtree to set any mathml-expected scriptsizes
    nsIFrame* childFrame = mFrames.FirstChild();
    while (childFrame) {
      nsIMathMLFrame* mathMLFrame;
      nsresult res = childFrame->QueryInterface(
        NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
      if (NS_SUCCEEDED(res) && mathMLFrame) {
        mathMLFrame->ReResolveScriptStyle(aPresContext, mStyleContext, 0);
      }
      childFrame->GetNextSibling(&childFrame);
    }
    return rv;
  }

protected:
  nsMathMLmathInlineFrame() {}
  virtual ~nsMathMLmathInlineFrame() {}
};
#endif /* nsMathMLContainerFrame_h___ */
