/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mats Palmgren <matspal@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  FrameChildListIDs(PRUint32 aIDs) : mIDs(aIDs) {}
  PRUint32 mIDs;
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
class NS_STACK_CLASS FrameChildListArrayIterator {
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
  PRUint32 mCurrentIndex;
};

/**
 * A class for retrieving a frame's child lists and iterate them.
 */
class NS_STACK_CLASS FrameChildListIterator
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
