/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBoxFrame.h"

class nsITreeBoxObject;

nsIFrame* NS_NewTreeColFrame(nsIPresShell* aPresShell, 
                             nsStyleContext* aContext);

class nsTreeColFrame : public nsBoxFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  nsTreeColFrame(nsIPresShell* aPresShell,
                 nsStyleContext* aContext):
    nsBoxFrame(aPresShell, aContext) {}

  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);

  virtual void DestroyFrom(nsIFrame* aDestructRoot);

  NS_IMETHOD BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                         const nsRect&           aDirtyRect,
                                         const nsDisplayListSet& aLists);

  NS_IMETHOD AttributeChanged(PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType);

  virtual void SetBounds(nsBoxLayoutState& aBoxLayoutState, const nsRect& aRect,
                         bool aRemoveOverflowArea = false);

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
