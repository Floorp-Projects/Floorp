/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSearchControlFrame_h__
#define nsSearchControlFrame_h__

#include "mozilla/Attributes.h"
#include "nsTextControlFrame.h"
#include "nsIAnonymousContentCreator.h"
#include "mozilla/RefPtr.h"

class nsITextControlFrame;
class nsPresContext;

namespace mozilla {
enum class PseudoStyleType : uint8_t;
class PresShell;
namespace dom {
class Element;
}  // namespace dom
}  // namespace mozilla

/**
 * This frame type is used for <input type=search>.
 */
class nsSearchControlFrame final : public nsTextControlFrame {
  friend nsIFrame* NS_NewSearchControlFrame(mozilla::PresShell* aPresShell,
                                            ComputedStyle* aStyle);

  using PseudoStyleType = mozilla::PseudoStyleType;
  using Element = mozilla::dom::Element;

  explicit nsSearchControlFrame(ComputedStyle* aStyle,
                                nsPresContext* aPresContext);

 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsSearchControlFrame)

  void DestroyFrom(nsIFrame* aDestructRoot,
                   PostDestroyData& aPostDestroyData) override;

  // nsIAnonymousContentCreator
  nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) override;
  void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                uint32_t aFilter) override;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"SearchControl"_ns, aResult);
  }
#endif

  Element* GetAnonClearButton() const { return mClearButton; }

  /**
   * Update visbility of the clear button depending on the value
   */
  void UpdateClearButtonState();

 private:
  // See nsSearchControlFrame::CreateAnonymousContent of a description of these
  RefPtr<Element> mOuterWrapper;
  RefPtr<Element> mClearButton;
};

#endif  // nsSearchControlFrame_h__
