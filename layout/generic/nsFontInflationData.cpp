/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Per-block-formatting-context manager of font size inflation for pan and zoom
 * UI. */

#include "nsFontInflationData.h"
#include "FrameProperties.h"
#include "nsTextControlFrame.h"
#include "nsListControlFrame.h"
#include "nsComboboxControlFrame.h"
#include "mozilla/PresShell.h"
#include "mozilla/ReflowInput.h"
#include "nsTextFrameUtils.h"

using namespace mozilla;
using namespace mozilla::layout;

NS_DECLARE_FRAME_PROPERTY_DELETABLE(FontInflationDataProperty,
                                    nsFontInflationData)

/* static */ nsFontInflationData* nsFontInflationData::FindFontInflationDataFor(
    const nsIFrame* aFrame) {
  // We have one set of font inflation data per block formatting context.
  const nsIFrame* bfc = FlowRootFor(aFrame);
  NS_ASSERTION(bfc->GetStateBits() & NS_FRAME_FONT_INFLATION_FLOW_ROOT,
               "should have found a flow root");
  MOZ_ASSERT(aFrame->GetWritingMode().IsVertical() ==
                 bfc->GetWritingMode().IsVertical(),
             "current writing mode should match that of our flow root");

  return bfc->GetProperty(FontInflationDataProperty());
}

/* static */
bool nsFontInflationData::UpdateFontInflationDataISizeFor(
    const ReflowInput& aReflowInput) {
  nsIFrame* bfc = aReflowInput.mFrame;
  NS_ASSERTION(bfc->GetStateBits() & NS_FRAME_FONT_INFLATION_FLOW_ROOT,
               "should have been given a flow root");
  nsFontInflationData* data = bfc->GetProperty(FontInflationDataProperty());
  bool oldInflationEnabled;
  nscoord oldUsableISize;
  if (data) {
    oldUsableISize = data->mUsableISize;
    oldInflationEnabled = data->mInflationEnabled;
  } else {
    data = new nsFontInflationData(bfc);
    bfc->SetProperty(FontInflationDataProperty(), data);
    oldUsableISize = -1;
    oldInflationEnabled = true; /* not relevant */
  }

  data->UpdateISize(aReflowInput);

  if (oldInflationEnabled != data->mInflationEnabled) return true;

  return oldInflationEnabled && oldUsableISize != data->mUsableISize;
}

/* static */
void nsFontInflationData::MarkFontInflationDataTextDirty(nsIFrame* aBFCFrame) {
  NS_ASSERTION(aBFCFrame->GetStateBits() & NS_FRAME_FONT_INFLATION_FLOW_ROOT,
               "should have been given a flow root");

  nsFontInflationData* data =
      aBFCFrame->GetProperty(FontInflationDataProperty());
  if (data) {
    data->MarkTextDirty();
  }
}

nsFontInflationData::nsFontInflationData(nsIFrame* aBFCFrame)
    : mBFCFrame(aBFCFrame),
      mUsableISize(0),
      mTextAmount(0),
      mTextThreshold(0),
      mInflationEnabled(false),
      mTextDirty(true) {}

/**
 * Find the closest common ancestor between aFrame1 and aFrame2, except
 * treating the parent of a frame as the first-in-flow of its parent (so
 * the result doesn't change when breaking changes).
 *
 * aKnownCommonAncestor is a known common ancestor of both.
 */
static nsIFrame* NearestCommonAncestorFirstInFlow(
    nsIFrame* aFrame1, nsIFrame* aFrame2, nsIFrame* aKnownCommonAncestor) {
  aFrame1 = aFrame1->FirstInFlow();
  aFrame2 = aFrame2->FirstInFlow();
  aKnownCommonAncestor = aKnownCommonAncestor->FirstInFlow();

  AutoTArray<nsIFrame*, 32> ancestors1, ancestors2;
  for (nsIFrame* f = aFrame1; f != aKnownCommonAncestor;
       (f = f->GetParent()) && (f = f->FirstInFlow())) {
    ancestors1.AppendElement(f);
  }
  for (nsIFrame* f = aFrame2; f != aKnownCommonAncestor;
       (f = f->GetParent()) && (f = f->FirstInFlow())) {
    ancestors2.AppendElement(f);
  }

  nsIFrame* result = aKnownCommonAncestor;
  uint32_t i1 = ancestors1.Length(), i2 = ancestors2.Length();
  while (i1-- != 0 && i2-- != 0) {
    if (ancestors1[i1] != ancestors2[i2]) {
      break;
    }
    result = ancestors1[i1];
  }

  return result;
}

static nscoord ComputeDescendantISize(const ReflowInput& aAncestorReflowInput,
                                      nsIFrame* aDescendantFrame) {
  nsIFrame* ancestorFrame = aAncestorReflowInput.mFrame->FirstInFlow();
  if (aDescendantFrame == ancestorFrame) {
    return aAncestorReflowInput.ComputedISize();
  }

  AutoTArray<nsIFrame*, 16> frames;
  for (nsIFrame* f = aDescendantFrame; f != ancestorFrame;
       f = f->GetParent()->FirstInFlow()) {
    frames.AppendElement(f);
  }

  // This ignores the inline-size contributions made by scrollbars, though in
  // reality we don't have any scrollbars on the sorts of devices on
  // which we use font inflation, so it's not a problem.  But it may
  // occasionally cause problems when writing tests on desktop.

  uint32_t len = frames.Length();
  ReflowInput* reflowInputs =
      static_cast<ReflowInput*>(moz_xmalloc(sizeof(ReflowInput) * len));
  nsPresContext* presContext = aDescendantFrame->PresContext();
  for (uint32_t i = 0; i < len; ++i) {
    const ReflowInput& parentReflowInput =
        (i == 0) ? aAncestorReflowInput : reflowInputs[i - 1];
    nsIFrame* frame = frames[len - i - 1];
    WritingMode wm = frame->GetWritingMode();
    LogicalSize availSize = parentReflowInput.ComputedSize(wm);
    availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;
    MOZ_ASSERT(frame->GetParent()->FirstInFlow() ==
                   parentReflowInput.mFrame->FirstInFlow(),
               "bad logic in this function");
    new (reflowInputs + i)
        ReflowInput(presContext, parentReflowInput, frame, availSize);
  }

  MOZ_ASSERT(reflowInputs[len - 1].mFrame == aDescendantFrame,
             "bad logic in this function");
  nscoord result = reflowInputs[len - 1].ComputedISize();

  for (uint32_t i = len; i-- != 0;) {
    reflowInputs[i].~ReflowInput();
  }
  free(reflowInputs);

  return result;
}

void nsFontInflationData::UpdateISize(const ReflowInput& aReflowInput) {
  nsIFrame* bfc = aReflowInput.mFrame;
  NS_ASSERTION(bfc->GetStateBits() & NS_FRAME_FONT_INFLATION_FLOW_ROOT,
               "must be block formatting context");

  nsIFrame* firstInflatableDescendant =
      FindEdgeInflatableFrameIn(bfc, eFromStart);
  if (!firstInflatableDescendant) {
    mTextAmount = 0;
    mTextThreshold = 0;  // doesn't matter
    mTextDirty = false;
    mInflationEnabled = false;
    return;
  }
  nsIFrame* lastInflatableDescendant = FindEdgeInflatableFrameIn(bfc, eFromEnd);
  MOZ_ASSERT(!firstInflatableDescendant == !lastInflatableDescendant,
             "null-ness should match; NearestCommonAncestorFirstInFlow"
             " will crash when passed null");

  // Particularly when we're computing for the root BFC, the inline-size of
  // nca might differ significantly for the inline-size of bfc.
  nsIFrame* nca = NearestCommonAncestorFirstInFlow(
      firstInflatableDescendant, lastInflatableDescendant, bfc);
  while (!nca->IsContainerForFontSizeInflation()) {
    nca = nca->GetParent()->FirstInFlow();
  }

  nscoord newNCAISize = ComputeDescendantISize(aReflowInput, nca);

  // See comment above "font.size.inflation.lineThreshold" in
  // modules/libpref/src/init/all.js .
  PresShell* presShell = bfc->PresShell();
  uint32_t lineThreshold = presShell->FontSizeInflationLineThreshold();
  nscoord newTextThreshold = (newNCAISize * lineThreshold) / 100;

  if (mTextThreshold <= mTextAmount && mTextAmount < newTextThreshold) {
    // Because we truncate our scan when we hit sufficient text, we now
    // need to rescan.
    mTextDirty = true;
  }

  // Font inflation increases the font size for a given flow root so that the
  // text is legible when we've zoomed such that the respective nearest common
  // ancestor's (NCA) full inline-size (ISize) fills the screen. We assume how-
  // ever that we don't want to zoom out further than the root iframe's ISize
  // (i.e. the viewport for a top-level document, or the containing iframe
  // otherwise), since in some cases zooming out further might not even be
  // possible or make sense.
  // Hence the ISize assumed to be usable for displaying text is limited to the
  // visible area.
  nsPresContext* presContext = bfc->PresContext();
  MOZ_ASSERT(
      bfc->GetWritingMode().IsVertical() == nca->GetWritingMode().IsVertical(),
      "writing mode of NCA should match that of its flow root");
  nscoord iFrameISize = bfc->GetWritingMode().IsVertical()
                            ? presContext->GetVisibleArea().height
                            : presContext->GetVisibleArea().width;
  mUsableISize = std::min(iFrameISize, newNCAISize);
  mTextThreshold = newTextThreshold;
  mInflationEnabled = mTextAmount >= mTextThreshold;
}

/* static */ nsIFrame* nsFontInflationData::FindEdgeInflatableFrameIn(
    nsIFrame* aFrame, SearchDirection aDirection) {
  // NOTE: This function has a similar structure to ScanTextIn!

  // FIXME: Should probably only scan the text that's actually going to
  // be inflated!

  nsIFormControlFrame* fcf = do_QueryFrame(aFrame);
  if (fcf) {
    return aFrame;
  }

  // FIXME: aDirection!
  AutoTArray<FrameChildList, 4> lists;
  aFrame->GetChildLists(&lists);
  for (uint32_t i = 0, len = lists.Length(); i < len; ++i) {
    const nsFrameList& list =
        lists[(aDirection == eFromStart) ? i : len - i - 1].mList;
    for (nsIFrame* kid = (aDirection == eFromStart) ? list.FirstChild()
                                                    : list.LastChild();
         kid; kid = (aDirection == eFromStart) ? kid->GetNextSibling()
                                               : kid->GetPrevSibling()) {
      if (kid->GetStateBits() & NS_FRAME_FONT_INFLATION_FLOW_ROOT) {
        // Goes in a different set of inflation data.
        continue;
      }

      if (kid->IsTextFrame()) {
        nsIContent* content = kid->GetContent();
        if (content && kid == content->GetPrimaryFrame()) {
          uint32_t len = nsTextFrameUtils::
              ComputeApproximateLengthWithWhitespaceCompression(
                  content, kid->StyleText());
          if (len != 0) {
            return kid;
          }
        }
      } else {
        nsIFrame* kidResult = FindEdgeInflatableFrameIn(kid, aDirection);
        if (kidResult) {
          return kidResult;
        }
      }
    }
  }

  return nullptr;
}

void nsFontInflationData::ScanText() {
  mTextDirty = false;
  mTextAmount = 0;
  ScanTextIn(mBFCFrame);
  mInflationEnabled = mTextAmount >= mTextThreshold;
}

static uint32_t DoCharCountOfLargestOption(nsIFrame* aContainer) {
  uint32_t result = 0;
  for (nsIFrame* option : aContainer->PrincipalChildList()) {
    uint32_t optionResult;
    if (option->GetContent()->IsHTMLElement(nsGkAtoms::optgroup)) {
      optionResult = DoCharCountOfLargestOption(option);
    } else {
      // REVIEW: Check the frame structure for this!
      optionResult = 0;
      for (nsIFrame* optionChild : option->PrincipalChildList()) {
        if (optionChild->IsTextFrame()) {
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

static uint32_t CharCountOfLargestOption(nsIFrame* aListControlFrame) {
  return DoCharCountOfLargestOption(
      static_cast<nsListControlFrame*>(aListControlFrame)
          ->GetOptionsContainer());
}

void nsFontInflationData::ScanTextIn(nsIFrame* aFrame) {
  // NOTE: This function has a similar structure to FindEdgeInflatableFrameIn!

  // FIXME: Should probably only scan the text that's actually going to
  // be inflated!

  nsIFrame::ChildListIterator lists(aFrame);
  for (; !lists.IsDone(); lists.Next()) {
    nsFrameList::Enumerator kids(lists.CurrentList());
    for (; !kids.AtEnd(); kids.Next()) {
      nsIFrame* kid = kids.get();
      if (kid->GetStateBits() & NS_FRAME_FONT_INFLATION_FLOW_ROOT) {
        // Goes in a different set of inflation data.
        continue;
      }

      LayoutFrameType fType = kid->Type();
      if (fType == LayoutFrameType::Text) {
        nsIContent* content = kid->GetContent();
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
      } else if (fType == LayoutFrameType::TextInput) {
        // We don't want changes to the amount of text in a text input
        // to change what we count towards inflation.
        nscoord fontSize = kid->StyleFont()->mFont.size;
        int32_t charCount = static_cast<nsTextControlFrame*>(kid)->GetCols();
        mTextAmount += charCount * fontSize;
      } else if (fType == LayoutFrameType::ComboboxControl) {
        // See textInputFrame above (with s/amount of text/selected option/).
        // Don't just recurse down to the list control inside, since we
        // need to exclude the display frame.
        nscoord fontSize = kid->StyleFont()->mFont.size;
        int32_t charCount = CharCountOfLargestOption(
            static_cast<nsComboboxControlFrame*>(kid)->GetDropDown());
        mTextAmount += charCount * fontSize;
      } else if (fType == LayoutFrameType::ListControl) {
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
