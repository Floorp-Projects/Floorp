/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPopupSetFrame_h__
#define nsPopupSetFrame_h__

#include "mozilla/Attributes.h"
#include "nsAtom.h"
#include "nsBoxFrame.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

nsIFrame* NS_NewPopupSetFrame(mozilla::PresShell* aPresShell,
                              mozilla::ComputedStyle* aStyle);

class nsPopupSetFrame final : public nsBoxFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsPopupSetFrame)

  explicit nsPopupSetFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : nsBoxFrame(aStyle, aPresContext, kClassID) {}

  ~nsPopupSetFrame() {}

  virtual void Init(nsIContent* aContent, nsContainerFrame* aParent,
                    nsIFrame* aPrevInFlow) override;

  virtual void SetInitialChildList(ChildListID aListID,
                                   nsFrameList& aChildList) override;
  virtual void AppendFrames(ChildListID aListID,
                            nsFrameList& aFrameList) override;
  virtual void RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame) override;
  virtual void InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                            nsFrameList& aFrameList) override;

  virtual const nsFrameList& GetChildList(ChildListID aList) const override;
  virtual void GetChildLists(nsTArray<ChildList>* aLists) const override;

  NS_IMETHOD DoXULLayout(nsBoxLayoutState& aBoxLayoutState) override;

  // Used to destroy our popup frames.
  virtual void DestroyFrom(nsIFrame* aDestructRoot,
                           PostDestroyData& aPostDestroyData) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(NS_LITERAL_STRING("PopupSet"), aResult);
  }
#endif

 protected:
  void AddPopupFrameList(nsFrameList& aPopupFrameList);
  void RemovePopupFrame(nsIFrame* aPopup);

  nsFrameList mPopupList;
};

#endif
