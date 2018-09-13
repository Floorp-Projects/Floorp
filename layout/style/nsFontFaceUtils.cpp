/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFontFaceUtils.h"

#include "gfxUserFontSet.h"
#include "nsFontMetrics.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"
#include "nsPlaceholderFrame.h"
#include "nsTArray.h"
#include "SVGTextFrame.h"

using namespace mozilla;

static bool
ComputedStyleContainsFont(ComputedStyle* aComputedStyle,
                          nsPresContext* aPresContext,
                          const gfxUserFontSet* aUserFontSet,
                          const gfxUserFontEntry* aFont)
{
  // if the font is null, simply check to see whether fontlist includes
  // downloadable fonts
  if (!aFont) {
    const mozilla::FontFamilyList& fontlist =
      aComputedStyle->StyleFont()->mFont.fontlist;
    return aUserFontSet->ContainsUserFontSetFonts(fontlist);
  }

  // first, check if the family name is in the fontlist
  NS_ConvertUTF8toUTF16 familyName(aFont->FamilyName());
  if (!aComputedStyle->StyleFont()->mFont.fontlist.Contains(familyName)) {
    return false;
  }

  // family name is in the fontlist, check to see if the font group
  // associated with the frame includes the specific userfont
  RefPtr<nsFontMetrics> fm =
    nsLayoutUtils::GetFontMetricsForComputedStyle(aComputedStyle,
                                                  aPresContext,
                                                  1.0f);

  if (fm->GetThebesFontGroup()->ContainsUserFont(aFont)) {
    return true;
  }

  return false;
}

static bool
FrameUsesFont(nsIFrame* aFrame, const gfxUserFontEntry* aFont)
{
  // check the style of the frame
  nsPresContext* pc = aFrame->PresContext();
  gfxUserFontSet* ufs = pc->GetUserFontSet();
  if (ComputedStyleContainsFont(aFrame->Style(), pc, ufs, aFont)) {
    return true;
  }

  // check additional styles
  int32_t contextIndex = 0;
  for (ComputedStyle* extraContext;
       (extraContext = aFrame->GetAdditionalComputedStyle(contextIndex));
       ++contextIndex) {
    if (ComputedStyleContainsFont(extraContext, pc, ufs, aFont)) {
      return true;
    }
  }

  return false;
}

static void
ScheduleReflow(nsIPresShell* aShell, nsIFrame* aFrame)
{
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
          if (f->GetStateBits() & NS_STATE_IS_OUTER_SVG ||
              !(f->IsFrameOfType(nsIFrame::eSVG) ||
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

  aShell->FrameNeedsReflow(f, nsIPresShell::eStyleChange, NS_FRAME_IS_DIRTY);
}

/* static */ void
nsFontFaceUtils::MarkDirtyForFontChange(nsIFrame* aSubtreeRoot,
                                        const gfxUserFontEntry* aFont)
{
  AutoTArray<nsIFrame*, 4> subtrees;
  subtrees.AppendElement(aSubtreeRoot);

  nsIPresShell* ps = aSubtreeRoot->PresShell();

  // check descendants, iterating over subtrees that may include
  // additional subtrees associated with placeholders
  do {
    nsIFrame* subtreeRoot = subtrees.PopLastElement();

    // Check all descendants to see if they use the font
    AutoTArray<nsIFrame*, 32> stack;
    stack.AppendElement(subtreeRoot);

    do {
      nsIFrame* f = stack.PopLastElement();

      // if this frame uses the font, mark its descendants dirty
      // and skip checking its children
      if (FrameUsesFont(f, aFont)) {
        ScheduleReflow(ps, f);
      } else {
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
            stack.AppendElement(kid);
          }
        }
      }
    } while (!stack.IsEmpty());
  } while (!subtrees.IsEmpty());
}
