/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layout/FrameChildList.h"

#include "nsIFrame.h"

namespace mozilla {
namespace layout {

FrameChildListIterator::FrameChildListIterator(const nsIFrame* aFrame)
  : FrameChildListArrayIterator(mLists)
{
  aFrame->GetChildLists(&mLists);
#ifdef DEBUG
  // Make sure that there are no duplicate list IDs.
  FrameChildListIDs ids;
  uint32_t count = mLists.Length();
  for (uint32_t i = 0; i < count; ++i) {
    NS_ASSERTION(!ids.Contains(mLists[i].mID),
                 "Duplicate item found!");
    ids |= mLists[i].mID;
  }
#endif
}

#ifdef DEBUG
const char*
ChildListName(FrameChildListID aListID)
{
  switch (aListID) {
    case kPrincipalList: return "";
    case kPopupList: return "PopupList";
    case kCaptionList: return "CaptionList";
    case kColGroupList: return "ColGroupList";
    case kSelectPopupList: return "SelectPopupList";
    case kAbsoluteList: return "AbsoluteList";
    case kFixedList: return "FixedList";
    case kOverflowList: return "OverflowList";
    case kOverflowContainersList: return "OverflowContainersList";
    case kExcessOverflowContainersList: return "ExcessOverflowContainersList";
    case kOverflowOutOfFlowList: return "OverflowOutOfFlowList";
    case kFloatList: return "FloatList";
    case kBulletList: return "BulletList";
    case kPushedFloatsList: return "PushedFloatsList";
    case kNoReflowPrincipalList: return "NoReflowPrincipalList";
  }

  NS_NOTREACHED("unknown list");
  return "UNKNOWN_FRAME_CHILD_LIST";
}
#endif

} // namespace layout
} // namespace mozilla
