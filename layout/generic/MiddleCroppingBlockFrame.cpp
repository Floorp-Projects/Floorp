/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MiddleCroppingBlockFrame.h"
#include "nsTextFrame.h"
#include "nsLayoutUtils.h"
#include "nsTextNode.h"
#include "nsLineLayout.h"
#include "gfxContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/intl/Segmenter.h"
#include "mozilla/ReflowInput.h"
#include "mozilla/ReflowOutput.h"

namespace mozilla {

NS_QUERYFRAME_HEAD(MiddleCroppingBlockFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
  NS_QUERYFRAME_ENTRY(MiddleCroppingBlockFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBlockFrame)

MiddleCroppingBlockFrame::MiddleCroppingBlockFrame(ComputedStyle* aStyle,
                                                   nsPresContext* aPresContext,
                                                   ClassID aClassID)
    : nsBlockFrame(aStyle, aPresContext, aClassID) {}

MiddleCroppingBlockFrame::~MiddleCroppingBlockFrame() = default;

void MiddleCroppingBlockFrame::UpdateDisplayedValue(const nsAString& aValue,
                                                    bool aIsCropped,
                                                    bool aNotify) {
  auto* text = mTextNode.get();
  uint32_t oldLength = aNotify ? 0 : text->TextLength();
  text->SetText(aValue, aNotify);
  if (!aNotify) {
    // We can't notify during Reflow so we need to tell the text frame about the
    // text content change we just did.
    if (auto* textFrame = static_cast<nsTextFrame*>(text->GetPrimaryFrame())) {
      textFrame->NotifyNativeAnonymousTextnodeChange(oldLength);
    }
    if (LinesBegin() != LinesEnd()) {
      AddStateBits(NS_BLOCK_NEEDS_BIDI_RESOLUTION);
      LinesBegin()->MarkDirty();
    }
  }
  mCropped = aIsCropped;
}

void MiddleCroppingBlockFrame::UpdateDisplayedValueToUncroppedValue(
    bool aNotify) {
  nsAutoString value;
  GetUncroppedValue(value);
  UpdateDisplayedValue(value, /* aIsCropped = */ false, aNotify);
}

nscoord MiddleCroppingBlockFrame::GetMinISize(gfxContext* aRenderingContext) {
  nscoord result;
  DISPLAY_MIN_INLINE_SIZE(this, result);

  // Our min inline size is our pref inline size
  result = GetPrefISize(aRenderingContext);
  return result;
}

nscoord MiddleCroppingBlockFrame::GetPrefISize(gfxContext* aRenderingContext) {
  nscoord result;
  DISPLAY_PREF_INLINE_SIZE(this, result);

  nsAutoString prevValue;
  bool restoreOldValue = false;

  // Make sure we measure with the uncropped value.
  if (mCropped && mCachedPrefISize == NS_INTRINSIC_ISIZE_UNKNOWN) {
    mTextNode->GetNodeValue(prevValue);
    restoreOldValue = true;
    UpdateDisplayedValueToUncroppedValue(false);
  }

  result = nsBlockFrame::GetPrefISize(aRenderingContext);

  if (restoreOldValue) {
    UpdateDisplayedValue(prevValue, /* aIsCropped = */ true, false);
  }

  return result;
}

bool MiddleCroppingBlockFrame::CropTextToWidth(gfxContext& aRenderingContext,
                                               nscoord aWidth,
                                               nsString& aText) const {
  if (aText.IsEmpty()) {
    return false;
  }

  RefPtr<nsFontMetrics> fm = nsLayoutUtils::GetFontMetricsForFrame(this, 1.0f);

  // see if the text will completely fit in the width given
  if (nsLayoutUtils::AppUnitWidthOfStringBidi(aText, this, *fm,
                                              aRenderingContext) <= aWidth) {
    return false;
  }

  DrawTarget* drawTarget = aRenderingContext.GetDrawTarget();
  const nsDependentString& kEllipsis = nsContentUtils::GetLocalizedEllipsis();

  // see if the width is even smaller than the ellipsis
  fm->SetTextRunRTL(false);
  const nscoord ellipsisWidth =
      nsLayoutUtils::AppUnitWidthOfString(kEllipsis, *fm, drawTarget);
  if (ellipsisWidth >= aWidth) {
    aText = kEllipsis;
    return true;
  }

  // determine how much of the string will fit in the max width
  nscoord totalWidth = ellipsisWidth;
  const Span text(aText);
  intl::GraphemeClusterBreakIteratorUtf16 leftIter(text);
  intl::GraphemeClusterBreakReverseIteratorUtf16 rightIter(text);
  uint32_t leftPos = 0;
  uint32_t rightPos = aText.Length();
  nsAutoString leftString, rightString;

  while (leftPos < rightPos) {
    Maybe<uint32_t> pos = leftIter.Next();
    Span chars = text.FromTo(leftPos, *pos);
    nscoord charWidth =
        nsLayoutUtils::AppUnitWidthOfString(chars, *fm, drawTarget);
    if (totalWidth + charWidth > aWidth) {
      break;
    }

    leftString.Append(chars);
    leftPos = *pos;
    totalWidth += charWidth;

    if (leftPos >= rightPos) {
      break;
    }

    pos = rightIter.Next();
    chars = text.FromTo(*pos, rightPos);
    charWidth = nsLayoutUtils::AppUnitWidthOfString(chars, *fm, drawTarget);
    if (totalWidth + charWidth > aWidth) {
      break;
    }

    rightString.Insert(chars, 0);
    rightPos = *pos;
    totalWidth += charWidth;
  }

  aText = leftString + kEllipsis + rightString;
  return true;
}

void MiddleCroppingBlockFrame::Reflow(nsPresContext* aPresContext,
                                      ReflowOutput& aDesiredSize,
                                      const ReflowInput& aReflowInput,
                                      nsReflowStatus& aStatus) {
  // Restore the uncropped value.
  nsAutoString value;
  GetUncroppedValue(value);
  bool cropped = false;
  while (true) {
    UpdateDisplayedValue(value, cropped, false);  // update the text node
    AddStateBits(NS_BLOCK_NEEDS_BIDI_RESOLUTION);
    LinesBegin()->MarkDirty();
    nsBlockFrame::Reflow(aPresContext, aDesiredSize, aReflowInput, aStatus);
    if (cropped) {
      break;
    }
    nscoord currentICoord = aReflowInput.mLineLayout
                                ? aReflowInput.mLineLayout->GetCurrentICoord()
                                : 0;
    const nscoord availSize = aReflowInput.AvailableISize() - currentICoord;
    const nscoord sizeToFit = std::min(aReflowInput.ComputedISize(), availSize);
    if (LinesBegin()->ISize() > sizeToFit) {
      // The value overflows - crop it and reflow again (once).
      if (CropTextToWidth(*aReflowInput.mRenderingContext, sizeToFit, value)) {
        nsBlockFrame::DidReflow(aPresContext, &aReflowInput);
        aStatus.Reset();
        MarkSubtreeDirty();
        AddStateBits(NS_BLOCK_NEEDS_BIDI_RESOLUTION);
        mCachedMinISize = NS_INTRINSIC_ISIZE_UNKNOWN;
        mCachedPrefISize = NS_INTRINSIC_ISIZE_UNKNOWN;
        cropped = true;
        continue;
      }
    }
    break;
  }
}

nsresult MiddleCroppingBlockFrame::CreateAnonymousContent(
    nsTArray<ContentInfo>& aContent) {
  auto* doc = PresContext()->Document();
  mTextNode = new (doc->NodeInfoManager()) nsTextNode(doc->NodeInfoManager());
  // Update the displayed text to reflect the current element's value.
  UpdateDisplayedValueToUncroppedValue(false);
  aContent.AppendElement(mTextNode);
  return NS_OK;
}

void MiddleCroppingBlockFrame::AppendAnonymousContentTo(
    nsTArray<nsIContent*>& aContent, uint32_t aFilter) {
  aContent.AppendElement(mTextNode);
}

void MiddleCroppingBlockFrame::Destroy(DestroyContext& aContext) {
  aContext.AddAnonymousContent(mTextNode.forget());
  nsBlockFrame::Destroy(aContext);
}

}  // namespace mozilla
