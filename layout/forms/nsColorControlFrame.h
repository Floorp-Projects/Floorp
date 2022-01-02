/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsColorControlFrame_h___
#define nsColorControlFrame_h___

#include "nsCOMPtr.h"
#include "nsHTMLButtonControlFrame.h"
#include "nsIAnonymousContentCreator.h"

namespace mozilla {
enum class PseudoStyleType : uint8_t;
class PresShell;
}  // namespace mozilla

// Class which implements the input type=color

class nsColorControlFrame final : public nsHTMLButtonControlFrame,
                                  public nsIAnonymousContentCreator {
  typedef mozilla::PseudoStyleType PseudoStyleType;
  typedef mozilla::dom::Element Element;

 public:
  friend nsIFrame* NS_NewColorControlFrame(mozilla::PresShell* aPresShell,
                                           ComputedStyle* aStyle);

  virtual void DestroyFrom(nsIFrame* aDestructRoot,
                           PostDestroyData& aPostDestroyData) override;

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsColorControlFrame)

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(
      nsTArray<ContentInfo>& aElements) override;
  virtual void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                        uint32_t aFilter) override;

  // nsIFrame
  virtual nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                    int32_t aModType) override;
  virtual nsContainerFrame* GetContentInsertionFrame() override;

  // Refresh the color swatch, using associated input's value
  nsresult UpdateColor();

 private:
  explicit nsColorControlFrame(ComputedStyle* aStyle,
                               nsPresContext* aPresContext);

  nsCOMPtr<Element> mColorContent;
};

#endif  // nsColorControlFrame_h___
