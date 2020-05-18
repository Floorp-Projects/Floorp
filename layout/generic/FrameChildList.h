/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FrameChildList_h_
#define FrameChildList_h_

#include "mozilla/EnumSet.h"
#include "nsFrameList.h"
#include "nsTArray.h"

class nsIFrame;

namespace mozilla {
namespace layout {

// enum FrameChildListID lives in nsFrameList.h to solve circular dependencies.

#ifdef DEBUG_FRAME_DUMP
extern const char* ChildListName(FrameChildListID aListID);
#endif

using FrameChildListIDs = EnumSet<FrameChildListID>;

class FrameChildList {
 public:
  FrameChildList(const nsFrameList& aList, FrameChildListID aID)
      : mList(aList), mID(aID) {}
  nsFrameList mList;
  FrameChildListID mID;
};

}  // namespace layout
}  // namespace mozilla

inline void nsFrameList::AppendIfNonempty(
    nsTArray<mozilla::layout::FrameChildList>* aLists,
    mozilla::layout::FrameChildListID aListID) const {
  if (NotEmpty()) {
    aLists->EmplaceBack(*this, aListID);
  }
}

#endif /* !defined(FrameChildList_h_) */
