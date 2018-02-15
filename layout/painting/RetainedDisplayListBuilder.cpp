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

/**
 * Code for doing display list building for a modified subset of the window,
 * and then merging it into the existing display list (for the full window).
 *
 * The approach primarily hinges on the observation that the ‘true’ ordering of
 * display items is represented by a DAG (only items that intersect in 2d space
 * have a defined ordering). Our display list is just one of a many possible linear
 * representations of this ordering.
 *
 * Each time a frame changes (gets a new style context, or has a size/position
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

void MarkFramesWithItemsAndImagesModified(nsDisplayList* aList)
{
  for (nsDisplayItem* i = aList->GetBottom(); i != nullptr; i = i->GetAbove()) {
    if (!i->HasDeletedFrame() && i->CanBeReused() && !i->Frame()->IsFrameModified()) {
      // If we have existing cached geometry for this item, then check that for
      // whether we need to invalidate for a sync decode. If we don't, then
      // use the item's flags.
      DisplayItemData* data = FrameLayerBuilder::GetOldDataFor(i);
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

bool IsAnyAncestorModified(nsIFrame* aFrame)
{
  nsIFrame* f = aFrame;
  while (f) {
    if (f->IsFrameModified()) {
      return true;
    }
    f = nsLayoutUtils::GetCrossDocParentFrame(f);
  }
  return false;
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
RetainedDisplayListBuilder::PreProcessDisplayList(nsDisplayList* aList,
                                                  AnimatedGeometryRoot* aAGR)
{
  bool modified = false;
  nsDisplayList saved;
  while (nsDisplayItem* i = aList->RemoveBottom()) {
    if (i->HasDeletedFrame() || !i->CanBeReused()) {
      i->Destroy(&mBuilder);
      modified = true;
      continue;
    }

    nsIFrame* f = i->Frame();

    if (i->GetChildren()) {
      if (PreProcessDisplayList(i->GetChildren(), SelectAGRForFrame(f, aAGR))) {
        modified = true;
      }
    }

    // TODO: We should be able to check the clipped bounds relative
    // to the common AGR (of both the existing item and the invalidated
    // frame) and determine if they can ever intersect.
    if (aAGR && i->GetAnimatedGeometryRoot()->GetAsyncAGR() != aAGR) {
      mBuilder.MarkFrameForDisplayIfVisible(f, mBuilder.RootReferenceFrame());
      modified = true;
    }

    // TODO: This is here because we sometimes reuse the previous display list
    // completely. For optimization, we could only restore the state for reused
    // display items.
    i->RestoreState();

    saved.AppendToTop(i);
  }
  aList->AppendToTop(&saved);
  aList->RestoreState();
  return modified;
}

bool IsSameItem(nsDisplayItem* aFirst, nsDisplayItem* aSecond)
{
  return aFirst->Frame() == aSecond->Frame() &&
         aFirst->GetPerFrameKey() == aSecond->GetPerFrameKey();
}

struct DisplayItemKey
{
  bool operator ==(const DisplayItemKey& aOther) const {
    return mFrame == aOther.mFrame &&
           mPerFrameKey == aOther.mPerFrameKey;
  }

  nsIFrame* mFrame;
  uint32_t mPerFrameKey;
};

class DisplayItemHashEntry : public PLDHashEntryHdr
{
public:
  typedef DisplayItemKey KeyType;
  typedef const DisplayItemKey* KeyTypePointer;

  explicit DisplayItemHashEntry(KeyTypePointer aKey)
    : mKey(*aKey) {}
  explicit DisplayItemHashEntry(const DisplayItemHashEntry& aCopy)=default;

  ~DisplayItemHashEntry() = default;

  KeyType GetKey() const { return mKey; }
  bool KeyEquals(KeyTypePointer aKey) const
  {
    return mKey == *aKey;
  }

  static KeyTypePointer KeyToPointer(KeyType& aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey)
  {
    if (!aKey)
      return 0;

    return mozilla::HashGeneric(aKey->mFrame, aKey->mPerFrameKey);
  }
  enum { ALLOW_MEMMOVE = true };

  DisplayItemKey mKey;
};

template<typename T>
void SwapAndRemove(nsTArray<T>& aArray, uint32_t aIndex)
{
  if (aIndex != (aArray.Length() - 1)) {
    T last = aArray.LastElement();
    aArray.LastElement() = aArray[aIndex];
    aArray[aIndex] = last;
  }

  aArray.RemoveElementAt(aArray.Length() - 1);
}

static bool
MergeFrameRects(nsDisplayLayerEventRegions* aOldItem,
                nsDisplayLayerEventRegions* aNewItem,
                nsDisplayLayerEventRegions::FrameRects nsDisplayLayerEventRegions::*aRectList,
                nsTArray<nsIFrame*>& aAddedFrames)
{
  bool modified = false;
  // Go through the old item's rect list and remove any rectangles
  // belonging to invalidated frames (deleted frames should
  // already be gone at this point)
  nsDisplayLayerEventRegions::FrameRects& oldRects = aOldItem->*aRectList;
  uint32_t i = 0;
  while (i < oldRects.mFrames.Length()) {
    // TODO: As mentioned in nsDisplayLayerEventRegions, this
    // operation might perform really poorly on a vector.
    nsIFrame* f = oldRects.mFrames[i];
    if (IsAnyAncestorModified(f)) {
      MOZ_ASSERT(f != aOldItem->Frame());
      f->RemoveDisplayItem(aOldItem);
      SwapAndRemove(oldRects.mFrames, i);
      SwapAndRemove(oldRects.mBoxes, i);
      modified = true;
    } else {
      i++;
    }
  }
  if (!aNewItem) {
    return modified;
  }

  // Copy items from the source list to the dest list, but
  // only if the dest doesn't already include them.
  nsDisplayItem* destItem = aOldItem;
  nsDisplayLayerEventRegions::FrameRects* destRects = &(aOldItem->*aRectList);
  nsDisplayLayerEventRegions::FrameRects* srcRects = &(aNewItem->*aRectList);

  for (uint32_t i = 0; i < srcRects->mFrames.Length(); i++) {
    nsIFrame* f = srcRects->mFrames[i];
    if (!f->HasDisplayItem(destItem)) {
      // If this frame isn't already in the destination item,
      // then add it!
      destRects->Add(f, srcRects->mBoxes[i]);

      // We also need to update RealDisplayItemData for 'f',
      // but that'll mess up this check for the following
      // FrameRects lists, so defer that until the end.
      aAddedFrames.AppendElement(f);
      MOZ_ASSERT(f != aOldItem->Frame());

      modified = true;
    }

  }
  return modified;
}

bool MergeLayerEventRegions(nsDisplayItem* aOldItem,
                            nsDisplayItem* aNewItem)
{
  nsDisplayLayerEventRegions* oldItem =
    static_cast<nsDisplayLayerEventRegions*>(aOldItem);
  nsDisplayLayerEventRegions* newItem =
    static_cast<nsDisplayLayerEventRegions*>(aNewItem);

  nsTArray<nsIFrame*> addedFrames;

  bool modified = false;
  modified |= MergeFrameRects(oldItem, newItem, &nsDisplayLayerEventRegions::mHitRegion, addedFrames);
  modified |= MergeFrameRects(oldItem, newItem, &nsDisplayLayerEventRegions::mMaybeHitRegion, addedFrames);
  modified |= MergeFrameRects(oldItem, newItem, &nsDisplayLayerEventRegions::mDispatchToContentHitRegion, addedFrames);
  modified |= MergeFrameRects(oldItem, newItem, &nsDisplayLayerEventRegions::mNoActionRegion, addedFrames);
  modified |= MergeFrameRects(oldItem, newItem, &nsDisplayLayerEventRegions::mHorizontalPanRegion, addedFrames);
  modified |= MergeFrameRects(oldItem, newItem, &nsDisplayLayerEventRegions::mVerticalPanRegion, addedFrames);

  // MergeFrameRects deferred updating the display item data list during
  // processing so that earlier calls didn't change the result of later
  // ones. Fix that up now.
  for (nsIFrame* f : addedFrames) {
    if (!f->HasDisplayItem(aOldItem)) {
      f->AddDisplayItem(aOldItem);
    }
  }
  return modified;
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

void UpdateASR(nsDisplayItem* aItem,
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

/**
 * Takes two display lists and merges them into an output list.
 *
 * The basic algorithm is:
 *
 * For-each item i in the new list:
 *     If the item has a matching item in the old list:
 *         Remove items from the start of the old list up until we reach an item that also exists in the new list (leaving the matched item in place):
 *             Add valid items to the merged list, destroy invalid items.
 *     Add i into the merged list.
 *     If the start of the old list matches i, remove and destroy it, otherwise mark the old version of i as used.
 * Add all remaining valid items from the old list into the merged list, skipping over (and destroying) any that are marked as used.
 *
 * If any item has a child display list, then we recurse into the merge
 * algorithm once we match up the new/old versions (if present).
 *
 * Example 1:
 *
 * Old List: A,B,C,D
 * Modified List: A,D
 * Invalidations: C,D
 *
 * We first match the A items, and add the new one to the merged list.
 * We then match the D items, copy B into the merged list, but not C
 * (since it's invalid). We then add the new D to the list and we're
 * finished.
 *
 * Merged List: A,B,D
 *
 * Example 2 (layout/reftests/retained-dl-zindex-1.html):
 *
 * Old List: A, B
 * Modified List: B, A
 * Invalidations: A
 *
 * In this example A has been explicitly moved to the back.
 *
 * We match the B items, but don't copy A since it's invalid, and then add the
 * new B into the merged list. We then add A, and we're done.
 *
 * Merged List: B, A
 *
 * Example 3:
 *
 * Old List: A, B
 * Modified List: B, A
 * Invalidations: -
 *
 * This can happen because a prior merge might have changed the ordering
 * for non-intersecting items.
 *
 * We match the B items, but don't copy A since it's also present in the new list
 * and then add the new B into the merged list. We then add A, and we're done.
 *
 * Merged List: B, A
 *
 * Example 4 (layout/reftests/retained-dl-zindex-2.html):
 *
 * Element A has two elements covering it (B and C), that don't intersect each
 * other. We then move C to the back.
 *
 * The correct initial ordering has B and C after A, in any order.
 *
 * Old List: A, B, C
 * Modified List: C, A
 * Invalidations: C
 *
 * We match the C items, but don't add anything from the old list because A is present
 * in both lists. We add C to the merged list, and mark the old version of C as reused.
 *
 * We then match A, add the new version the merged list and delete the old version.
 *
 * We then process the remainder of the old list, B is added (since it is valid,
 * and hasn't been mark as reused), C is destroyed since it's marked as reused and
 * is already present in the merged list.
 *
 * Merged List: C, A, B
 */
bool
RetainedDisplayListBuilder::MergeDisplayLists(nsDisplayList* aNewList,
                                              nsDisplayList* aOldList,
                                              nsDisplayList* aOutList,
                                              Maybe<const ActiveScrolledRoot*>& aOutContainerASR)
{
  bool modified = false;

  nsDisplayList merged;
  const auto UseItem = [&](nsDisplayItem* aItem) {
    const ActiveScrolledRoot* itemClipASR =
      aItem->GetClipChain() ? aItem->GetClipChain()->mASR : nullptr;

    const ActiveScrolledRoot* finiteBoundsASR = ActiveScrolledRoot::PickDescendant(
      itemClipASR, aItem->GetActiveScrolledRoot());
    if (!aOutContainerASR) {
      aOutContainerASR = Some(finiteBoundsASR);
    } else {
      aOutContainerASR =
        Some(ActiveScrolledRoot::PickAncestor(aOutContainerASR.value(), finiteBoundsASR));
    }

    merged.AppendToTop(aItem);
  };

  const auto ReuseItem = [&](nsDisplayItem* aItem) {
    UseItem(aItem);
    aItem->SetReused(true);

    if (aItem->GetType() == DisplayItemType::TYPE_SUBDOCUMENT) {
      IncrementSubDocPresShellPaintCount(aItem);
    }
  };

  const bool newListIsEmpty = aNewList->IsEmpty();
  if (!newListIsEmpty) {
    // Build a hashtable of items in the old list so we can look for them quickly.
    // We have similar data in the nsIFrame DisplayItems() property, but it doesn't
    // know which display list items are in, and we only want to match items in
    // this list.
    nsDataHashtable<DisplayItemHashEntry, nsDisplayItem*> oldListLookup(aOldList->Count());

    for (nsDisplayItem* i = aOldList->GetBottom(); i != nullptr; i = i->GetAbove()) {
      i->SetReused(false);
      oldListLookup.Put({ i->Frame(), i->GetPerFrameKey() }, i);
    }

    nsDataHashtable<DisplayItemHashEntry, nsDisplayItem*> newListLookup(aNewList->Count());
    for (nsDisplayItem* i = aNewList->GetBottom(); i != nullptr; i = i->GetAbove()) {
#ifdef DEBUG
      if (newListLookup.Get({ i->Frame(), i->GetPerFrameKey() }, nullptr)) {
        MOZ_CRASH_UNSAFE_PRINTF("Duplicate display items detected!: %s(0x%p) type=%d key=%d",
                                  i->Name(), i->Frame(),
                                  static_cast<int>(i->GetType()), i->GetPerFrameKey());
      }
#endif
      newListLookup.Put({ i->Frame(), i->GetPerFrameKey() }, i);
    }

    while (nsDisplayItem* newItem = aNewList->RemoveBottom()) {
      if (nsDisplayItem* oldItem = oldListLookup.Get({ newItem->Frame(), newItem->GetPerFrameKey() })) {
        // The new item has a matching counterpart in the old list that we haven't yet reached,
        // so copy all valid items from the old list into the merged list until we get to the
        // matched item.
        nsDisplayItem* old = nullptr;
        while ((old = aOldList->GetBottom()) && old != oldItem) {
          if (IsAnyAncestorModified(old->FrameForInvalidation())) {
            // The old item is invalid, discard it.
            oldListLookup.Remove({ old->Frame(), old->GetPerFrameKey() });
            aOldList->RemoveBottom();
            old->Destroy(&mBuilder);
            modified = true;
          } else if (newListLookup.Get({ old->Frame(), old->GetPerFrameKey() })) {
            // This old item is also in the new list, but we haven't got to it yet.
            // Stop now, and we'll deal with it when we get to the new entry.
            modified = true;
            break;
          } else {
            // Recurse into the child list (without a matching new list) to
            // ensure that we find and remove any invalidated items.
            if (old->GetChildren()) {
              nsDisplayList empty;
              Maybe<const ActiveScrolledRoot*> containerASRForChildren;
              if (MergeDisplayLists(&empty, old->GetChildren(),
                                    old->GetChildren(), containerASRForChildren)) {
                modified = true;
              }
              UpdateASR(old, containerASRForChildren);
              old->UpdateBounds(&mBuilder);
            }
            aOldList->RemoveBottom();
            ReuseItem(old);
          }
        }
        bool destroy = false;
        if (old == oldItem) {
          // If we advanced the old list until the matching item then we can pop
          // the matching item off the old list and make sure we clean it up.
          aOldList->RemoveBottom();
          destroy = true;
        } else {
          // If we didn't get to the matching item, then mark the old item
          // as being reused (since we're adding the new version to the new
          // list now) so that we don't add it twice at the end.
          oldItem->SetReused(true);
        }

        // Recursively merge any child lists, destroy the old item and add
        // the new one to the list.
        if (destroy &&
            oldItem->GetType() == DisplayItemType::TYPE_LAYER_EVENT_REGIONS &&
            !IsAnyAncestorModified(oldItem->FrameForInvalidation())) {
          // Event regions items don't have anything interesting other than
          // the lists of regions and frames, so we have no need to use the
          // newer item. Always use the old item instead since we assume it's
          // likely to have the bigger lists and merging will be quicker.
          if (MergeLayerEventRegions(oldItem, newItem)) {
            modified = true;
          }
          ReuseItem(oldItem);
          newItem->Destroy(&mBuilder);
        } else {
          if (IsAnyAncestorModified(oldItem->FrameForInvalidation())) {
            modified = true;
          } else if (oldItem->GetChildren()) {
            MOZ_ASSERT(newItem->GetChildren());
            Maybe<const ActiveScrolledRoot*> containerASRForChildren;
            if (MergeDisplayLists(newItem->GetChildren(), oldItem->GetChildren(),
                                  newItem->GetChildren(), containerASRForChildren)) {
              modified = true;
            }
            UpdateASR(newItem, containerASRForChildren);
            newItem->UpdateBounds(&mBuilder);
          }

          if (destroy) {
            oldItem->Destroy(&mBuilder);
          }
          UseItem(newItem);
        }
      } else {
        // If there was no matching item in the old list, then we only need to
        // add the new item to the merged list.
        modified = true;
        UseItem(newItem);
      }
    }
  }

  // Reuse the remaining valid items from the old display list.
  while (nsDisplayItem* old = aOldList->RemoveBottom()) {
    if (!IsAnyAncestorModified(old->FrameForInvalidation()) &&
        (!old->IsReused() || newListIsEmpty)) {
      if (old->GetChildren()) {
        // We are calling MergeDisplayLists() to ensure that the display items
        // with modified or deleted children will be correctly handled.
        // Passing an empty new display list as an argument skips the merging
        // loop above and jumps back here.
        nsDisplayList empty;
        Maybe<const ActiveScrolledRoot*> containerASRForChildren;

        if (MergeDisplayLists(&empty, old->GetChildren(),
                              old->GetChildren(), containerASRForChildren)) {
          modified = true;
        }
        UpdateASR(old, containerASRForChildren);
        old->UpdateBounds(&mBuilder);
      }
      if (old->GetType() == DisplayItemType::TYPE_LAYER_EVENT_REGIONS) {
        if (MergeLayerEventRegions(old, nullptr)) {
          modified = true;
        }
      }
      ReuseItem(old);
    } else {
      old->Destroy(&mBuilder);
      modified = true;
    }
  }

  aOutList->AppendToTop(&merged);
  return modified;
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
ProcessFrame(nsIFrame* aFrame, nsDisplayListBuilder& aBuilder,
             AnimatedGeometryRoot** aAGR, nsRect& aOverflow,
             nsIFrame* aStopAtFrame, nsTArray<nsIFrame*>& aOutFramesWithProps,
             const bool aStopAtStackingContext)
{
  nsIFrame* currentFrame = aFrame;

  aBuilder.MarkFrameForDisplayIfVisible(aFrame, aBuilder.RootReferenceFrame());

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

      ProcessFrame(placeholder, aBuilder, &dummyAGR, placeholderOverflow,
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
        currentFrame->IsStackingContext()) {
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
      nsRect previousVisible = wrapperItem->GetVisibleRectForChildren();
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
  for (nsIFrame* f : aModifiedFrames) {
    MOZ_ASSERT(f);

    if (f->HasOverrideDirtyRegion()) {
      aOutFramesWithProps.AppendElement(f);
    }

    if (f->HasAnyStateBits(NS_FRAME_IN_POPUP)) {
      continue;
    }

    // TODO: There is almost certainly a faster way of doing this, probably can be combined with the ancestor
    // walk for TransformFrameRectToAncestor.
    AnimatedGeometryRoot* agr = mBuilder.FindAnimatedGeometryRootFor(f)->GetAsyncAGR();

    CRR_LOG("Processing frame %p with agr %p\n", f, agr->mFrame);

    // Convert the frame's overflow rect into the coordinate space
    // of the nearest stacking context that has an existing display item.
    // We store that as a dirty rect on that stacking context so that we build
    // all items that intersect the changed frame within the stacking context,
    // and then we use MarkFrameForDisplayIfVisible to make sure the stacking
    // context itself gets built. We don't need to build items that intersect outside
    // of the stacking context, since we know the stacking context item exists in
    // the old list, so we can trivially merge without needing other items.
    nsRect overflow = f->GetVisualOverflowRectRelativeToSelf();

    // If the modified frame is also a caret frame, include the caret area.
    // This is needed because some frames (for example text frames without text)
    // might have an empty overflow rect.
    if (f == mBuilder.GetCaretFrame()) {
      overflow.UnionRect(overflow, mBuilder.GetCaretRect());
    }

    ProcessFrame(f, mBuilder, &agr, overflow, mBuilder.RootReferenceFrame(),
                 aOutFramesWithProps, true);

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
  const bool shouldBuildPartial = !mList.IsEmpty() && ShouldBuildPartial(modifiedFrames.Frames());

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
                           &modifiedAGR, framesWithProps.Frames())) {
    mBuilder.LeavePresShell(mBuilder.RootReferenceFrame(), &mList);
    return PartialUpdateResult::Failed;
  }

  modifiedDirty.IntersectRect(modifiedDirty, mBuilder.RootReferenceFrame()->GetVisualOverflowRectRelativeToSelf());

  PartialUpdateResult result = PartialUpdateResult::NoChange;
  if (PreProcessDisplayList(&mList, modifiedAGR) ||
      !modifiedDirty.IsEmpty() ||
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

  mBuilder.LeavePresShell(mBuilder.RootReferenceFrame(), &mList);
  return result;
}
