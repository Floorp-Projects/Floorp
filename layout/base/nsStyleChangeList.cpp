/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * a list of the recomputation that needs to be done in response to a
 * style change
 */

#include "nsStyleChangeList.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsFrameManager.h"

void
nsStyleChangeList::AppendChange(nsIFrame* aFrame, nsIContent* aContent, nsChangeHint aHint)
{
  MOZ_ASSERT(aFrame || (aHint & nsChangeHint_ReconstructFrame),
             "must have frame");
  MOZ_ASSERT(aHint, "No hint to process?");
  MOZ_ASSERT(!(aHint & nsChangeHint_NeutralChange),
             "Neutral changes do not need extra processing, "
             "and should be stripped out");
  MOZ_ASSERT(aContent || !(aHint & nsChangeHint_ReconstructFrame),
             "must have content");
  // XXXbz we should make this take Element instead of nsIContent
  MOZ_ASSERT(!aContent || aContent->IsElement() ||
             // display:contents elements posts the changes for their children:
             (aFrame && aContent->GetParent() &&
             aFrame->PresContext()->FrameManager()->
               GetDisplayContentsStyleFor(aContent->GetParent())) ||
             (aContent->IsNodeOfType(nsINode::eTEXT) &&
              aContent->IsStyledByServo() &&
              aContent->HasFlag(NODE_NEEDS_FRAME) &&
              aHint & nsChangeHint_ReconstructFrame),
             "Shouldn't be trying to restyle non-elements directly, "
             "except if it's a display:contents child or a text node "
             "doing lazy frame construction");
  MOZ_ASSERT(!(aHint & nsChangeHint_AllReflowHints) ||
             (aHint & nsChangeHint_NeedReflow),
             "Reflow hint bits set without actually asking for a reflow");

  // If Servo fires reconstruct at a node, it is the only change hint fired at
  // that node.
  if (IsServo()) {
    for (size_t i = 0; i < Length(); ++i) {
      MOZ_ASSERT(!aContent || !((aHint | (*this)[i].mHint) & nsChangeHint_ReconstructFrame) ||
                 (*this)[i].mContent != aContent);
    }
  } else {
    // Filter out all other changes for same content for Gecko (Servo asserts against this
    // case above).
    if (aContent && (aHint & nsChangeHint_ReconstructFrame)) {
      // NOTE: This is captured by reference to please static analysis.
      // Capturing it by value as a pointer should be fine in this case.
      RemoveElementsBy([&](const nsStyleChangeData& aData) {
        return aData.mContent == aContent;
      });
    }
  }

  if (!IsEmpty() && aFrame && aFrame == LastElement().mFrame) {
    LastElement().mHint |= aHint;
    return;
  }

  AppendElement(nsStyleChangeData { aFrame, aContent, aHint });
}
