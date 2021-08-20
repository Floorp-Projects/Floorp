/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "RetainedDisplayListBuilder.h"

#include "mozilla/StaticPrefs_layout.h"
#include "nsIFrame.h"
#include "nsIFrameInlines.h"
#include "nsIScrollableFrame.h"
#include "nsPlaceholderFrame.h"
#include "nsSubDocumentFrame.h"
#include "nsViewManager.h"
#include "nsCanvasFrame.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/DisplayPortUtils.h"
#include "mozilla/PresShell.h"
#include "mozilla/ProfilerLabels.h"

/**
 * Code for doing display list building for a modified subset of the window,
 * and then merging it into the existing display list (for the full window).
 *
 * The approach primarily hinges on the observation that the 'true' ordering
 * of display items is represented by a DAG (only items that intersect in 2d
 * space have a defined ordering). Our display list is just one of a many
 * possible linear representations of this ordering.
 *
 * Each time a frame changes (gets a new ComputedStyle, or has a size/position
 * change), we schedule a paint (as we do currently), but also reord the frame
 * that changed.
 *
 * When the next paint occurs we union the overflow areas (in screen space) of
 * the changed frames, and compute a rect/region that contains all changed
 * items. We then build a display list just for this subset of the screen and
 * merge it into the display list from last paint.
 *
 * Any items that exist in one list and not the other must not have a defined
 * ordering in the DAG, since they need to intersect to have an ordering and
 * we would have built both in the new list if they intersected. Given that, we
 * can align items that appear in both lists, and any items that appear between
 * matched items can be inserted into the merged list in any order.
 */

using mozilla::dom::Document;

namespace mozilla {

void RetainedDisplayListData::AddModifiedFrame(nsIFrame* aFrame) {
  MOZ_ASSERT(!aFrame->IsFrameModified());
  Flags(aFrame) |= RetainedDisplayListData::FrameFlags::Modified;
  mModifiedFramesCount++;
}

RetainedDisplayListData* GetRetainedDisplayListData(nsIFrame* aRootFrame) {
  RetainedDisplayListData* data =
      aRootFrame->GetProperty(RetainedDisplayListData::DisplayListData());

  return data;
}

RetainedDisplayListData* GetOrSetRetainedDisplayListData(nsIFrame* aRootFrame) {
  RetainedDisplayListData* data = GetRetainedDisplayListData(aRootFrame);

  if (!data) {
    data = new RetainedDisplayListData();
    aRootFrame->SetProperty(RetainedDisplayListData::DisplayListData(), data);
  }

  MOZ_ASSERT(data);
  return data;
}

static void MarkFramesWithItemsAndImagesModified(nsDisplayList* aList) {
  for (nsDisplayItem* i : *aList) {
    if (!i->HasDeletedFrame() && i->CanBeReused() &&
        !i->Frame()->IsFrameModified()) {
      // If we have existing cached geometry for this item, then check that for
      // whether we need to invalidate for a sync decode. If we don't, then
      // use the item's flags.
      DisplayItemData* data = FrameLayerBuilder::GetOldDataFor(i);
      // XXX: handle webrender case
      bool invalidate = false;
      if (data && data->GetGeometry()) {
        invalidate = data->GetGeometry()->InvalidateForSyncDecodeImages();
      } else if (!(i->GetFlags() & TYPE_RENDERS_NO_IMAGES)) {
        invalidate = true;
      }

      if (invalidate) {
        i->FrameForInvalidation()->MarkNeedsDisplayItemRebuild();
        if (i->GetDependentFrame()) {
          i->GetDependentFrame()->MarkNeedsDisplayItemRebuild();
        }
      }
    }
    if (i->GetChildren()) {
      MarkFramesWithItemsAndImagesModified(i->GetChildren());
    }
  }
}

static AnimatedGeometryRoot* SelectAGRForFrame(
    nsIFrame* aFrame, AnimatedGeometryRoot* aParentAGR) {
  if (!aFrame->IsStackingContext() || !aFrame->IsFixedPosContainingBlock()) {
    return aParentAGR;
  }

  if (!aFrame->HasOverrideDirtyRegion()) {
    return nullptr;
  }

  nsDisplayListBuilder::DisplayListBuildingData* data =
      aFrame->GetProperty(nsDisplayListBuilder::DisplayListBuildingRect());

  return data && data->mModifiedAGR ? data->mModifiedAGR.get() : nullptr;
}

void RetainedDisplayListBuilder::AddSizeOfIncludingThis(
    nsWindowSizes& aSizes) const {
  aSizes.mLayoutRetainedDisplayListSize += aSizes.mState.mMallocSizeOf(this);
  mBuilder.AddSizeOfExcludingThis(aSizes);
  mList.AddSizeOfExcludingThis(aSizes);
}

bool AnyContentAncestorModified(nsIFrame* aFrame, nsIFrame* aStopAtFrame) {
  nsIFrame* f = aFrame;
  while (f) {
    if (f->IsFrameModified()) {
      return true;
    }

    if (aStopAtFrame && f == aStopAtFrame) {
      break;
    }

    f = nsLayoutUtils::GetDisplayListParent(f);
  }

  return false;
}

// Removes any display items that belonged to a frame that was deleted,
// and mark frames that belong to a different AGR so that get their
// items built again.
// TODO: We currently descend into all children even if we don't have an AGR
// to mark, as child stacking contexts might. It would be nice if we could
// jump into those immediately rather than walking the entire thing.
bool RetainedDisplayListBuilder::PreProcessDisplayList(
    RetainedDisplayList* aList, AnimatedGeometryRoot* aAGR,
    PartialUpdateResult& aUpdated, nsIFrame* aOuterFrame, uint32_t aCallerKey,
    uint32_t aNestingDepth, bool aKeepLinked) {
  // The DAG merging algorithm does not have strong mechanisms in place to keep
  // the complexity of the resulting DAG under control. In some cases we can
  // build up edges very quickly. Detect those cases and force a full display
  // list build if we hit them.
  static const uint32_t kMaxEdgeRatio = 5;
  const bool initializeDAG = !aList->mDAG.Length();
  if (!aKeepLinked && !initializeDAG &&
      aList->mDAG.mDirectPredecessorList.Length() >
          (aList->mDAG.mNodesInfo.Length() * kMaxEdgeRatio)) {
    return false;
  }

  // If we had aKeepLinked=true for this list on the previous paint, then
  // mOldItems will already be initialized as it won't have been consumed during
  // a merge.
  const bool initializeOldItems = aList->mOldItems.IsEmpty();
  if (initializeOldItems) {
    aList->mOldItems.SetCapacity(aList->Count());
  } else {
    MOZ_RELEASE_ASSERT(!initializeDAG);
  }

  MOZ_RELEASE_ASSERT(
      initializeDAG ||
      aList->mDAG.Length() ==
          (initializeOldItems ? aList->Count() : aList->mOldItems.Length()));

  nsDisplayList out;

  size_t i = 0;
  while (nsDisplayItem* item = aList->RemoveBottom()) {
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
    item->SetMergedPreProcessed(false, true);
#endif

    // If we have a previously initialized old items list, then it can differ
    // from the current list due to items removed for having a deleted frame.
    // We can't easily remove these, since the DAG has entries for those indices
    // and it's hard to rewrite in-place.
    // Skip over entries with no current item to keep the iterations in sync.
    if (!initializeOldItems) {
      while (!aList->mOldItems[i].mItem) {
        i++;
      }
    }

    if (initializeDAG) {
      if (i == 0) {
        aList->mDAG.AddNode(Span<const MergedListIndex>());
      } else {
        MergedListIndex previous(i - 1);
        aList->mDAG.AddNode(Span<const MergedListIndex>(&previous, 1));
      }
    }

    if (!item->CanBeReused() || item->HasDeletedFrame() ||
        AnyContentAncestorModified(item->FrameForInvalidation(), aOuterFrame)) {
      if (initializeOldItems) {
        aList->mOldItems.AppendElement(OldItemInfo(nullptr));
      } else {
        MOZ_RELEASE_ASSERT(aList->mOldItems[i].mItem == item);
        aList->mOldItems[i].mItem = nullptr;
      }

      if (item->IsGlassItem() && item == mBuilder.GetGlassDisplayItem()) {
        mBuilder.ClearGlassDisplayItem();
      }

      item->Destroy(&mBuilder);
      Metrics()->mRemovedItems++;

      i++;
      aUpdated = PartialUpdateResult::Updated;
      continue;
    }

    if (initializeOldItems) {
      aList->mOldItems.AppendElement(OldItemInfo(item));
    }

    // If we're not going to keep the list linked, then this old item entry
    // is the only pointer to the item. Let it know that it now strongly
    // owns the item, so it can destroy it if it goes away.
    aList->mOldItems[i].mOwnsItem = !aKeepLinked;

    item->SetOldListIndex(aList, OldListIndex(i), aCallerKey, aNestingDepth);

    nsIFrame* f = item->Frame();

    if (item->GetChildren()) {
      // If children inside this list were invalid, then we'd have walked the
      // ancestors and set ForceDescendIntoVisible on the current frame. If an
      // ancestor is modified, then we'll throw this away entirely. Either way,
      // we won't need to run merging on this sublist, and we can keep the items
      // linked into their display list.
      // The caret can move without invalidating, but we always set the force
      // descend into frame state bit on that frame, so check for that too.
      // TODO: AGR marking below can call MarkFrameForDisplayIfVisible and make
      // us think future siblings need to be merged, even though we don't really
      // need to.
      bool keepLinked = aKeepLinked;
      nsIFrame* invalid = item->FrameForInvalidation();
      if (!invalid->ForceDescendIntoIfVisible() &&
          !invalid->HasAnyStateBits(NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO)) {
        keepLinked = true;
      }

      if (!PreProcessDisplayList(item->GetChildren(),
                                 SelectAGRForFrame(f, aAGR), aUpdated,
                                 item->Frame(), item->GetPerFrameKey(),
                                 aNestingDepth + 1, keepLinked)) {
        MOZ_RELEASE_ASSERT(
            !aKeepLinked,
            "Can't early return since we need to move the out list back");
        return false;
      }
    }

    // TODO: We should be able to check the clipped bounds relative
    // to the common AGR (of both the existing item and the invalidated
    // frame) and determine if they can ever intersect.
    // TODO: We only really need to build the ancestor container item that is a
    // sibling of the changed thing to get correct ordering. The changed content
    // is a frame though, and it's hard to map that to container items in this
    // list.
    if (aAGR && item->GetAnimatedGeometryRoot()->GetAsyncAGR() != aAGR) {
      mBuilder.MarkFrameForDisplayIfVisible(f, mBuilder.RootReferenceFrame());
    }

    // TODO: This is here because we sometimes reuse the previous display list
    // completely. For optimization, we could only restore the state for reused
    // display items.
    if (item->RestoreState()) {
      item->InvalidateItemCacheEntry();
    }

    // If we're going to keep this linked list and not merge it, then mark the
    // item as used and put it back into the list.
    if (aKeepLinked) {
      item->SetReused(true);
      if (item->GetChildren()) {
        item->UpdateBounds(Builder());
      }
      if (item->GetType() == DisplayItemType::TYPE_SUBDOCUMENT) {
        IncrementSubDocPresShellPaintCount(item);
      }
      out.AppendToTop(item);
    }
    i++;
  }

  MOZ_RELEASE_ASSERT(aList->mOldItems.Length() == aList->mDAG.Length());
  aList->RestoreState();

  if (aKeepLinked) {
    aList->AppendToTop(&out);
  }
  return true;
}

void RetainedDisplayListBuilder::IncrementSubDocPresShellPaintCount(
    nsDisplayItem* aItem) {
  MOZ_ASSERT(aItem->GetType() == DisplayItemType::TYPE_SUBDOCUMENT);

  nsSubDocumentFrame* subDocFrame =
      static_cast<nsDisplaySubDocument*>(aItem)->SubDocumentFrame();
  MOZ_ASSERT(subDocFrame);

  PresShell* presShell = subDocFrame->GetSubdocumentPresShellForPainting(0);
  MOZ_ASSERT(presShell);

  mBuilder.IncrementPresShellPaintCount(presShell);
}

static Maybe<const ActiveScrolledRoot*> SelectContainerASR(
    const DisplayItemClipChain* aClipChain, const ActiveScrolledRoot* aItemASR,
    Maybe<const ActiveScrolledRoot*>& aContainerASR) {
  const ActiveScrolledRoot* itemClipASR =
      aClipChain ? aClipChain->mASR : nullptr;

  const ActiveScrolledRoot* finiteBoundsASR =
      ActiveScrolledRoot::PickDescendant(itemClipASR, aItemASR);

  if (!aContainerASR) {
    return Some(finiteBoundsASR);
  }

  return Some(
      ActiveScrolledRoot::PickAncestor(*aContainerASR, finiteBoundsASR));
}

static void UpdateASR(nsDisplayItem* aItem,
                      Maybe<const ActiveScrolledRoot*>& aContainerASR) {
  if (!aContainerASR) {
    return;
  }

  nsDisplayWrapList* wrapList = aItem->AsDisplayWrapList();
  if (!wrapList) {
    aItem->SetActiveScrolledRoot(*aContainerASR);
    return;
  }

  wrapList->SetActiveScrolledRoot(ActiveScrolledRoot::PickAncestor(
      wrapList->GetFrameActiveScrolledRoot(), *aContainerASR));
}

static void CopyASR(nsDisplayItem* aOld, nsDisplayItem* aNew) {
  aNew->SetActiveScrolledRoot(aOld->GetActiveScrolledRoot());
}

OldItemInfo::OldItemInfo(nsDisplayItem* aItem)
    : mItem(aItem), mUsed(false), mDiscarded(false), mOwnsItem(false) {
  if (mItem) {
    // Clear cached modified frame state when adding an item to the old list.
    mItem->SetModifiedFrame(false);
  }
}

void OldItemInfo::AddedMatchToMergedList(RetainedDisplayListBuilder* aBuilder,
                                         MergedListIndex aIndex) {
  AddedToMergedList(aIndex);
}

void OldItemInfo::Discard(RetainedDisplayListBuilder* aBuilder,
                          nsTArray<MergedListIndex>&& aDirectPredecessors) {
  MOZ_ASSERT(!IsUsed());
  mUsed = mDiscarded = true;
  mDirectPredecessors = std::move(aDirectPredecessors);
  if (mItem) {
    MOZ_ASSERT(mOwnsItem);
    mItem->Destroy(aBuilder->Builder());
    aBuilder->Metrics()->mRemovedItems++;
  }
  mItem = nullptr;
}

bool OldItemInfo::IsChanged() {
  return !mItem || !mItem->CanBeReused() || mItem->HasDeletedFrame();
}

/**
 * A C++ implementation of Markus Stange's merge-dags algorithm.
 * https://github.com/mstange/merge-dags
 *
 * MergeState handles combining a new list of display items into an existing
 * DAG and computes the new DAG in a single pass.
 * Each time we add a new item, we resolve all dependencies for it, so that the
 * resulting list and DAG are built in topological ordering.
 */
class MergeState {
 public:
  MergeState(RetainedDisplayListBuilder* aBuilder,
             RetainedDisplayList& aOldList, nsDisplayItem* aOuterItem)
      : mBuilder(aBuilder),
        mOldList(&aOldList),
        mOldItems(std::move(aOldList.mOldItems)),
        mOldDAG(
            std::move(*reinterpret_cast<DirectedAcyclicGraph<OldListUnits>*>(
                &aOldList.mDAG))),
        mOuterItem(aOuterItem),
        mResultIsModified(false) {
    mMergedDAG.EnsureCapacityFor(mOldDAG);
    MOZ_RELEASE_ASSERT(mOldItems.Length() == mOldDAG.Length());
  }

  Maybe<MergedListIndex> ProcessItemFromNewList(
      nsDisplayItem* aNewItem, const Maybe<MergedListIndex>& aPreviousItem) {
    OldListIndex oldIndex;
    MOZ_DIAGNOSTIC_ASSERT(aNewItem->HasModifiedFrame() ==
                          HasModifiedFrame(aNewItem));
    if (!aNewItem->HasModifiedFrame() &&
        HasMatchingItemInOldList(aNewItem, &oldIndex)) {
      mBuilder->Metrics()->mRebuiltItems++;
      nsDisplayItem* oldItem = mOldItems[oldIndex.val].mItem;
      MOZ_DIAGNOSTIC_ASSERT(oldItem->GetPerFrameKey() ==
                                aNewItem->GetPerFrameKey() &&
                            oldItem->Frame() == aNewItem->Frame());
      if (!mOldItems[oldIndex.val].IsChanged()) {
        MOZ_DIAGNOSTIC_ASSERT(!mOldItems[oldIndex.val].IsUsed());
        nsDisplayItem* destItem;
        if (ShouldUseNewItem(aNewItem)) {
          destItem = aNewItem;
        } else {
          destItem = oldItem;
          // The building rect can depend on the overflow rect (when the parent
          // frame is position:fixed), which can change without invalidating
          // the frame/items. If we're using the old item, copy the building
          // rect across from the new item.
          oldItem->SetBuildingRect(aNewItem->GetBuildingRect());
        }

        if (destItem == aNewItem) {
          if (oldItem->IsGlassItem() &&
              oldItem == mBuilder->Builder()->GetGlassDisplayItem()) {
            mBuilder->Builder()->ClearGlassDisplayItem();
          }
        }  // aNewItem can't be the glass item on the builder yet.

        if (destItem->IsGlassItem()) {
          if (destItem != oldItem ||
              destItem != mBuilder->Builder()->GetGlassDisplayItem()) {
            mBuilder->Builder()->SetGlassDisplayItem(destItem);
          }
        }

        MergeChildLists(aNewItem, oldItem, destItem);

        AutoTArray<MergedListIndex, 2> directPredecessors =
            ProcessPredecessorsOfOldNode(oldIndex);
        MergedListIndex newIndex = AddNewNode(
            destItem, Some(oldIndex), directPredecessors, aPreviousItem);
        mOldItems[oldIndex.val].AddedMatchToMergedList(mBuilder, newIndex);
        if (destItem == aNewItem) {
          oldItem->Destroy(mBuilder->Builder());
        } else {
          aNewItem->Destroy(mBuilder->Builder());
        }
        return Some(newIndex);
      }
    }
    mResultIsModified = true;
    if (aNewItem->IsGlassItem()) {
      mBuilder->Builder()->SetGlassDisplayItem(aNewItem);
    }
    return Some(AddNewNode(aNewItem, Nothing(), Span<MergedListIndex>(),
                           aPreviousItem));
  }

  void MergeChildLists(nsDisplayItem* aNewItem, nsDisplayItem* aOldItem,
                       nsDisplayItem* aOutItem) {
    if (!aOutItem->GetChildren()) {
      return;
    }

    Maybe<const ActiveScrolledRoot*> containerASRForChildren;
    nsDisplayList empty;
    const bool modified = mBuilder->MergeDisplayLists(
        aNewItem ? aNewItem->GetChildren() : &empty, aOldItem->GetChildren(),
        aOutItem->GetChildren(), containerASRForChildren, aOutItem);
    if (modified) {
      aOutItem->InvalidateCachedChildInfo(mBuilder->Builder());
      UpdateASR(aOutItem, containerASRForChildren);
      mResultIsModified = true;
    } else if (aOutItem == aNewItem) {
      // If nothing changed, but we copied the contents across to
      // the new item, then also copy the ASR data.
      CopyASR(aOldItem, aNewItem);
    }
    // Ideally we'd only UpdateBounds if something changed, but
    // nsDisplayWrapList also uses this to update the clip chain for the
    // current ASR, which gets reset during RestoreState(), so we always need
    // to run it again.
    aOutItem->UpdateBounds(mBuilder->Builder());
  }

  bool ShouldUseNewItem(nsDisplayItem* aNewItem) {
    // Generally we want to use the old item when the frame isn't marked as
    // modified so that any cached information on the item (or referencing the
    // item) gets retained. Quite a few FrameLayerBuilder performance
    // improvements benefit by this. Sometimes, however, we can end up where the
    // new item paints something different from the old item, even though we
    // haven't modified the frame, and it's hard to fix. In these cases we just
    // always use the new item to be safe.
    DisplayItemType type = aNewItem->GetType();
    if (type == DisplayItemType::TYPE_CANVAS_BACKGROUND_COLOR ||
        type == DisplayItemType::TYPE_SOLID_COLOR) {
      // The canvas background color item can paint the color from another
      // frame, and even though we schedule a paint, we don't mark the canvas
      // frame as invalid.
      return true;
    }

    if (type == DisplayItemType::TYPE_TABLE_BORDER_COLLAPSE) {
      // We intentionally don't mark the root table frame as modified when a
      // subframe changes, even though the border collapse item for the root
      // frame is what paints the changed border. Marking the root frame as
      // modified would rebuild display items for the whole table area, and we
      // don't want that.
      return true;
    }

    if (type == DisplayItemType::TYPE_TEXT_OVERFLOW) {
      // Text overflow marker items are created with the wrapping block as their
      // frame, and have an index value to note which line they are created for.
      // Their rendering can change if the items on that line change, which may
      // not mark the block as modified. We rebuild them if we build any item on
      // the line, so we should always get new items if they might have changed
      // rendering, and it's easier to just use the new items rather than
      // computing if we actually need them.
      return true;
    }

    if (type == DisplayItemType::TYPE_SUBDOCUMENT) {
      // nsDisplaySubDocument::mShouldFlatten can change without an invalidation
      // (and is the reason we unconditionally build the subdocument item), so
      // always use the new one to make sure we get the right value.
      return true;
    }

    if (type == DisplayItemType::TYPE_CARET) {
      // The caret can change position while still being owned by the same frame
      // and we don't invalidate in that case. Use the new version since the
      // changed bounds are needed for DLBI.
      return true;
    }

    if (type == DisplayItemType::TYPE_MASK ||
        type == DisplayItemType::TYPE_FILTER ||
        type == DisplayItemType::TYPE_SVG_WRAPPER) {
      // SVG items have some invalidation issues, see bugs 1494110 and 1494663.
      return true;
    }

    if (type == DisplayItemType::TYPE_TRANSFORM) {
      // Prerendering of transforms can change without frame invalidation.
      return true;
    }

    return false;
  }

  RetainedDisplayList Finalize() {
    for (size_t i = 0; i < mOldDAG.Length(); i++) {
      if (mOldItems[i].IsUsed()) {
        continue;
      }

      AutoTArray<MergedListIndex, 2> directPredecessors =
          ResolveNodeIndexesOldToMerged(
              mOldDAG.GetDirectPredecessors(OldListIndex(i)));
      ProcessOldNode(OldListIndex(i), std::move(directPredecessors));
    }

    RetainedDisplayList result;
    result.AppendToTop(&mMergedItems);
    result.mDAG = std::move(mMergedDAG);
    MOZ_RELEASE_ASSERT(result.mDAG.Length() == result.Count());
    return result;
  }

  bool HasMatchingItemInOldList(nsDisplayItem* aItem, OldListIndex* aOutIndex) {
    // Look for an item that matches aItem's frame and per-frame-key, but isn't
    // the same item.
    uint32_t outerKey = mOuterItem ? mOuterItem->GetPerFrameKey() : 0;
    nsIFrame* frame = aItem->Frame();
    for (nsDisplayItem* i : frame->DisplayItems()) {
      if (i != aItem && i->Frame() == frame &&
          i->GetPerFrameKey() == aItem->GetPerFrameKey()) {
        if (i->GetOldListIndex(mOldList, outerKey, aOutIndex)) {
          return true;
        }
      }
    }
    return false;
  }

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  bool HasModifiedFrame(nsDisplayItem* aItem) {
    nsIFrame* stopFrame = mOuterItem ? mOuterItem->Frame() : nullptr;
    return AnyContentAncestorModified(aItem->FrameForInvalidation(), stopFrame);
  }
#endif

  void UpdateContainerASR(nsDisplayItem* aItem) {
    mContainerASR = SelectContainerASR(
        aItem->GetClipChain(), aItem->GetActiveScrolledRoot(), mContainerASR);
  }

  MergedListIndex AddNewNode(
      nsDisplayItem* aItem, const Maybe<OldListIndex>& aOldIndex,
      Span<const MergedListIndex> aDirectPredecessors,
      const Maybe<MergedListIndex>& aExtraDirectPredecessor) {
    UpdateContainerASR(aItem);
    aItem->NotifyUsed(mBuilder->Builder());

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
    for (nsDisplayItem* i : aItem->Frame()->DisplayItems()) {
      if (i->Frame() == aItem->Frame() &&
          i->GetPerFrameKey() == aItem->GetPerFrameKey()) {
        MOZ_DIAGNOSTIC_ASSERT(!i->IsMergedItem());
      }
    }

    aItem->SetMergedPreProcessed(true, false);
#endif

    mMergedItems.AppendToTop(aItem);
    mBuilder->Metrics()->mTotalItems++;

    MergedListIndex newIndex =
        mMergedDAG.AddNode(aDirectPredecessors, aExtraDirectPredecessor);
    return newIndex;
  }

  void ProcessOldNode(OldListIndex aNode,
                      nsTArray<MergedListIndex>&& aDirectPredecessors) {
    nsDisplayItem* item = mOldItems[aNode.val].mItem;
    if (mOldItems[aNode.val].IsChanged()) {
      if (item && item->IsGlassItem() &&
          item == mBuilder->Builder()->GetGlassDisplayItem()) {
        mBuilder->Builder()->ClearGlassDisplayItem();
      }

      mOldItems[aNode.val].Discard(mBuilder, std::move(aDirectPredecessors));
      mResultIsModified = true;
    } else {
      MergeChildLists(nullptr, item, item);

      if (item->GetType() == DisplayItemType::TYPE_SUBDOCUMENT) {
        mBuilder->IncrementSubDocPresShellPaintCount(item);
      }
      item->SetReused(true);
      mBuilder->Metrics()->mReusedItems++;
      mOldItems[aNode.val].AddedToMergedList(
          AddNewNode(item, Some(aNode), aDirectPredecessors, Nothing()));
    }
  }

  struct PredecessorStackItem {
    PredecessorStackItem(OldListIndex aNode, Span<OldListIndex> aPredecessors)
        : mNode(aNode),
          mDirectPredecessors(aPredecessors),
          mCurrentPredecessorIndex(0) {}

    bool IsFinished() {
      return mCurrentPredecessorIndex == mDirectPredecessors.Length();
    }

    OldListIndex GetAndIncrementCurrentPredecessor() {
      return mDirectPredecessors[mCurrentPredecessorIndex++];
    }

    OldListIndex mNode;
    Span<OldListIndex> mDirectPredecessors;
    size_t mCurrentPredecessorIndex;
  };

  AutoTArray<MergedListIndex, 2> ProcessPredecessorsOfOldNode(
      OldListIndex aNode) {
    AutoTArray<PredecessorStackItem, 256> mStack;
    mStack.AppendElement(
        PredecessorStackItem(aNode, mOldDAG.GetDirectPredecessors(aNode)));

    while (true) {
      if (mStack.LastElement().IsFinished()) {
        // If we've finished processing all the entries in the current set, then
        // pop it off the processing stack and process it.
        PredecessorStackItem item = mStack.PopLastElement();
        AutoTArray<MergedListIndex, 2> result =
            ResolveNodeIndexesOldToMerged(item.mDirectPredecessors);

        if (mStack.IsEmpty()) {
          return result;
        }

        ProcessOldNode(item.mNode, std::move(result));
      } else {
        // Grab the current predecessor, push predecessors of that onto the
        // processing stack (if it hasn't already been processed), and then
        // advance to the next entry.
        OldListIndex currentIndex =
            mStack.LastElement().GetAndIncrementCurrentPredecessor();
        if (!mOldItems[currentIndex.val].IsUsed()) {
          mStack.AppendElement(PredecessorStackItem(
              currentIndex, mOldDAG.GetDirectPredecessors(currentIndex)));
        }
      }
    }
  }

  AutoTArray<MergedListIndex, 2> ResolveNodeIndexesOldToMerged(
      Span<OldListIndex> aDirectPredecessors) {
    AutoTArray<MergedListIndex, 2> result;
    result.SetCapacity(aDirectPredecessors.Length());
    for (OldListIndex index : aDirectPredecessors) {
      OldItemInfo& oldItem = mOldItems[index.val];
      if (oldItem.IsDiscarded()) {
        for (MergedListIndex inner : oldItem.mDirectPredecessors) {
          if (!result.Contains(inner)) {
            result.AppendElement(inner);
          }
        }
      } else {
        result.AppendElement(oldItem.mIndex);
      }
    }
    return result;
  }

  RetainedDisplayListBuilder* mBuilder;
  RetainedDisplayList* mOldList;
  Maybe<const ActiveScrolledRoot*> mContainerASR;
  nsTArray<OldItemInfo> mOldItems;
  DirectedAcyclicGraph<OldListUnits> mOldDAG;
  // Unfortunately we can't use strong typing for the hashtables
  // since they internally encode the type with the mOps pointer,
  // and assert when we try swap the contents
  nsDisplayList mMergedItems;
  DirectedAcyclicGraph<MergedListUnits> mMergedDAG;
  nsDisplayItem* mOuterItem;
  bool mResultIsModified;
};

#ifdef DEBUG
void VerifyNotModified(nsDisplayList* aList) {
  for (nsDisplayItem* item : *aList) {
    MOZ_ASSERT(!AnyContentAncestorModified(item->FrameForInvalidation()));

    if (item->GetChildren()) {
      VerifyNotModified(item->GetChildren());
    }
  }
}
#endif

/**
 * Takes two display lists and merges them into an output list.
 *
 * Display lists wthout an explicit DAG are interpreted as linear DAGs (with a
 * maximum of one direct predecessor and one direct successor per node). We add
 * the two DAGs together, and then output the topological sorted ordering as the
 * final display list.
 *
 * Once we've merged a list, we then retain the DAG (as part of the
 * RetainedDisplayList object) to use for future merges.
 */
bool RetainedDisplayListBuilder::MergeDisplayLists(
    nsDisplayList* aNewList, RetainedDisplayList* aOldList,
    RetainedDisplayList* aOutList,
    mozilla::Maybe<const mozilla::ActiveScrolledRoot*>& aOutContainerASR,
    nsDisplayItem* aOuterItem) {
  AUTO_PROFILER_LABEL_CATEGORY_PAIR(GRAPHICS_DisplayListMerging);

  if (!aOldList->IsEmpty()) {
    // If we still have items in the actual list, then it is because
    // PreProcessDisplayList decided that it was sure it can't be modified. We
    // can just use it directly, and throw any new items away.

    aNewList->DeleteAll(&mBuilder);
#ifdef DEBUG
    VerifyNotModified(aOldList);
#endif

    if (aOldList != aOutList) {
      *aOutList = std::move(*aOldList);
    }

    return false;
  }

  MergeState merge(this, *aOldList, aOuterItem);

  Maybe<MergedListIndex> previousItemIndex;
  while (nsDisplayItem* item = aNewList->RemoveBottom()) {
    Metrics()->mNewItems++;
    previousItemIndex = merge.ProcessItemFromNewList(item, previousItemIndex);
  }

  *aOutList = merge.Finalize();
  aOutContainerASR = merge.mContainerASR;
  return merge.mResultIsModified;
}

static nsIFrame* GetRootFrameForPainting(nsDisplayListBuilder* aBuilder,
                                         Document& aDocument) {
  // Although this is the actual subdocument, it might not be
  // what painting uses. Walk up to the nsSubDocumentFrame owning
  // us, and then ask that which subdoc it's going to paint.

  PresShell* presShell = aDocument.GetPresShell();
  if (!presShell) {
    return nullptr;
  }
  nsView* rootView = presShell->GetViewManager()->GetRootView();
  if (!rootView) {
    return nullptr;
  }

  // There should be an anonymous inner view between the root view
  // of the subdoc, and the view for the nsSubDocumentFrame.
  nsView* innerView = rootView->GetParent();
  if (!innerView) {
    return nullptr;
  }

  nsView* subDocView = innerView->GetParent();
  if (!subDocView) {
    return nullptr;
  }

  nsIFrame* subDocFrame = subDocView->GetFrame();
  if (!subDocFrame) {
    return nullptr;
  }

  nsSubDocumentFrame* subdocumentFrame = do_QueryFrame(subDocFrame);
  MOZ_ASSERT(subdocumentFrame);
  presShell = subdocumentFrame->GetSubdocumentPresShellForPainting(
      aBuilder->IsIgnoringPaintSuppression()
          ? nsSubDocumentFrame::IGNORE_PAINT_SUPPRESSION
          : 0);
  return presShell ? presShell->GetRootFrame() : nullptr;
}

static void TakeAndAddModifiedAndFramesWithPropsFromRootFrame(
    nsDisplayListBuilder* aBuilder, nsTArray<nsIFrame*>* aModifiedFrames,
    nsTArray<nsIFrame*>* aFramesWithProps, nsIFrame* aRootFrame,
    Document& aDoc) {
  MOZ_ASSERT(aRootFrame);

  if (RetainedDisplayListData* data = GetRetainedDisplayListData(aRootFrame)) {
    for (auto it = data->ConstIterator(); !it.Done(); it.Next()) {
      nsIFrame* frame = it.Key();
      const RetainedDisplayListData::FrameFlags& flags = it.Data();

      if (flags & RetainedDisplayListData::FrameFlags::Modified) {
        aModifiedFrames->AppendElement(frame);
      }

      if (flags & RetainedDisplayListData::FrameFlags::HasProps) {
        aFramesWithProps->AppendElement(frame);
      }

      if (flags & RetainedDisplayListData::FrameFlags::HadWillChange) {
        aBuilder->RemoveFromWillChangeBudgets(frame);
      }
    }

    data->Clear();
  }

  auto recurse = [&](Document& aSubDoc) {
    if (nsIFrame* rootFrame = GetRootFrameForPainting(aBuilder, aSubDoc)) {
      TakeAndAddModifiedAndFramesWithPropsFromRootFrame(
          aBuilder, aModifiedFrames, aFramesWithProps, rootFrame, aSubDoc);
    }
    return CallState::Continue;
  };
  aDoc.EnumerateSubDocuments(recurse);
}

static void GetModifiedAndFramesWithProps(
    nsDisplayListBuilder* aBuilder, nsTArray<nsIFrame*>* aOutModifiedFrames,
    nsTArray<nsIFrame*>* aOutFramesWithProps) {
  nsIFrame* rootFrame = aBuilder->RootReferenceFrame();
  MOZ_ASSERT(rootFrame);

  Document* rootDoc = rootFrame->PresContext()->Document();
  TakeAndAddModifiedAndFramesWithPropsFromRootFrame(
      aBuilder, aOutModifiedFrames, aOutFramesWithProps, rootFrame, *rootDoc);
}

// ComputeRebuildRegion  debugging
// #define CRR_DEBUG 1
#if CRR_DEBUG
#  define CRR_LOG(...) printf_stderr(__VA_ARGS__)
#else
#  define CRR_LOG(...)
#endif

static nsDisplayItem* GetFirstDisplayItemWithChildren(nsIFrame* aFrame) {
  for (nsDisplayItem* i : aFrame->DisplayItems()) {
    if (i->HasChildren()) {
      return static_cast<nsDisplayItem*>(i);
    }
  }
  return nullptr;
}

static bool IsInPreserve3DContext(const nsIFrame* aFrame) {
  return aFrame->Extend3DContext() ||
         aFrame->Combines3DTransformWithAncestors();
}

// Returns true if |aFrame| can store a display list building rect.
// These limitations are necessary to guarantee that
// 1) Just enough items are rebuilt to properly update display list
// 2) Modified frames will be visited during a partial display list build.
static bool CanStoreDisplayListBuildingRect(nsDisplayListBuilder* aBuilder,
                                            nsIFrame* aFrame) {
  return aFrame != aBuilder->RootReferenceFrame() &&
         aFrame->IsStackingContext() && aFrame->IsFixedPosContainingBlock() &&
         // Split frames might have placeholders for modified frames in their
         // unmodified continuation frame.
         !aFrame->GetPrevContinuation() && !aFrame->GetNextContinuation();
}

static bool ProcessFrameInternal(nsIFrame* aFrame,
                                 nsDisplayListBuilder* aBuilder,
                                 AnimatedGeometryRoot** aAGR, nsRect& aOverflow,
                                 const nsIFrame* aStopAtFrame,
                                 nsTArray<nsIFrame*>& aOutFramesWithProps,
                                 const bool aStopAtStackingContext) {
  nsIFrame* currentFrame = aFrame;

  while (currentFrame != aStopAtFrame) {
    CRR_LOG("currentFrame: %p (placeholder=%d), aOverflow: %d %d %d %d\n",
            currentFrame, !aStopAtStackingContext, aOverflow.x, aOverflow.y,
            aOverflow.width, aOverflow.height);

    // If the current frame is an OOF frame, DisplayListBuildingData needs to be
    // set on all the ancestor stacking contexts of the  placeholder frame, up
    // to the containing block of the OOF frame. This is done to ensure that the
    // content that might be behind the OOF frame is built for merging.
    nsIFrame* placeholder = currentFrame->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW)
                                ? currentFrame->GetPlaceholderFrame()
                                : nullptr;

    if (placeholder) {
      nsRect placeholderOverflow = aOverflow;
      auto rv = nsLayoutUtils::TransformRect(currentFrame, placeholder,
                                             placeholderOverflow);
      if (rv != nsLayoutUtils::TRANSFORM_SUCCEEDED) {
        placeholderOverflow = nsRect();
      }

      CRR_LOG("Processing placeholder %p for OOF frame %p\n", placeholder,
              currentFrame);

      CRR_LOG("OOF frame draw area: %d %d %d %d\n", placeholderOverflow.x,
              placeholderOverflow.y, placeholderOverflow.width,
              placeholderOverflow.height);

      // Tracking AGRs for the placeholder processing is not necessary, as the
      // goal is to only modify the DisplayListBuildingData rect.
      AnimatedGeometryRoot* dummyAGR = nullptr;

      // Find a common ancestor frame to handle frame continuations.
      // TODO: It might be possible to write a more specific and efficient
      // function for this.
      const nsIFrame* ancestor = nsLayoutUtils::FindNearestCommonAncestorFrame(
          currentFrame->GetParent(), placeholder->GetParent());

      if (!ProcessFrameInternal(placeholder, aBuilder, &dummyAGR,
                                placeholderOverflow, ancestor,
                                aOutFramesWithProps, false)) {
        return false;
      }
    }

    // Convert 'aOverflow' into the coordinate space of the nearest stacking
    // context or display port ancestor and update 'currentFrame' to point to
    // that frame.
    aOverflow = nsLayoutUtils::TransformFrameRectToAncestor(
        currentFrame, aOverflow, aStopAtFrame, nullptr, nullptr,
        /* aStopAtStackingContextAndDisplayPortAndOOFFrame = */ true,
        &currentFrame);
    if (IsInPreserve3DContext(currentFrame)) {
      return false;
    }

    MOZ_ASSERT(currentFrame);

    // Check whether the current frame is a scrollable frame with display port.
    nsRect displayPort;
    nsIScrollableFrame* sf = do_QueryFrame(currentFrame);
    nsIContent* content = sf ? currentFrame->GetContent() : nullptr;

    if (content && DisplayPortUtils::GetDisplayPort(content, &displayPort)) {
      CRR_LOG("Frame belongs to displayport frame %p\n", currentFrame);

      // Get overflow relative to the scrollport (from the scrollframe)
      nsRect r = aOverflow - sf->GetScrollPortRect().TopLeft();
      r.IntersectRect(r, displayPort);
      if (!r.IsEmpty()) {
        nsRect* rect = currentFrame->GetProperty(
            nsDisplayListBuilder::DisplayListBuildingDisplayPortRect());
        if (!rect) {
          rect = new nsRect();
          currentFrame->SetProperty(
              nsDisplayListBuilder::DisplayListBuildingDisplayPortRect(), rect);
          currentFrame->SetHasOverrideDirtyRegion(true);
          aOutFramesWithProps.AppendElement(currentFrame);
        }
        rect->UnionRect(*rect, r);
        CRR_LOG("Adding area to displayport draw area: %d %d %d %d\n", r.x, r.y,
                r.width, r.height);

        // TODO: Can we just use MarkFrameForDisplayIfVisible, plus
        // MarkFramesForDifferentAGR to ensure that this displayport, plus any
        // items that move relative to it get rebuilt, and then not contribute
        // to the root dirty area?
        aOverflow = sf->GetScrollPortRect();
      } else {
        // Don't contribute to the root dirty area at all.
        aOverflow.SetEmpty();
      }
    } else {
      aOverflow.IntersectRect(aOverflow,
                              currentFrame->InkOverflowRectRelativeToSelf());
    }

    if (aOverflow.IsEmpty()) {
      break;
    }

    if (CanStoreDisplayListBuildingRect(aBuilder, currentFrame)) {
      CRR_LOG("Frame belongs to stacking context frame %p\n", currentFrame);
      // If we found an intermediate stacking context with an existing display
      // item then we can store the dirty rect there and stop. If we couldn't
      // find one then we need to keep bubbling up to the next stacking context.
      nsDisplayItem* wrapperItem =
          GetFirstDisplayItemWithChildren(currentFrame);
      if (!wrapperItem) {
        continue;
      }

      // Store the stacking context relative dirty area such
      // that display list building will pick it up when it
      // gets to it.
      nsDisplayListBuilder::DisplayListBuildingData* data =
          currentFrame->GetProperty(
              nsDisplayListBuilder::DisplayListBuildingRect());
      if (!data) {
        data = new nsDisplayListBuilder::DisplayListBuildingData();
        currentFrame->SetProperty(
            nsDisplayListBuilder::DisplayListBuildingRect(), data);
        currentFrame->SetHasOverrideDirtyRegion(true);
        aOutFramesWithProps.AppendElement(currentFrame);
      }
      CRR_LOG("Adding area to stacking context draw area: %d %d %d %d\n",
              aOverflow.x, aOverflow.y, aOverflow.width, aOverflow.height);
      data->mDirtyRect.UnionRect(data->mDirtyRect, aOverflow);

      if (!aStopAtStackingContext) {
        // Continue ascending the frame tree until we reach aStopAtFrame.
        continue;
      }

      // Grab the visible (display list building) rect for children of this
      // wrapper item and convert into into coordinate relative to the current
      // frame.
      nsRect previousVisible = wrapperItem->GetBuildingRectForChildren();
      if (wrapperItem->ReferenceFrameForChildren() ==
          wrapperItem->ReferenceFrame()) {
        previousVisible -= wrapperItem->ToReferenceFrame();
      } else {
        MOZ_ASSERT(wrapperItem->ReferenceFrameForChildren() ==
                   wrapperItem->Frame());
      }

      if (!previousVisible.Contains(aOverflow)) {
        // If the overflow area of the changed frame isn't contained within the
        // old item, then we might change the size of the item and need to
        // update its sorting accordingly. Keep propagating the overflow area up
        // so that we build intersecting items for sorting.
        continue;
      }

      if (!data->mModifiedAGR) {
        data->mModifiedAGR = *aAGR;
      } else if (data->mModifiedAGR != *aAGR) {
        data->mDirtyRect = currentFrame->InkOverflowRectRelativeToSelf();
        CRR_LOG(
            "Found multiple modified AGRs within this stacking context, "
            "giving up\n");
      }

      // Don't contribute to the root dirty area at all.
      aOverflow.SetEmpty();
      *aAGR = nullptr;

      break;
    }
  }
  return true;
}

bool RetainedDisplayListBuilder::ProcessFrame(
    nsIFrame* aFrame, nsDisplayListBuilder* aBuilder, nsIFrame* aStopAtFrame,
    nsTArray<nsIFrame*>& aOutFramesWithProps, const bool aStopAtStackingContext,
    nsRect* aOutDirty, AnimatedGeometryRoot** aOutModifiedAGR) {
  if (aFrame->HasOverrideDirtyRegion()) {
    aOutFramesWithProps.AppendElement(aFrame);
  }

  if (aFrame->HasAnyStateBits(NS_FRAME_IN_POPUP)) {
    return true;
  }

  // TODO: There is almost certainly a faster way of doing this, probably can be
  // combined with the ancestor walk for TransformFrameRectToAncestor.
  AnimatedGeometryRoot* agr =
      aBuilder->FindAnimatedGeometryRootFor(aFrame)->GetAsyncAGR();

  CRR_LOG("Processing frame %p with agr %p\n", aFrame, agr->mFrame);

  // Convert the frame's overflow rect into the coordinate space
  // of the nearest stacking context that has an existing display item.
  // We store that as a dirty rect on that stacking context so that we build
  // all items that intersect the changed frame within the stacking context,
  // and then we use MarkFrameForDisplayIfVisible to make sure the stacking
  // context itself gets built. We don't need to build items that intersect
  // outside of the stacking context, since we know the stacking context item
  // exists in the old list, so we can trivially merge without needing other
  // items.
  nsRect overflow = aFrame->InkOverflowRectRelativeToSelf();

  // If the modified frame is also a caret frame, include the caret area.
  // This is needed because some frames (for example text frames without text)
  // might have an empty overflow rect.
  if (aFrame == aBuilder->GetCaretFrame()) {
    overflow.UnionRect(overflow, aBuilder->GetCaretRect());
  }

  if (!ProcessFrameInternal(aFrame, aBuilder, &agr, overflow, aStopAtFrame,
                            aOutFramesWithProps, aStopAtStackingContext)) {
    return false;
  }

  if (!overflow.IsEmpty()) {
    aOutDirty->UnionRect(*aOutDirty, overflow);
    CRR_LOG("Adding area to root draw area: %d %d %d %d\n", overflow.x,
            overflow.y, overflow.width, overflow.height);

    // If we get changed frames from multiple AGRS, then just give up as it gets
    // really complex to track which items would need to be marked in
    // MarkFramesForDifferentAGR.
    if (!*aOutModifiedAGR) {
      CRR_LOG("Setting %p as root stacking context AGR\n", agr);
      *aOutModifiedAGR = agr;
    } else if (agr && *aOutModifiedAGR != agr) {
      CRR_LOG("Found multiple AGRs in root stacking context, giving up\n");
      return false;
    }
  }
  return true;
}

static void AddFramesForContainingBlock(nsIFrame* aBlock,
                                        const nsFrameList& aFrames,
                                        nsTArray<nsIFrame*>& aExtraFrames) {
  for (nsIFrame* f : aFrames) {
    if (!f->IsFrameModified() && AnyContentAncestorModified(f, aBlock)) {
      CRR_LOG("Adding invalid OOF %p\n", f);
      aExtraFrames.AppendElement(f);
    }
  }
}

// Placeholder descendants of aFrame don't contribute to aFrame's overflow area.
// Find all the containing blocks that might own placeholders under us, walk
// their OOF frames list, and manually invalidate any frames that are
// descendants of a modified frame (us, or another frame we'll get to soon).
// This is combined with the work required for MarkFrameForDisplayIfVisible,
// so that we can avoid an extra ancestor walk, and we can reuse the flag
// to detect when we've already visited an ancestor (and thus all further
// ancestors must also be visited).
static void FindContainingBlocks(nsIFrame* aFrame,
                                 nsTArray<nsIFrame*>& aExtraFrames) {
  for (nsIFrame* f = aFrame; f; f = nsLayoutUtils::GetDisplayListParent(f)) {
    if (f->ForceDescendIntoIfVisible()) {
      return;
    }
    f->SetForceDescendIntoIfVisible(true);
    CRR_LOG("Considering OOFs for %p\n", f);

    AddFramesForContainingBlock(f, f->GetChildList(nsIFrame::kFloatList),
                                aExtraFrames);
    AddFramesForContainingBlock(f, f->GetChildList(f->GetAbsoluteListID()),
                                aExtraFrames);
  }
}

/**
 * Given a list of frames that has been modified, computes the region that we
 * need to do display list building for in order to build all modified display
 * items.
 *
 * When a modified frame is within a stacking context (with an existing display
 * item), then we only contribute to the build area within the stacking context,
 * as well as forcing display list building to descend to the stacking context.
 * We don't need to add build area outside of the stacking context (and force
 * items above/below the stacking context container item to be built), since
 * just matching the position of the stacking context container item is
 * sufficient to ensure correct ordering during merging.
 *
 * We need to rebuild all items that might intersect with the modified frame,
 * both now and during async changes on the compositor. We do this by rebuilding
 * the area covered by the changed frame, as well as rebuilding all items that
 * have a different (async) AGR to the changed frame. If we have changes to
 * multiple AGRs (within a stacking context), then we rebuild that stacking
 * context entirely.
 *
 * @param aModifiedFrames The list of modified frames.
 * @param aOutDirty The result region to use for display list building.
 * @param aOutModifiedAGR The modified AGR for the root stacking context.
 * @param aOutFramesWithProps The list of frames to which we attached partial
 * build data so that it can be cleaned up.
 *
 * @return true if we succesfully computed a partial rebuild region, false if a
 * full build is required.
 */
bool RetainedDisplayListBuilder::ComputeRebuildRegion(
    nsTArray<nsIFrame*>& aModifiedFrames, nsRect* aOutDirty,
    AnimatedGeometryRoot** aOutModifiedAGR,
    nsTArray<nsIFrame*>& aOutFramesWithProps) {
  CRR_LOG("Computing rebuild regions for %zu frames:\n",
          aModifiedFrames.Length());
  nsTArray<nsIFrame*> extraFrames;
  for (nsIFrame* f : aModifiedFrames) {
    MOZ_ASSERT(f);

    mBuilder.AddFrameMarkedForDisplayIfVisible(f);
    FindContainingBlocks(f, extraFrames);

    if (!ProcessFrame(f, &mBuilder, mBuilder.RootReferenceFrame(),
                      aOutFramesWithProps, true, aOutDirty, aOutModifiedAGR)) {
      return false;
    }
  }

  // Since we set modified to true on the extraFrames, add them to
  // aModifiedFrames so that it will get reverted.
  aModifiedFrames.AppendElements(extraFrames);

  for (nsIFrame* f : extraFrames) {
    f->SetFrameIsModified(true);

    if (!ProcessFrame(f, &mBuilder, mBuilder.RootReferenceFrame(),
                      aOutFramesWithProps, true, aOutDirty, aOutModifiedAGR)) {
      return false;
    }
  }

  return true;
}

bool RetainedDisplayListBuilder::ShouldBuildPartial(
    nsTArray<nsIFrame*>& aModifiedFrames) {
  if (mList.IsEmpty()) {
    // Partial builds without a previous display list do not make sense.
    Metrics()->mPartialUpdateFailReason = PartialUpdateFailReason::EmptyList;
    return false;
  }

  if (aModifiedFrames.Length() >
      StaticPrefs::layout_display_list_rebuild_frame_limit()) {
    // Computing a dirty rect with too many modified frames can be slow.
    Metrics()->mPartialUpdateFailReason = PartialUpdateFailReason::RebuildLimit;
    return false;
  }

  // We don't support retaining with overlay scrollbars, since they require
  // us to look at the display list and pick the highest z-index, which
  // we can't do during partial building.
  if (mBuilder.DisablePartialUpdates()) {
    mBuilder.SetDisablePartialUpdates(false);
    Metrics()->mPartialUpdateFailReason = PartialUpdateFailReason::Disabled;
    return false;
  }

  for (nsIFrame* f : aModifiedFrames) {
    MOZ_ASSERT(f);

    const LayoutFrameType type = f->Type();

    // If we have any modified frames of the following types, it is likely that
    // doing a partial rebuild of the display list will be slower than doing a
    // full rebuild.
    // This is because these frames either intersect or may intersect with most
    // of the page content. This is either due to display port size or different
    // async AGR.
    if (type == LayoutFrameType::Viewport ||
        type == LayoutFrameType::PageContent ||
        type == LayoutFrameType::Canvas || type == LayoutFrameType::Scrollbar) {
      Metrics()->mPartialUpdateFailReason = PartialUpdateFailReason::FrameType;
      return false;
    }

    // Detect root scroll frame and do a full rebuild for them too for the same
    // reasons as above, but also because top layer items should to be marked
    // modified if the root scroll frame is modified. Putting this check here
    // means we don't need to check everytime a frame is marked modified though.
    if (type == LayoutFrameType::Scroll && f->GetParent() &&
        !f->GetParent()->GetParent()) {
      Metrics()->mPartialUpdateFailReason = PartialUpdateFailReason::FrameType;
      return false;
    }
  }

  return true;
}

void RetainedDisplayListBuilder::InvalidateCaretFramesIfNeeded() {
  if (mPreviousCaret == mBuilder.GetCaretFrame()) {
    // The current caret frame is the same as the previous one.
    return;
  }

  if (mPreviousCaret) {
    mPreviousCaret->MarkNeedsDisplayItemRebuild();
  }

  if (mBuilder.GetCaretFrame()) {
    mBuilder.GetCaretFrame()->MarkNeedsDisplayItemRebuild();
  }

  mPreviousCaret = mBuilder.GetCaretFrame();
}

static void ClearFrameProps(nsTArray<nsIFrame*>& aFrames) {
  for (nsIFrame* f : aFrames) {
    if (f->HasOverrideDirtyRegion()) {
      f->SetHasOverrideDirtyRegion(false);
      f->RemoveProperty(nsDisplayListBuilder::DisplayListBuildingRect());
      f->RemoveProperty(
          nsDisplayListBuilder::DisplayListBuildingDisplayPortRect());
    }

    f->SetFrameIsModified(false);
  }
}

class AutoClearFramePropsArray {
 public:
  explicit AutoClearFramePropsArray(size_t aCapacity) : mFrames(aCapacity) {}
  AutoClearFramePropsArray() = default;
  ~AutoClearFramePropsArray() { ClearFrameProps(mFrames); }

  nsTArray<nsIFrame*>& Frames() { return mFrames; }
  bool IsEmpty() const { return mFrames.IsEmpty(); }

 private:
  nsTArray<nsIFrame*> mFrames;
};

void RetainedDisplayListBuilder::ClearFramesWithProps() {
  AutoClearFramePropsArray modifiedFrames;
  AutoClearFramePropsArray framesWithProps;
  GetModifiedAndFramesWithProps(&mBuilder, &modifiedFrames.Frames(),
                                &framesWithProps.Frames());
}

PartialUpdateResult RetainedDisplayListBuilder::AttemptPartialUpdate(
    nscolor aBackstop) {
  mBuilder.RemoveModifiedWindowRegions();

  if (mBuilder.ShouldSyncDecodeImages()) {
    MarkFramesWithItemsAndImagesModified(&mList);
  }

  InvalidateCaretFramesIfNeeded();

  mBuilder.EnterPresShell(mBuilder.RootReferenceFrame());

  // We set the override dirty regions during ComputeRebuildRegion or in
  // DisplayPortUtils::InvalidateForDisplayPortChange. The display port change
  // also marks the frame modified, so those regions are cleared here as well.
  AutoClearFramePropsArray modifiedFrames(64);
  AutoClearFramePropsArray framesWithProps;
  GetModifiedAndFramesWithProps(&mBuilder, &modifiedFrames.Frames(),
                                &framesWithProps.Frames());

  // Do not allow partial builds if the |ShouldBuildPartial()| heuristic fails.
  bool shouldBuildPartial = ShouldBuildPartial(modifiedFrames.Frames());

  nsRect modifiedDirty;
  AnimatedGeometryRoot* modifiedAGR = nullptr;
  PartialUpdateResult result = PartialUpdateResult::NoChange;
  if (!shouldBuildPartial ||
      !ComputeRebuildRegion(modifiedFrames.Frames(), &modifiedDirty,
                            &modifiedAGR, framesWithProps.Frames()) ||
      !PreProcessDisplayList(&mList, modifiedAGR, result)) {
    mBuilder.SetPartialBuildFailed(true);
    mBuilder.LeavePresShell(mBuilder.RootReferenceFrame(), nullptr);
    mList.DeleteAll(&mBuilder);
    return PartialUpdateResult::Failed;
  }

  // This is normally handled by EnterPresShell, but we skipped it so that we
  // didn't call MarkFrameForDisplayIfVisible before ComputeRebuildRegion.
  nsIScrollableFrame* sf = mBuilder.RootReferenceFrame()
                               ->PresShell()
                               ->GetRootScrollFrameAsScrollable();
  if (sf) {
    nsCanvasFrame* canvasFrame = do_QueryFrame(sf->GetScrolledFrame());
    if (canvasFrame) {
      mBuilder.MarkFrameForDisplayIfVisible(canvasFrame,
                                            mBuilder.RootReferenceFrame());
    }
  }

  modifiedDirty.IntersectRect(
      modifiedDirty,
      mBuilder.RootReferenceFrame()->InkOverflowRectRelativeToSelf());

  mBuilder.SetDirtyRect(modifiedDirty);
  mBuilder.SetPartialUpdate(true);
  mBuilder.SetPartialBuildFailed(false);

  nsDisplayList modifiedDL;
  mBuilder.RootReferenceFrame()->BuildDisplayListForStackingContext(
      &mBuilder, &modifiedDL);
  if (!modifiedDL.IsEmpty()) {
    nsLayoutUtils::AddExtraBackgroundItems(
        &mBuilder, &modifiedDL, mBuilder.RootReferenceFrame(),
        nsRect(nsPoint(0, 0), mBuilder.RootReferenceFrame()->GetSize()),
        mBuilder.RootReferenceFrame()->InkOverflowRectRelativeToSelf(),
        aBackstop);
  }
  mBuilder.SetPartialUpdate(false);

  if (mBuilder.PartialBuildFailed()) {
    mBuilder.LeavePresShell(mBuilder.RootReferenceFrame(), nullptr);
    mList.DeleteAll(&mBuilder);
    modifiedDL.DeleteAll(&mBuilder);
    Metrics()->mPartialUpdateFailReason = PartialUpdateFailReason::Content;
    return PartialUpdateResult::Failed;
  }

  // printf_stderr("Painting --- Modified list (dirty %d,%d,%d,%d):\n",
  //              modifiedDirty.x, modifiedDirty.y, modifiedDirty.width,
  //              modifiedDirty.height);
  // nsIFrame::PrintDisplayList(&mBuilder, modifiedDL);

  // |modifiedDL| can sometimes be empty here. We still perform the
  // display list merging to prune unused items (for example, items that
  // are not visible anymore) from the old list.
  // TODO: Optimization opportunity. In this case, MergeDisplayLists()
  // unnecessarily creates a hashtable of the old items.
  // TODO: Ideally we could skip this if result is NoChange, but currently when
  // we call RestoreState on nsDisplayWrapList it resets the clip to the base
  // clip, and we need the UpdateBounds call (within MergeDisplayLists) to
  // move it to the correct inner clip.
  Maybe<const ActiveScrolledRoot*> dummy;
  if (MergeDisplayLists(&modifiedDL, &mList, &mList, dummy)) {
    result = PartialUpdateResult::Updated;
  }

  // printf_stderr("Painting --- Merged list:\n");
  // nsIFrame::PrintDisplayList(&mBuilder, mList);

  mBuilder.LeavePresShell(mBuilder.RootReferenceFrame(), List());
  return result;
}

}  // namespace mozilla
