/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Per-block-formatting-context manager of font size inflation for pan and zoom UI. */

#include "nsFontInflationData.h"
#include "FramePropertyTable.h"
#include "nsTextFragment.h"
#include "nsIFormControlFrame.h"
#include "nsTextControlFrame.h"
#include "nsListControlFrame.h"
#include "nsComboboxControlFrame.h"
#include "nsHTMLReflowState.h"
#include "nsTextFrameUtils.h"

using namespace mozilla;
using namespace mozilla::layout;

static void
DestroyFontInflationData(void *aPropertyValue)
{
  delete static_cast<nsFontInflationData*>(aPropertyValue);
}

NS_DECLARE_FRAME_PROPERTY(FontInflationDataProperty, DestroyFontInflationData);

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

/* static */ void
nsFontInflationData::UpdateFontInflationDataWidthFor(const nsHTMLReflowState& aReflowState)
{
  nsIFrame *bfc = aReflowState.frame;
  NS_ASSERTION(bfc->GetStateBits() & NS_FRAME_FONT_INFLATION_FLOW_ROOT,
               "should have been given a flow root");
  FrameProperties bfcProps(bfc->Properties());
  nsFontInflationData *data = static_cast<nsFontInflationData*>(
                                bfcProps.Get(FontInflationDataProperty()));
  if (!data) {
    data = new nsFontInflationData(bfc);
    bfcProps.Set(FontInflationDataProperty(), data);
  }

  data->UpdateWidth(aReflowState);
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
  aFrame1 = aFrame1->GetFirstInFlow();
  aFrame2 = aFrame2->GetFirstInFlow();
  aKnownCommonAncestor = aKnownCommonAncestor->GetFirstInFlow();

  nsAutoTArray<nsIFrame*, 32> ancestors1, ancestors2;
  for (nsIFrame *f = aFrame1; f != aKnownCommonAncestor;
       (f = f->GetParent()) && (f = f->GetFirstInFlow())) {
    ancestors1.AppendElement(f);
  }
  for (nsIFrame *f = aFrame2; f != aKnownCommonAncestor;
       (f = f->GetParent()) && (f = f->GetFirstInFlow())) {
    ancestors2.AppendElement(f);
  }

  nsIFrame *result = aKnownCommonAncestor;
  PRUint32 i1 = ancestors1.Length(),
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
  nsIFrame *ancestorFrame = aAncestorReflowState.frame->GetFirstInFlow();
  if (aDescendantFrame == ancestorFrame) {
    return aAncestorReflowState.ComputedWidth();
  }

  AutoInfallibleTArray<nsIFrame*, 16> frames;
  for (nsIFrame *f = aDescendantFrame; f != ancestorFrame;
       f = f->GetParent()->GetFirstInFlow()) {
    frames.AppendElement(f);
  }

  PRUint32 len = frames.Length();
  nsHTMLReflowState *reflowStates = static_cast<nsHTMLReflowState*>
                                (moz_xmalloc(sizeof(nsHTMLReflowState) * len));
  nsPresContext *presContext = aDescendantFrame->PresContext();
  for (PRUint32 i = 0; i < len; ++i) {
    const nsHTMLReflowState &parentReflowState =
      (i == 0) ? aAncestorReflowState : reflowStates[i - 1];
    nsSize availSize(parentReflowState.ComputedWidth(), NS_UNCONSTRAINEDSIZE);
    nsIFrame *frame = frames[len - i - 1];
    NS_ABORT_IF_FALSE(frame->GetParent()->GetFirstInFlow() ==
                        parentReflowState.frame->GetFirstInFlow(),
                      "bad logic in this function");
    new (reflowStates + i) nsHTMLReflowState(presContext, parentReflowState,
                                             frame, availSize);
  }

  NS_ABORT_IF_FALSE(reflowStates[len - 1].frame == aDescendantFrame,
                    "bad logic in this function");
  nscoord result = reflowStates[len - 1].ComputedWidth();

  for (PRUint32 i = len; i-- != 0; ) {
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
  NS_ABORT_IF_FALSE(!firstInflatableDescendant == !lastInflatableDescendant,
                    "null-ness should match; NearestCommonAncestorFirstInFlow"
                    " will crash when passed null");

  // Particularly when we're computing for the root BFC, the width of
  // nca might differ significantly for the width of bfc.
  nsIFrame *nca = NearestCommonAncestorFirstInFlow(firstInflatableDescendant,
                                                   lastInflatableDescendant,
                                                   bfc);
  while (!nsLayoutUtils::IsContainerForFontSizeInflation(nca)) {
    nca = nca->GetParent()->GetFirstInFlow();
  }

  nscoord newNCAWidth = ComputeDescendantWidth(aReflowState, nca);

  // See comment above "font.size.inflation.lineThreshold" in
  // modules/libpref/src/init/all.js .
  PRUint32 lineThreshold = nsLayoutUtils::FontSizeInflationLineThreshold();
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
  for (PRUint32 i = 0, len = lists.Length(); i < len; ++i) {
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
          PRUint32 len = nsTextFrameUtils::
            ComputeApproximateLengthWithWhitespaceCompression(
              content, kid->GetStyleText());
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

  return nsnull;
}

void
nsFontInflationData::ScanText()
{
  mTextDirty = false;
  mTextAmount = 0;
  ScanTextIn(mBFCFrame);
  mInflationEnabled = mTextAmount >= mTextThreshold;
}

static PRUint32
DoCharCountOfLargestOption(nsIFrame *aContainer)
{
  PRUint32 result = 0;
  for (nsIFrame* option = aContainer->GetFirstPrincipalChild();
       option; option = option->GetNextSibling()) {
    PRUint32 optionResult;
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
              optionChild->GetContent(), optionChild->GetStyleText());
        }
      }
    }
    if (optionResult > result) {
      result = optionResult;
    }
  }
  return result;
}

static PRUint32
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
          PRUint32 len = nsTextFrameUtils::
            ComputeApproximateLengthWithWhitespaceCompression(
              content, kid->GetStyleText());
          if (len != 0) {
            nscoord fontSize = kid->GetStyleFont()->mFont.size;
            if (fontSize > 0) {
              mTextAmount += fontSize * len;
            }
          }
        }
      } else if (fType == nsGkAtoms::textInputFrame) {
        // We don't want changes to the amount of text in a text input
        // to change what we count towards inflation.
        nscoord fontSize = kid->GetStyleFont()->mFont.size;
        PRInt32 charCount = static_cast<nsTextControlFrame*>(kid)->GetCols();
        mTextAmount += charCount * fontSize;
      } else if (fType == nsGkAtoms::comboboxControlFrame) {
        // See textInputFrame above (with s/amount of text/selected option/).
        // Don't just recurse down to the list control inside, since we
        // need to exclude the display frame.
        nscoord fontSize = kid->GetStyleFont()->mFont.size;
        PRInt32 charCount = CharCountOfLargestOption(
          static_cast<nsComboboxControlFrame*>(kid)->GetDropDown());
        mTextAmount += charCount * fontSize;
      } else if (fType == nsGkAtoms::listControlFrame) {
        // See textInputFrame above (with s/amount of text/selected option/).
        nscoord fontSize = kid->GetStyleFont()->mFont.size;
        PRInt32 charCount = CharCountOfLargestOption(kid);
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
