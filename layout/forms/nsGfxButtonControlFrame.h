/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGfxButtonControlFrame_h___
#define nsGfxButtonControlFrame_h___

#include "mozilla/Attributes.h"
#include "nsHTMLButtonControlFrame.h"
#include "nsCOMPtr.h"
#include "nsIAnonymousContentCreator.h"

// Class which implements the input[type=button, reset, submit] and
// browse button for input[type=file].
// The label for button is specified through generated content
// in the ua.css file.

class nsGfxButtonControlFrame final
  : public nsHTMLButtonControlFrame
  , public nsIAnonymousContentCreator
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  explicit nsGfxButtonControlFrame(nsStyleContext* aContext);

  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;

  virtual nsresult HandleEvent(nsPresContext* aPresContext,
                               mozilla::WidgetGUIEvent* aEvent,
                               nsEventStatus* aEventStatus) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  NS_DECL_QUERYFRAME

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) override;
  virtual void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                        uint32_t aFilter) override;

  virtual nsresult AttributeChanged(int32_t         aNameSpaceID,
                                    nsIAtom*        aAttribute,
                                    int32_t         aModType) override;

  virtual bool IsLeaf() const override;

  virtual nsContainerFrame* GetContentInsertionFrame() override;

protected:
  nsresult GetDefaultLabel(nsXPIDLString& aLabel) const;

  nsresult GetLabel(nsXPIDLString& aLabel);

  virtual bool IsInput() override { return true; }
private:
  nsCOMPtr<nsIContent> mTextContent;
};


#endif

