/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"
#include "nsBoxFrame.h"

class nsITreeBoxObject;

nsIFrame* NS_NewTreeColFrame(nsIPresShell* aPresShell, 
                             nsStyleContext* aContext);

class nsTreeColFrame final : public nsBoxFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  explicit nsTreeColFrame(nsStyleContext* aContext):
    nsBoxFrame(aContext) {}

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;

  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;

  virtual void BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                           const nsRect&           aDirtyRect,
                                           const nsDisplayListSet& aLists) override;

  virtual nsresult AttributeChanged(int32_t aNameSpaceID,
                                    nsIAtom* aAttribute,
                                    int32_t aModType) override;

  virtual void SetXULBounds(nsBoxLayoutState& aBoxLayoutState, const nsRect& aRect,
                            bool aRemoveOverflowArea = false) override;

  friend nsIFrame* NS_NewTreeColFrame(nsIPresShell* aPresShell,
                                      nsStyleContext* aContext);

protected:
  virtual ~nsTreeColFrame();

  /**
   * @return the tree box object of the tree this column belongs to, or nullptr.
   */
  nsITreeBoxObject* GetTreeBoxObject();

  /**
   * Helper method that gets the nsITreeColumns object this column belongs to
   * and calls InvalidateColumns() on it.
   */
  void InvalidateColumns(bool aCanWalkFrameTree = true);
};
