/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Novell code.
 *
 * The Initial Developer of the Novell Code is Novell Corporation.
 * Portions created by Novell are Copyright (C) 2005 Novell. All Rights Reserved.
 *
 * Contributor(s):
 *     robert@ocallahan.org
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
 */

#include "nsDisplayList.h"

#include "nsCSSRendering.h"
#include "nsISelectionController.h"
#include "nsIPresShell.h"
#include "nsRegion.h"
#include "nsIViewManager.h"
#include "nsIBlender.h"
#include "nsTransform2D.h"

nsDisplayListBuilder::nsDisplayListBuilder(nsIFrame* aReferenceFrame,
    PRBool aIsForEvents, nsIFrame* aMovingFrame)
    : mReferenceFrame(aReferenceFrame),
      mMovingFrame(aMovingFrame),
      mIgnoreScrollFrame(nsnull),
      mEventDelivery(aIsForEvents),
      mIsAtRootOfPseudoStackingContext(PR_FALSE) {
  PL_InitArenaPool(&mPool, "displayListArena", 1024, sizeof(void*)-1);

  nsPresContext* pc = aReferenceFrame->GetPresContext();
  nsIPresShell *shell = pc->PresShell();
  PRBool suppressed;
  shell->IsPaintingSuppressed(&suppressed);
  mIsBackgroundOnly = suppressed;
  if (pc->IsRenderingOnlySelection()) {
    nsCOMPtr<nsISelectionController> selcon(do_QueryInterface(shell));
    if (selcon) {
      selcon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                           getter_AddRefs(mBoundingSelection));
    }
  }
}

nsDisplayListBuilder::~nsDisplayListBuilder() {
  PL_FreeArenaPool(&mPool);
  PL_FinishArenaPool(&mPool);
}

void*
nsDisplayListBuilder::Allocate(size_t aSize) {
  void *tmp;
  PL_ARENA_ALLOCATE(tmp, &mPool, aSize);
  return tmp;
}

void nsDisplayListSet::MoveTo(const nsDisplayListSet& aDestination) const
{
  aDestination.BorderBackground()->AppendToTop(BorderBackground());
  aDestination.BlockBorderBackgrounds()->AppendToTop(BlockBorderBackgrounds());
  aDestination.Floats()->AppendToTop(Floats());
  aDestination.Content()->AppendToTop(Content());
  aDestination.PositionedDescendants()->AppendToTop(PositionedDescendants());
  aDestination.Outlines()->AppendToTop(Outlines());
}

// Suitable for leaf items only, overridden by nsDisplayWrapList
PRBool
nsDisplayItem::OptimizeVisibility(nsDisplayListBuilder* aBuilder,
                                  nsRegion* aVisibleRegion) {
  nsRect bounds = GetBounds(aBuilder);
  if (!aVisibleRegion->Intersects(bounds))
    return PR_FALSE;
  
  nsIFrame* f = GetUnderlyingFrame();
  NS_ASSERTION(f, "GetUnderlyingFrame() must return non-null for leaf items");
  if (aBuilder->HasMovingFrames() && aBuilder->IsMovingFrame(f)) {
    // If this frame is in the moving subtree, and it doesn't
    // require repainting just because it's moved, then just remove it now
    // because it's not relevant.
    if (!IsVaryingRelativeToFrame(aBuilder, aBuilder->GetRootMovingFrame()))
      return PR_FALSE;
    // keep it, but don't let it cover other display items (see nsLayoutUtils::
    // ComputeRepaintRegionForCopy)
    return PR_TRUE;
  }

  if (IsOpaque(aBuilder)) {
    aVisibleRegion->SimpleSubtract(bounds);
  }
  return PR_TRUE;
}

void
nsDisplayList::FlattenTo(nsVoidArray* aElements) {
  nsDisplayItem* item;
  while ((item = RemoveBottom()) != nsnull) {
    if (item->GetType() == nsDisplayItem::TYPE_WRAPLIST) {
      item->GetList()->FlattenTo(aElements);
      item->nsDisplayItem::~nsDisplayItem();
    } else {
      aElements->AppendElement(item);
    }
  }
}

void
nsDisplayList::OptimizeVisibility(nsDisplayListBuilder* aBuilder,
                                  nsRegion* aVisibleRegion) {
  nsVoidArray elements;
  FlattenTo(&elements);
  
  for (PRInt32 i = elements.Count() - 1; i >= 0; --i) {
    nsDisplayItem* item = NS_STATIC_CAST(nsDisplayItem*, elements.ElementAt(i));
    nsDisplayItem* belowItem = i < 1 ? nsnull :
      NS_STATIC_CAST(nsDisplayItem*, elements.ElementAt(i - 1));

    if (belowItem && item->TryMerge(aBuilder, belowItem)) {
      belowItem->nsDisplayItem::~nsDisplayItem();
      elements.ReplaceElementAt(item, i - 1);
      continue;
    }
    
    if (item->OptimizeVisibility(aBuilder, aVisibleRegion)) {
      AppendToBottom(item);
    } else {
      item->nsDisplayItem::~nsDisplayItem();
    }
  }
}

void nsDisplayList::Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
                          const nsRect& aDirtyRect) const {
  for (nsDisplayItem* i = GetBottom(); i != nsnull; i = i->GetAbove()) {
    i->Paint(aBuilder, aCtx, aDirtyRect);
  }
}

PRUint32 nsDisplayList::Count() const {
  PRUint32 count = 0;
  for (nsDisplayItem* i = GetBottom(); i; i = i->GetAbove()) {
    ++count;
  }
  return count;
}

nsDisplayItem* nsDisplayList::RemoveBottom() {
  nsDisplayItem* item = mSentinel.mAbove;
  if (!item)
    return nsnull;
  mSentinel.mAbove = item->mAbove;
  if (item == mTop) {
    // must have been the only item
    mTop = &mSentinel;
  }
  item->mAbove = nsnull;
  return item;
}

void nsDisplayList::DeleteBottom() {
  nsDisplayItem* item = RemoveBottom();
  if (item) {
    item->nsDisplayItem::~nsDisplayItem();
  }
}

void nsDisplayList::DeleteAll() {
  nsDisplayItem* item;
  while ((item = RemoveBottom()) != nsnull) {
    item->nsDisplayItem::~nsDisplayItem();
  }
}

nsIFrame* nsDisplayList::HitTest(nsDisplayListBuilder* aBuilder, nsPoint aPt) const {
  nsVoidArray elements;
  nsDisplayItem* item;
  for (item = GetBottom(); item; item = item->GetAbove()) {
    elements.AppendElement(item);
  }
  for (PRInt32 i = elements.Count() - 1; i >= 0; --i) {
    item = NS_STATIC_CAST(nsDisplayItem*, elements.ElementAt(i));
    if (item->GetBounds(aBuilder).Contains(aPt)) {
      nsIFrame* f = item->HitTest(aBuilder, aPt);
      // Handle the XUL 'mousethrough' feature.
      if (f) {
        PRBool mouseThrough = PR_FALSE;
        f->GetMouseThrough(mouseThrough);
        if (!mouseThrough)
          return f;
      }
    }
  }
  return nsnull;
}

typedef PRBool (* SortLEQ)(nsDisplayItem* aItem1, nsDisplayItem* aItem2,
                           void* aClosure);

static void Sort(nsDisplayList* aList, PRInt32 aCount, SortLEQ aCmp, void* aClosure) {
  if (aCount < 2)
    return;

  nsDisplayList list1;
  nsDisplayList list2;
  int i;
  PRInt32 half = aCount/2;
  PRBool sorted = PR_TRUE;
  nsDisplayItem* prev = nsnull;
  for (i = 0; i < aCount; ++i) {
    nsDisplayItem* item = aList->RemoveBottom();
    (i < half ? &list1 : &list2)->AppendToTop(item);
    if (sorted && prev && !aCmp(prev, item, aClosure)) {
      sorted = PR_FALSE;
    }
    prev = item;
  }
  if (sorted) {
    aList->AppendToTop(&list1);
    aList->AppendToTop(&list2);
    return;
  }
  
  Sort(&list1, half, aCmp, aClosure);
  Sort(&list2, aCount - half, aCmp, aClosure);

  for (i = 0; i < aCount; ++i) {
    if (list1.GetBottom() &&
        (!list2.GetBottom() ||
         aCmp(list1.GetBottom(), list2.GetBottom(), aClosure))) {
      aList->AppendToTop(list1.RemoveBottom());
    } else {
      aList->AppendToTop(list2.RemoveBottom());
    }
  }
}

static PRBool IsContentLEQ(nsDisplayItem* aItem1, nsDisplayItem* aItem2,
                           void* aClosure) {
  // These GetUnderlyingFrame calls return non-null because we're only used
  // in sorting
  return nsLayoutUtils::CompareTreePosition(
      aItem1->GetUnderlyingFrame()->GetContent(),
      aItem2->GetUnderlyingFrame()->GetContent(),
      NS_STATIC_CAST(nsIContent*, aClosure)) <= 0;
}

static PRBool IsZOrderLEQ(nsDisplayItem* aItem1, nsDisplayItem* aItem2,
                          void* aClosure) {
  // These GetUnderlyingFrame calls return non-null because we're only used
  // in sorting
  PRInt32 diff = nsLayoutUtils::GetZIndex(aItem1->GetUnderlyingFrame()) -
    nsLayoutUtils::GetZIndex(aItem2->GetUnderlyingFrame());
  if (diff == 0)
    return IsContentLEQ(aItem1, aItem2, aClosure);
  return diff < 0;
}

void nsDisplayList::ExplodeAnonymousChildLists(nsDisplayListBuilder* aBuilder) {
  // See if there's anything to do
  PRBool anyAnonymousItems = PR_FALSE;
  nsDisplayItem* i;
  for (i = GetBottom(); i != nsnull; i = i->GetAbove()) {
    if (!i->GetUnderlyingFrame()) {
      anyAnonymousItems = PR_TRUE;
      break;
    }
  }
  if (!anyAnonymousItems)
    return;

  nsDisplayList tmp;
  while ((i = RemoveBottom()) != nsnull) {
    if (i->GetUnderlyingFrame()) {
      tmp.AppendToTop(i);
    } else {
      nsDisplayList* list = i->GetList();
      NS_ASSERTION(list, "leaf items can't be anonymous");
      list->ExplodeAnonymousChildLists(aBuilder);
      nsDisplayItem* j;
      while ((j = list->RemoveBottom()) != nsnull) {
        tmp.AppendToTop(NS_STATIC_CAST(nsDisplayWrapList*, i)->
            WrapWithClone(aBuilder, j));
      }
      i->nsDisplayItem::~nsDisplayItem();
    }
  }
  
  AppendToTop(&tmp);
}

void nsDisplayList::SortByZOrder(nsDisplayListBuilder* aBuilder,
                                 nsIContent* aCommonAncestor) {
  ExplodeAnonymousChildLists(aBuilder);
  Sort(this, Count(), IsZOrderLEQ, aCommonAncestor);
}

void nsDisplayList::SortByContentOrder(nsDisplayListBuilder* aBuilder,
                                       nsIContent* aCommonAncestor) {
  ExplodeAnonymousChildLists(aBuilder);
  Sort(this, Count(), IsContentLEQ, aCommonAncestor);
}

static PRBool
NonZeroStyleCoord(const nsStyleCoord& aCoord) {
  switch (aCoord.GetUnit()) {
  case eStyleUnit_Percent:
    return aCoord.GetPercentValue() > 0;
  case eStyleUnit_Coord:
    return aCoord.GetCoordValue() > 0;
  case eStyleUnit_Null:
    return PR_FALSE;
  default:
    return PR_TRUE;
  }
}

static PRBool
HasNonZeroBorderRadius(const nsStyleBorder* aBorder) {
  nsStyleCoord coord;
  aBorder->mBorderRadius.GetTop(coord);
  if (NonZeroStyleCoord(coord)) return PR_TRUE;    
  aBorder->mBorderRadius.GetRight(coord);
  if (NonZeroStyleCoord(coord)) return PR_TRUE;    
  aBorder->mBorderRadius.GetBottom(coord);
  if (NonZeroStyleCoord(coord)) return PR_TRUE;    
  aBorder->mBorderRadius.GetLeft(coord);
  if (NonZeroStyleCoord(coord)) return PR_TRUE;    

  return PR_FALSE;
}

PRBool
nsDisplayBackground::IsOpaque(nsDisplayListBuilder* aBuilder) {
  // theme background overrides any other background
  if (mFrame->IsThemed())
    return PR_FALSE;

  PRBool isCanvas;
  const nsStyleBackground* bg;
  PRBool hasBG =
    nsCSSRendering::FindBackground(mFrame->GetPresContext(), mFrame, &bg, &isCanvas);
  if (!hasBG || (bg->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT) ||
      bg->mBackgroundClip != NS_STYLE_BG_CLIP_BORDER ||
      HasNonZeroBorderRadius(mFrame->GetStyleBorder()) ||
      NS_GET_A(bg->mBackgroundColor) < 255)
    return PR_FALSE;
  return PR_TRUE;
}

PRBool
nsDisplayBackground::IsUniform(nsDisplayListBuilder* aBuilder) {
  // theme background overrides any other background
  if (mFrame->IsThemed())
    return PR_FALSE;

  PRBool isCanvas;
  const nsStyleBackground* bg;
  PRBool hasBG =
    nsCSSRendering::FindBackground(mFrame->GetPresContext(), mFrame, &bg, &isCanvas);
  if (!hasBG)
    return PR_TRUE;
  if ((bg->mBackgroundFlags & NS_STYLE_BG_IMAGE_NONE) &&
      !HasNonZeroBorderRadius(mFrame->GetStyleBorder()) &&
      bg->mBackgroundClip == NS_STYLE_BG_CLIP_BORDER)
    return PR_TRUE;
  return PR_FALSE;
}

PRBool
nsDisplayBackground::IsVaryingRelativeToFrame(nsDisplayListBuilder* aBuilder,
    nsIFrame* aAncestorFrame)
{
  PRBool isCanvas;
  const nsStyleBackground* bg;
  PRBool hasBG =
    nsCSSRendering::FindBackground(mFrame->GetPresContext(), mFrame, &bg, &isCanvas);
  if (!hasBG)
    return PR_FALSE;
  if (!bg->HasFixedBackground())
    return PR_FALSE;

  // aAncestorFrame is an ancestor of this frame ... if it's in our document
  // then we'll be moving relative to the viewport, so we will change our
  // display. If it's in some ancestor document then we won't be moving
  // relative to the viewport so we won't change our display.
  for (nsIFrame* f = mFrame->GetParent(); f; f = f->GetParent()) {
    if (f == aAncestorFrame)
      return PR_TRUE;
  }
  return PR_FALSE;
}

void
nsDisplayBackground::Paint(nsDisplayListBuilder* aBuilder,
     nsIRenderingContext* aCtx, const nsRect& aDirtyRect) {
  nsPoint offset = aBuilder->ToReferenceFrame(mFrame);
  nsCSSRendering::PaintBackground(mFrame->GetPresContext(), *aCtx, mFrame,
                                  aDirtyRect, nsRect(offset, mFrame->GetSize()),
                                  *mFrame->GetStyleBorder(),
                                  *mFrame->GetStylePadding(),
                                  mFrame->HonorPrintBackgroundSettings());
}

nsRect
nsDisplayOutline::GetBounds(nsDisplayListBuilder* aBuilder) {
  return mFrame->GetOverflowRect() + aBuilder->ToReferenceFrame(mFrame);
}

void
nsDisplayOutline::Paint(nsDisplayListBuilder* aBuilder,
     nsIRenderingContext* aCtx, const nsRect& aDirtyRect) {
  // TODO join outlines together
  nsPoint offset = aBuilder->ToReferenceFrame(mFrame);
  nsCSSRendering::PaintOutline(mFrame->GetPresContext(), *aCtx, mFrame,
                               aDirtyRect, nsRect(offset, mFrame->GetSize()),
                               *mFrame->GetStyleBorder(),
                               *mFrame->GetStyleOutline(),                              
                               mFrame->GetStyleContext(), 0);
}

void
nsDisplayBorder::Paint(nsDisplayListBuilder* aBuilder,
     nsIRenderingContext* aCtx, const nsRect& aDirtyRect) {
  nsPoint offset = aBuilder->ToReferenceFrame(mFrame);
  nsCSSRendering::PaintBorder(mFrame->GetPresContext(), *aCtx, mFrame,
                              aDirtyRect, nsRect(offset, mFrame->GetSize()),
                              *mFrame->GetStyleBorder(),
                              mFrame->GetStyleContext(), mFrame->GetSkipSides());
}

nsDisplayWrapList::nsDisplayWrapList(nsIFrame* aFrame, nsDisplayList* aList)
  : mFrame(aFrame) {
  mList.AppendToTop(aList);
}

nsDisplayWrapList::nsDisplayWrapList(nsIFrame* aFrame, nsDisplayItem* aItem)
  : mFrame(aFrame) {
  mList.AppendToTop(aItem);
}

nsDisplayWrapList::~nsDisplayWrapList() {
  mList.DeleteAll();
}

nsIFrame*
nsDisplayWrapList::HitTest(nsDisplayListBuilder* aBuilder, nsPoint aPt) {
  return mList.HitTest(aBuilder, aPt);
}

nsRect
nsDisplayWrapList::GetBounds(nsDisplayListBuilder* aBuilder) {
  nsRect bounds;
  for (nsDisplayItem* i = mList.GetBottom(); i != nsnull; i = i->GetAbove()) {
    bounds.UnionRect(bounds, i->GetBounds(aBuilder));
  }
  return bounds;
}

PRBool
nsDisplayWrapList::OptimizeVisibility(nsDisplayListBuilder* aBuilder,
                                      nsRegion* aVisibleRegion) {
  mList.OptimizeVisibility(aBuilder, aVisibleRegion);
  // If none of the items are visible, they will all have been deleted
  return mList.GetTop() != nsnull;
}

PRBool
nsDisplayWrapList::IsOpaque(nsDisplayListBuilder* aBuilder) {
  // We could try to do something but let's conservatively just return PR_FALSE.
  // We reimplement OptimizeVisibility and that's what really matters
  return PR_FALSE;
}

PRBool nsDisplayWrapList::IsUniform(nsDisplayListBuilder* aBuilder) {
  // We could try to do something but let's conservatively just return PR_FALSE.
  return PR_FALSE;
}

PRBool nsDisplayWrapList::IsVaryingRelativeToFrame(nsDisplayListBuilder* aBuilder,
                                                   nsIFrame* aFrame) {
  for (nsDisplayItem* i = mList.GetBottom(); i != nsnull; i = i->GetAbove()) {
    if (i->IsVaryingRelativeToFrame(aBuilder, aFrame))
      return PR_TRUE;
  }
  return PR_FALSE;
}

void nsDisplayWrapList::Paint(nsDisplayListBuilder* aBuilder,
     nsIRenderingContext* aCtx, const nsRect& aDirtyRect) {
  mList.Paint(aBuilder, aCtx, aDirtyRect);
}

static nsresult
WrapDisplayList(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                nsDisplayList* aList, nsDisplayWrapper* aWrapper) {
  if (!aList->GetTop())
    return NS_OK;
  nsDisplayItem* item = aWrapper->WrapList(aBuilder, aFrame, aList);
  if (!item)
    return NS_ERROR_OUT_OF_MEMORY;
  // aList was emptied
  aList->AppendToTop(item);
  return NS_OK;
}

static nsresult
WrapEachDisplayItem(nsDisplayListBuilder* aBuilder,
                    nsDisplayList* aList, nsDisplayWrapper* aWrapper) {
  nsDisplayList newList;
  nsDisplayItem* item;
  while ((item = aList->RemoveBottom())) {
    item = aWrapper->WrapItem(aBuilder, item);
    if (!item)
      return NS_ERROR_OUT_OF_MEMORY;
    newList.AppendToTop(item);
  }
  // aList was emptied
  aList->AppendToTop(&newList);
  return NS_OK;
}

nsresult nsDisplayWrapper::WrapLists(nsDisplayListBuilder* aBuilder,
    nsIFrame* aFrame, const nsDisplayListSet& aIn, const nsDisplayListSet& aOut)
{
  nsresult rv = WrapListsInPlace(aBuilder, aFrame, aIn);
  NS_ENSURE_SUCCESS(rv, rv);

  if (&aOut == &aIn)
    return NS_OK;
  aOut.BorderBackground()->AppendToTop(aIn.BorderBackground());
  aOut.BlockBorderBackgrounds()->AppendToTop(aIn.BlockBorderBackgrounds());
  aOut.Floats()->AppendToTop(aIn.Floats());
  aOut.Content()->AppendToTop(aIn.Content());
  aOut.PositionedDescendants()->AppendToTop(aIn.PositionedDescendants());
  aOut.Outlines()->AppendToTop(aIn.Outlines());
  return NS_OK;
}

nsresult nsDisplayWrapper::WrapListsInPlace(nsDisplayListBuilder* aBuilder,
    nsIFrame* aFrame, const nsDisplayListSet& aLists)
{
  nsresult rv;
  if (WrapBorderBackground()) {
    // Our border-backgrounds are in-flow
    rv = WrapDisplayList(aBuilder, aFrame, aLists.BorderBackground(), this);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  // Our block border-backgrounds are in-flow
  rv = WrapDisplayList(aBuilder, aFrame, aLists.BlockBorderBackgrounds(), this);
  NS_ENSURE_SUCCESS(rv, rv);
  // The floats are not in flow
  rv = WrapEachDisplayItem(aBuilder, aLists.Floats(), this);
  NS_ENSURE_SUCCESS(rv, rv);
  // Our child content is in flow
  rv = WrapDisplayList(aBuilder, aFrame, aLists.Content(), this);
  NS_ENSURE_SUCCESS(rv, rv);
  // The positioned descendants may not be in-flow
  rv = WrapEachDisplayItem(aBuilder, aLists.PositionedDescendants(), this);
  NS_ENSURE_SUCCESS(rv, rv);
  // The outlines may not be in-flow
  return WrapEachDisplayItem(aBuilder, aLists.Outlines(), this);
}

nsDisplayOpacity::nsDisplayOpacity(nsIFrame* aFrame, nsDisplayList* aList)
    : nsDisplayWrapList(aFrame, aList), mNeedAlpha(PR_TRUE) {
}

PRBool nsDisplayOpacity::IsOpaque(nsDisplayListBuilder* aBuilder) {
  // We are never opaque, if our opacity was < 1 then we wouldn't have
  // been created.
  return PR_FALSE;
}

void nsDisplayOpacity::Paint(nsDisplayListBuilder* aBuilder,
                             nsIRenderingContext* aCtx, const nsRect& aDirtyRect)
{
  // XXX This way of handling 'opacity' creates exponential time blowup in the
  // depth of nested translucent elements. This will be fixed when we move to
  // cairo with support for real alpha channels in surfaces, so we don't have
  // to do this white/black hack anymore.
  nsIViewManager* vm = mFrame->GetPresContext()->GetViewManager();
  nsRect bounds;
  bounds.IntersectRect(GetBounds(aBuilder), aDirtyRect);
  nsIViewManager::BlendingBuffers* buffers =
      vm->CreateBlendingBuffers(aCtx, PR_FALSE, nsnull, mNeedAlpha, bounds);
  if (!buffers) {
    NS_WARNING("Could not create blending buffers for translucent painting; ignoring opacity");
    nsDisplayWrapList::Paint(aBuilder, aCtx, aDirtyRect);
    return;
  }
  
  // Paint onto black, and also onto white if necessary
  nsDisplayWrapList::Paint(aBuilder, buffers->mBlackCX, bounds);
  if (buffers->mWhiteCX) {
    nsDisplayWrapList::Paint(aBuilder, buffers->mWhiteCX, bounds);
  }

  nsTransform2D* transform;
  nsresult rv = aCtx->GetCurrentTransform(transform);
  if (NS_FAILED(rv))
    return;

  nsRect damageRectInPixels = bounds;
  transform->TransformCoord(&damageRectInPixels.x, &damageRectInPixels.y,
                            &damageRectInPixels.width, &damageRectInPixels.height);
  // If blender creation failed then we would have not received a buffers object
  nsIBlender* blender = vm->GetBlender();
  blender->Blend(0, 0, damageRectInPixels.width, damageRectInPixels.height,
                 buffers->mBlackCX, aCtx,
                 damageRectInPixels.x, damageRectInPixels.y,
                 mFrame->GetStyleDisplay()->mOpacity, buffers->mWhiteCX,
                 NS_RGB(0, 0, 0), NS_RGB(255, 255, 255));
  delete buffers;
}

PRBool nsDisplayOpacity::OptimizeVisibility(nsDisplayListBuilder* aBuilder,
                                            nsRegion* aVisibleRegion) {
  // Our children are translucent so we should not allow them to subtract
  // area from aVisibleRegion. We do need to find out what is visible under
  // our children in the temporary compositing buffer, because if our children
  // paint our entire bounds opaquely then we don't need an alpha channel in
  // the temporary compositing buffer.
  nsRegion visibleUnderChildren = *aVisibleRegion;
  PRBool anyVisibleChildren =
    nsDisplayWrapList::OptimizeVisibility(aBuilder, &visibleUnderChildren);
  if (!anyVisibleChildren)
    return PR_FALSE;

  mNeedAlpha = visibleUnderChildren.Intersects(GetBounds(aBuilder));
  return PR_TRUE;
}

PRBool nsDisplayOpacity::TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem) {
  if (aItem->GetType() != TYPE_OPACITY)
    return PR_FALSE;
  // items for the same content element should be merged into a single
  // compositing group
  // aItem->GetUnderlyingFrame() returns non-null because it's nsDisplayOpacity
  if (aItem->GetUnderlyingFrame()->GetContent() != mFrame->GetContent())
    return PR_FALSE;
  mList.AppendToBottom(&NS_STATIC_CAST(nsDisplayOpacity*, aItem)->mList);
  return PR_TRUE;
}

nsDisplayClip::nsDisplayClip(nsIFrame* aFrame, nsDisplayItem* aItem,
    const nsRect& aRect)
   : nsDisplayWrapList(aFrame, aItem), mClip(aRect) {
}

nsDisplayClip::nsDisplayClip(nsIFrame* aFrame, nsDisplayList* aList,
    const nsRect& aRect)
   : nsDisplayWrapList(aFrame, aList), mClip(aRect) {
}

nsRect nsDisplayClip::GetBounds(nsDisplayListBuilder* aBuilder) {
  nsRect r = nsDisplayWrapList::GetBounds(aBuilder);
  r.IntersectRect(mClip, r);
  return r;
}

void nsDisplayClip::Paint(nsDisplayListBuilder* aBuilder,
     nsIRenderingContext* aCtx, const nsRect& aDirtyRect) {
  nsRect dirty;
  dirty.IntersectRect(mClip, aDirtyRect);
  aCtx->PushState();
  aCtx->SetClipRect(dirty, nsClipCombine_kIntersect);
  nsDisplayWrapList::Paint(aBuilder, aCtx, dirty);
  aCtx->PopState();
}

PRBool nsDisplayClip::OptimizeVisibility(nsDisplayListBuilder* aBuilder,
                                         nsRegion* aVisibleRegion) {
  nsRegion clipped;
  clipped.And(*aVisibleRegion, mClip);
  nsRegion rNew(clipped);
  PRBool anyVisible = nsDisplayWrapList::OptimizeVisibility(aBuilder, &rNew);
  nsRegion subtracted;
  subtracted.Sub(clipped, rNew);
  aVisibleRegion->SimpleSubtract(subtracted);
  return anyVisible;
}

PRBool nsDisplayClip::TryMerge(nsDisplayListBuilder* aBuilder,
                               nsDisplayItem* aItem) {
  if (aItem->GetType() != TYPE_CLIP)
    return PR_FALSE;
  nsDisplayClip* other = NS_STATIC_CAST(nsDisplayClip*, aItem);
  if (other->mClip != mClip)
    return PR_FALSE;
  mList.AppendToBottom(&other->mList);
  return PR_TRUE;
}

nsDisplayWrapList* nsDisplayClip::WrapWithClone(nsDisplayListBuilder* aBuilder,
                                                nsDisplayItem* aItem) {
  return new (aBuilder) nsDisplayClip(aItem->GetUnderlyingFrame(), aItem, mClip);
}
