/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the font size inflation manager.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Per-block-formatting-context manager of font size inflation for pan and zoom UI. */

#include "nsFontInflationData.h"
#include "FramePropertyTable.h"
#include "nsTextFragment.h"
#include "nsIFormControlFrame.h"
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
       f = f->GetParent()) {
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
    nca = nca->GetParent();
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

      if (kid->GetType() == nsGkAtoms::textFrame) {
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
