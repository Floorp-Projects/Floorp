/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLmoFrame_h___
#define nsMathMLmoFrame_h___

#include "mozilla/Attributes.h"
#include "nsMathMLTokenFrame.h"
#include "nsMathMLChar.h"

//
// <mo> -- operator, fence, or separator
//

class nsMathMLmoFrame : public nsMathMLTokenFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS(nsMathMLmoFrame)

  friend nsIFrame* NS_NewMathMLmoFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  virtual eMathMLFrameType GetMathMLFrameType() override;

  virtual void
  SetAdditionalStyleContext(int32_t          aIndex, 
                            nsStyleContext*  aStyleContext) override;
  virtual nsStyleContext*
  GetAdditionalStyleContext(int32_t aIndex) const override;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  NS_IMETHOD
  InheritAutomaticData(nsIFrame* aParent) override;

  NS_IMETHOD
  TransmitAutomaticData() override;

  virtual void
  SetInitialChildList(ChildListID     aListID,
                      nsFrameList&    aChildList) override;

  virtual void
  Reflow(nsPresContext*          aPresContext,
         ReflowOutput&     aDesiredSize,
         const ReflowInput& aReflowInput,
         nsReflowStatus&          aStatus) override;

  virtual nsresult
  Place(DrawTarget*          aDrawTarget,
        bool                 aPlaceOrigin,
        ReflowOutput& aDesiredSize) override;

  virtual void MarkIntrinsicISizesDirty() override;

  virtual void
  GetIntrinsicISizeMetrics(gfxContext* aRenderingContext,
                           ReflowOutput& aDesiredSize) override;

  virtual nsresult
  AttributeChanged(int32_t         aNameSpaceID,
                   nsIAtom*        aAttribute,
                   int32_t         aModType) override;

  // This method is called by the parent frame to ask <mo> 
  // to stretch itself.
  NS_IMETHOD
  Stretch(DrawTarget*          aDrawTarget,
          nsStretchDirection   aStretchDirection,
          nsBoundingMetrics&   aContainerSize,
          ReflowOutput& aDesiredStretchSize) override;

  virtual nsresult
  ChildListChanged(int32_t aModType) override
  {
    ProcessTextData();
    return nsMathMLContainerFrame::ChildListChanged(aModType);
  }

protected:
  explicit nsMathMLmoFrame(nsStyleContext* aContext) :
    nsMathMLTokenFrame(aContext, kClassID), mFlags(0), mMinSize(0), mMaxSize(0) {}
  virtual ~nsMathMLmoFrame();
  
  nsMathMLChar     mMathMLChar; // Here is the MathMLChar that will deal with the operator.
  nsOperatorFlags  mFlags;
  float            mMinSize;
  float            mMaxSize;

  bool UseMathMLChar();

  // overload the base method so that we can setup our nsMathMLChar
  void ProcessTextData();

  // helper to get our 'form' and lookup in the Operator Dictionary to fetch 
  // our default data that may come from there, and to complete the setup
  // using attributes that we may have
  void
  ProcessOperatorData();

  // helper to double check thar our char should be rendered as a selected char
  bool
  IsFrameInSelection(nsIFrame* aFrame);
};

#endif /* nsMathMLmoFrame_h___ */
