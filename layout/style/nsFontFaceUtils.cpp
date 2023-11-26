/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFontFaceUtils.h"

#include "gfxTextRun.h"
#include "gfxUserFontSet.h"
#include "mozilla/PresShell.h"
#include "mozilla/RestyleManager.h"
#include "nsFontMetrics.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"
#include "nsPlaceholderFrame.h"
#include "nsTArray.h"

using namespace mozilla;

enum class FontUsageKind {
  // The frame did not use the given font.
  None = 0,
  // The frame uses the given font, but doesn't use font-metric-dependent units,
  // which means that its style doesn't depend on this font.
  Frame = 1 << 0,
  // The frame uses has some font-metric-dependent units on this font.
  // This means that its style depends on this font, and we need to restyle the
  // element the frame came from.
  FontMetrics = 1 << 1,

  Max = Frame | FontMetrics,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(FontUsageKind);

static bool IsFontReferenced(const ComputedStyle& aStyle,
                             const nsAString& aFamilyName) {
  for (const auto& family :
       aStyle.StyleFont()->mFont.family.families.list.AsSpan()) {
    if (family.IsNamedFamily(aFamilyName)) {
      return true;
    }
  }
  return false;
}

static FontUsageKind StyleFontUsage(nsIFrame* aFrame, ComputedStyle* aStyle,
                                    nsPresContext* aPresContext,
                                    const gfxUserFontEntry* aFont,
                                    const nsAString& aFamilyName,
                                    bool aIsExtraStyle) {
  MOZ_ASSERT(NS_ConvertUTF8toUTF16(aFont->FamilyName()) == aFamilyName);

  auto FontIsUsed = [&aFont, &aPresContext,
                     &aFamilyName](ComputedStyle* aStyle) {
    if (!IsFontReferenced(*aStyle, aFamilyName)) {
      return false;
    }

    // family name is in the fontlist, check to see if the font group
    // associated with the frame includes the specific userfont.
    //
    // TODO(emilio): Is this check really useful? I guess it's useful for
    // different font faces of the same font?
    RefPtr<nsFontMetrics> fm = nsLayoutUtils::GetFontMetricsForComputedStyle(
        aStyle, aPresContext, 1.0f);
    return fm->GetThebesFontGroup()->ContainsUserFont(aFont);
  };

  auto usage = FontUsageKind::None;

  if (FontIsUsed(aStyle)) {
    usage |= FontUsageKind::Frame;
    if (aStyle->DependsOnSelfFontMetrics()) {
      usage |= FontUsageKind::FontMetrics;
    }
  }

  if (aStyle->DependsOnInheritedFontMetrics() &&
      !(usage & FontUsageKind::FontMetrics)) {
    ComputedStyle* parentStyle = nullptr;
    if (aIsExtraStyle) {
      parentStyle = aFrame->Style();
    } else {
      nsIFrame* provider = nullptr;
      parentStyle = aFrame->GetParentComputedStyle(&provider);
    }

    if (parentStyle && FontIsUsed(parentStyle)) {
      usage |= FontUsageKind::FontMetrics;
    }
  }

  return usage;
}

static FontUsageKind FrameFontUsage(nsIFrame* aFrame,
                                    nsPresContext* aPresContext,
                                    const gfxUserFontEntry* aFont,
                                    const nsAString& aFamilyName) {
  // check the style of the frame
  FontUsageKind kind = StyleFontUsage(aFrame, aFrame->Style(), aPresContext,
                                      aFont, aFamilyName, /* extra = */ false);
  if (kind == FontUsageKind::Max) {
    return kind;
  }

  // check additional styles
  int32_t contextIndex = 0;
  for (ComputedStyle* extraContext;
       (extraContext = aFrame->GetAdditionalComputedStyle(contextIndex));
       ++contextIndex) {
    kind |= StyleFontUsage(aFrame, extraContext, aPresContext, aFont,
                           aFamilyName, /* extra = */ true);
    if (kind == FontUsageKind::Max) {
      break;
    }
  }

  return kind;
}

// TODO(emilio): Can we use the restyle-hint machinery instead of this?
static void ScheduleReflow(PresShell* aPresShell, nsIFrame* aFrame) {
  nsIFrame* f = aFrame;
  if (f->IsSVGFrame() || f->IsInSVGTextSubtree()) {
    // SVG frames (and the non-SVG descendants of an SVGTextFrame) need special
    // reflow handling.  We need to search upwards for the first displayed
    // SVGOuterSVGFrame or non-SVG frame, which is the frame we can call
    // FrameNeedsReflow on.  (This logic is based on
    // SVGUtils::ScheduleReflowSVG and
    // SVGTextFrame::ScheduleReflowSVGNonDisplayText.)
    if (f->HasAnyStateBits(NS_FRAME_IS_NONDISPLAY)) {
      while (f) {
        if (!f->HasAnyStateBits(NS_FRAME_IS_NONDISPLAY)) {
          if (f->IsSubtreeDirty()) {
            // This is a displayed frame, so if it is already dirty, we
            // will be reflowed soon anyway.  No need to call
            // FrameNeedsReflow again, then.
            return;
          }
          if (!f->HasAnyStateBits(NS_FRAME_SVG_LAYOUT) ||
              !f->IsInSVGTextSubtree()) {
            break;
          }
          f->AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
        }
        f = f->GetParent();
      }
      MOZ_ASSERT(f, "should have found an ancestor frame to reflow");
    }
  }

  aPresShell->FrameNeedsReflow(f, IntrinsicDirty::FrameAncestorsAndDescendants,
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

  const bool usesMetricsFromStyle = pc->StyleSet()->UsesFontMetrics();

  // StyleSingleFontFamily::IsNamedFamily expects a UTF-16 string. Convert it
  // once here rather than on each call.
  NS_ConvertUTF8toUTF16 familyName(aFont->FamilyName());

  // check descendants, iterating over subtrees that may include
  // additional subtrees associated with placeholders
  do {
    nsIFrame* subtreeRoot = subtrees.PopLastElement();

    // Check all descendants to see if they use the font
    AutoTArray<std::pair<nsIFrame*, ReflowAlreadyScheduled>, 32> stack;
    stack.AppendElement(
        std::make_pair(subtreeRoot, ReflowAlreadyScheduled::No));

    do {
      auto pair = stack.PopLastElement();
      nsIFrame* f = pair.first;
      ReflowAlreadyScheduled alreadyScheduled = pair.second;

      // if this frame uses the font, mark its descendants dirty
      // and skip checking its children
      FontUsageKind kind = FrameFontUsage(f, pc, aFont, familyName);
      if (kind != FontUsageKind::None) {
        if ((kind & FontUsageKind::Frame) &&
            alreadyScheduled == ReflowAlreadyScheduled::No) {
          ScheduleReflow(presShell, f);
          alreadyScheduled = ReflowAlreadyScheduled::Yes;
        }
        if (kind & FontUsageKind::FontMetrics) {
          MOZ_ASSERT(f->GetContent() && f->GetContent()->IsElement(),
                     "How could we target a non-element with selectors?");
          f->PresContext()->RestyleManager()->PostRestyleEvent(
              dom::Element::FromNode(f->GetContent()),
              RestyleHint::RECASCADE_SELF, nsChangeHint(0));
        }
      }

      if (alreadyScheduled == ReflowAlreadyScheduled::No ||
          usesMetricsFromStyle) {
        if (f->IsPlaceholderFrame()) {
          nsIFrame* oof = nsPlaceholderFrame::GetRealFrameForPlaceholder(f);
          if (!nsLayoutUtils::IsProperAncestorFrame(subtreeRoot, oof)) {
            // We have another distinct subtree we need to mark.
            subtrees.AppendElement(oof);
          }
        }

        for (const auto& childList : f->ChildLists()) {
          for (nsIFrame* kid : childList.mList) {
            stack.AppendElement(std::make_pair(kid, alreadyScheduled));
          }
        }
      }
    } while (!stack.IsEmpty());
  } while (!subtrees.IsEmpty());
}
