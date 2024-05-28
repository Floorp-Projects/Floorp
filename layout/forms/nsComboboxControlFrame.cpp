/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsComboboxControlFrame.h"

#include "gfxContext.h"
#include "gfxUtils.h"
#include "nsCOMPtr.h"
#include "nsDeviceContext.h"
#include "nsFocusManager.h"
#include "nsGkAtoms.h"
#include "nsHTMLParts.h"
#include "nsIFormControl.h"
#include "nsILayoutHistoryState.h"
#include "nsListControlFrame.h"
#include "nsPIDOMWindow.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsISelectControlFrame.h"
#include "nsContentUtils.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/HTMLSelectElement.h"
#include "mozilla/dom/Document.h"
#include "mozilla/ServoStyleSet.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsLayoutUtils.h"
#include "nsDisplayList.h"
#include "nsITheme.h"
#include "nsStyleConsts.h"
#include "nsTextFrameUtils.h"
#include "nsTextRunTransformations.h"
#include "HTMLSelectEventListener.h"
#include "mozilla/Likely.h"
#include <algorithm>
#include "nsTextNode.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/PresShell.h"
#include "mozilla/PresShellInlines.h"

using namespace mozilla;
using namespace mozilla::gfx;

NS_IMETHODIMP
nsComboboxControlFrame::RedisplayTextEvent::Run() {
  if (mControlFrame) mControlFrame->HandleRedisplayTextEvent();
  return NS_OK;
}

// Drop down list event management.
// The combo box uses the following strategy for managing the drop-down list.
// If the combo box or its arrow button is clicked on the drop-down list is
// displayed If mouse exits the combo box with the drop-down list displayed the
// drop-down list is asked to capture events The drop-down list will capture all
// events including mouse down and up and will always return with
// ListWasSelected method call regardless of whether an item in the list was
// actually selected.
// The ListWasSelected code will turn off mouse-capture for the drop-down list.
// The drop-down list does not explicitly set capture when it is in the
// drop-down mode.

nsComboboxControlFrame* NS_NewComboboxControlFrame(PresShell* aPresShell,
                                                   ComputedStyle* aStyle) {
  nsComboboxControlFrame* it = new (aPresShell)
      nsComboboxControlFrame(aStyle, aPresShell->GetPresContext());
  return it;
}

NS_IMPL_FRAMEARENA_HELPERS(nsComboboxControlFrame)

nsComboboxControlFrame::nsComboboxControlFrame(ComputedStyle* aStyle,
                                               nsPresContext* aPresContext)
    : nsHTMLButtonControlFrame(aStyle, aPresContext, kClassID) {}

nsComboboxControlFrame::~nsComboboxControlFrame() = default;

NS_QUERYFRAME_HEAD(nsComboboxControlFrame)
  NS_QUERYFRAME_ENTRY(nsComboboxControlFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
  NS_QUERYFRAME_ENTRY(nsISelectControlFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsHTMLButtonControlFrame)

#ifdef ACCESSIBILITY
a11y::AccType nsComboboxControlFrame::AccessibleType() {
  return a11y::eHTMLComboboxType;
}
#endif

bool nsComboboxControlFrame::HasDropDownButton() const {
  const nsStyleDisplay* disp = StyleDisplay();
  // FIXME(emilio): Blink also shows this for menulist-button and such... Seems
  // more similar to our mac / linux implementation.
  return disp->EffectiveAppearance() == StyleAppearance::Menulist &&
         (!IsThemed(disp) ||
          PresContext()->Theme()->ThemeNeedsComboboxDropmarker());
}

nscoord nsComboboxControlFrame::DropDownButtonISize() {
  if (!HasDropDownButton()) {
    return 0;
  }

  nsPresContext* pc = PresContext();
  LayoutDeviceIntSize dropdownButtonSize = pc->Theme()->GetMinimumWidgetSize(
      pc, this, StyleAppearance::MozMenulistArrowButton);
  return pc->DevPixelsToAppUnits(dropdownButtonSize.width);
}

int32_t nsComboboxControlFrame::CharCountOfLargestOptionForInflation() const {
  uint32_t maxLength = 0;
  nsAutoString label;
  for (auto i : IntegerRange(Select().Options()->Length())) {
    GetOptionText(i, label);
    maxLength = std::max(
        maxLength,
        nsTextFrameUtils::ComputeApproximateLengthWithWhitespaceCompression(
            label, StyleText()));
  }
  if (MOZ_UNLIKELY(maxLength > uint32_t(INT32_MAX))) {
    return INT32_MAX;
  }
  return int32_t(maxLength);
}

nscoord nsComboboxControlFrame::GetLongestOptionISize(
    gfxContext* aRenderingContext) const {
  // Compute the width of each option's (potentially text-transformed) text,
  // and use the widest one as part of our intrinsic size.
  nscoord maxOptionSize = 0;
  nsAutoString label;
  nsAutoString transformedLabel;
  RefPtr<nsFontMetrics> fm =
      nsLayoutUtils::GetInflatedFontMetricsForFrame(this);
  const nsStyleText* textStyle = StyleText();
  auto textTransform = textStyle->mTextTransform.IsNone()
                           ? Nothing()
                           : Some(textStyle->mTextTransform);
  nsAtom* language = StyleFont()->mLanguage;
  AutoTArray<bool, 50> charsToMergeArray;
  AutoTArray<bool, 50> deletedCharsArray;
  for (auto i : IntegerRange(Select().Options()->Length())) {
    GetOptionText(i, label);
    const nsAutoString* stringToUse = &label;
    if (textTransform ||
        textStyle->mWebkitTextSecurity != StyleTextSecurity::None) {
      transformedLabel.Truncate();
      charsToMergeArray.SetLengthAndRetainStorage(0);
      deletedCharsArray.SetLengthAndRetainStorage(0);
      nsCaseTransformTextRunFactory::TransformString(
          label, transformedLabel, textTransform,
          textStyle->TextSecurityMaskChar(),
          /* aCaseTransformsOnly = */ false, language, charsToMergeArray,
          deletedCharsArray);
      stringToUse = &transformedLabel;
    }
    maxOptionSize = std::max(maxOptionSize,
                             nsLayoutUtils::AppUnitWidthOfStringBidi(
                                 *stringToUse, this, *fm, *aRenderingContext));
  }
  if (maxOptionSize) {
    // HACK: Add one app unit to workaround silly Netgear router styling, see
    // bug 1769580. In practice since this comes from font metrics is unlikely
    // to be perceivable.
    maxOptionSize += 1;
  }
  return maxOptionSize;
}

nscoord nsComboboxControlFrame::GetIntrinsicISize(gfxContext* aRenderingContext,
                                                  IntrinsicISizeType aType) {
  Maybe<nscoord> containISize = ContainIntrinsicISize(NS_UNCONSTRAINEDSIZE);
  if (containISize && *containISize != NS_UNCONSTRAINEDSIZE) {
    return *containISize;
  }

  nscoord displayISize = 0;
  if (!containISize && !StyleContent()->mContent.IsNone()) {
    displayISize += GetLongestOptionISize(aRenderingContext);
  }

  // Add room for the dropmarker button (if there is one).
  displayISize += DropDownButtonISize();
  return displayISize;
}

nscoord nsComboboxControlFrame::GetMinISize(gfxContext* aRenderingContext) {
  nscoord minISize;
  minISize = GetIntrinsicISize(aRenderingContext, IntrinsicISizeType::MinISize);
  return minISize;
}

nscoord nsComboboxControlFrame::GetPrefISize(gfxContext* aRenderingContext) {
  nscoord prefISize;
  prefISize =
      GetIntrinsicISize(aRenderingContext, IntrinsicISizeType::PrefISize);
  return prefISize;
}

dom::HTMLSelectElement& nsComboboxControlFrame::Select() const {
  return *static_cast<dom::HTMLSelectElement*>(GetContent());
}

void nsComboboxControlFrame::GetOptionText(uint32_t aIndex,
                                           nsAString& aText) const {
  aText.Truncate();
  if (Element* el = Select().Options()->GetElementAt(aIndex)) {
    static_cast<dom::HTMLOptionElement*>(el)->GetRenderedLabel(aText);
  }
}

void nsComboboxControlFrame::Reflow(nsPresContext* aPresContext,
                                    ReflowOutput& aDesiredSize,
                                    const ReflowInput& aReflowInput,
                                    nsReflowStatus& aStatus) {
  MarkInReflow();
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");
  // Constraints we try to satisfy:

  // 1) Default inline size of button is the vertical scrollbar size
  // 2) If the inline size of button is bigger than our inline size, set
  //    inline size of button to 0.
  // 3) Default block size of button is block size of display area
  // 4) Inline size of display area is whatever is left over from our
  //    inline size after allocating inline size for the button.
  // Make sure the displayed text is the same as the selected option,
  // bug 297389.
  mDisplayedIndex = Select().SelectedIndex();

  // In dropped down mode the "selected index" is the hovered menu item,
  // we want the last selected item which is |mDisplayedIndex| in this case.
  RedisplayText();

  WritingMode wm = aReflowInput.GetWritingMode();

  // Check if the theme specifies a minimum size for the dropdown button
  // first.
  const nscoord buttonISize = DropDownButtonISize();
  const auto padding = aReflowInput.ComputedLogicalPadding(wm);

  // We ignore inline-end-padding (by adding it to our label box size) if we
  // have a dropdown button, so that the button aligns with the end of the
  // padding box.
  mDisplayISize = aReflowInput.ComputedISize() - buttonISize;
  if (buttonISize) {
    mDisplayISize += padding.IEnd(wm);
  }

  nsHTMLButtonControlFrame::Reflow(aPresContext, aDesiredSize, aReflowInput,
                                   aStatus);
}

void nsComboboxControlFrame::Init(nsIContent* aContent,
                                  nsContainerFrame* aParent,
                                  nsIFrame* aPrevInFlow) {
  nsHTMLButtonControlFrame::Init(aContent, aParent, aPrevInFlow);

  mEventListener = new HTMLSelectEventListener(
      Select(), HTMLSelectEventListener::SelectType::Combobox);
}

nsresult nsComboboxControlFrame::RedisplaySelectedText() {
  nsAutoScriptBlocker scriptBlocker;
  mDisplayedIndex = Select().SelectedIndex();
  return RedisplayText();
}

nsresult nsComboboxControlFrame::RedisplayText() {
  nsString previewValue;
  nsString previousText(mDisplayedOptionTextOrPreview);

  Select().GetPreviewValue(previewValue);
  // Get the text to display
  if (!previewValue.IsEmpty()) {
    mDisplayedOptionTextOrPreview = previewValue;
  } else if (mDisplayedIndex != -1 && !StyleContent()->mContent.IsNone()) {
    GetOptionText(mDisplayedIndex, mDisplayedOptionTextOrPreview);
  } else {
    mDisplayedOptionTextOrPreview.Truncate();
  }

  // Send reflow command because the new text maybe larger
  nsresult rv = NS_OK;
  if (!previousText.Equals(mDisplayedOptionTextOrPreview)) {
    // Don't call ActuallyDisplayText(true) directly here since that could cause
    // recursive frame construction. See bug 283117 and the comment in
    // HandleRedisplayTextEvent() below.

    // Revoke outstanding events to avoid out-of-order events which could mean
    // displaying the wrong text.
    mRedisplayTextEvent.Revoke();

    NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
                 "If we happen to run our redisplay event now, we might kill "
                 "ourselves!");
    mRedisplayTextEvent = new RedisplayTextEvent(this);
    nsContentUtils::AddScriptRunner(mRedisplayTextEvent.get());
  }
  return rv;
}

void nsComboboxControlFrame::HandleRedisplayTextEvent() {
  // First, make sure that the content model is up to date and we've constructed
  // the frames for all our content in the right places. Otherwise they'll end
  // up under the wrong insertion frame when we ActuallyDisplayText, since that
  // flushes out the content sink by calling SetText on a DOM node with aNotify
  // set to true.  See bug 289730.
  AutoWeakFrame weakThis(this);
  PresContext()->Document()->FlushPendingNotifications(
      FlushType::ContentAndNotify);
  if (!weakThis.IsAlive()) {
    return;
  }
  mRedisplayTextEvent.Forget();
  ActuallyDisplayText(true);
  // Note: `this` might be dead here.
}

void nsComboboxControlFrame::ActuallyDisplayText(bool aNotify) {
  RefPtr<dom::Text> displayContent = mDisplayLabel->GetFirstChild()->AsText();
  // Have to use a space character of some sort for line-block-size calculations
  // to be right. Also, the space character must be zero-width in order for the
  // inline-size calculations to be consistent between size-contained comboboxes
  // vs. empty comboboxes.
  //
  // XXXdholbert Does this space need to be "non-breaking"? I'm not sure if it
  // matters, but we previously had a comment here (added in 2002) saying "Have
  // to use a non-breaking space for line-height calculations to be right". So
  // I'll stick with a non-breaking space for now...
  displayContent->SetText(mDisplayedOptionTextOrPreview.IsEmpty()
                              ? u"\ufeff"_ns
                              : mDisplayedOptionTextOrPreview,
                          aNotify);
}

bool nsComboboxControlFrame::IsDroppedDown() const {
  return Select().OpenInParentProcess();
}

//----------------------------------------------------------------------
// nsISelectControlFrame
//----------------------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::DoneAddingChildren(bool aIsDone) { return NS_OK; }

NS_IMETHODIMP
nsComboboxControlFrame::AddOption(int32_t aIndex) {
  if (aIndex <= mDisplayedIndex) {
    ++mDisplayedIndex;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsComboboxControlFrame::RemoveOption(int32_t aIndex) {
  if (Select().Options()->Length()) {
    if (aIndex < mDisplayedIndex) {
      --mDisplayedIndex;
    } else if (aIndex == mDisplayedIndex) {
      mDisplayedIndex = 0;  // IE6 compat
      RedisplayText();
    }
  } else {
    // If we removed the last option, we need to blank things out
    mDisplayedIndex = -1;
    RedisplayText();
  }
  return NS_OK;
}

NS_IMETHODIMP_(void)
nsComboboxControlFrame::OnSetSelectedIndex(int32_t aOldIndex,
                                           int32_t aNewIndex) {
  nsAutoScriptBlocker scriptBlocker;
  mDisplayedIndex = aNewIndex;
  RedisplayText();
}

// End nsISelectControlFrame
//----------------------------------------------------------------------

nsresult nsComboboxControlFrame::HandleEvent(nsPresContext* aPresContext,
                                             WidgetGUIEvent* aEvent,
                                             nsEventStatus* aEventStatus) {
  NS_ENSURE_ARG_POINTER(aEventStatus);

  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  return nsHTMLButtonControlFrame::HandleEvent(aPresContext, aEvent,
                                               aEventStatus);
}

nsresult nsComboboxControlFrame::CreateAnonymousContent(
    nsTArray<ContentInfo>& aElements) {
  dom::Document* doc = mContent->OwnerDoc();
  mDisplayLabel = doc->CreateHTMLElement(nsGkAtoms::label);

  {
    RefPtr<nsTextNode> text = doc->CreateEmptyTextNode();
    mDisplayLabel->AppendChildTo(text, false, IgnoreErrors());
  }

  // set the value of the text node
  mDisplayedIndex = Select().SelectedIndex();
  if (mDisplayedIndex != -1) {
    GetOptionText(mDisplayedIndex, mDisplayedOptionTextOrPreview);
  }
  ActuallyDisplayText(false);

  aElements.AppendElement(mDisplayLabel);
  if (HasDropDownButton()) {
    mButtonContent = mContent->OwnerDoc()->CreateHTMLElement(nsGkAtoms::button);
    {
      // This gives the button a reasonable height. This could be done via CSS
      // instead, but relative font units like 1lh don't play very well with our
      // font inflation implementation, so we do it this way instead.
      RefPtr<nsTextNode> text = doc->CreateTextNode(u"\ufeff"_ns);
      mButtonContent->AppendChildTo(text, false, IgnoreErrors());
    }
    // Make someone to listen to the button.
    mButtonContent->SetAttr(kNameSpaceID_None, nsGkAtoms::type, u"button"_ns,
                            false);
    // Set tabindex="-1" so that the button is not tabbable
    mButtonContent->SetAttr(kNameSpaceID_None, nsGkAtoms::tabindex, u"-1"_ns,
                            false);
    aElements.AppendElement(mButtonContent);
  }

  return NS_OK;
}

void nsComboboxControlFrame::AppendAnonymousContentTo(
    nsTArray<nsIContent*>& aElements, uint32_t aFilter) {
  if (mDisplayLabel) {
    aElements.AppendElement(mDisplayLabel);
  }

  if (mButtonContent) {
    aElements.AppendElement(mButtonContent);
  }
}

namespace mozilla {

class ComboboxLabelFrame final : public nsBlockFrame {
 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(ComboboxLabelFrame)

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const final {
    return MakeFrameName(u"ComboboxLabel"_ns, aResult);
  }
#endif

  void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
              const ReflowInput& aReflowInput, nsReflowStatus& aStatus) final;

 public:
  ComboboxLabelFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : nsBlockFrame(aStyle, aPresContext, kClassID) {}
};

NS_QUERYFRAME_HEAD(ComboboxLabelFrame)
  NS_QUERYFRAME_ENTRY(ComboboxLabelFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBlockFrame)
NS_IMPL_FRAMEARENA_HELPERS(ComboboxLabelFrame)

void ComboboxLabelFrame::Reflow(nsPresContext* aPresContext,
                                ReflowOutput& aDesiredSize,
                                const ReflowInput& aReflowInput,
                                nsReflowStatus& aStatus) {
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  const nsComboboxControlFrame* combobox =
      do_QueryFrame(GetParent()->GetParent());
  MOZ_ASSERT(combobox, "Combobox's frame tree is wrong!");
  MOZ_ASSERT(aReflowInput.ComputedPhysicalBorderPadding() == nsMargin(),
             "We shouldn't have border and padding in UA!");

  ReflowInput state(aReflowInput);
  state.SetComputedISize(combobox->mDisplayISize);
  nsBlockFrame::Reflow(aPresContext, aDesiredSize, state, aStatus);
  aStatus.Reset();  // this type of frame can't be split
}

}  // namespace mozilla

nsIFrame* NS_NewComboboxLabelFrame(PresShell* aPresShell,
                                   ComputedStyle* aStyle) {
  return new (aPresShell)
      ComboboxLabelFrame(aStyle, aPresShell->GetPresContext());
}

void nsComboboxControlFrame::Destroy(DestroyContext& aContext) {
  // Revoke any pending RedisplayTextEvent
  mRedisplayTextEvent.Revoke();
  mEventListener->Detach();

  aContext.AddAnonymousContent(mDisplayLabel.forget());
  aContext.AddAnonymousContent(mButtonContent.forget());
  nsHTMLButtonControlFrame::Destroy(aContext);
}

//---------------------------------------------------------
// gets the content (an option) by index and then set it as
// being selected or not selected
//---------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::OnOptionSelected(int32_t aIndex, bool aSelected) {
  if (aSelected) {
    nsAutoScriptBlocker blocker;
    mDisplayedIndex = aIndex;
    RedisplayText();
  } else {
    AutoWeakFrame weakFrame(this);
    RedisplaySelectedText();
    if (weakFrame.IsAlive()) {
      FireValueChangeEvent();  // Fire after old option is unselected
    }
  }
  return NS_OK;
}

void nsComboboxControlFrame::FireValueChangeEvent() {
  // Fire ValueChange event to indicate data value of combo box has changed
  // FIXME(emilio): This shouldn't be exposed to content.
  nsContentUtils::AddScriptRunner(new AsyncEventDispatcher(
      mContent, u"ValueChange"_ns, CanBubble::eYes, ChromeOnlyDispatch::eNo));
}
