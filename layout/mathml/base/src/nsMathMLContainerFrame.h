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
#include "nsIMathMLFrame.h"
#include "nsMathMLParts.h"

/*
 * Base class for MathML container frames. It acts like an inferred 
 * mrow. By default, this frame uses its Reflow() method to lay its 
 * children horizontally and ensure that their baselines are aligned.
 * The Reflow() method relies upon Place() to position children.
 * By overloading Place() in derived classes, it is therefore possible
 * to position children in various customized ways.
 * 
 * This frame is a *math-aware frame* in the sense that given the markup
 * <tag>base arguments</tag>, the method InsertScriptLevelStyleContext()
 * can be used to render the 'base' in normal-font, and the 'arguments'
 * in small-font. This is a common functionality to tags like msub, msup, 
 * msubsup, mover, munder, munderover, mmultiscripts. All are  derived 
 * from this base class. They use SetInitialChildList() to trigger
 * InsertScriptLevelStyleContext() for the very first time as soon as all
 * their children are known. However, each of these tags has its own 
 * Reflow() and/or Place() methods to lay its children as appropriate,
 * thus overriding the default behavior in this base class.
 * 
 * Other tags like mi that do not have 'arguments' can be derived from
 * this base class as well. The class caters for empty arguments.
 * Notice that mi implements its own SetInitialChildList() method 
 * to switch to a normal-font (rather than italics) if its text content 
 * is not a single character (as per the MathML REC).
 * 
 */

class nsMathMLContainerFrame : public nsHTMLContainerFrame, public nsIMathMLFrame {
public:

  // nsIMathMLFrame methods  -- see documentation in nsIMathMLFrame.h
  
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  NS_IMETHOD
  GetBoundingMetrics(nsBoundingMetrics& aBoundingMetrics);

  NS_IMETHOD
  SetBoundingMetrics(const nsBoundingMetrics& aBoundingMetrics);

  NS_IMETHOD
  GetReference(nsPoint& aReference);

  NS_IMETHOD
  SetReference(const nsPoint& aReference);

  NS_IMETHOD
  Stretch(nsIPresContext*      aPresContext,
          nsIRenderingContext& aRenderingContext,
          nsStretchDirection   aStretchDirection,
          nsStretchMetrics&    aContainerSize,
          nsStretchMetrics&    aDesiredStretchSize);
#if 0
  NS_IMETHOD
  GetDesiredStretchSize(nsIPresContext*      aPresContext,
                        nsIRenderingContext& aRenderingContext,
                        nsStretchMetrics&    aDesiredStretchSize);
#endif
  NS_IMETHOD
  Place(nsIPresContext*      aPresContext,
        nsIRenderingContext& aRenderingContext,
        PRBool               aPlaceOrigin,
        nsHTMLReflowMetrics& aDesiredSize);

  NS_IMETHOD
  EmbellishOperator();

  NS_IMETHOD
  GetEmbellishData(nsEmbellishData& aEmbellishData);

  NS_IMETHOD
  SetEmbellishData(const nsEmbellishData& aEmbellishData);

  NS_IMETHOD
  GetPresentationData(nsPresentationData& aPresentationData);

  NS_IMETHOD
  SetPresentationData(const nsPresentationData& aPresentationData);

  NS_IMETHOD
  UpdatePresentationData(PRInt32 aScriptLevelIncrement, 
                         PRBool  aDisplayStyle);

  NS_IMETHOD
  UpdatePresentationDataFromChildAt(PRInt32 aIndex, 
                                    PRInt32 aScriptLevelIncrement,
                                    PRBool  aDisplayStyle);

  // nsHTMLContainerFrame methods
 
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
  Paint(nsIPresContext*      aPresContext,
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

  // helper function to reflow all children before placing them.
  // With MathML, we have to reflow all children and use their desired size
  // information in order to figure out how to lay them. 
  // aDirection = 0 means children will later be laid horizontally
  // aDirection = 1 means children will later be laid vertically
  NS_IMETHOD
  ReflowChildren(PRInt32                  aDirection,
                 nsIPresContext*          aPresContext,
                 nsHTMLReflowMetrics&     aDesiredSize,
                 const nsHTMLReflowState& aReflowState,
                 nsReflowStatus&          aStatus);

  // helper function to fire a Stretch command on all children,
  // Note that if the container is embellished, the stretch is not fired
  // on its embellished child. It is the responsibility of the parent
  // of the embellished container to fire a stretch that is then passed
  // onto the embellished child.
  // If an embellished container doesn't have a parent that will fire
  // a stretch, it will do it itself (see FinalizeReflow).
  NS_IMETHOD
  StretchChildren(nsIPresContext*      aPresContext,
                  nsIRenderingContext& aRenderingContext,
                  nsStretchDirection   aStretchDirection,
                  nsStretchMetrics&    aContainerSize);

  // helper method to complete the post-reflow hook and ensure that embellished
  // operators don't terminate their Reflow without receiving a Stretch command.
  NS_IMETHOD
  FinalizeReflow(PRInt32              aDirection,
                 nsIPresContext*      aPresContext,
                 nsIRenderingContext& aRenderingContext,
                 nsHTMLReflowMetrics& aDesiredSize);

  // helper method to alter the style contexts of subscript/superscript elements
  // XXX this is pretty much a hack until the content model caters for MathML and
  // the style system has some provisions for MathML
  NS_IMETHOD
  InsertScriptLevelStyleContext(nsIPresContext* aPresContext);

  // helper to find the smallest font-size on a tree so that we don't insert
  // scriptlevel fonts that lead to unreadable results on deeper nodes below us.
  // XXX expensive recursive function, need something better, with cache
  static PRInt32
  FindSmallestFontSizeFor(nsIFrame* aFrame);

  // helper to check if a frame is an embellished container
  static PRBool
  IsEmbellishOperator(nsIFrame* aFrame);

  // helper methods for processing empty MathML frames (with whitespace only)
  static PRBool
  IsOnlyWhitespace(nsIFrame* aFrame);

  static void
  ReflowEmptyChild(nsIPresContext* aPresContext,
                   nsIFrame*       aFrame);

  // helper method to facilitate getting the reflow and bounding metrics
  // IMPORTANT: This function is only meant to be called in Place() methods 
  // where it is assumed that the frame's rect is still acting as place holder
  // for the frame's ascent and descent information
  static void
  GetReflowAndBoundingMetricsFor(nsIFrame*            aFrame,
                                 nsHTMLReflowMetrics& aReflowMetrics,
                                 nsBoundingMetrics&   aBoundingMetrics);

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

  // 2 levels of subscript shifts
  static void
    GetSubScriptShifts (nsIFontMetrics *fm, 
                        nscoord& aSubScriptShift1, 
                        nscoord& aSubScriptShift2)
    {
      // XXX for now an alias for GetSubscriptOffset
      fm->GetSubscriptOffset (aSubScriptShift1);
      aSubScriptShift2 = aSubScriptShift1 
        = NSToCoordRound(0.5f * aSubScriptShift1);
    }

  // 3 levels of superscript shifts
  static void
    GetSupScriptShifts (nsIFontMetrics *fm, 
                        nscoord& aSupScriptShift1, 
                        nscoord& aSupScriptShift2, 
                        nscoord& aSupScriptShift3)
    {
      // XXX for now an alias for GetSupscriptOffset
      fm->GetSuperscriptOffset (aSupScriptShift1);
      aSupScriptShift2 = aSupScriptShift3 = aSupScriptShift1
        = NSToCoordRound(0.75f * aSupScriptShift1); 
    }

  // these are TeX specific params not found in ordinary fonts
#if 0
  static void
    GetSubDrop (nsIFontMetrics *fm, nscoord& aSubDrop)
    {
      nscoord xHeight;
      fm->GetXHeight (xHeight);
      aSubDrop = NSToCoordRound(0.3f * xHeight);
    }

  static void
    GetSupDrop (nsIFontMetrics *fm, nscoord& aSupDrop)
    {
      nscoord xHeight;
      fm->GetXHeight (xHeight);
      aSupDrop = NSToCoordRound(0.3f * xHeight);
    }
#endif

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
  GetNumShifts (nsIFontMetrics *fm, 
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
  GetDenShifts (nsIFontMetrics *fm, 
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

#if 0
  NS_IMETHOD
  GetItalicCorrection (nscoord& italicCorrection)
    {
      italicCorrection = mItalicCorrection;
      
      return NS_OK;
    }
  
  NS_IMETHOD
  SetItalicCorrection (nscoord italicCorrection)
    {
      mItalicCorrection = italicCorrection;
      
      return NS_OK;
    }
#endif

  NS_IMETHOD
  GetRuleThickness (nsIFontMetrics *fm, nscoord& ruleThickness)
    {
      nscoord xHeight;
      fm->GetXHeight (xHeight);
      ruleThickness = NSToCoordRound (40.000f/430.556f * xHeight);
      
      return NS_OK;
    }

protected:

  // information about the presentation policy of the frame
  nsPresentationData mPresentationData;

  // information about a container that is an embellished operator
  nsEmbellishData mEmbellishData;
  
  // Metrics that _exactly_ enclose the text of the frame
  nsBoundingMetrics mBoundingMetrics;
  
  // Reference point of the frame: mReference.y is the baseline
  nsPoint mReference; 
  
  virtual PRIntn GetSkipSides() const { return 0; }
};

// XXX hack until the content model caters for MathML and the style system 
// has some provisions for MathML
class nsMathMLWrapperFrame : public nsMathMLContainerFrame {
public:
  friend nsresult NS_NewMathMLWrapperFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  NS_IMETHOD
  Reflow(nsIPresContext*          aPresContext,
         nsHTMLReflowMetrics&     aDesiredSize,
         const nsHTMLReflowState& aReflowState,
         nsReflowStatus&          aStatus);

  NS_IMETHOD
  Place(nsIPresContext*      aPresContext,
        nsIRenderingContext& aRenderingContext,
        PRBool               aPlaceOrigin,
        nsHTMLReflowMetrics& aDesiredSize);

protected:
  nsMathMLWrapperFrame();
  virtual ~nsMathMLWrapperFrame();
  
  virtual PRIntn GetSkipSides() const { return 0; }
};

#endif /* nsMathMLContainerFrame_h___ */
