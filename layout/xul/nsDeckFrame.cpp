/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsDeckFrame.h"
#include "mozilla/ComputedStyle.h"
#include "mozilla/PresShell.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsNameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsHTMLParts.h"
#include "nsCSSRendering.h"
#include "nsViewManager.h"
#include "nsBoxLayoutState.h"
#include "nsStackLayout.h"
#include "nsDisplayList.h"
#include "nsContainerFrame.h"
#include "nsContentUtils.h"
#include "nsXULPopupManager.h"
#include "nsImageBoxFrame.h"
#include "nsImageFrame.h"

#ifdef ACCESSIBILITY
#  include "nsAccessibilityService.h"
#endif

using namespace mozilla;

nsIFrame* NS_NewDeckFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell) nsDeckFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsDeckFrame)

NS_QUERYFRAME_HEAD(nsDeckFrame)
  NS_QUERYFRAME_ENTRY(nsDeckFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBoxFrame)

nsDeckFrame::nsDeckFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
    : nsBoxFrame(aStyle, aPresContext, kClassID), mIndex(0) {
  nsCOMPtr<nsBoxLayout> layout;
  NS_NewStackLayout(layout);
  SetXULLayoutManager(layout);
}

nsresult nsDeckFrame::AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                       int32_t aModType) {
  nsresult rv =
      nsBoxFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);

  // if the index changed hide the old element and make the new element visible
  if (aAttribute == nsGkAtoms::selectedIndex) {
    IndexChanged();
  }

  return rv;
}

void nsDeckFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                       nsIFrame* aPrevInFlow) {
  nsBoxFrame::Init(aContent, aParent, aPrevInFlow);

  mIndex = GetSelectedIndex();
}

void nsDeckFrame::ShowBox(nsIFrame* aBox) { Animate(aBox, true); }

void nsDeckFrame::HideBox(nsIFrame* aBox) {
  PresShell::ClearMouseCapture(aBox);
  Animate(aBox, false);
}

void nsDeckFrame::IndexChanged() {
  // did the index change?
  int32_t index = GetSelectedIndex();

  if (index == mIndex) return;

  // redraw
  InvalidateFrame();

  // hide the currently showing box
  nsIFrame* currentBox = GetSelectedBox();
  if (currentBox)  // only hide if it exists
    HideBox(currentBox);

  mIndex = index;

  ShowBox(GetSelectedBox());

#ifdef ACCESSIBILITY
  nsAccessibilityService* accService = GetAccService();
  if (accService) {
    accService->DeckPanelSwitched(PresContext()->GetPresShell(), mContent,
                                  currentBox, GetSelectedBox());
  }
#endif

  // Force any popups that might be anchored on elements within hidden
  // box to update.
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm && currentBox) {
    pm->UpdatePopupPositions(currentBox->PresContext()->RefreshDriver());
  }
}

int32_t nsDeckFrame::GetSelectedIndex() {
  // default index is 0
  int32_t index = 0;

  // get the index attribute
  nsAutoString value;
  if (mContent->AsElement()->GetAttr(kNameSpaceID_None,
                                     nsGkAtoms::selectedIndex, value)) {
    nsresult error;

    // convert it to an integer
    index = value.ToInteger(&error);
  }

  return index;
}

nsIFrame* nsDeckFrame::GetSelectedBox() {
  return (mIndex >= 0) ? mFrames.FrameAt(mIndex) : nullptr;
}

void nsDeckFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                   const nsDisplayListSet& aLists) {
  // if a tab is hidden all its children are too.
  if (StyleVisibility()->mVisible == StyleVisibility::Hidden) {
    return;
  }
  nsBoxFrame::BuildDisplayList(aBuilder, aLists);
}

void nsDeckFrame::RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame) {
  nsIFrame* currentFrame = GetSelectedBox();
  if (currentFrame && aOldFrame && currentFrame != aOldFrame) {
    // If the frame we're removing is at an index that's less
    // than mIndex, that means we're going to be shifting indexes
    // by 1.
    //
    // We attempt to keep the same child displayed by automatically
    // updating our internal notion of the current index.
    int32_t removedIndex = mFrames.IndexOf(aOldFrame);
    MOZ_ASSERT(removedIndex >= 0,
               "A deck child was removed that was not in mFrames.");
    if (removedIndex < mIndex) {
      mIndex--;
      // This is going to cause us to handle the index change in IndexedChanged,
      // but since the new index will match mIndex, it's essentially a noop.
      nsContentUtils::AddScriptRunner(new nsSetAttrRunnable(
          mContent->AsElement(), nsGkAtoms::selectedIndex, mIndex));
    }
  }
  nsBoxFrame::RemoveFrame(aListID, aOldFrame);
}

void nsDeckFrame::BuildDisplayListForChildren(nsDisplayListBuilder* aBuilder,
                                              const nsDisplayListSet& aLists) {
  // only paint the selected box
  nsIFrame* box = GetSelectedBox();
  if (!box) return;

  // Putting the child in the background list. This is a little weird but
  // it matches what we were doing before.
  nsDisplayListSet set(aLists, aLists.BlockBorderBackgrounds());
  BuildDisplayListForChild(aBuilder, box, set);
}

void nsDeckFrame::Animate(nsIFrame* aParentBox, bool start) {
  if (!aParentBox) return;

  nsImageBoxFrame* imgBoxFrame = do_QueryFrame(aParentBox);
  nsImageFrame* imgFrame = do_QueryFrame(aParentBox);

  if (imgBoxFrame) {
    if (start)
      imgBoxFrame->RestartAnimation();
    else
      imgBoxFrame->StopAnimation();
  }

  if (imgFrame) {
    if (start)
      imgFrame->RestartAnimation();
    else
      imgFrame->StopAnimation();
  }

  for (const auto& childList : aParentBox->ChildLists()) {
    for (nsIFrame* child : childList.mList) {
      Animate(child, start);
    }
  }
}

NS_IMETHODIMP
nsDeckFrame::DoXULLayout(nsBoxLayoutState& aState) {
  // Make sure we tweak the state so it does not resize our children.
  // We will do that.
  ReflowChildFlags oldFlags = aState.LayoutFlags();
  aState.SetLayoutFlags(ReflowChildFlags::NoSizeView);

  // do a normal layout
  nsresult rv = nsBoxFrame::DoXULLayout(aState);

  // <deck> and <tabpanels> other than our browser's tab shouldn't have any
  // <browser> or <iframe> to avoid running into troubles with Fission.
  MOZ_ASSERT(
      (mContent->IsXULElement(nsGkAtoms::tabpanels) &&
       mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::id,
#ifdef MOZ_THUNDERBIRD
                                          u"tabpanelcontainer"_ns,
#else
                                          u"tabbrowser-tabpanels"_ns,
#endif
                                          eCaseMatters)) ||
      !HasPossiblyRemoteContents());

  // run though each child. Hide all but the selected one
  nsIFrame* box = nsIFrame::GetChildXULBox(this);

  nscoord count = 0;
  while (box) {
    // make collapsed children not show up
    if (count != mIndex) {
      HideBox(box);
    } else {
      ShowBox(box);
    }
    box = GetNextXULBox(box);
    count++;
  }

  aState.SetLayoutFlags(oldFlags);

  return rv;
}

bool nsDeckFrame::HasPossiblyRemoteContents() const {
  auto hasRemoteOrMayChangeRemoteNessAttribute =
      [](dom::Element& aElement) -> bool {
    return (aElement.AttrValueIs(kNameSpaceID_None, nsGkAtoms::remote,
                                 nsGkAtoms::_true, eCaseMatters) ||
            aElement.HasAttribute(u"maychangeremoteness"_ns));
  };

  for (nsIContent* node = mContent; node; node = node->GetNextNode(mContent)) {
    if ((node->IsXULElement(nsGkAtoms::browser) ||
         node->IsHTMLElement(nsGkAtoms::iframe)) &&
        hasRemoteOrMayChangeRemoteNessAttribute(*(node->AsElement()))) {
      return true;
    }
  }
  return false;
}
