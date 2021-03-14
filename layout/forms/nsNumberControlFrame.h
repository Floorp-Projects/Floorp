/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNumberControlFrame_h__
#define nsNumberControlFrame_h__

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"
#include "nsTextControlFrame.h"
#include "nsIFormControlFrame.h"
#include "nsIAnonymousContentCreator.h"
#include "nsCOMPtr.h"

class nsITextControlFrame;
class nsPresContext;

namespace mozilla {
enum class PseudoStyleType : uint8_t;
class PresShell;
class WidgetEvent;
class WidgetGUIEvent;
namespace dom {
class HTMLInputElement;
}  // namespace dom
}  // namespace mozilla

/**
 * This frame type is used for <input type=number>.
 *
 * TODO(emilio): Maybe merge with nsTextControlFrame?
 */
class nsNumberControlFrame final : public nsTextControlFrame {
  friend nsIFrame* NS_NewNumberControlFrame(mozilla::PresShell* aPresShell,
                                            ComputedStyle* aStyle);

  typedef mozilla::PseudoStyleType PseudoStyleType;
  typedef mozilla::dom::Element Element;
  typedef mozilla::dom::HTMLInputElement HTMLInputElement;
  typedef mozilla::WidgetEvent WidgetEvent;
  typedef mozilla::WidgetGUIEvent WidgetGUIEvent;

  explicit nsNumberControlFrame(ComputedStyle* aStyle,
                                nsPresContext* aPresContext);

 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsNumberControlFrame)

  void DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData&) override;

#ifdef ACCESSIBILITY
  mozilla::a11y::AccType AccessibleType() override;
#endif

  // nsIAnonymousContentCreator
  nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) override;
  void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                uint32_t aFilter) override;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"NumberControl"_ns, aResult);
  }
#endif

  /**
   * If the frame is the frame for an nsNumberControlFrame's anonymous text
   * field, returns the nsNumberControlFrame. Else returns nullptr.
   */
  static nsNumberControlFrame* GetNumberControlFrameForTextField(
      nsIFrame* aFrame);

  /**
   * If the frame is the frame for an nsNumberControlFrame's up or down spin
   * button, returns the nsNumberControlFrame. Else returns nullptr.
   */
  static nsNumberControlFrame* GetNumberControlFrameForSpinButton(
      nsIFrame* aFrame);

  enum SpinButtonEnum { eSpinButtonNone, eSpinButtonUp, eSpinButtonDown };

  /**
   * Returns one of the SpinButtonEnum values to depending on whether the
   * pointer event is over the spin-up button, the spin-down button, or
   * neither.
   */
  int32_t GetSpinButtonForPointerEvent(WidgetGUIEvent* aEvent) const;

  void SpinnerStateChanged() const;

  bool SpinnerUpButtonIsDepressed() const;
  bool SpinnerDownButtonIsDepressed() const;

  /**
   * Our element had HTMLInputElement::Select() called on it.
   */
  void HandleSelectCall();

  bool ShouldUseNativeStyleForSpinner() const;

 private:
  // See nsNumberControlFrame::CreateAnonymousContent for a description of
  // these.
  nsCOMPtr<Element> mSpinBox;
  nsCOMPtr<Element> mSpinUp;
  nsCOMPtr<Element> mSpinDown;
};

#endif  // nsNumberControlFrame_h__
