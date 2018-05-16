/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "RetainedDisplayListBuilder.h"

#include "DisplayListChecker.h"
#include "gfxPrefs.h"
#include "nsPlaceholderFrame.h"
#include "nsSubDocumentFrame.h"
#include "nsViewManager.h"
#include "nsCanvasFrame.h"

/**
 * Code for doing display list building for a modified subset of the window,
 * and then merging it into the existing display list (for the full window).
 *
 * The approach primarily hinges on the observation that the ‘true’ ordering of
 * display items is represented by a DAG (only items that intersect in 2d space
 * have a defined ordering). Our display list is just one of a many possible linear
 * representations of this ordering.
 *
 * Each time a frame changes (gets a new ComputedStyle, or has a size/position
 * change), we schedule a paint (as we do currently), but also reord the frame that
 * changed.
 *
 * When the next paint occurs we union the overflow areas (in screen space) of the
 * changed frames, and compute a rect/region that contains all changed items. We
 * then build a display list just for this subset of the screen and merge it into
 * the display list from last paint.
 *
 * Any items that exist in one list and not the other must not have a defined
 * ordering in the DAG, since they need to intersect to have an ordering and
 * we would have built both in the new list if they intersected. Given that, we
 * can align items that appear in both lists, and any items that appear between
 * matched items can be inserted into the merged list in any order.
 */

using namespace mozilla;

static void
MarkFramesWithItemsAndImagesModified(nsDisplayList* aList)
{
  for (nsDisplayItem* i = aList->GetBottom(); i != nullptr; i = i->GetAbove()) {
    if (!i->HasDeletedFrame() && i->CanBeReused() && !i->Frame()->IsFrameModified()) {
      // If we have existing cached geometry for this item, then check that for
      // whether we need to invalidate for a sync decode. If we don't, then
      // use the item's flags.
      DisplayItemData* data = FrameLayerBuilder::GetOldDataFor(i);
      //XXX: handle webrender case
      bool invalidate = false;
      if (data &&
          data->GetGeometry()) {
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

static AnimatedGeometryRoot*
SelectAGRForFrame(nsIFrame* aFrame, AnimatedGeometryRoot* aParentAGR)
{
  if (!aFrame->IsStackingContext()) {
    return aParentAGR;
  }

  if (!aFrame->HasOverrideDirtyRegion()) {
    return nullptr;
  }

  nsDisplayListBuilder::DisplayListBuildingData* data =
    aFrame->GetProperty(nsDisplayListBuilder::DisplayListBuildingRect());

  return data && data->mModifiedAGR ? data->mModifiedAGR.get()
                                    : nullptr;
}

// Removes any display items that belonged to a frame that was deleted,
// and mark frames that belong to a different AGR so that get their
// items built again.
// TODO: We currently descend into all children even if we don't have an AGR
// to mark, as child stacking contexts might. It would be nice if we could
// jump into those immediately rather than walking the entire thing.
bool
RetainedDisplayListBuilder::PreProcessDisplayList(RetainedDisplayList* aList,
                                                  AnimatedGeometryRoot* aAGR)
{
  // The DAG merging algorithm does not have strong mechanisms in place to keep the
  // complexity of the resulting DAG under control. In some cases we can build up
  // edges very quickly. Detect those cases and force a full display list build if
  // we hit them.
  static const uint32_t kMaxEdgeRatio = 5;
  bool initializeDAG = !aList->mDAG.Length();
  if (!initializeDAG &&
      aList->mDAG.mDirectPredecessorList.Length() >
      (aList->mDAG.mNodesInfo.Length() * kMaxEdgeRatio)) {
    return false;

  }

  nsDisplayList saved;
  aList->mOldItems.SetCapacity(aList->Count());
  MOZ_ASSERT(aList->mOldItems.IsEmpty());
  while (nsDisplayItem* item = aList->RemoveBottom()) {
    if (item->HasDeletedFrame() || !item->CanBeReused()) {
      size_t i = aList->mOldItems.Length();
      aList->mOldItems.AppendElement(OldItemInfo(nullptr));
      item->Destroy(&mBuilder);

      if (initializeDAG) {
        if (i == 0) {
          aList->mDAG.AddNode(Span<const MergedListIndex>());
        } else {
          MergedListIndex previous(i - 1);
          aList->mDAG.AddNode(Span<const MergedListIndex>(&previous, 1));
        }
      }
      continue;
    }

    size_t i = aList->mOldItems.Length();
    aList->mOldItems.AppendElement(OldItemInfo(item));
    item->SetOldListIndex(aList, OldListIndex(i));
    if (initializeDAG) {
      if (i == 0) {
        aList->mDAG.AddNode(Span<const MergedListIndex>());
      } else {
        MergedListIndex previous(i - 1);
        aList->mDAG.AddNode(Span<const MergedListIndex>(&previous, 1));
      }
    }

    nsIFrame* f = item->Frame();

    if (item->GetChildren()) {
      if (!PreProcessDisplayList(item->GetChildren(), SelectAGRForFrame(f, aAGR))) {
        mBuilder.MarkFrameForDisplayIfVisible(f, mBuilder.RootReferenceFrame());
        mBuilder.MarkFrameModifiedDuringBuilding(f);
      }
    }

    // TODO: We should be able to check the clipped bounds relative
    // to the common AGR (of both the existing item and the invalidated
    // frame) and determine if they can ever intersect.
    if (aAGR && item->GetAnimatedGeometryRoot()->GetAsyncAGR() != aAGR) {
      mBuilder.MarkFrameForDisplayIfVisible(f, mBuilder.RootReferenceFrame());
    }

    // TODO: This is here because we sometimes reuse the previous display list
    // completely. For optimization, we could only restore the state for reused
    // display items.
    item->RestoreState();
  }
  MOZ_ASSERT(aList->mOldItems.Length() == aList->mDAG.Length());
  aList->RestoreState();
  return true;
}

void
RetainedDisplayListBuilder::IncrementSubDocPresShellPaintCount(nsDisplayItem* aItem)
{
  MOZ_ASSERT(aItem->GetType() == DisplayItemType::TYPE_SUBDOCUMENT);

  nsSubDocumentFrame* subDocFrame =
    static_cast<nsDisplaySubDocument*>(aItem)->SubDocumentFrame();
  MOZ_ASSERT(subDocFrame);

  nsIPresShell* presShell = subDocFrame->GetSubdocumentPresShellForPainting(0);
  MOZ_ASSERT(presShell);

  mBuilder.IncrementPresShellPaintCount(presShell);
}

static bool
AnyContentAncestorModified(nsIFrame* aFrame,
                           nsIFrame* aStopAtFrame = nullptr)
{
  for (nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetParentOrPlaceholderForCrossDoc(f)) {
    if (f->IsFrameModified()) {
      return true;
    }

    if (aStopAtFrame && f == aStopAtFrame) {
      break;
    }
  }

  return false;
}

static void
UpdateASR(nsDisplayItem* aItem,
          Maybe<const ActiveScrolledRoot*>& aContainerASR)
{
  if (!aContainerASR) {
    return;
  }

  nsDisplayWrapList* wrapList = aItem->AsDisplayWrapList();
  if (!wrapList) {
    aItem->SetActiveScrolledRoot(aContainerASR.value());
    return;
  }

  wrapList->SetActiveScrolledRoot(
    ActiveScrolledRoot::PickAncestor(wrapList->GetFrameActiveScrolledRoot(),
                                     aContainerASR.value()));
}

void
OldItemInfo::AddedMatchToMergedList(RetainedDisplayListBuilder* aBuilder,
                                    MergedListIndex aIndex)
{
  mItem->Destroy(aBuilder->Builder());
  AddedToMergedList(aIndex);
}

void
OldItemInfo::Discard(RetainedDisplayListBuilder* aBuilder,
                     nsTArray<MergedListIndex>&& aDirectPredecessors)
{
  MOZ_ASSERT(!IsUsed());
  mUsed = mDiscarded = true;
  mDirectPredecessors = Move(aDirectPredecessors);
  if (mItem) {
    mItem->Destroy(aBuilder->Builder());
  }
  mItem = nullptr;
}

bool
OldItemInfo::IsChanged()
{
  return !mItem || mItem->HasDeletedFrame() || !mItem->CanBeReused();
}

/**
 * A C++ implementation of Markus Stange's merge-dags algorithm.
 * https://github.com/mstange/merge-dags
 *
 * MergeState handles combining a new list of display items into an existing
 * DAG and computes the new DAG in a single pass.
 * Each time we add a new item, we resolve all dependencies for it, so that the resulting
 * list and DAG are built in topological ordering.
 */
class MergeState {
public:
  MergeState(RetainedDisplayListBuilder* aBuilder, RetainedDisplayList& aOldList)
    : mBuilder(aBuilder)
    , mOldList(&aOldList)
    , mOldItems(Move(aOldList.mOldItems))
    , mOldDAG(Move(*reinterpret_cast<DirectedAcyclicGraph<OldListUnits>*>(&aOldList.mDAG)))
    , mResultIsModified(false)
  {
    mMergedDAG.EnsureCapacityFor(mOldDAG);
  }

  MergedListIndex ProcessItemFromNewList(nsDisplayItem* aNewItem, const Maybe<MergedListIndex>& aPreviousItem) {
    OldListIndex oldIndex;
    if (!HasModifiedFrame(aNewItem) &&
        HasMatchingItemInOldList(aNewItem, &oldIndex)) {
      nsDisplayItem* oldItem = mOldItems[oldIndex.val].mItem;
      if (!mOldItems[oldIndex.val].IsChanged()) {
        MOZ_ASSERT(!mOldItems[oldIndex.val].IsUsed());
        if (aNewItem->GetChildren()) {
          Maybe<const ActiveScrolledRoot*> containerASRForChildren;
          if (mBuilder->MergeDisplayLists(aNewItem->GetChildren(),
                                          oldItem->GetChildren(),
                                          aNewItem->GetChildren(),
                                          containerASRForChildren)) {
            mResultIsModified = true;

          }
          UpdateASR(aNewItem, containerASRForChildren);
          aNewItem->UpdateBounds(mBuilder->Builder());
        }

        AutoTArray<MergedListIndex, 2> directPredecessors = ProcessPredecessorsOfOldNode(oldIndex);
        MergedListIndex newIndex = AddNewNode(aNewItem, Some(oldIndex), directPredecessors, aPreviousItem);
        mOldItems[oldIndex.val].AddedMatchToMergedList(mBuilder, newIndex);
        return newIndex;
      }
    }
    mResultIsModified = true;
    return AddNewNode(aNewItem, Nothing(), Span<MergedListIndex>(), aPreviousItem);
  }

  RetainedDisplayList Finalize() {
    for (size_t i = 0; i < mOldDAG.Length(); i++) {
      if (mOldItems[i].IsUsed()) {
        continue;
      }

      AutoTArray<MergedListIndex, 2> directPredecessors =
        ResolveNodeIndexesOldToMerged(mOldDAG.GetDirectPredecessors(OldListIndex(i)));
      ProcessOldNode(OldListIndex(i), Move(directPredecessors));
    }

    RetainedDisplayList result;
    result.AppendToTop(&mMergedItems);
    result.mDAG = Move(mMergedDAG);
    return result;
  }

  bool HasMatchingItemInOldList(nsDisplayItem* aItem, OldListIndex* aOutIndex)
  {
    nsIFrame::DisplayItemArray* items = aItem->Frame()->GetProperty(nsIFrame::DisplayItems());
    // Look for an item that matches aItem's frame and per-frame-key, but isn't the same item.
    for (nsDisplayItem* i : *items) {
      if (i != aItem && i->Frame() == aItem->Frame() &&
          i->GetPerFrameKey() == aItem->GetPerFrameKey()) {
        *aOutIndex = i->GetOldListIndex(mOldList);
        return true;
      }
    }
    return false;
  }

  bool HasModifiedFrame(nsDisplayItem* aItem) {
    return AnyContentAncestorModified(aItem->FrameForInvalidation());
  }

  void UpdateContainerASR(nsDisplayItem* aItem)
  {
    const ActiveScrolledRoot* itemClipASR =
      aItem->GetClipChain() ? aItem->GetClipChain()->mASR : nullptr;

    const ActiveScrolledRoot* finiteBoundsASR = ActiveScrolledRoot::PickDescendant(
      itemClipASR, aItem->GetActiveScrolledRoot());
    if (!mContainerASR) {
      mContainerASR = Some(finiteBoundsASR);
    } else {
      mContainerASR = Some(ActiveScrolledRoot::PickAncestor(mContainerASR.value(), finiteBoundsASR));
    }

  }

  MergedListIndex AddNewNode(nsDisplayItem* aItem,
                             const Maybe<OldListIndex>& aOldIndex,
                             Span<const MergedListIndex> aDirectPredecessors,
                             const Maybe<MergedListIndex>& aExtraDirectPredecessor) {
    UpdateContainerASR(aItem);
    mMergedItems.AppendToTop(aItem);
    MergedListIndex newIndex = mMergedDAG.AddNode(aDirectPredecessors, aExtraDirectPredecessor);
    return newIndex;
  }

  void ProcessOldNode(OldListIndex aNode, nsTArray<MergedListIndex>&& aDirectPredecessors) {
    nsDisplayItem* item = mOldItems[aNode.val].mItem;
    if (mOldItems[aNode.val].IsChanged() || HasModifiedFrame(item)) {
      mOldItems[aNode.val].Discard(mBuilder, Move(aDirectPredecessors));
      mResultIsModified = true;
    } else {
      if (item->GetChildren()) {
        Maybe<const ActiveScrolledRoot*> containerASRForChildren;
        nsDisplayList empty;
        if (mBuilder->MergeDisplayLists(&empty, item->GetChildren(), item->GetChildren(),
                                        containerASRForChildren)) {
          mResultIsModified = true;
        }
        UpdateASR(item, containerASRForChildren);
        item->UpdateBounds(mBuilder->Builder());
      }
      if (item->GetType() == DisplayItemType::TYPE_SUBDOCUMENT) {
        mBuilder->IncrementSubDocPresShellPaintCount(item);
      }
      item->SetReused(true);
      mOldItems[aNode.val].AddedToMergedList(
        AddNewNode(item, Some(aNode), aDirectPredecessors, Nothing()));
    }
  }

  struct PredecessorStackItem {
    PredecessorStackItem(OldListIndex aNode, Span<OldListIndex> aPredecessors)
     : mNode(aNode)
     , mDirectPredecessors(aPredecessors)
     , mCurrentPredecessorIndex(0)
    {}

    bool IsFinished() {
      return mCurrentPredecessorIndex == mDirectPredecessors.Length();
    }

    OldListIndex GetAndIncrementCurrentPredecessor() { return mDirectPredecessors[mCurrentPredecessorIndex++]; }

    OldListIndex mNode;
    Span<OldListIndex> mDirectPredecessors;
    size_t mCurrentPredecessorIndex;
  };

  AutoTArray<MergedListIndex, 2> ProcessPredecessorsOfOldNode(OldListIndex aNode) {
    AutoTArray<PredecessorStackItem,256> mStack;
    mStack.AppendElement(PredecessorStackItem(aNode, mOldDAG.GetDirectPredecessors(aNode)));

    while (true) {
      if (mStack.LastElement().IsFinished()) {
        // If we've finished processing all the entries in the current set, then pop
        // it off the processing stack and process it.
        PredecessorStackItem item = mStack.PopLastElement();
        AutoTArray<MergedListIndex,2> result =
          ResolveNodeIndexesOldToMerged(item.mDirectPredecessors);
        if (mStack.IsEmpty()) {
          return result;
        } else {
          ProcessOldNode(item.mNode, Move(result));
        }
      } else {
        // Grab the current predecessor, push predecessors of that onto the processing
        // stack (if it hasn't already been processed), and then advance to the next entry.
        OldListIndex currentIndex = mStack.LastElement().GetAndIncrementCurrentPredecessor();
        if (!mOldItems[currentIndex.val].IsUsed()) {
          mStack.AppendElement(
            PredecessorStackItem(currentIndex, mOldDAG.GetDirectPredecessors(currentIndex)));
        }
      }
    }
  }

  AutoTArray<MergedListIndex, 2> ResolveNodeIndexesOldToMerged(Span<OldListIndex> aDirectPredecessors) {
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
  bool mResultIsModified;
};

void RetainedDisplayList::ClearDAG()
{
  mDAG.Clear();
}

/**
 * Takes two display lists and merges them into an output list.
 *
 * Display lists wthout an explicit DAG are interpreted as linear DAGs (with a maximum
 * of one direct predecessor and one direct successor per node). We add the two DAGs
 * together, and then output the topological sorted ordering as the final display list.
 *
 * Once we've merged a list, we then retain the DAG (as part of the RetainedDisplayList
 * object) to use for future merges.
 */
bool
RetainedDisplayListBuilder::MergeDisplayLists(nsDisplayList* aNewList,
                                              RetainedDisplayList* aOldList,
                                              RetainedDisplayList* aOutList,
                                              mozilla::Maybe<const mozilla::ActiveScrolledRoot*>& aOutContainerASR)
{
  MergeState merge(this, *aOldList);

  Maybe<MergedListIndex> previousItemIndex;
  while (nsDisplayItem* item = aNewList->RemoveBottom()) {
    previousItemIndex = Some(merge.ProcessItemFromNewList(item, previousItemIndex));
  }

  *aOutList = Move(merge.Finalize());
  aOutContainerASR = merge.mContainerASR;
  return merge.mResultIsModified;
}

static void
TakeAndAddModifiedAndFramesWithPropsFromRootFrame(
  nsTArray<nsIFrame*>* aModifiedFrames,
  nsTArray<nsIFrame*>* aFramesWithProps,
  nsIFrame* aRootFrame)
{
  MOZ_ASSERT(aRootFrame);

  nsTArray<nsIFrame*>* frames =
    aRootFrame->GetProperty(nsIFrame::ModifiedFrameList());

  if (frames) {
    for (nsIFrame* f : *frames) {
      if (f) {
        aModifiedFrames->AppendElement(f);
      }
    }

    frames->Clear();
  }

  frames =
    aRootFrame->GetProperty(nsIFrame::OverriddenDirtyRectFrameList());

  if (frames) {
    for (nsIFrame* f : *frames) {
      if (f) {
        aFramesWithProps->AppendElement(f);
      }
    }

    frames->Clear();
  }
}

struct CbData {
  nsDisplayListBuilder* builder;
  nsTArray<nsIFrame*>* modifiedFrames;
  nsTArray<nsIFrame*>* framesWithProps;
};

static nsIFrame*
GetRootFrameForPainting(nsDisplayListBuilder* aBuilder, nsIDocument* aDocument)
{
  // Although this is the actual subdocument, it might not be
  // what painting uses. Walk up to the nsSubDocumentFrame owning
  // us, and then ask that which subdoc it's going to paint.

  nsIPresShell* presShell = aDocument->GetShell();
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
    aBuilder->IsIgnoringPaintSuppression() ? nsSubDocumentFrame::IGNORE_PAINT_SUPPRESSION : 0);
  return presShell ? presShell->GetRootFrame() : nullptr;
}

static bool
SubDocEnumCb(nsIDocument* aDocument, void* aData)
{
  MOZ_ASSERT(aDocument);
  MOZ_ASSERT(aData);

  CbData* data = static_cast<CbData*>(aData);

  nsIFrame* rootFrame = GetRootFrameForPainting(data->builder, aDocument);
  if (rootFrame) {
    TakeAndAddModifiedAndFramesWithPropsFromRootFrame(data->modifiedFrames,
                                                      data->framesWithProps,
                                                      rootFrame);

    nsIDocument* innerDoc = rootFrame->PresShell()->GetDocument();
    if (innerDoc) {
      innerDoc->EnumerateSubDocuments(SubDocEnumCb, aData);
    }
  }
  return true;
}

static void
GetModifiedAndFramesWithProps(nsDisplayListBuilder* aBuilder,
                              nsTArray<nsIFrame*>* aOutModifiedFrames,
                              nsTArray<nsIFrame*>* aOutFramesWithProps)
{
  MOZ_ASSERT(aBuilder->RootReferenceFrame());

  TakeAndAddModifiedAndFramesWithPropsFromRootFrame(aOutModifiedFrames,
                                                    aOutFramesWithProps,
                                                    aBuilder->RootReferenceFrame());

  nsIDocument* rootdoc = aBuilder->RootReferenceFrame()->PresContext()->Document();

  if (rootdoc) {
    CbData data = {
      aBuilder,
      aOutModifiedFrames,
      aOutFramesWithProps
    };

    rootdoc->EnumerateSubDocuments(SubDocEnumCb, &data);
  }
}

// ComputeRebuildRegion  debugging
// #define CRR_DEBUG 1
#if CRR_DEBUG
#  define CRR_LOG(...) printf_stderr(__VA_ARGS__)
#else
#  define CRR_LOG(...)
#endif

static nsDisplayItem*
GetFirstDisplayItemWithChildren(nsIFrame* aFrame)
{
  nsIFrame::DisplayItemArray* items = aFrame->GetProperty(nsIFrame::DisplayItems());
  if (!items) {
    return nullptr;
  }
  
  for (nsDisplayItem* i : *items) {
    if (i->GetChildren()) {
      return i;
    }
  }
  return nullptr;
}

static nsIFrame*
HandlePreserve3D(nsIFrame* aFrame, nsRect& aOverflow)
{
  // Preserve-3d frames don't have valid overflow areas, and they might
  // have singular transforms (despite still being visible when combined
  // with their ancestors). If we're at one, jump up to the root of the
  // preserve-3d context and use the whole overflow area.
  nsIFrame* last = aFrame;
  while (aFrame->Extend3DContext() ||
         aFrame->Combines3DTransformWithAncestors()) {
    last = aFrame;
    aFrame = aFrame->GetParent();
  }
  if (last != aFrame) {
    aOverflow = last->GetVisualOverflowRectRelativeToParent();
    CRR_LOG("HandlePreserve3D() Updated overflow rect to: %d %d %d %d\n",
             aOverflow.x, aOverflow.y, aOverflow.width, aOverflow.height);
  }

  return aFrame;
}

static void
ProcessFrameInternal(nsIFrame* aFrame, nsDisplayListBuilder& aBuilder,
                     AnimatedGeometryRoot** aAGR, nsRect& aOverflow,
                     nsIFrame* aStopAtFrame, nsTArray<nsIFrame*>& aOutFramesWithProps,
                     const bool aStopAtStackingContext)
{
  nsIFrame* currentFrame = aFrame;

  while (currentFrame != aStopAtFrame) {
    CRR_LOG("currentFrame: %p (placeholder=%d), aOverflow: %d %d %d %d\n",
             currentFrame, !aStopAtStackingContext,
             aOverflow.x, aOverflow.y, aOverflow.width, aOverflow.height);

    currentFrame = HandlePreserve3D(currentFrame, aOverflow);

    // If the current frame is an OOF frame, DisplayListBuildingData needs to be
    // set on all the ancestor stacking contexts of the  placeholder frame, up
    // to the containing block of the OOF frame. This is done to ensure that the
    // content that might be behind the OOF frame is built for merging.
    nsIFrame* placeholder = currentFrame->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW)
                          ? currentFrame->GetPlaceholderFrame()
                          : nullptr;

    if (placeholder) {
      // The rect aOverflow is in the coordinate space of the containing block.
      // Convert it to a coordinate space of the placeholder frame.
      nsRect placeholderOverflow =
        aOverflow + currentFrame->GetOffsetTo(placeholder);

      CRR_LOG("Processing placeholder %p for OOF frame %p\n",
              placeholder, currentFrame);

      CRR_LOG("OOF frame draw area: %d %d %d %d\n",
              placeholderOverflow.x, placeholderOverflow.y,
              placeholderOverflow.width, placeholderOverflow.height);

      // Tracking AGRs for the placeholder processing is not necessary, as the
      // goal is to only modify the DisplayListBuildingData rect.
      AnimatedGeometryRoot* dummyAGR = nullptr;

      // Find a common ancestor frame to handle frame continuations.
      // TODO: It might be possible to write a more specific and efficient
      // function for this.
      nsIFrame* ancestor =
        nsLayoutUtils::FindNearestCommonAncestorFrame(currentFrame->GetParent(),
                                                      placeholder->GetParent());

      ProcessFrameInternal(placeholder, aBuilder, &dummyAGR, placeholderOverflow,
                           ancestor, aOutFramesWithProps, false);
    }

    // Convert 'aOverflow' into the coordinate space of the nearest stacking context
    // or display port ancestor and update 'currentFrame' to point to that frame.
    aOverflow = nsLayoutUtils::TransformFrameRectToAncestor(currentFrame, aOverflow, aStopAtFrame,
                                                           nullptr, nullptr,
                                                           /* aStopAtStackingContextAndDisplayPortAndOOFFrame = */ true,
                                                           &currentFrame);
    MOZ_ASSERT(currentFrame);

    if (nsLayoutUtils::FrameHasDisplayPort(currentFrame)) {
      CRR_LOG("Frame belongs to displayport frame %p\n", currentFrame);
      nsIScrollableFrame* sf = do_QueryFrame(currentFrame);
      MOZ_ASSERT(sf);
      nsRect displayPort;
      DebugOnly<bool> hasDisplayPort =
        nsLayoutUtils::GetDisplayPort(currentFrame->GetContent(), &displayPort,
                                      RelativeTo::ScrollPort);
      MOZ_ASSERT(hasDisplayPort);
      // get it relative to the scrollport (from the scrollframe)
      nsRect r = aOverflow - sf->GetScrollPortRect().TopLeft();
      r.IntersectRect(r, displayPort);
      if (!r.IsEmpty()) {
        nsRect* rect =
          currentFrame->GetProperty(nsDisplayListBuilder::DisplayListBuildingDisplayPortRect());
        if (!rect) {
          rect = new nsRect();
          currentFrame->SetProperty(nsDisplayListBuilder::DisplayListBuildingDisplayPortRect(), rect);
          currentFrame->SetHasOverrideDirtyRegion(true);
          aOutFramesWithProps.AppendElement(currentFrame);
        }
        rect->UnionRect(*rect, r);
        CRR_LOG("Adding area to displayport draw area: %d %d %d %d\n",
                r.x, r.y, r.width, r.height);

        // TODO: Can we just use MarkFrameForDisplayIfVisible, plus MarkFramesForDifferentAGR to
        // ensure that this displayport, plus any items that move relative to it get rebuilt,
        // and then not contribute to the root dirty area?
        aOverflow = sf->GetScrollPortRect();
      } else {
        // Don't contribute to the root dirty area at all.
        aOverflow.SetEmpty();
      }
    } else {
      aOverflow.IntersectRect(aOverflow, currentFrame->GetVisualOverflowRectRelativeToSelf());
    }

    if (aOverflow.IsEmpty()) {
      break;
    }

    if (currentFrame != aBuilder.RootReferenceFrame() &&
        currentFrame->IsStackingContext() &&
        currentFrame->IsFixedPosContainingBlock()) {
      CRR_LOG("Frame belongs to stacking context frame %p\n", currentFrame);
      // If we found an intermediate stacking context with an existing display item
      // then we can store the dirty rect there and stop. If we couldn't find one then
      // we need to keep bubbling up to the next stacking context.
      nsDisplayItem* wrapperItem = GetFirstDisplayItemWithChildren(currentFrame);
      if (!wrapperItem) {
        continue;
      }

      // Store the stacking context relative dirty area such
      // that display list building will pick it up when it
      // gets to it.
      nsDisplayListBuilder::DisplayListBuildingData* data =
        currentFrame->GetProperty(nsDisplayListBuilder::DisplayListBuildingRect());
      if (!data) {
        data = new nsDisplayListBuilder::DisplayListBuildingData();
        currentFrame->SetProperty(nsDisplayListBuilder::DisplayListBuildingRect(), data);
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

      // Grab the visible (display list building) rect for children of this wrapper
      // item and convert into into coordinate relative to the current frame.
      nsRect previousVisible = wrapperItem->GetBuildingRectForChildren();
      if (wrapperItem->ReferenceFrameForChildren() == wrapperItem->ReferenceFrame()) {
        previousVisible -= wrapperItem->ToReferenceFrame();
      } else {
        MOZ_ASSERT(wrapperItem->ReferenceFrameForChildren() == wrapperItem->Frame());
      }

      if (!previousVisible.Contains(aOverflow)) {
        // If the overflow area of the changed frame isn't contained within the old
        // item, then we might change the size of the item and need to update its
        // sorting accordingly. Keep propagating the overflow area up so that we
        // build intersecting items for sorting.
        continue;
      }


      if (!data->mModifiedAGR) {
        data->mModifiedAGR = *aAGR;
      } else if (data->mModifiedAGR != *aAGR) {
        data->mDirtyRect = currentFrame->GetVisualOverflowRectRelativeToSelf();
        CRR_LOG("Found multiple modified AGRs within this stacking context, giving up\n");
      }

      // Don't contribute to the root dirty area at all.
      aOverflow.SetEmpty();
      *aAGR = nullptr;

      break;
    }
  }
}

bool
RetainedDisplayListBuilder::ProcessFrame(nsIFrame* aFrame, nsDisplayListBuilder& aBuilder,
             nsIFrame* aStopAtFrame, nsTArray<nsIFrame*>& aOutFramesWithProps,
             const bool aStopAtStackingContext,
             nsRect* aOutDirty,
             AnimatedGeometryRoot** aOutModifiedAGR)
{
  if (aFrame->HasOverrideDirtyRegion()) {
    aOutFramesWithProps.AppendElement(aFrame);
  }

  if (aFrame->HasAnyStateBits(NS_FRAME_IN_POPUP)) {
    return true;
  }

  // TODO: There is almost certainly a faster way of doing this, probably can be combined with the ancestor
  // walk for TransformFrameRectToAncestor.
  AnimatedGeometryRoot* agr = aBuilder.FindAnimatedGeometryRootFor(aFrame)->GetAsyncAGR();

  CRR_LOG("Processing frame %p with agr %p\n", aFrame, agr->mFrame);

  // Convert the frame's overflow rect into the coordinate space
  // of the nearest stacking context that has an existing display item.
  // We store that as a dirty rect on that stacking context so that we build
  // all items that intersect the changed frame within the stacking context,
  // and then we use MarkFrameForDisplayIfVisible to make sure the stacking
  // context itself gets built. We don't need to build items that intersect outside
  // of the stacking context, since we know the stacking context item exists in
  // the old list, so we can trivially merge without needing other items.
  nsRect overflow = aFrame->GetVisualOverflowRectRelativeToSelf();

  // If the modified frame is also a caret frame, include the caret area.
  // This is needed because some frames (for example text frames without text)
  // might have an empty overflow rect.
  if (aFrame == aBuilder.GetCaretFrame()) {
    overflow.UnionRect(overflow, aBuilder.GetCaretRect());
  }

  ProcessFrameInternal(aFrame, aBuilder, &agr, overflow, aStopAtFrame,
                       aOutFramesWithProps, aStopAtStackingContext);

  if (!overflow.IsEmpty()) {
    aOutDirty->UnionRect(*aOutDirty, overflow);
    CRR_LOG("Adding area to root draw area: %d %d %d %d\n",
            overflow.x, overflow.y, overflow.width, overflow.height);

    // If we get changed frames from multiple AGRS, then just give up as it gets really complex to
    // track which items would need to be marked in MarkFramesForDifferentAGR.
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

static void
AddFramesForContainingBlock(nsIFrame* aBlock,
                            const nsFrameList& aFrames,
                            nsTArray<nsIFrame*>& aExtraFrames)
{
  for (nsIFrame* f : aFrames) {
    if (!f->IsFrameModified() &&
        AnyContentAncestorModified(f, aBlock)) {
      CRR_LOG("Adding invalid OOF %p\n", f);
      aExtraFrames.AppendElement(f);
    }
  }
}

// Placeholder descendants of aFrame don't contribute to aFrame's overflow area.
// Find all the containing blocks that might own placeholders under us, walk
// their OOF frames list, and manually invalidate any frames that are descendants
// of a modified frame (us, or another frame we'll get to soon).
// This is combined with the work required for MarkFrameForDisplayIfVisible,
// so that we can avoid an extra ancestor walk, and we can reuse the flag
// to detect when we've already visited an ancestor (and thus all further ancestors
// must also be visited).
void FindContainingBlocks(nsIFrame* aFrame,
                          nsTArray<nsIFrame*>& aExtraFrames)
{
  for (nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetParentOrPlaceholderForCrossDoc(f)) {
    if (f->ForceDescendIntoIfVisible())
      return;
    f->SetForceDescendIntoIfVisible(true);
    CRR_LOG("Considering OOFs for %p\n", f);

    AddFramesForContainingBlock(f, f->GetChildList(nsIFrame::kFloatList), aExtraFrames);
    AddFramesForContainingBlock(f, f->GetChildList(f->GetAbsoluteListID()), aExtraFrames);
  }
}

/**
 * Given a list of frames that has been modified, computes the region that we need to
 * do display list building for in order to build all modified display items.
 *
 * When a modified frame is within a stacking context (with an existing display item),
 * then we only contribute to the build area within the stacking context, as well as forcing
 * display list building to descend to the stacking context. We don't need to add build
 * area outside of the stacking context (and force items above/below the stacking context
 * container item to be built), since just matching the position of the stacking context
 * container item is sufficient to ensure correct ordering during merging.
 *
 * We need to rebuild all items that might intersect with the modified frame, both now
 * and during async changes on the compositor. We do this by rebuilding the area covered
 * by the changed frame, as well as rebuilding all items that have a different (async)
 * AGR to the changed frame. If we have changes to multiple AGRs (within a stacking
 * context), then we rebuild that stacking context entirely.
 *
 * @param aModifiedFrames The list of modified frames.
 * @param aOutDirty The result region to use for display list building.
 * @param aOutModifiedAGR The modified AGR for the root stacking context.
 * @param aOutFramesWithProps The list of frames to which we attached partial build
 * data so that it can be cleaned up.
 *
 * @return true if we succesfully computed a partial rebuild region, false if a full
 * build is required.
 */
bool
RetainedDisplayListBuilder::ComputeRebuildRegion(nsTArray<nsIFrame*>& aModifiedFrames,
                                                 nsRect* aOutDirty,
                                                 AnimatedGeometryRoot** aOutModifiedAGR,
                                                 nsTArray<nsIFrame*>& aOutFramesWithProps)
{
  CRR_LOG("Computing rebuild regions for %zu frames:\n", aModifiedFrames.Length());
  nsTArray<nsIFrame*> extraFrames;
  for (nsIFrame* f : aModifiedFrames) {
    MOZ_ASSERT(f);

    mBuilder.AddFrameMarkedForDisplayIfVisible(f);
    FindContainingBlocks(f, extraFrames);

    if (!ProcessFrame(f, mBuilder, mBuilder.RootReferenceFrame(),
                      aOutFramesWithProps, true,
                      aOutDirty, aOutModifiedAGR)) {
      return false;
    }
  }

  for (nsIFrame* f : extraFrames) {
    mBuilder.MarkFrameModifiedDuringBuilding(f);

    if (!ProcessFrame(f, mBuilder, mBuilder.RootReferenceFrame(),
                      aOutFramesWithProps, true,
                      aOutDirty, aOutModifiedAGR)) {
      return false;
    }
  }

  return true;
}

/*
 * A simple early exit heuristic to avoid slow partial display list rebuilds.
 */
static bool
ShouldBuildPartial(nsTArray<nsIFrame*>& aModifiedFrames)
{
  if (aModifiedFrames.Length() > gfxPrefs::LayoutRebuildFrameLimit()) {
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
        type == LayoutFrameType::Canvas ||
        type == LayoutFrameType::Scrollbar) {
      return false;
    }
  }

  return true;
}

static void
ClearFrameProps(nsTArray<nsIFrame*>& aFrames)
{
  for (nsIFrame* f : aFrames) {
    if (f->HasOverrideDirtyRegion()) {
      f->SetHasOverrideDirtyRegion(false);
      f->DeleteProperty(nsDisplayListBuilder::DisplayListBuildingRect());
      f->DeleteProperty(nsDisplayListBuilder::DisplayListBuildingDisplayPortRect());
    }

    f->SetFrameIsModified(false);
  }
}

class AutoClearFramePropsArray
{
public:
  AutoClearFramePropsArray() = default;

  ~AutoClearFramePropsArray()
  {
    ClearFrameProps(mFrames);
  }

  nsTArray<nsIFrame*>& Frames() { return mFrames; }

  bool IsEmpty() const { return mFrames.IsEmpty(); }

private:
  nsTArray<nsIFrame*> mFrames;
};

void
RetainedDisplayListBuilder::ClearFramesWithProps()
{
  AutoClearFramePropsArray modifiedFrames;
  AutoClearFramePropsArray framesWithProps;
  GetModifiedAndFramesWithProps(&mBuilder, &modifiedFrames.Frames(), &framesWithProps.Frames());
}

auto
RetainedDisplayListBuilder::AttemptPartialUpdate(
  nscolor aBackstop,
  mozilla::DisplayListChecker* aChecker) -> PartialUpdateResult
{
  mBuilder.RemoveModifiedWindowRegions();
  mBuilder.ClearWindowOpaqueRegion();

  if (mBuilder.ShouldSyncDecodeImages()) {
    MarkFramesWithItemsAndImagesModified(&mList);
  }

  mBuilder.EnterPresShell(mBuilder.RootReferenceFrame());

  // We set the override dirty regions during ComputeRebuildRegion or in
  // nsLayoutUtils::InvalidateForDisplayPortChange. The display port change also
  // marks the frame modified, so those regions are cleared here as well.
  AutoClearFramePropsArray modifiedFrames;
  AutoClearFramePropsArray framesWithProps;
  GetModifiedAndFramesWithProps(&mBuilder, &modifiedFrames.Frames(), &framesWithProps.Frames());

  // Do not allow partial builds if the retained display list is empty, or if
  // ShouldBuildPartial heuristic fails.
  bool shouldBuildPartial = !mList.IsEmpty() && ShouldBuildPartial(modifiedFrames.Frames());

  // We don't support retaining with overlay scrollbars, since they require
  // us to look at the display list and pick the highest z-index, which
  // we can't do during partial building.
  if (mBuilder.DisablePartialUpdates()) {
    shouldBuildPartial = false;
    mBuilder.SetDisablePartialUpdates(false);
  }

  if (mPreviousCaret != mBuilder.GetCaretFrame()) {
    if (mPreviousCaret) {
      if (mBuilder.MarkFrameModifiedDuringBuilding(mPreviousCaret)) {
        modifiedFrames.Frames().AppendElement(mPreviousCaret);
      }
    }

    if (mBuilder.GetCaretFrame()) {
      if (mBuilder.MarkFrameModifiedDuringBuilding(mBuilder.GetCaretFrame())) {
        modifiedFrames.Frames().AppendElement(mBuilder.GetCaretFrame());
      }
    }

    mPreviousCaret = mBuilder.GetCaretFrame();
  }

  nsRect modifiedDirty;
  AnimatedGeometryRoot* modifiedAGR = nullptr;
  if (!shouldBuildPartial ||
      !ComputeRebuildRegion(modifiedFrames.Frames(), &modifiedDirty,
                           &modifiedAGR, framesWithProps.Frames()) ||
      !PreProcessDisplayList(&mList, modifiedAGR)) {
    mBuilder.LeavePresShell(mBuilder.RootReferenceFrame(), List());
    mList.ClearDAG();
    return PartialUpdateResult::Failed;
  }

  // This is normally handled by EnterPresShell, but we skipped it so that we
  // didn't call MarkFrameForDisplayIfVisible before ComputeRebuildRegion.
  nsIScrollableFrame* sf = mBuilder.RootReferenceFrame()->PresShell()->GetRootScrollFrameAsScrollable();
  if (sf) {
    nsCanvasFrame* canvasFrame = do_QueryFrame(sf->GetScrolledFrame());
    if (canvasFrame) {
      mBuilder.MarkFrameForDisplayIfVisible(canvasFrame, mBuilder.RootReferenceFrame());
    }
  }

  modifiedDirty.IntersectRect(modifiedDirty, mBuilder.RootReferenceFrame()->GetVisualOverflowRectRelativeToSelf());

  PartialUpdateResult result = PartialUpdateResult::NoChange;
  if (!modifiedDirty.IsEmpty() ||
      !framesWithProps.IsEmpty()) {
    result = PartialUpdateResult::Updated;
  }

  mBuilder.SetDirtyRect(modifiedDirty);
  mBuilder.SetPartialUpdate(true);

  nsDisplayList modifiedDL;
  mBuilder.RootReferenceFrame()->BuildDisplayListForStackingContext(&mBuilder, &modifiedDL);
  if (!modifiedDL.IsEmpty()) {
    nsLayoutUtils::AddExtraBackgroundItems(mBuilder, modifiedDL, mBuilder.RootReferenceFrame(),
                                           nsRect(nsPoint(0, 0), mBuilder.RootReferenceFrame()->GetSize()),
                                           mBuilder.RootReferenceFrame()->GetVisualOverflowRectRelativeToSelf(),
                                           aBackstop);
  }
  mBuilder.SetPartialUpdate(false);

  if (mBuilder.PartialBuildFailed()) {
    mBuilder.SetPartialBuildFailed(false);
    mBuilder.LeavePresShell(mBuilder.RootReferenceFrame(), List());
    mList.ClearDAG();
    return PartialUpdateResult::Failed;
  }

  if (aChecker) {
    aChecker->Set(&modifiedDL, "TM");
  }

  //printf_stderr("Painting --- Modified list (dirty %d,%d,%d,%d):\n",
  //              modifiedDirty.x, modifiedDirty.y, modifiedDirty.width, modifiedDirty.height);
  //nsFrame::PrintDisplayList(&mBuilder, modifiedDL);

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

  //printf_stderr("Painting --- Merged list:\n");
  //nsFrame::PrintDisplayList(&mBuilder, mList);

  mBuilder.LeavePresShell(mBuilder.RootReferenceFrame(), List());
  return result;
}
