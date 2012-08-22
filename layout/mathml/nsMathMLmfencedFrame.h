/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLmfencedFrame_h
#define nsMathMLmfencedFrame_h

#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"

//
// <mfenced> -- surround content with a pair of fences
//

class nsMathMLmfencedFrame : public nsMathMLContainerFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewMathMLmfencedFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  virtual void
  SetAdditionalStyleContext(int32_t          aIndex, 
                            nsStyleContext*  aStyleContext);
  virtual nsStyleContext*
  GetAdditionalStyleContext(int32_t aIndex) const;

  NS_IMETHOD
  InheritAutomaticData(nsIFrame* aParent);

  NS_IMETHOD
  SetInitialChildList(ChildListID     aListID,
                      nsFrameList&    aChildList);

  NS_IMETHOD
  Reflow(nsPresContext*          aPresContext,
         nsHTMLReflowMetrics&     aDesiredSize,
         const nsHTMLReflowState& aReflowState,
         nsReflowStatus&          aStatus);

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  virtual nscoord
  GetIntrinsicWidth(nsRenderingContext* aRenderingContext);

  NS_IMETHOD
  AttributeChanged(int32_t         aNameSpaceID,
                   nsIAtom*        aAttribute,
                   int32_t         aModType);

  // override the base method because we must keep separators in sync
  virtual nsresult
  ChildListChanged(int32_t aModType);

  // override the base method so that we can deal with fences and separators
  virtual nscoord
  FixInterFrameSpacing(nsHTMLReflowMetrics& aDesiredSize);

  // helper routines to format the MathMLChars involved here
  static nsresult
  ReflowChar(nsPresContext*      aPresContext,
             nsRenderingContext& aRenderingContext,
             nsMathMLChar*        aMathMLChar,
             nsOperatorFlags      aForm,
             int32_t              aScriptLevel,
             nscoord              axisHeight,
             nscoord              leading,
             nscoord              em,
             nsBoundingMetrics&   aContainerSize,
             nscoord&             aAscent,
             nscoord&             aDescent,
             bool                 aRTL);

  static void
  PlaceChar(nsMathMLChar*      aMathMLChar,
            nscoord            aDesiredSize,
            nsBoundingMetrics& bm,
            nscoord&           dx);

protected:
  nsMathMLmfencedFrame(nsStyleContext* aContext) : nsMathMLContainerFrame(aContext) {}
  virtual ~nsMathMLmfencedFrame();
  
  virtual int GetSkipSides() const { return 0; }

  nsMathMLChar* mOpenChar;
  nsMathMLChar* mCloseChar;
  nsMathMLChar* mSeparatorsChar;
  int32_t       mSeparatorsCount;

  // clean up
  void
  RemoveFencesAndSeparators();

  // add fences and separators when all child frames are known
  void
  CreateFencesAndSeparators(nsPresContext* aPresContext);
};

#endif /* nsMathMLmfencedFrame_h */
