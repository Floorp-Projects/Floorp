/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsColorControlFrame_h___
#define nsColorControlFrame_h___

#include "nsCOMPtr.h"
#include "nsHTMLButtonControlFrame.h"
#include "nsIAnonymousContentCreator.h"

namespace mozilla {
enum class CSSPseudoElementType : uint8_t;
} // namespace mozilla

// Class which implements the input type=color

class nsColorControlFrame final : public nsHTMLButtonControlFrame,
                                  public nsIAnonymousContentCreator
{
  typedef mozilla::CSSPseudoElementType CSSPseudoElementType;
  typedef mozilla::dom::Element Element;

public:
  friend nsIFrame* NS_NewColorControlFrame(nsIPresShell* aPresShell,
                                           nsStyleContext* aContext);

  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;

  NS_DECL_QUERYFRAME_TARGET(nsColorControlFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) override;
  virtual void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                        uint32_t aFilter) override;

  // nsIFrame
  virtual nsresult AttributeChanged(int32_t  aNameSpaceID,
                                    nsIAtom* aAttribute,
                                    int32_t  aModType) override;
  virtual bool IsLeaf() const override { return true; }
  virtual nsContainerFrame* GetContentInsertionFrame() override;

  virtual Element* GetPseudoElement(CSSPseudoElementType aType) override;

  // Refresh the color swatch, using associated input's value
  nsresult UpdateColor();

private:
  explicit nsColorControlFrame(nsStyleContext* aContext);

  nsCOMPtr<Element> mColorContent;
};


#endif // nsColorControlFrame_h___
