/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsComboboxControlFrame.h"

#include "gfxContext.h"
#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/PathHelpers.h"
#include "nsCOMPtr.h"
#include "nsDeviceContext.h"
#include "nsFocusManager.h"
#include "nsCheckboxRadioFrame.h"
#include "nsGkAtoms.h"
#include "nsCSSAnonBoxes.h"
#include "nsHTMLParts.h"
#include "nsIFormControl.h"
#include "nsILayoutHistoryState.h"
#include "nsNameSpaceManager.h"
#include "nsListControlFrame.h"
#include "nsPIDOMWindow.h"
#include "mozilla/PresState.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsIContentInlines.h"
#include "nsIDOMEventListener.h"
#include "nsISelectControlFrame.h"
#include "nsContentUtils.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/HTMLSelectElement.h"
#include "mozilla/dom/Document.h"
#include "nsIScrollableFrame.h"
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
#include "mozilla/MouseEvents.h"
#include "mozilla/PresShell.h"
#include "mozilla/PresShellInlines.h"
#include "mozilla/Unused.h"
#include "gfx2DGlue.h"
#include "mozilla/widget/nsAutoRollup.h"

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

  if (it) {
    it->AddStateBits(NS_BLOCK_STATIC_BFC);
  }

  return it;
}

NS_IMPL_FRAMEARENA_HELPERS(nsComboboxControlFrame)

//-----------------------------------------------------------
// Reflow Debugging Macros
// These let us "see" how many reflow counts are happening
//-----------------------------------------------------------
#ifdef DO_REFLOW_COUNTER

#  define MAX_REFLOW_CNT 1024
static int32_t gTotalReqs = 0;
;
static int32_t gTotalReflows = 0;
;
static int32_t gReflowControlCntRQ[MAX_REFLOW_CNT];
static int32_t gReflowControlCnt[MAX_REFLOW_CNT];
static int32_t gReflowInx = -1;

#  define REFLOW_COUNTER() \
    if (mReflowId > -1) gReflowControlCnt[mReflowId]++;

#  define REFLOW_COUNTER_REQUEST() \
    if (mReflowId > -1) gReflowControlCntRQ[mReflowId]++;

#  define REFLOW_COUNTER_DUMP(__desc)                                   \
    if (mReflowId > -1) {                                               \
      gTotalReqs += gReflowControlCntRQ[mReflowId];                     \
      gTotalReflows += gReflowControlCnt[mReflowId];                    \
      printf("** Id:%5d %s RF: %d RQ: %d   %d/%d  %5.2f\n", mReflowId,  \
             (__desc), gReflowControlCnt[mReflowId],                    \
             gReflowControlCntRQ[mReflowId], gTotalReflows, gTotalReqs, \
             float(gTotalReflows) / float(gTotalReqs) * 100.0f);        \
    }

#  define REFLOW_COUNTER_INIT()           \
    if (gReflowInx < MAX_REFLOW_CNT) {    \
      gReflowInx++;                       \
      mReflowId = gReflowInx;             \
      gReflowControlCnt[mReflowId] = 0;   \
      gReflowControlCntRQ[mReflowId] = 0; \
    } else {                              \
      mReflowId = -1;                     \
    }

// reflow messages
#  define REFLOW_DEBUG_MSG(_msg1) printf((_msg1))
#  define REFLOW_DEBUG_MSG2(_msg1, _msg2) printf((_msg1), (_msg2))
#  define REFLOW_DEBUG_MSG3(_msg1, _msg2, _msg3) \
    printf((_msg1), (_msg2), (_msg3))
#  define REFLOW_DEBUG_MSG4(_msg1, _msg2, _msg3, _msg4) \
    printf((_msg1), (_msg2), (_msg3), (_msg4))

#else  //-------------

#  define REFLOW_COUNTER_REQUEST()
#  define REFLOW_COUNTER()
#  define REFLOW_COUNTER_DUMP(__desc)
#  define REFLOW_COUNTER_INIT()

#  define REFLOW_DEBUG_MSG(_msg)
#  define REFLOW_DEBUG_MSG2(_msg1, _msg2)
#  define REFLOW_DEBUG_MSG3(_msg1, _msg2, _msg3)
#  define REFLOW_DEBUG_MSG4(_msg1, _msg2, _msg3, _msg4)

#endif

//------------------------------------------
// This is for being VERY noisy
//------------------------------------------
#ifdef DO_VERY_NOISY
#  define REFLOW_NOISY_MSG(_msg1) printf((_msg1))
#  define REFLOW_NOISY_MSG2(_msg1, _msg2) printf((_msg1), (_msg2))
#  define REFLOW_NOISY_MSG3(_msg1, _msg2, _msg3) \
    printf((_msg1), (_msg2), (_msg3))
#  define REFLOW_NOISY_MSG4(_msg1, _msg2, _msg3, _msg4) \
    printf((_msg1), (_msg2), (_msg3), (_msg4))
#else
#  define REFLOW_NOISY_MSG(_msg)
#  define REFLOW_NOISY_MSG2(_msg1, _msg2)
#  define REFLOW_NOISY_MSG3(_msg1, _msg2, _msg3)
#  define REFLOW_NOISY_MSG4(_msg1, _msg2, _msg3, _msg4)
#endif

//------------------------------------------
// Displays value in pixels or twips
//------------------------------------------
#ifdef DO_PIXELS
#  define PX(__v) __v / 15
#else
#  define PX(__v) __v
#endif

//------------------------------------------------------
//-- Done with macros
//------------------------------------------------------

nsComboboxControlFrame::nsComboboxControlFrame(ComputedStyle* aStyle,
                                               nsPresContext* aPresContext)
    : nsBlockFrame(aStyle, aPresContext, kClassID),
      mDisplayFrame(nullptr),
      mButtonFrame(nullptr),
      mDisplayISize(0),
      mMaxDisplayISize(0),
      mRecentSelectedIndex(NS_SKIP_NOTIFY_INDEX),
      mDisplayedIndex(-1),
      mInRedisplayText(false),
      mIsOpenInParentProcess(false){REFLOW_COUNTER_INIT()}

      //--------------------------------------------------------------
      nsComboboxControlFrame::~nsComboboxControlFrame() {
  REFLOW_COUNTER_DUMP("nsCCF");
}

//--------------------------------------------------------------

NS_QUERYFRAME_HEAD(nsComboboxControlFrame)
  NS_QUERYFRAME_ENTRY(nsComboboxControlFrame)
  NS_QUERYFRAME_ENTRY(nsIFormControlFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
  NS_QUERYFRAME_ENTRY(nsISelectControlFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBlockFrame)

#ifdef ACCESSIBILITY
a11y::AccType nsComboboxControlFrame::AccessibleType() {
  return a11y::eHTMLComboboxType;
}
#endif

void nsComboboxControlFrame::SetFocus(bool aOn, bool aRepaint) {
  // This is needed on a temporary basis. It causes the focus
  // rect to be drawn. This is much faster than ReResolvingStyle
  // Bug 32920
  InvalidateFrame();
}

nsPoint nsComboboxControlFrame::GetCSSTransformTranslation() {
  nsIFrame* frame = this;
  bool is3DTransform = false;
  Matrix transform;
  while (frame) {
    nsIFrame* parent;
    Matrix4x4Flagged ctm = frame->GetTransformMatrix(
        ViewportType::Layout, RelativeTo{nullptr}, &parent);
    Matrix matrix;
    if (ctm.Is2D(&matrix)) {
      transform = transform * matrix;
    } else {
      is3DTransform = true;
      break;
    }
    frame = parent;
  }
  nsPoint translation;
  if (!is3DTransform && !transform.HasNonTranslation()) {
    nsPresContext* pc = PresContext();
    // To get the translation introduced only by transforms we subtract the
    // regular non-transform translation.
    nsRootPresContext* rootPC = pc->GetRootPresContext();
    if (rootPC) {
      int32_t apd = pc->AppUnitsPerDevPixel();
      translation.x = NSFloatPixelsToAppUnits(transform._31, apd);
      translation.y = NSFloatPixelsToAppUnits(transform._32, apd);
      translation -= GetOffsetToCrossDoc(rootPC->PresShell()->GetRootFrame());
    }
  }
  return translation;
}

//----------------------------------------------------------
//
//----------------------------------------------------------
#ifdef DO_REFLOW_DEBUG
static int myCounter = 0;

static void printSize(char* aDesc, nscoord aSize) {
  printf(" %s: ", aDesc);
  if (aSize == NS_UNCONSTRAINEDSIZE) {
    printf("UC");
  } else {
    printf("%d", PX(aSize));
  }
}
#endif

//-------------------------------------------------------------------
//-- Main Reflow for the Combobox
//-------------------------------------------------------------------

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

  nscoord displayISize = mDisplayFrame->IntrinsicISizeOffsets().padding;
  if (!containISize && !StyleContent()->mContent.IsNone()) {
    displayISize += GetLongestOptionISize(aRenderingContext);
  }

  // Add room for the dropmarker button (if there is one).
  displayISize += DropDownButtonISize();
  return displayISize;
}

nscoord nsComboboxControlFrame::GetMinISize(gfxContext* aRenderingContext) {
  nscoord minISize;
  DISPLAY_MIN_INLINE_SIZE(this, minISize);
  minISize = GetIntrinsicISize(aRenderingContext, IntrinsicISizeType::MinISize);
  return minISize;
}

nscoord nsComboboxControlFrame::GetPrefISize(gfxContext* aRenderingContext) {
  nscoord prefISize;
  DISPLAY_PREF_INLINE_SIZE(this, prefISize);
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

  if (!mDisplayFrame) {
    NS_ERROR("Why did the frame constructor allow this to happen?  Fix it!!");
    return;
  }

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
  const auto borderPadding = aReflowInput.ComputedLogicalBorderPadding(wm);
  const auto padding = aReflowInput.ComputedLogicalPadding(wm);
  const auto border = borderPadding - padding;

  mDisplayISize = aReflowInput.ComputedISize() - buttonISize;
  mMaxDisplayISize = mDisplayISize + padding.IEnd(wm);

  nsBlockFrame::Reflow(aPresContext, aDesiredSize, aReflowInput, aStatus);

  // The button should occupy the same space as a scrollbar, and its position
  // starts from the border edge.
  if (mButtonFrame) {
    LogicalRect buttonRect(wm);
    buttonRect.IStart(wm) = borderPadding.IStart(wm) + mMaxDisplayISize;
    buttonRect.BStart(wm) = border.BStart(wm);

    buttonRect.ISize(wm) = buttonISize;
    buttonRect.BSize(wm) = mDisplayFrame->BSize(wm) + padding.BStartEnd(wm);

    const nsSize containerSize = aDesiredSize.PhysicalSize();
    mButtonFrame->SetRect(buttonRect, containerSize);
  }

  if (!aStatus.IsInlineBreakBefore() && !aStatus.IsFullyComplete()) {
    // This frame didn't fit inside a fragmentation container.  Splitting
    // a nsComboboxControlFrame makes no sense, so we override the status here.
    aStatus.Reset();
  }
}

void nsComboboxControlFrame::Init(nsIContent* aContent,
                                  nsContainerFrame* aParent,
                                  nsIFrame* aPrevInFlow) {
  nsBlockFrame::Init(aContent, aParent, aPrevInFlow);

  mEventListener = new HTMLSelectEventListener(
      Select(), HTMLSelectEventListener::SelectType::Combobox);
}

#ifdef DEBUG_FRAME_DUMP
nsresult nsComboboxControlFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(u"ComboboxControl"_ns, aResult);
}
#endif

///////////////////////////////////////////////////////////////

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

  REFLOW_DEBUG_MSG2(
      "RedisplayText \"%s\"\n",
      NS_LossyConvertUTF16toASCII(mDisplayedOptionTextOrPreview).get());

  // Send reflow command because the new text maybe larger
  nsresult rv = NS_OK;
  if (mDisplayContent && !previousText.Equals(mDisplayedOptionTextOrPreview)) {
    // Don't call ActuallyDisplayText(true) directly here since that
    // could cause recursive frame construction. See bug 283117 and the comment
    // in HandleRedisplayTextEvent() below.

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
  // First, make sure that the content model is up to date and we've
  // constructed the frames for all our content in the right places.
  // Otherwise they'll end up under the wrong insertion frame when we
  // ActuallyDisplayText, since that flushes out the content sink by
  // calling SetText on a DOM node with aNotify set to true.  See bug
  // 289730.
  AutoWeakFrame weakThis(this);
  PresContext()->Document()->FlushPendingNotifications(
      FlushType::ContentAndNotify);
  if (!weakThis.IsAlive()) return;

  // Redirect frame insertions during this method (see
  // GetContentInsertionFrame()) so that any reframing that the frame
  // constructor forces upon us is inserted into the correct parent
  // (mDisplayFrame). See bug 282607.
  MOZ_ASSERT(!mInRedisplayText, "Nested RedisplayText");
  mInRedisplayText = true;
  mRedisplayTextEvent.Forget();

  ActuallyDisplayText(true);
  if (!weakThis.IsAlive()) {
    return;
  }

  // XXXbz This should perhaps be IntrinsicDirty::None. Check.
  PresShell()->FrameNeedsReflow(mDisplayFrame,
                                IntrinsicDirty::FrameAncestorsAndDescendants,
                                NS_FRAME_IS_DIRTY);

  mInRedisplayText = false;
}

void nsComboboxControlFrame::ActuallyDisplayText(bool aNotify) {
  RefPtr<nsTextNode> displayContent = mDisplayContent;
  if (mDisplayedOptionTextOrPreview.IsEmpty()) {
    // Have to use a space character of some sort for line-block-size
    // calculations to be right. Also, the space character must be zero-width
    // in order for the the inline-size calculations to be consistent between
    // size-contained comboboxes vs. empty comboboxes.
    //
    // XXXdholbert Does this space need to be "non-breaking"? I'm not sure
    // if it matters, but we previously had a comment here (added in 2002)
    // saying "Have to use a non-breaking space for line-height calculations
    // to be right". So I'll stick with a non-breaking space for now...
    static const char16_t space = 0xFEFF;
    displayContent->SetText(&space, 1, aNotify);
  } else {
    displayContent->SetText(mDisplayedOptionTextOrPreview, aNotify);
  }
}

int32_t nsComboboxControlFrame::GetIndexOfDisplayArea() {
  return mDisplayedIndex;
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

  if (mContent->AsElement()->State().HasState(dom::ElementState::DISABLED)) {
    return NS_OK;
  }

  // If we have style that affects how we are selected, feed event down to
  // nsIFrame::HandleEvent so that selection takes place when appropriate.
  if (IsContentDisabled()) {
    return nsBlockFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
  }
  return NS_OK;
}

nsContainerFrame* nsComboboxControlFrame::GetContentInsertionFrame() {
  return mInRedisplayText ? mDisplayFrame : nullptr;
}

void nsComboboxControlFrame::AppendDirectlyOwnedAnonBoxes(
    nsTArray<OwnedAnonBox>& aResult) {
  aResult.AppendElement(OwnedAnonBox(mDisplayFrame));
}

nsresult nsComboboxControlFrame::CreateAnonymousContent(
    nsTArray<ContentInfo>& aElements) {
  // The frames used to display the combo box and the button used to popup the
  // dropdown list are created through anonymous content. The dropdown list is
  // not created through anonymous content because its frame is initialized
  // specifically for the drop-down case and it is placed a special list
  // referenced through NS_COMBO_FRAME_POPUP_LIST_INDEX to keep separate from
  // the layout of the display and button.
  //
  // Note: The value attribute of the display content is set when an item is
  // selected in the dropdown list. If the content specified below does not
  // honor the value attribute than nothing will be displayed.

  // For now the content that is created corresponds to two input buttons. It
  // would be better to create the tag as something other than input, but then
  // there isn't any way to create a button frame since it isn't possible to set
  // the display type in CSS2 to create a button frame.

  // create content used for display
  // nsAtom* tag = NS_Atomize("mozcombodisplay");

  // Add a child text content node for the label

  nsNodeInfoManager* nimgr = mContent->NodeInfo()->NodeInfoManager();

  mDisplayContent = new (nimgr) nsTextNode(nimgr);

  // set the value of the text node
  mDisplayedIndex = Select().SelectedIndex();
  if (mDisplayedIndex != -1) {
    GetOptionText(mDisplayedIndex, mDisplayedOptionTextOrPreview);
  }
  ActuallyDisplayText(false);

  aElements.AppendElement(mDisplayContent);
  if (HasDropDownButton()) {
    mButtonContent = mContent->OwnerDoc()->CreateHTMLElement(nsGkAtoms::button);
    if (!mButtonContent) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // make someone to listen to the button.
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
  if (mDisplayContent) {
    aElements.AppendElement(mDisplayContent);
  }

  if (mButtonContent) {
    aElements.AppendElement(mButtonContent);
  }
}

nsIContent* nsComboboxControlFrame::GetDisplayNode() const {
  return mDisplayContent;
}

// XXXbz this is a for-now hack.  Now that display:inline-block works,
// need to revisit this.
class nsComboboxDisplayFrame final : public nsBlockFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsComboboxDisplayFrame)

  nsComboboxDisplayFrame(ComputedStyle* aStyle,
                         nsComboboxControlFrame* aComboBox)
      : nsBlockFrame(aStyle, aComboBox->PresContext(), kClassID),
        mComboBox(aComboBox) {}

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const final {
    return MakeFrameName(u"ComboboxDisplay"_ns, aResult);
  }
#endif

  void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
              const ReflowInput& aReflowInput, nsReflowStatus& aStatus) final;

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) final;

 protected:
  nsComboboxControlFrame* mComboBox;
};

NS_IMPL_FRAMEARENA_HELPERS(nsComboboxDisplayFrame)

void nsComboboxDisplayFrame::Reflow(nsPresContext* aPresContext,
                                    ReflowOutput& aDesiredSize,
                                    const ReflowInput& aReflowInput,
                                    nsReflowStatus& aStatus) {
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");
  MOZ_ASSERT(aReflowInput.mParentReflowInput &&
                 aReflowInput.mParentReflowInput->mFrame == mComboBox,
             "Combobox's frame tree is wrong!");

  ReflowInput state(aReflowInput);
  if (state.ComputedBSize() == NS_UNCONSTRAINEDSIZE) {
    state.SetLineHeight(state.mParentReflowInput->GetLineHeight());
  }
  const WritingMode wm = aReflowInput.GetWritingMode();
  const LogicalMargin bp = state.ComputedLogicalBorderPadding(wm);
  MOZ_ASSERT(bp.BStartEnd(wm) == 0,
             "We shouldn't have border and padding in the block axis in UA!");
  nscoord inlineBp = bp.IStartEnd(wm);
  nscoord computedISize = mComboBox->mDisplayISize - inlineBp;

  // Other UAs ignore padding in some (but not all) platforms for (themed only)
  // comboboxes. Instead of doing that, we prevent that padding if present from
  // clipping the display text, by enforcing the display text minimum size in
  // that situation.
  const bool shouldHonorMinISize =
      mComboBox->StyleDisplay()->EffectiveAppearance() ==
      StyleAppearance::Menulist;
  if (shouldHonorMinISize) {
    computedISize = std::max(state.ComputedMinISize(), computedISize);
    // Don't let this size go over mMaxDisplayISize, since that'd be
    // observable via clientWidth / scrollWidth.
    computedISize =
        std::min(computedISize, mComboBox->mMaxDisplayISize - inlineBp);
  }

  state.SetComputedISize(std::max(0, computedISize));
  nsBlockFrame::Reflow(aPresContext, aDesiredSize, state, aStatus);
  aStatus.Reset();  // this type of frame can't be split
}

void nsComboboxDisplayFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                              const nsDisplayListSet& aLists) {
  nsDisplayListCollection set(aBuilder);
  nsBlockFrame::BuildDisplayList(aBuilder, set);

  // remove background items if parent frame is themed
  if (mComboBox->IsThemed()) {
    set.BorderBackground()->DeleteAll(aBuilder);
  }

  set.MoveTo(aLists);
}

nsIFrame* nsComboboxControlFrame::CreateFrameForDisplayNode() {
  MOZ_ASSERT(mDisplayContent);

  // Get PresShell
  mozilla::PresShell* ps = PresShell();
  ServoStyleSet* styleSet = ps->StyleSet();

  // create the ComputedStyle for the anonymous block frame and text frame
  RefPtr<ComputedStyle> computedStyle =
      styleSet->ResolveInheritingAnonymousBoxStyle(
          PseudoStyleType::mozDisplayComboboxControlFrame, mComputedStyle);

  RefPtr<ComputedStyle> textComputedStyle =
      styleSet->ResolveStyleForText(mDisplayContent, mComputedStyle);

  // Start by creating our anonymous block frame
  mDisplayFrame = new (ps) nsComboboxDisplayFrame(computedStyle, this);
  mDisplayFrame->Init(mContent, this, nullptr);

  // Create a text frame and put it inside the block frame
  nsIFrame* textFrame = NS_NewTextFrame(ps, textComputedStyle);

  // initialize the text frame
  textFrame->Init(mDisplayContent, mDisplayFrame, nullptr);
  mDisplayContent->SetPrimaryFrame(textFrame);

  mDisplayFrame->SetInitialChildList(FrameChildListID::Principal,
                                     nsFrameList(textFrame, textFrame));
  return mDisplayFrame;
}

void nsComboboxControlFrame::Destroy(DestroyContext& aContext) {
  // Revoke any pending RedisplayTextEvent
  mRedisplayTextEvent.Revoke();

  mEventListener->Detach();

  // Cleanup frames in popup child list
  aContext.AddAnonymousContent(mDisplayContent.forget());
  aContext.AddAnonymousContent(mButtonContent.forget());
  nsBlockFrame::Destroy(aContext);
}

const nsFrameList& nsComboboxControlFrame::GetChildList(
    ChildListID aListID) const {
  return nsBlockFrame::GetChildList(aListID);
}

void nsComboboxControlFrame::GetChildLists(nsTArray<ChildList>* aLists) const {
  nsBlockFrame::GetChildLists(aLists);
}

void nsComboboxControlFrame::SetInitialChildList(ChildListID aListID,
                                                 nsFrameList&& aChildList) {
  for (nsIFrame* f : aChildList) {
    MOZ_ASSERT(f->GetParent() == this, "Unexpected parent");
    nsCOMPtr<nsIFormControl> formControl = do_QueryInterface(f->GetContent());
    if (formControl &&
        formControl->ControlType() == FormControlType::ButtonButton) {
      mButtonFrame = f;
      break;
    }
  }
  nsBlockFrame::SetInitialChildList(aListID, std::move(aChildList));
}

namespace mozilla {

class nsDisplayComboboxFocus : public nsPaintedDisplayItem {
 public:
  nsDisplayComboboxFocus(nsDisplayListBuilder* aBuilder,
                         nsComboboxControlFrame* aFrame)
      : nsPaintedDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayComboboxFocus);
  }
  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayComboboxFocus)

  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;
  NS_DISPLAY_DECL_NAME("ComboboxFocus", TYPE_COMBOBOX_FOCUS)
};

void nsDisplayComboboxFocus::Paint(nsDisplayListBuilder* aBuilder,
                                   gfxContext* aCtx) {
  static_cast<nsComboboxControlFrame*>(mFrame)->PaintFocus(
      *aCtx->GetDrawTarget(), ToReferenceFrame());
}

}  // namespace mozilla

void nsComboboxControlFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                              const nsDisplayListSet& aLists) {
  if (aBuilder->IsForEventDelivery()) {
    // Don't allow children to receive events.
    // REVIEW: following old GetFrameForPoint
    DisplayBorderBackgroundOutline(aBuilder, aLists);
  } else {
    // REVIEW: Our in-flow child frames are inline-level so they will paint in
    // our content list, so we don't need to mess with layers.
    nsBlockFrame::BuildDisplayList(aBuilder, aLists);
  }

  // draw a focus indicator only when focus rings should be drawn
  if (Select().State().HasState(dom::ElementState::FOCUSRING) && IsThemed() &&
      PresContext()->Theme()->ThemeWantsButtonInnerFocusRing()) {
    aLists.Content()->AppendNewToTop<nsDisplayComboboxFocus>(aBuilder, this);
  }

  DisplaySelectionOverlay(aBuilder, aLists.Content());
}

void nsComboboxControlFrame::PaintFocus(DrawTarget& aDrawTarget, nsPoint aPt) {
  /* Do we need to do anything? */
  dom::ElementState state = mContent->AsElement()->State();
  if (state.HasState(dom::ElementState::DISABLED) ||
      !state.HasState(dom::ElementState::FOCUS)) {
    return;
  }

  int32_t appUnitsPerDevPixel = PresContext()->AppUnitsPerDevPixel();

  nsRect clipRect = mDisplayFrame->GetRect() + aPt;
  aDrawTarget.PushClipRect(
      NSRectToSnappedRect(clipRect, appUnitsPerDevPixel, aDrawTarget));

  StrokeOptions strokeOptions;
  nsLayoutUtils::InitDashPattern(strokeOptions, StyleBorderStyle::Dotted);
  ColorPattern color(ToDeviceColor(StyleText()->mColor));
  nscoord onePixel = nsPresContext::CSSPixelsToAppUnits(1);
  clipRect.width -= onePixel;
  clipRect.height -= onePixel;
  Rect r = ToRect(nsLayoutUtils::RectToGfxRect(clipRect, appUnitsPerDevPixel));
  StrokeSnappedEdgesOfRect(r, aDrawTarget, color, strokeOptions);

  aDrawTarget.PopClip();
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
  nsContentUtils::AddScriptRunner(new AsyncEventDispatcher(
      mContent, u"ValueChange"_ns, CanBubble::eYes, ChromeOnlyDispatch::eNo));
}
