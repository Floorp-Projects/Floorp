/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsTreeColFrame.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "mozilla/ComputedStyle.h"
#include "nsNameSpaceManager.h"
#include "nsIBoxObject.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/TreeBoxObject.h"
#include "nsTreeColumns.h"
#include "nsDisplayList.h"
#include "nsTreeBodyFrame.h"
#include "nsXULElement.h"

//
// NS_NewTreeColFrame
//
// Creates a new col frame
//
nsIFrame*
NS_NewTreeColFrame(nsIPresShell* aPresShell, ComputedStyle* aStyle)
{
  return new (aPresShell) nsTreeColFrame(aStyle);
}

NS_IMPL_FRAMEARENA_HELPERS(nsTreeColFrame)

// Destructor
nsTreeColFrame::~nsTreeColFrame()
{
}

void
nsTreeColFrame::Init(nsIContent*       aContent,
                     nsContainerFrame* aParent,
                     nsIFrame*         aPrevInFlow)
{
  nsBoxFrame::Init(aContent, aParent, aPrevInFlow);
  InvalidateColumns();
}

void
nsTreeColFrame::DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData& aPostDestroyData)
{
  InvalidateColumns(false);
  nsBoxFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

class nsDisplayXULTreeColSplitterTarget final : public nsDisplayItem
{
public:
  nsDisplayXULTreeColSplitterTarget(nsDisplayListBuilder* aBuilder,
                                    nsIFrame* aFrame) :
    nsDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayXULTreeColSplitterTarget);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayXULTreeColSplitterTarget() {
    MOZ_COUNT_DTOR(nsDisplayXULTreeColSplitterTarget);
  }
#endif

  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState,
                       nsTArray<nsIFrame*> *aOutFrames) override;
  NS_DISPLAY_DECL_NAME("XULTreeColSplitterTarget", TYPE_XUL_TREE_COL_SPLITTER_TARGET)
};

void
nsDisplayXULTreeColSplitterTarget::HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                                           HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames)
{
  nsRect rect = aRect - ToReferenceFrame();
  // If we are in either in the first 4 pixels or the last 4 pixels, we're going to
  // do something really strange.  Check for an adjacent splitter.
  bool left = false;
  bool right = false;
  if (mFrame->GetSize().width - nsPresContext::CSSPixelsToAppUnits(4) <= rect.XMost()) {
    right = true;
  } else if (nsPresContext::CSSPixelsToAppUnits(4) > rect.x) {
    left = true;
  }

  // Swap left and right for RTL trees in order to find the correct splitter
  if (mFrame->StyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL) {
    bool tmp = left;
    left = right;
    right = tmp;
  }

  if (left || right) {
    // We are a header. Look for the correct splitter.
    nsIFrame* child;
    if (left)
      child = mFrame->GetPrevSibling();
    else
      child = mFrame->GetNextSibling();

    if (child && child->GetContent()->NodeInfo()->Equals(nsGkAtoms::splitter,
                                                         kNameSpaceID_XUL)) {
      aOutFrames->AppendElement(child);
    }
  }

}

void
nsTreeColFrame::BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                            const nsDisplayListSet& aLists)
{
  if (!aBuilder->IsForEventDelivery()) {
    nsBoxFrame::BuildDisplayListForChildren(aBuilder, aLists);
    return;
  }

  nsDisplayListCollection set(aBuilder);
  nsBoxFrame::BuildDisplayListForChildren(aBuilder, set);

  WrapListsInRedirector(aBuilder, set, aLists);

  aLists.Content()->AppendToTop(
    MakeDisplayItem<nsDisplayXULTreeColSplitterTarget>(aBuilder, this));
}

nsresult
nsTreeColFrame::AttributeChanged(int32_t aNameSpaceID,
                                 nsAtom* aAttribute,
                                 int32_t aModType)
{
  nsresult rv = nsBoxFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                             aModType);

  if (aAttribute == nsGkAtoms::ordinal || aAttribute == nsGkAtoms::primary) {
    InvalidateColumns();
  }

  return rv;
}

void
nsTreeColFrame::SetXULBounds(nsBoxLayoutState& aBoxLayoutState,
                             const nsRect& aRect,
                             bool aRemoveOverflowArea)
{
  nscoord oldWidth = mRect.width;

  nsBoxFrame::SetXULBounds(aBoxLayoutState, aRect, aRemoveOverflowArea);
  if (mRect.width != oldWidth) {
    nsITreeBoxObject* treeBoxObject = GetTreeBoxObject();
    if (treeBoxObject) {
      treeBoxObject->Invalidate();
    }
  }
}

nsITreeBoxObject*
nsTreeColFrame::GetTreeBoxObject()
{
  nsITreeBoxObject* result = nullptr;

  nsIContent* parent = mContent->GetParent();
  if (parent) {
    nsIContent* grandParent = parent->GetParent();
    RefPtr<nsXULElement> treeElement =
      nsXULElement::FromNodeOrNull(grandParent);
    if (treeElement) {
      nsCOMPtr<nsIBoxObject> boxObject =
        treeElement->GetBoxObject(IgnoreErrors());

      nsCOMPtr<nsITreeBoxObject> treeBoxObject = do_QueryInterface(boxObject);
      result = treeBoxObject.get();
    }
  }
  return result;
}

void
nsTreeColFrame::InvalidateColumns(bool aCanWalkFrameTree)
{
  nsITreeBoxObject* treeBoxObject = GetTreeBoxObject();
  if (treeBoxObject) {
    RefPtr<nsTreeColumns> columns;

    if (aCanWalkFrameTree) {
      treeBoxObject->GetColumns(getter_AddRefs(columns));
    } else {
      nsTreeBodyFrame* body = static_cast<mozilla::dom::TreeBoxObject*>
        (treeBoxObject)->GetCachedTreeBodyFrame();
      if (body) {
        columns = body->Columns();
      }
    }

    if (columns)
      columns->InvalidateColumns();
  }
}
