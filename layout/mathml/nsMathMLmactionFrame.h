/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLmactionFrame_h___
#define nsMathMLmactionFrame_h___

#include "nsCOMPtr.h"
#include "nsMathMLSelectedFrame.h"
#include "nsIDOMEventListener.h"
#include "mozilla/Attributes.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

//
// <maction> -- bind actions to a subexpression
//

class nsMathMLmactionFrame final : public nsMathMLSelectedFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsMathMLmactionFrame)

  friend nsIFrame* NS_NewMathMLmactionFrame(mozilla::PresShell* aPresShell,
                                            ComputedStyle* aStyle);

  virtual void Init(nsIContent* aContent, nsContainerFrame* aParent,
                    nsIFrame* aPrevInFlow) override;

  virtual void SetInitialChildList(ChildListID aListID,
                                   nsFrameList& aChildList) override;

  virtual nsresult ChildListChanged(int32_t aModType) override;

  virtual nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                    int32_t aModType) override;

 private:
  void MouseClick();
  void MouseOver();
  void MouseOut();

  class MouseListener final : public nsIDOMEventListener {
   private:
    ~MouseListener() {}

   public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIDOMEVENTLISTENER

    explicit MouseListener(nsMathMLmactionFrame* aOwner) : mOwner(aOwner) {}

    nsMathMLmactionFrame* mOwner;
  };

 protected:
  explicit nsMathMLmactionFrame(ComputedStyle* aStyle,
                                nsPresContext* aPresContext)
      : nsMathMLSelectedFrame(aStyle, aPresContext, kClassID) {}
  virtual ~nsMathMLmactionFrame();

 private:
  int32_t mActionType;
  int32_t mChildCount;
  int32_t mSelection;
  RefPtr<MouseListener> mListener;

  // helper to return the frame for the attribute selection="number"
  nsIFrame* GetSelectedFrame() override;
};

#endif /* nsMathMLmactionFrame_h___ */
