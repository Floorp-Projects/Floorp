/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RETAINEDDISPLAYLISTBUILDER_H_
#define RETAINEDDISPLAYLISTBUILDER_H_

#include "nsDisplayList.h"
#include "mozilla/Maybe.h"
#include "mozilla/TypedEnumBits.h"

namespace mozilla {
class DisplayListChecker;
} // namespace mozilla


/**
 * RetainedDisplayListData contains frame invalidation information. It is stored
 * in root frames, and used by RetainedDisplayListBuilder.
 * Currently this is implemented as a map of frame pointers to flags.
 */
struct RetainedDisplayListData
{
  NS_DECLARE_FRAME_PROPERTY_DELETABLE(DisplayListData, RetainedDisplayListData)

  enum class FrameFlags : uint8_t
  {
    None = 0,
    Modified = 1 << 0,
    HasProps = 1 << 1,
    HadWillChange = 1 << 2
  };

  /**
   * Removes all the frames from this RetainedDisplayListData.
   */
  void Clear() { mFrames.Clear(); }

  /**
   * Returns a mutable reference to flags set for the given |aFrame|. If the
   * frame does not exist in this RetainedDisplayListData, it is added with
   * default constructible flags FrameFlags::None.
   */
  FrameFlags& Flags(nsIFrame* aFrame) { return mFrames.GetOrInsert(aFrame); }

  /**
   * Returns flags set for the given |aFrame|, or FrameFlags::None if the frame
   * is not in this RetainedDisplayListData.
   */
  FrameFlags GetFlags(nsIFrame* aFrame) const { return mFrames.Get(aFrame); }

  /**
   * Returns an iterator to the underlying frame storage.
   */
  auto Iterator() { return mFrames.Iter(); }

  /**
   * Removes the given |aFrame| from this RetainedDisplayListData.
   */
  bool Remove(nsIFrame* aFrame) { return mFrames.Remove(aFrame); }

private:
  nsDataHashtable<nsPtrHashKey<nsIFrame>, FrameFlags> mFrames;
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(RetainedDisplayListData::FrameFlags)

/**
 * Returns RetainedDisplayListData property for the given |aRootFrame|, or
 * nullptr if the property is not set.
 */
RetainedDisplayListData*
GetRetainedDisplayListData(nsIFrame* aRootFrame);

/**
 * Returns RetainedDisplayListData property for the given |aRootFrame|. Creates
 * and sets a new RetainedDisplayListData property if it is not already set.
 */
RetainedDisplayListData*
GetOrSetRetainedDisplayListData(nsIFrame* aRootFrame);

struct RetainedDisplayListBuilder
{
  RetainedDisplayListBuilder(nsIFrame* aReferenceFrame,
                             nsDisplayListBuilderMode aMode,
                             bool aBuildCaret)
    : mBuilder(aReferenceFrame, aMode, aBuildCaret, true)
  {
  }
  ~RetainedDisplayListBuilder() { mList.DeleteAll(&mBuilder); }

  nsDisplayListBuilder* Builder() { return &mBuilder; }

  nsDisplayList* List() { return &mList; }

  enum class PartialUpdateResult
  {
    Failed,
    NoChange,
    Updated
  };

  PartialUpdateResult AttemptPartialUpdate(
    nscolor aBackstop,
    mozilla::DisplayListChecker* aChecker);

  /**
   * Iterates through the display list builder reference frame document and
   * subdocuments, and clears the modified frame lists from the root frames.
   * Also clears the frame properties set by RetainedDisplayListBuilder for all
   * the frames in the modified frame lists.
   */
  void ClearFramesWithProps();

  NS_DECLARE_FRAME_PROPERTY_DELETABLE(Cached, RetainedDisplayListBuilder)

private:
  bool PreProcessDisplayList(RetainedDisplayList* aList,
                             AnimatedGeometryRoot* aAGR,
                             uint32_t aCallerKey = 0,
                             uint32_t aNestingDepth = 0);
  bool MergeDisplayLists(
    nsDisplayList* aNewList,
    RetainedDisplayList* aOldList,
    RetainedDisplayList* aOutList,
    mozilla::Maybe<const mozilla::ActiveScrolledRoot*>& aOutContainerASR,
    uint32_t aOuterKey = 0);

  bool ComputeRebuildRegion(nsTArray<nsIFrame*>& aModifiedFrames,
                            nsRect* aOutDirty,
                            AnimatedGeometryRoot** aOutModifiedAGR,
                            nsTArray<nsIFrame*>& aOutFramesWithProps);
  bool ProcessFrame(nsIFrame* aFrame,
                    nsDisplayListBuilder& aBuilder,
                    nsIFrame* aStopAtFrame,
                    nsTArray<nsIFrame*>& aOutFramesWithProps,
                    const bool aStopAtStackingContext,
                    nsRect* aOutDirty,
                    AnimatedGeometryRoot** aOutModifiedAGR);

  void IncrementSubDocPresShellPaintCount(nsDisplayItem* aItem);

  friend class MergeState;

  nsDisplayListBuilder mBuilder;
  RetainedDisplayList mList;
  WeakFrame mPreviousCaret;
};

#endif // RETAINEDDISPLAYLISTBUILDER_H_
