/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RETAINEDDISPLAYLISTBUILDER_H_
#define RETAINEDDISPLAYLISTBUILDER_H_

#include "nsDisplayList.h"
#include "mozilla/Maybe.h"

namespace mozilla {
class DisplayListChecker;
} // namespace mozilla

struct RetainedDisplayListBuilder {
  RetainedDisplayListBuilder(nsIFrame* aReferenceFrame,
                             nsDisplayListBuilderMode aMode,
                             bool aBuildCaret)
    : mBuilder(aReferenceFrame, aMode, aBuildCaret, true)
  {}
  ~RetainedDisplayListBuilder()
  {
    mList.DeleteAll(&mBuilder);
  }

  nsDisplayListBuilder* Builder() { return &mBuilder; }

  nsDisplayList* List() { return &mList; }

  enum class PartialUpdateResult {
    Failed,
    NoChange,
    Updated
  };

  PartialUpdateResult AttemptPartialUpdate(nscolor aBackstop,
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
  bool PreProcessDisplayList(RetainedDisplayList* aList, AnimatedGeometryRoot* aAGR);
  bool MergeDisplayLists(nsDisplayList* aNewList,
                         RetainedDisplayList* aOldList,
                         RetainedDisplayList* aOutList,
                         mozilla::Maybe<const mozilla::ActiveScrolledRoot*>& aOutContainerASR);

  bool ComputeRebuildRegion(nsTArray<nsIFrame*>& aModifiedFrames,
                            nsRect* aOutDirty,
                            AnimatedGeometryRoot** aOutModifiedAGR,
                            nsTArray<nsIFrame*>& aOutFramesWithProps);
  bool ProcessFrame(nsIFrame* aFrame, nsDisplayListBuilder& aBuilder,
                    nsIFrame* aStopAtFrame, nsTArray<nsIFrame*>& aOutFramesWithProps,
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
