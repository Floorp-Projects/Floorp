/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Per-block-formatting-context manager of font size inflation for pan and zoom UI. */

#include "nsFontInflationData.h"
#include "FramePropertyTable.h"
#include "nsTextControlFrame.h"
#include "nsListControlFrame.h"
#include "nsComboboxControlFrame.h"
#include "nsHTMLReflowState.h"
#include "nsTextFrameUtils.h"

using namespace mozilla;
using namespace mozilla::layout;

NS_DECLARE_FRAME_PROPERTY(FontInflationDataProperty,
                          DeleteValue<nsFontInflationData>)

/* static */ nsFontInflationData*
nsFontInflationData::FindFontInflationDataFor(const nsIFrame *aFrame)
{
  // We have one set of font inflation data per block formatting context.
  const nsIFrame *bfc = FlowRootFor(aFrame);
  NS_ASSERTION(bfc->GetStateBits() & NS_FRAME_FONT_INFLATION_FLOW_ROOT,
               "should have found a flow root");

  return static_cast<nsFontInflationData*>(
             bfc->Properties().Get(FontInflationDataProperty()));
}

/* static */ bool
nsFontInflationData::UpdateFontInflationDataWidthFor(const nsHTMLReflowState& aReflowState)
{
  nsIFrame *bfc = aReflowState.frame;
  NS_ASSERTION(bfc->GetStateBits() & NS_FRAME_FONT_INFLATION_FLOW_ROOT,
               "should have been given a flow root");
  FrameProperties bfcProps(bfc->Properties());
  nsFontInflationData *data = static_cast<nsFontInflationData*>(
                                bfcProps.Get(FontInflationDataProperty()));
  bool oldInflationEnabled;
  nscoord oldNCAWidth;
  if (data) {
    oldNCAWidth = data->mNCAWidth;
    oldInflationEnabled = data->mInflationEnabled;
  } else {
    data = new nsFontInflationData(bfc);
    bfcProps.Set(FontInflationDataProperty(), data);
    oldNCAWidth = -1;
    oldInflationEnabled = true; /* not relevant */
  }

  data->UpdateWidth(aReflowState);

  if (oldInflationEnabled != data->mInflationEnabled)
    return true;

  return oldInflationEnabled &&
         oldNCAWidth != data->mNCAWidth;
}

/* static */ void
nsFontInflationData::MarkFontInflationDataTextDirty(nsIFrame *aBFCFrame)
{
  NS_ASSERTION(aBFCFrame->GetStateBits() & NS_FRAME_FONT_INFLATION_FLOW_ROOT,
               "should have been given a flow root");

  FrameProperties bfcProps(aBFCFrame->Properties());
  nsFontInflationData *data = static_cast<nsFontInflationData*>(
                                bfcProps.Get(FontInflationDataProperty()));
  if (data) {
    data->MarkTextDirty();
  }
}

nsFontInflationData::nsFontInflationData(nsIFrame *aBFCFrame)
  : mBFCFrame(aBFCFrame)
  , mNCAWidth(0)
  , mTextAmount(0)
  , mTextThreshold(0)
  , mInflationEnabled(false)
  , mTextDirty(true)
{
}

/**
 * Find the closest common ancestor between aFrame1 and aFrame2, except
 * treating the parent of a frame as the first-in-flow of its parent (so
 * the result doesn't change when breaking changes).
 *
 * aKnownCommonAncestor is a known common ancestor of both.
 */
static nsIFrame*
NearestCommonAncestorFirstInFlow(nsIFrame *aFrame1, nsIFrame *aFrame2,
                                 nsIFrame *aKnownCommonAncestor)
{
  aFrame1 = aFrame1->FirstInFlow();
  aFrame2 = aFrame2->FirstInFlow();
  aKnownCommonAncestor = aKnownCommonAncestor->FirstInFlow();

  nsAutoTArray<nsIFrame*, 32> ancestors1, ancestors2;
  for (nsIFrame *f = aFrame1; f != aKnownCommonAncestor;
       (f = f->GetParent()) && (f = f->FirstInFlow())) {
    ancestors1.AppendElement(f);
  }
  for (nsIFrame *f = aFrame2; f != aKnownCommonAncestor;
       (f = f->GetParent()) && (f = f->FirstInFlow())) {
    ancestors2.AppendElement(f);
  }

  nsIFrame *result = aKnownCommonAncestor;
  uint32_t i1 = ancestors1.Length(),
           i2 = ancestors2.Length();
  while (i1-- != 0 && i2-- != 0) {
    if (ancestors1[i1] != ancestors2[i2]) {
      break;
    }
    result = ancestors1[i1];
  }

  return result;
}

static nscoord
ComputeDescendantWidth(const nsHTMLReflowState& aAncestorReflowState,
                       nsIFrame *aDescendantFrame)
{
  nsIFrame *ancestorFrame = aAncestorReflowState.frame->FirstInFlow();
  if (aDescendantFrame == ancestorFrame) {
    return aAncestorReflowState.ComputedWidth();
  }

  AutoInfallibleTArray<nsIFrame*, 16> frames;
  for (nsIFrame *f = aDescendantFrame; f != ancestorFrame;
       f = f->GetParent()->FirstInFlow()) {
    frames.AppendElement(f);
  }

  // This ignores the width contributions made by scrollbars, though in
  // reality we don't have any scrollbars on the sorts of devices on
  // which we use font inflation, so it's not a problem.  But it may
  // occasionally cause problems when writing tests on desktop.

  uint32_t len = frames.Length();
  nsHTMLReflowState *reflowStates = static_cast<nsHTMLReflowState*>
                                (moz_xmalloc(sizeof(nsHTMLReflowState) * len));
  nsPresContext *presContext = aDescendantFrame->PresContext();
  for (uint32_t i = 0; i < len; ++i) {
    const nsHTMLReflowState &parentReflowState =
      (i == 0) ? aAncestorReflowState : reflowStates[i - 1];
    nsIFrame *frame = frames[len - i - 1];
    WritingMode wm = frame->GetWritingMode();
    LogicalSize availSize = parentReflowState.ComputedSize(wm);
    availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;
    MOZ_ASSERT(frame->GetParent()->FirstInFlow() ==
                 parentReflowState.frame->FirstInFlow(),
               "bad logic in this function");
    new (reflowStates + i) nsHTMLReflowState(presContext, parentReflowState,
                                             frame, availSize);
  }

  MOZ_ASSERT(reflowStates[len - 1].frame == aDescendantFrame,
             "bad logic in this function");
  nscoord result = reflowStates[len - 1].ComputedWidth();

  for (uint32_t i = len; i-- != 0; ) {
    reflowStates[i].~nsHTMLReflowState();
  }
  moz_free(reflowStates);

  return result;
}

void
nsFontInflationData::UpdateWidth(const nsHTMLReflowState &aReflowState)
{
  nsIFrame *bfc = aReflowState.frame;
  NS_ASSERTION(bfc->GetStateBits() & NS_FRAME_FONT_INFLATION_FLOW_ROOT,
               "must be block formatting context");

  nsIFrame *firstInflatableDescendant =
             FindEdgeInflatableFrameIn(bfc, eFromStart);
  if (!firstInflatableDescendant) {
    mTextAmount = 0;
    mTextThreshold = 0; // doesn't matter
    mTextDirty = false;
    mInflationEnabled = false;
    return;
  }
  nsIFrame *lastInflatableDescendant =
             FindEdgeInflatableFrameIn(bfc, eFromEnd);
  MOZ_ASSERT(!firstInflatableDescendant == !lastInflatableDescendant,
             "null-ness should match; NearestCommonAncestorFirstInFlow"
             " will crash when passed null");

  // Particularly when we're computing for the root BFC, the width of
  // nca might differ significantly for the width of bfc.
  nsIFrame *nca = NearestCommonAncestorFirstInFlow(firstInflatableDescendant,
                                                   lastInflatableDescendant,
                                                   bfc);
  while (!nca->IsContainerForFontSizeInflation()) {
    nca = nca->GetParent()->FirstInFlow();
  }

  nscoord newNCAWidth = ComputeDescendantWidth(aReflowState, nca);

  // See comment above "font.size.inflation.lineThreshold" in
  // modules/libpref/src/init/all.js .
  nsIPresShell* presShell = bfc->PresContext()->PresShell();
  uint32_t lineThreshold = presShell->FontSizeInflationLineThreshold();
  nscoord newTextThreshold = (newNCAWidth * lineThreshold) / 100;

  if (mTextThreshold <= mTextAmount && mTextAmount < newTextThreshold) {
    // Because we truncate our scan when we hit sufficient text, we now
    // need to rescan.
    mTextDirty = true;
  }

  mNCAWidth = newNCAWidth;
  mTextThreshold = newTextThreshold;
  mInflationEnabled = mTextAmount >= mTextThreshold;
}

/* static */ nsIFrame*
nsFontInflationData::FindEdgeInflatableFrameIn(nsIFrame* aFrame,
                                               SearchDirection aDirection)
{
  // NOTE: This function has a similar structure to ScanTextIn!

  // FIXME: Should probably only scan the text that's actually going to
  // be inflated!

  nsIFormControlFrame* fcf = do_QueryFrame(aFrame);
  if (fcf) {
    return aFrame;
  }

  // FIXME: aDirection!
  nsAutoTArray<FrameChildList, 4> lists;
  aFrame->GetChildLists(&lists);
  for (uint32_t i = 0, len = lists.Length(); i < len; ++i) {
    const nsFrameList& list =
      lists[(aDirection == eFromStart) ? i : len - i - 1].mList;
    for (nsIFrame *kid = (aDirection == eFromStart) ? list.FirstChild()
                                                    : list.LastChild();
         kid;
         kid = (aDirection == eFromStart) ? kid->GetNextSibling()
                                          : kid->GetPrevSibling()) {
      if (kid->GetStateBits() & NS_FRAME_FONT_INFLATION_FLOW_ROOT) {
        // Goes in a different set of inflation data.
        continue;
      }

      if (kid->GetType() == nsGkAtoms::textFrame) {
        nsIContent *content = kid->GetContent();
        if (content && kid == content->GetPrimaryFrame()) {
          uint32_t len = nsTextFrameUtils::
            ComputeApproximateLengthWithWhitespaceCompression(
              content, kid->StyleText());
          if (len != 0) {
            return kid;
          }
        }
      } else {
        nsIFrame *kidResult =
          FindEdgeInflatableFrameIn(kid, aDirection);
        if (kidResult) {
          return kidResult;
        }
      }
    }
  }

  return nullptr;
}

void
nsFontInflationData::ScanText()
{
  mTextDirty = false;
  mTextAmount = 0;
  ScanTextIn(mBFCFrame);
  mInflationEnabled = mTextAmount >= mTextThreshold;
}

static uint32_t
DoCharCountOfLargestOption(nsIFrame *aContainer)
{
  uint32_t result = 0;
  for (nsIFrame* option = aContainer->GetFirstPrincipalChild();
       option; option = option->GetNextSibling()) {
    uint32_t optionResult;
    if (option->GetContent()->IsHTML(nsGkAtoms::optgroup)) {
      optionResult = DoCharCountOfLargestOption(option);
    } else {
      // REVIEW: Check the frame structure for this!
      optionResult = 0;
      for (nsIFrame *optionChild = option->GetFirstPrincipalChild();
           optionChild; optionChild = optionChild->GetNextSibling()) {
        if (optionChild->GetType() == nsGkAtoms::textFrame) {
          optionResult += nsTextFrameUtils::
            ComputeApproximateLengthWithWhitespaceCompression(
              optionChild->GetContent(), optionChild->StyleText());
        }
      }
    }
    if (optionResult > result) {
      result = optionResult;
    }
  }
  return result;
}

static uint32_t
CharCountOfLargestOption(nsIFrame *aListControlFrame)
{
  return DoCharCountOfLargestOption(
    static_cast<nsListControlFrame*>(aListControlFrame)->GetOptionsContainer());
}

void
nsFontInflationData::ScanTextIn(nsIFrame *aFrame)
{
  // NOTE: This function has a similar structure to FindEdgeInflatableFrameIn!

  // FIXME: Should probably only scan the text that's actually going to
  // be inflated!

  nsIFrame::ChildListIterator lists(aFrame);
  for (; !lists.IsDone(); lists.Next()) {
    nsFrameList::Enumerator kids(lists.CurrentList());
    for (; !kids.AtEnd(); kids.Next()) {
      nsIFrame *kid = kids.get();
      if (kid->GetStateBits() & NS_FRAME_FONT_INFLATION_FLOW_ROOT) {
        // Goes in a different set of inflation data.
        continue;
      }

      nsIAtom *fType = kid->GetType();
      if (fType == nsGkAtoms::textFrame) {
        nsIContent *content = kid->GetContent();
        if (content && kid == content->GetPrimaryFrame()) {
          uint32_t len = nsTextFrameUtils::
            ComputeApproximateLengthWithWhitespaceCompression(
              content, kid->StyleText());
          if (len != 0) {
            nscoord fontSize = kid->StyleFont()->mFont.size;
            if (fontSize > 0) {
              mTextAmount += fontSize * len;
            }
          }
        }
      } else if (fType == nsGkAtoms::textInputFrame) {
        // We don't want changes to the amount of text in a text input
        // to change what we count towards inflation.
        nscoord fontSize = kid->StyleFont()->mFont.size;
        int32_t charCount = static_cast<nsTextControlFrame*>(kid)->GetCols();
        mTextAmount += charCount * fontSize;
      } else if (fType == nsGkAtoms::comboboxControlFrame) {
        // See textInputFrame above (with s/amount of text/selected option/).
        // Don't just recurse down to the list control inside, since we
        // need to exclude the display frame.
        nscoord fontSize = kid->StyleFont()->mFont.size;
        int32_t charCount = CharCountOfLargestOption(
          static_cast<nsComboboxControlFrame*>(kid)->GetDropDown());
        mTextAmount += charCount * fontSize;
      } else if (fType == nsGkAtoms::listControlFrame) {
        // See textInputFrame above (with s/amount of text/selected option/).
        nscoord fontSize = kid->StyleFont()->mFont.size;
        int32_t charCount = CharCountOfLargestOption(kid);
        mTextAmount += charCount * fontSize;
      } else {
        // recursive step
        ScanTextIn(kid);
      }

      if (mTextAmount >= mTextThreshold) {
        return;
      }
    }
  }
}
