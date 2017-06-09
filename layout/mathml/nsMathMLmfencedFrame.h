/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLmfencedFrame_h
#define nsMathMLmfencedFrame_h

#include "mozilla/Attributes.h"
#include "nsMathMLContainerFrame.h"

class nsFontMetrics;

//
// <mfenced> -- surround content with a pair of fences
//

class nsMathMLmfencedFrame final : public nsMathMLContainerFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS(nsMathMLmfencedFrame)

  friend nsIFrame* NS_NewMathMLmfencedFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  virtual void
  SetAdditionalStyleContext(int32_t          aIndex, 
                            nsStyleContext*  aStyleContext) override;
  virtual nsStyleContext*
  GetAdditionalStyleContext(int32_t aIndex) const override;

  NS_IMETHOD
  InheritAutomaticData(nsIFrame* aParent) override;

  virtual void
  SetInitialChildList(ChildListID     aListID,
                      nsFrameList&    aChildList) override;

  virtual void
  Reflow(nsPresContext*          aPresContext,
         ReflowOutput&     aDesiredSize,
         const ReflowInput& aReflowInput,
         nsReflowStatus&          aStatus) override;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  virtual void
  GetIntrinsicISizeMetrics(gfxContext* aRenderingContext,
                           ReflowOutput& aDesiredSize) override;

  virtual nsresult
  AttributeChanged(int32_t         aNameSpaceID,
                   nsIAtom*        aAttribute,
                   int32_t         aModType) override;

  // override the base method because we must keep separators in sync
  virtual nsresult
  ChildListChanged(int32_t aModType) override;

  // override the base method so that we can deal with fences and separators
  virtual nscoord
  FixInterFrameSpacing(ReflowOutput& aDesiredSize) override;

  // helper routines to format the MathMLChars involved here
  static nsresult
  ReflowChar(nsPresContext*       aPresContext,
             DrawTarget*          aDrawTarget,
             nsFontMetrics&       aFontMetrics,
             float                aFontSizeInflation,
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

  virtual bool
  IsMrowLike() override
  {
    // Always treated as an mrow with > 1 child as
    // <mfenced> <mo>%</mo> </mfenced>
    // renders equivalently to
    // <mrow> <mo> ( </mo> <mo>%</mo> <mo> ) </mo> </mrow>
    // This also holds with multiple children.  (MathML3 3.3.8.1)
    return true;
  }

protected:
  explicit nsMathMLmfencedFrame(nsStyleContext* aContext)
    : nsMathMLContainerFrame(aContext, kClassID)
    , mOpenChar(nullptr)
    , mCloseChar(nullptr)
    , mSeparatorsChar(nullptr)
    , mSeparatorsCount(0)
  {}

  virtual ~nsMathMLmfencedFrame();
  
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
