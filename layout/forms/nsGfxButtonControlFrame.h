/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGfxButtonControlFrame_h___
#define nsGfxButtonControlFrame_h___

#include "nsHTMLButtonControlFrame.h"
#include "nsIAnonymousContentCreator.h"

class nsTextNode;

// Class which implements the input[type=button, reset, submit] and
// browse button for input[type=file].
// The label for button is specified through generated content
// in the ua.css file.

class nsGfxButtonControlFrame final : public nsHTMLButtonControlFrame,
                                      public nsIAnonymousContentCreator {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsGfxButtonControlFrame)

  explicit nsGfxButtonControlFrame(ComputedStyle* aStyle,
                                   nsPresContext* aPresContext);

  void Destroy(DestroyContext&) override;

  nsresult HandleEvent(nsPresContext* aPresContext,
                       mozilla::WidgetGUIEvent* aEvent,
                       nsEventStatus* aEventStatus) override;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override;
#endif

  NS_DECL_QUERYFRAME

  // nsIAnonymousContentCreator
  nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) override;
  void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                uint32_t aFilter) override;

  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) override;

  nsContainerFrame* GetContentInsertionFrame() override { return this; }

 protected:
  nsresult GetDefaultLabel(nsAString& aLabel) const;

  nsresult GetLabel(nsString& aLabel);

 private:
  RefPtr<nsTextNode> mTextContent;
};

#endif
