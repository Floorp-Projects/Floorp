/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPopupSetFrame.h"
#include "nsGkAtoms.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsPresContext.h"
#include "mozilla/ComputedStyle.h"
#include "nsBoxLayoutState.h"
#include "nsIScrollableFrame.h"
#include "nsIPopupContainer.h"
#include "nsMenuPopupFrame.h"

nsIFrame*
NS_NewPopupSetFrame(nsIPresShell* aPresShell, ComputedStyle* aStyle)
{
  return new (aPresShell) nsPopupSetFrame(aStyle);
}

NS_IMPL_FRAMEARENA_HELPERS(nsPopupSetFrame)

void
nsPopupSetFrame::Init(nsIContent*       aContent,
                      nsContainerFrame* aParent,
                      nsIFrame*         aPrevInFlow)
{
  nsBoxFrame::Init(aContent, aParent, aPrevInFlow);

  // Normally the root box is our grandparent, but in case of wrapping
  // it can be our great-grandparent.
  nsIPopupContainer *popupContainer =
    nsIPopupContainer::GetPopupContainer(PresContext()->GetPresShell());
  if (popupContainer) {
    popupContainer->SetPopupSetFrame(this);
  }
}

void
nsPopupSetFrame::AppendFrames(ChildListID     aListID,
                              nsFrameList&    aFrameList)
{
  if (aListID == kPopupList) {
    AddPopupFrameList(aFrameList);
    return;
  }
  nsBoxFrame::AppendFrames(aListID, aFrameList);
}

void
nsPopupSetFrame::RemoveFrame(ChildListID     aListID,
                             nsIFrame*       aOldFrame)
{
  if (aListID == kPopupList) {
    RemovePopupFrame(aOldFrame);
    return;
  }
  nsBoxFrame::RemoveFrame(aListID, aOldFrame);
}

void
nsPopupSetFrame::InsertFrames(ChildListID     aListID,
                              nsIFrame*       aPrevFrame,
                              nsFrameList&    aFrameList)
{
  if (aListID == kPopupList) {
    AddPopupFrameList(aFrameList);
    return;
  }
  nsBoxFrame::InsertFrames(aListID, aPrevFrame, aFrameList);
}

void
nsPopupSetFrame::SetInitialChildList(ChildListID     aListID,
                                     nsFrameList&    aChildList)
{
  if (aListID == kPopupList) {
    NS_ASSERTION(mPopupList.IsEmpty(),
                 "SetInitialChildList on non-empty child list");
    AddPopupFrameList(aChildList);
    return;
  }
  nsBoxFrame::SetInitialChildList(aListID, aChildList);
}

const nsFrameList&
nsPopupSetFrame::GetChildList(ChildListID aListID) const
{
  if (kPopupList == aListID) {
    return mPopupList;
  }
  return nsBoxFrame::GetChildList(aListID);
}

void
nsPopupSetFrame::GetChildLists(nsTArray<ChildList>* aLists) const
{
  nsBoxFrame::GetChildLists(aLists);
  mPopupList.AppendIfNonempty(aLists, kPopupList);
}

void
nsPopupSetFrame::DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData& aPostDestroyData)
{
  mPopupList.DestroyFramesFrom(aDestructRoot, aPostDestroyData);

  // Normally the root box is our grandparent, but in case of wrapping
  // it can be our great-grandparent.
  nsIPopupContainer *popupContainer =
    nsIPopupContainer::GetPopupContainer(PresContext()->GetPresShell());
  if (popupContainer) {
    popupContainer->SetPopupSetFrame(nullptr);
  }

  nsBoxFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

NS_IMETHODIMP
nsPopupSetFrame::DoXULLayout(nsBoxLayoutState& aState)
{
  // lay us out
  nsresult rv = nsBoxFrame::DoXULLayout(aState);

  // lay out all of our currently open popups.
  for (nsFrameList::Enumerator e(mPopupList); !e.AtEnd(); e.Next()) {
    nsMenuPopupFrame* popupChild = static_cast<nsMenuPopupFrame*>(e.get());
    popupChild->LayoutPopup(aState, nullptr, nullptr, false);
  }

  return rv;
}

void
nsPopupSetFrame::RemovePopupFrame(nsIFrame* aPopup)
{
  MOZ_ASSERT((aPopup->GetStateBits() & NS_FRAME_OUT_OF_FLOW) &&
             aPopup->IsMenuPopupFrame(),
             "removing wrong type of frame in popupset's ::popupList");

  mPopupList.DestroyFrame(aPopup);
}

void
nsPopupSetFrame::AddPopupFrameList(nsFrameList& aPopupFrameList)
{
#ifdef DEBUG
  for (nsFrameList::Enumerator e(aPopupFrameList); !e.AtEnd(); e.Next()) {
    NS_ASSERTION((e.get()->GetStateBits() & NS_FRAME_OUT_OF_FLOW) &&
                 e.get()->IsMenuPopupFrame(),
                 "adding wrong type of frame in popupset's ::popupList");
  }
#endif
  mPopupList.InsertFrames(nullptr, nullptr, aPopupFrameList);
}
