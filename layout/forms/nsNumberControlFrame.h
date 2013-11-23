/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNumberControlFrame_h__
#define nsNumberControlFrame_h__

#include "nsContainerFrame.h"
#include "nsIFormControlFrame.h"
#include "nsIAnonymousContentCreator.h"
#include "nsCOMPtr.h"

class nsPresContext;

namespace mozilla {
namespace dom {
class HTMLInputElement;
}
}

/**
 * This frame type is used for <input type=number>.
 */
class nsNumberControlFrame MOZ_FINAL : public nsContainerFrame
                                     , public nsIAnonymousContentCreator
{
  friend nsIFrame*
  NS_NewNumberControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  typedef mozilla::dom::HTMLInputElement HTMLInputElement;

  nsNumberControlFrame(nsStyleContext* aContext);

public:
  NS_DECL_QUERYFRAME_TARGET(nsNumberControlFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  virtual void DestroyFrom(nsIFrame* aDestructRoot) MOZ_OVERRIDE;

  virtual bool IsLeaf() const MOZ_OVERRIDE { return true; }

  NS_IMETHOD Reflow(nsPresContext*           aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus) MOZ_OVERRIDE;

  NS_IMETHOD AttributeChanged(int32_t  aNameSpaceID,
                              nsIAtom* aAttribute,
                              int32_t  aModType) MOZ_OVERRIDE;

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) MOZ_OVERRIDE;
  virtual void AppendAnonymousContentTo(nsBaseContentList& aElements,
                                        uint32_t aFilter) MOZ_OVERRIDE;

#ifdef NS_DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const MOZ_OVERRIDE {
    return MakeFrameName(NS_LITERAL_STRING("NumberControl"), aResult);
  }
#endif

  virtual nsIAtom* GetType() const MOZ_OVERRIDE;

  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
  {
    return nsContainerFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

  /**
   * When our HTMLInputElement's value changes, it calls this method to tell
   * us to sync up our anonymous text input field child.
   */
  void UpdateForValueChange(const nsAString& aValue);

  /**
   * Called to notify this frame that its HTMLInputElement is currently
   * processing a DOM 'input' event.
   */
  void HandlingInputEvent(bool aHandlingEvent)
  {
    mHandlingInputEvent = aHandlingEvent;
  }

  HTMLInputElement* GetAnonTextControl();

private:

  nsresult MakeAnonymousElement(nsIContent** aResult,
                                nsTArray<ContentInfo>& aElements,
                                nsIAtom* aTagName,
                                nsCSSPseudoElements::Type aPseudoType,
                                nsStyleContext* aParentContext);

  nsresult ReflowAnonymousContent(nsPresContext* aPresContext,
                                  nsHTMLReflowMetrics& aWrappersDesiredSize,
                                  const nsHTMLReflowState& aReflowState,
                                  nsIFrame* aOuterWrapperFrame);

  /**
   * The text field used to edit and show the number.
   * @see nsNumberControlFrame::CreateAnonymousContent.
   */
  nsCOMPtr<nsIContent> mOuterWrapper;
  nsCOMPtr<nsIContent> mTextField;
  nsCOMPtr<nsIContent> mSpinBox;
  nsCOMPtr<nsIContent> mSpinUp;
  nsCOMPtr<nsIContent> mSpinDown;
  bool mHandlingInputEvent;
};

#endif // nsNumberControlFrame_h__
