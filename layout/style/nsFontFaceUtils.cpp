/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxUserFontSet.h"
#include "nsFontFaceUtils.h"
#include "nsFontMetrics.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"
#include "nsPlaceholderFrame.h"
#include "nsTArray.h"
#include "SVGTextFrame.h"

static bool
StyleContextContainsFont(nsStyleContext* aStyleContext,
                         const gfxUserFontSet* aUserFontSet,
                         const gfxUserFontEntry* aFont)
{
  // if the font is null, simply check to see whether fontlist includes
  // downloadable fonts
  if (!aFont) {
    const mozilla::FontFamilyList& fontlist =
      aStyleContext->StyleFont()->mFont.fontlist;
    return aUserFontSet->ContainsUserFontSetFonts(fontlist);
  }

  // first, check if the family name is in the fontlist
  const nsString& familyName = aFont->FamilyName();
  if (!aStyleContext->StyleFont()->mFont.fontlist.Contains(familyName)) {
    return false;
  }

  // family name is in the fontlist, check to see if the font group
  // associated with the frame includes the specific userfont
  RefPtr<nsFontMetrics> fm =
    nsLayoutUtils::GetFontMetricsForStyleContext(aStyleContext, 1.0f);

  if (fm->GetThebesFontGroup()->ContainsUserFont(aFont)) {
    return true;
  }

  return false;
}

static bool
FrameUsesFont(nsIFrame* aFrame, const gfxUserFontEntry* aFont)
{
  // check the style context of the frame
  gfxUserFontSet* ufs = aFrame->PresContext()->GetUserFontSet();
  if (StyleContextContainsFont(aFrame->StyleContext(), ufs, aFont)) {
    return true;
  }

  // check additional style contexts
  int32_t contextIndex = 0;
  for (nsStyleContext* extraContext;
       (extraContext = aFrame->GetAdditionalStyleContext(contextIndex));
       ++contextIndex) {
    if (StyleContextContainsFont(extraContext, ufs, aFont)) {
      return true;
    }
  }

  return false;
}

static void
ScheduleReflow(nsIPresShell* aShell, nsIFrame* aFrame)
{
  nsIFrame* f = aFrame;
  if (f->IsFrameOfType(nsIFrame::eSVG) || f->IsSVGText()) {
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
              !(f->IsFrameOfType(nsIFrame::eSVG) || f->IsSVGText())) {
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

  nsIPresShell* ps = aSubtreeRoot->PresContext()->PresShell();

  // check descendants, iterating over subtrees that may include
  // additional subtrees associated with placeholders
  do {
    nsIFrame* subtreeRoot = subtrees.ElementAt(subtrees.Length() - 1);
    subtrees.RemoveElementAt(subtrees.Length() - 1);

    // Check all descendants to see if they use the font
    AutoTArray<nsIFrame*, 32> stack;
    stack.AppendElement(subtreeRoot);

    do {
      nsIFrame* f = stack.ElementAt(stack.Length() - 1);
      stack.RemoveElementAt(stack.Length() - 1);

      // if this frame uses the font, mark its descendants dirty
      // and skip checking its children
      if (FrameUsesFont(f, aFont)) {
        ScheduleReflow(ps, f);
      } else {
        if (f->GetType() == nsGkAtoms::placeholderFrame) {
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
