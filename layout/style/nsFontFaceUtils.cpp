/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFontFaceUtils.h"

#include "gfxUserFontSet.h"
#include "mozilla/PresShell.h"
#include "mozilla/RestyleManager.h"
#include "nsFontMetrics.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"
#include "nsPlaceholderFrame.h"
#include "nsTArray.h"
#include "SVGTextFrame.h"

using namespace mozilla;

enum class FontUsageKind {
  // The frame did not use the given font.
  None = 0,
  // The frame uses the given font, but doesn't use font-metric-dependent units,
  // which means that its style doesn't depend on this font.
  Frame,
  // The frame uses this font and also has some font-metric-dependent units.
  // This means that its style depends on this font, and we need to restyle the
  // element the frame came from.
  FrameAndFontMetrics,
};

static FontUsageKind StyleFontUsage(ComputedStyle* aComputedStyle,
                                    nsPresContext* aPresContext,
                                    const gfxUserFontSet* aUserFontSet,
                                    const gfxUserFontEntry* aFont) {
  // first, check if the family name is in the fontlist
  if (!aComputedStyle->StyleFont()->mFont.fontlist.Contains(
          aFont->FamilyName())) {
    return FontUsageKind::None;
  }

  // family name is in the fontlist, check to see if the font group
  // associated with the frame includes the specific userfont
  RefPtr<nsFontMetrics> fm = nsLayoutUtils::GetFontMetricsForComputedStyle(
      aComputedStyle, aPresContext, 1.0f);

  if (!fm->GetThebesFontGroup()->ContainsUserFont(aFont)) {
    return FontUsageKind::None;
  }

  if (aComputedStyle->DependsOnFontMetrics()) {
    MOZ_ASSERT(aPresContext->UsesExChUnits());
    return FontUsageKind::FrameAndFontMetrics;
  }

  return FontUsageKind::Frame;
}

static FontUsageKind FrameFontUsage(nsIFrame* aFrame,
                                    nsPresContext* aPresContext,
                                    const gfxUserFontEntry* aFont) {
  // check the style of the frame
  gfxUserFontSet* ufs = aPresContext->GetUserFontSet();
  FontUsageKind kind =
      StyleFontUsage(aFrame->Style(), aPresContext, ufs, aFont);
  if (kind == FontUsageKind::FrameAndFontMetrics) {
    return kind;
  }

  // check additional styles
  int32_t contextIndex = 0;
  for (ComputedStyle* extraContext;
       (extraContext = aFrame->GetAdditionalComputedStyle(contextIndex));
       ++contextIndex) {
    kind =
        std::max(kind, StyleFontUsage(extraContext, aPresContext, ufs, aFont));
    if (kind == FontUsageKind::FrameAndFontMetrics) {
      break;
    }
  }

  return kind;
}

// TODO(emilio): Can we use the restyle-hint machinery instead of this?
static void ScheduleReflow(PresShell* aPresShell, nsIFrame* aFrame) {
  nsIFrame* f = aFrame;
  if (f->IsFrameOfType(nsIFrame::eSVG) || nsSVGUtils::IsInSVGTextSubtree(f)) {
    // SVG frames (and the non-SVG descendants of an SVGTextFrame) need special
    // reflow handling.  We need to search upwards for the first displayed
    // nsSVGOuterSVGFrame or non-SVG frame, which is the frame we can call
    // FrameNeedsReflow on.  (This logic is based on
    // nsSVGUtils::ScheduleReflowSVG and
    // SVGTextFrame::ScheduleReflowSVGNonDisplayText.)
    if (f->GetStateBits() & NS_FRAME_IS_NONDISPLAY) {
      while (f) {
        if (!(f->GetStateBits() & NS_FRAME_IS_NONDISPLAY)) {
          if (NS_SUBTREE_DIRTY(f)) {
            // This is a displayed frame, so if it is already dirty, we
            // will be reflowed soon anyway.  No need to call
            // FrameNeedsReflow again, then.
            return;
          }
          if (f->IsSVGOuterSVGFrame() || !(f->IsFrameOfType(nsIFrame::eSVG) ||
                                           nsSVGUtils::IsInSVGTextSubtree(f))) {
            break;
          }
          f->AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
        }
        f = f->GetParent();
      }
      MOZ_ASSERT(f, "should have found an ancestor frame to reflow");
    }
  }

  aPresShell->FrameNeedsReflow(f, IntrinsicDirty::StyleChange,
                               NS_FRAME_IS_DIRTY);
}

enum class ReflowAlreadyScheduled {
  No,
  Yes,
};

/* static */
void nsFontFaceUtils::MarkDirtyForFontChange(nsIFrame* aSubtreeRoot,
                                             const gfxUserFontEntry* aFont) {
  MOZ_ASSERT(aFont);
  AutoTArray<nsIFrame*, 4> subtrees;
  subtrees.AppendElement(aSubtreeRoot);

  nsPresContext* pc = aSubtreeRoot->PresContext();
  PresShell* presShell = pc->PresShell();

  // check descendants, iterating over subtrees that may include
  // additional subtrees associated with placeholders
  do {
    nsIFrame* subtreeRoot = subtrees.PopLastElement();

    // Check all descendants to see if they use the font
    AutoTArray<Pair<nsIFrame*, ReflowAlreadyScheduled>, 32> stack;
    stack.AppendElement(MakePair(subtreeRoot, ReflowAlreadyScheduled::No));

    do {
      auto pair = stack.PopLastElement();
      nsIFrame* f = pair.first();
      ReflowAlreadyScheduled alreadyScheduled = pair.second();

      // if this frame uses the font, mark its descendants dirty
      // and skip checking its children
      FontUsageKind kind = FrameFontUsage(f, pc, aFont);
      if (kind != FontUsageKind::None) {
        if (alreadyScheduled == ReflowAlreadyScheduled::No) {
          ScheduleReflow(presShell, f);
          alreadyScheduled = ReflowAlreadyScheduled::Yes;
        }
        if (kind == FontUsageKind::FrameAndFontMetrics) {
          MOZ_ASSERT(f->GetContent() && f->GetContent()->IsElement(),
                     "How could we target a non-element with selectors?");
          f->PresContext()->RestyleManager()->PostRestyleEvent(
              Element::FromNode(f->GetContent()),
              StyleRestyleHint_RECASCADE_SELF, nsChangeHint(0));
        }
      }

      if (alreadyScheduled == ReflowAlreadyScheduled::No ||
          pc->UsesExChUnits()) {
        if (f->IsPlaceholderFrame()) {
          nsIFrame* oof = nsPlaceholderFrame::GetRealFrameForPlaceholder(f);
          if (!nsLayoutUtils::IsProperAncestorFrame(subtreeRoot, oof)) {
            // We have another distinct subtree we need to mark.
            subtrees.AppendElement(oof);
          }
        }

        nsIFrame::ChildListIterator lists(f);
        for (; !lists.IsDone(); lists.Next()) {
          nsFrameList::Enumerator childFrames(lists.CurrentList());
          for (; !childFrames.AtEnd(); childFrames.Next()) {
            nsIFrame* kid = childFrames.get();
            stack.AppendElement(MakePair(kid, alreadyScheduled));
          }
        }
      }
    } while (!stack.IsEmpty());
  } while (!subtrees.IsEmpty());
}
