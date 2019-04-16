/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**

  Eric D Vaughan
  A frame that can have multiple children. Only one child may be displayed at
one time. So the can be flipped though like a deck of cards.

**/

#ifndef nsDeckFrame_h___
#define nsDeckFrame_h___

#include "mozilla/Attributes.h"
#include "nsBoxFrame.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

class nsDeckFrame final : public nsBoxFrame {
 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsDeckFrame)

  friend nsIFrame* NS_NewDeckFrame(mozilla::PresShell* aPresShell,
                                   ComputedStyle* aStyle);

  virtual nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                    int32_t aModType) override;

  NS_IMETHOD DoXULLayout(nsBoxLayoutState& aState) override;

  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                const nsDisplayListSet& aLists) override;

  virtual void RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame) override;

  virtual void BuildDisplayListForChildren(
      nsDisplayListBuilder* aBuilder, const nsDisplayListSet& aLists) override;

  virtual void Init(nsIContent* aContent, nsContainerFrame* aParent,
                    nsIFrame* aPrevInFlow) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(NS_LITERAL_STRING("Deck"), aResult);
  }
#endif

  explicit nsDeckFrame(ComputedStyle* aStyle, nsPresContext* aPresContext);

  nsIFrame* GetSelectedBox();

 protected:
  void IndexChanged();
  int32_t GetSelectedIndex();
  void HideBox(nsIFrame* aBox);
  void ShowBox(nsIFrame* aBox);

 private:
  int32_t mIndex;

  void Animate(nsIFrame*, bool);

};  // class nsDeckFrame

#endif
