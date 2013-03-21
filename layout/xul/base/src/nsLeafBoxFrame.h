/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsLeafBoxFrame_h___
#define nsLeafBoxFrame_h___

#include "mozilla/Attributes.h"
#include "nsLeafFrame.h"
#include "nsBox.h"

class nsAccessKeyInfo;

class nsLeafBoxFrame : public nsLeafFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewLeafBoxFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  virtual nsSize GetPrefSize(nsBoxLayoutState& aState);
  virtual nsSize GetMinSize(nsBoxLayoutState& aState);
  virtual nsSize GetMaxSize(nsBoxLayoutState& aState) MOZ_OVERRIDE;
  virtual nscoord GetFlex(nsBoxLayoutState& aState) MOZ_OVERRIDE;
  virtual nscoord GetBoxAscent(nsBoxLayoutState& aState) MOZ_OVERRIDE;

  virtual nsIAtom* GetType() const MOZ_OVERRIDE;
  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
  {
    // This is bogus, but it's what we've always done.
    // Note that nsLeafFrame is also eReplacedContainsBlock.
    return nsLeafFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock | nsIFrame::eXULBox));
  }

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif

  // nsIHTMLReflow overrides

  virtual void MarkIntrinsicWidthsDirty() MOZ_OVERRIDE;
  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;

  // Our auto size is that provided by nsFrame, not nsLeafFrame
  virtual nsSize ComputeAutoSize(nsRenderingContext *aRenderingContext,
                                 nsSize aCBSize, nscoord aAvailableWidth,
                                 nsSize aMargin, nsSize aBorder,
                                 nsSize aPadding, bool aShrinkWrap) MOZ_OVERRIDE;

  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus) MOZ_OVERRIDE;

  NS_IMETHOD CharacterDataChanged(CharacterDataChangeInfo* aInfo) MOZ_OVERRIDE;

  virtual void Init(nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsIFrame*        asPrevInFlow) MOZ_OVERRIDE;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  NS_IMETHOD AttributeChanged(int32_t aNameSpaceID,
                              nsIAtom* aAttribute,
                              int32_t aModType) MOZ_OVERRIDE;

  virtual bool ComputesOwnOverflowArea() MOZ_OVERRIDE { return false; }

protected:

  NS_IMETHOD DoLayout(nsBoxLayoutState& aState) MOZ_OVERRIDE;

#ifdef DEBUG_LAYOUT
  virtual void GetBoxName(nsAutoString& aName) MOZ_OVERRIDE;
#endif

  virtual nscoord GetIntrinsicWidth() MOZ_OVERRIDE;

 nsLeafBoxFrame(nsIPresShell* aShell, nsStyleContext* aContext);

private:

 void UpdateMouseThrough();


}; // class nsLeafBoxFrame

#endif /* nsLeafBoxFrame_h___ */
