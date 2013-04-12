/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FrameChildList_h_
#define FrameChildList_h_

#include "nsFrameList.h"
#include "nsTArray.h"

class nsIFrame;

namespace mozilla {
namespace layout {

// enum FrameChildListID lives in nsFrameList.h to solve circular dependencies.

#ifdef DEBUG
extern const char* ChildListName(FrameChildListID aListID);
#endif

class FrameChildListIDs {
friend class FrameChildListIterator;
 public:
  FrameChildListIDs() : mIDs(0) {}
  FrameChildListIDs(const FrameChildListIDs& aOther) : mIDs(aOther.mIDs) {}
  FrameChildListIDs(FrameChildListID aListID) : mIDs(aListID) {}

  FrameChildListIDs operator|(FrameChildListIDs aOther) const {
    return FrameChildListIDs(mIDs | aOther.mIDs);
  }
  FrameChildListIDs& operator|=(FrameChildListIDs aOther) {
    mIDs |= aOther.mIDs;
    return *this;
  }
  bool operator==(FrameChildListIDs aOther) const {
    return mIDs == aOther.mIDs;
  }
  bool operator!=(FrameChildListIDs aOther) const {
    return !(*this == aOther);
  }
  bool Contains(FrameChildListIDs aOther) const {
    return (mIDs & aOther.mIDs) == aOther.mIDs;
  }

 protected:
  FrameChildListIDs(uint32_t aIDs) : mIDs(aIDs) {}
  uint32_t mIDs;
};

class FrameChildList {
 public:
  FrameChildList(const nsFrameList& aList, FrameChildListID aID)
    : mList(aList), mID(aID) {}
  nsFrameList mList;
  FrameChildListID mID;
};

/**
 * A class to iterate frame child lists.
 */
class MOZ_STACK_CLASS FrameChildListArrayIterator {
 public:
  FrameChildListArrayIterator(const nsTArray<FrameChildList>& aLists)
    : mLists(aLists), mCurrentIndex(0) {}
  bool IsDone() const { return mCurrentIndex >= mLists.Length(); }
  FrameChildListID CurrentID() const {
    NS_ASSERTION(!IsDone(), "CurrentID(): iterator at end");
    return mLists[mCurrentIndex].mID;
  }
  const nsFrameList& CurrentList() const {
    NS_ASSERTION(!IsDone(), "CurrentList(): iterator at end");
    return mLists[mCurrentIndex].mList;
  }
  void Next() {
    NS_ASSERTION(!IsDone(), "Next(): iterator at end");
    ++mCurrentIndex;
  }

protected:
  const nsTArray<FrameChildList>& mLists;
  uint32_t mCurrentIndex;
};

/**
 * A class for retrieving a frame's child lists and iterate them.
 */
class MOZ_STACK_CLASS FrameChildListIterator
  : public FrameChildListArrayIterator {
 public:
  FrameChildListIterator(const nsIFrame* aFrame);

protected:
  nsAutoTArray<FrameChildList,4> mLists;
};

} // namespace layout
} // namespace mozilla

inline mozilla::layout::FrameChildListIDs
operator|(mozilla::layout::FrameChildListID aLeftOp,
          mozilla::layout::FrameChildListID aRightOp)
{
  return mozilla::layout::FrameChildListIDs(aLeftOp) |
         mozilla::layout::FrameChildListIDs(aRightOp);
}

inline mozilla::layout::FrameChildListIDs
operator|(mozilla::layout::FrameChildListID aLeftOp,
          mozilla::layout::FrameChildListIDs aRightOp)
{
  return mozilla::layout::FrameChildListIDs(aLeftOp) | aRightOp;
}

inline void nsFrameList::AppendIfNonempty(
         nsTArray<mozilla::layout::FrameChildList>* aLists,
         mozilla::layout::FrameChildListID aListID) const
{
  if (NotEmpty()) {
    aLists->AppendElement(mozilla::layout::FrameChildList(*this, aListID));
  }
}

#endif /* !defined(FrameChildList_h_) */
