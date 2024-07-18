/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_DepthOrderedFrameList_h
#define mozilla_DepthOrderedFrameList_h

#include "mozilla/ReverseIterator.h"
#include "nsTArray.h"

class nsIFrame;

namespace mozilla {

class DepthOrderedFrameList {
 public:
  // Add a dirty root.
  void Add(nsIFrame* aFrame);
  // Remove this frame if present.
  void Remove(nsIFrame* aFrame);
  // Remove and return one of the shallowest dirty roots from the list.
  // (If two roots are at the same depth, order is indeterminate.)
  nsIFrame* PopShallowestRoot();
  // Remove all dirty roots.
  void Clear() { mList.Clear(); }
  // Is this frame one of the elements in the list?
  bool Contains(nsIFrame* aFrame) const { return mList.Contains(aFrame); }
  // Are there no elements?
  bool IsEmpty() const { return mList.IsEmpty(); }

  // Is the given frame an ancestor of any dirty root?
  bool FrameIsAncestorOfAnyElement(nsIFrame* aFrame) const;

  auto IterFromShallowest() const { return Reversed(mList); }

  // The number of elements that there are.
  size_t Length() const { return mList.Length(); }
  nsIFrame* ElementAt(size_t i) const { return mList.ElementAt(i); }
  void RemoveElementAt(size_t i) { mList.RemoveElementAt(i); }

 private:
  struct FrameAndDepth {
    nsIFrame* mFrame;
    const uint32_t mDepth;

    // Easy conversion to nsIFrame*, as it's the most likely need.
    operator nsIFrame*() const { return mFrame; }

    // Used to sort by reverse depths, i.e., deeper < shallower.
    class CompareByReverseDepth {
     public:
      bool Equals(const FrameAndDepth& aA, const FrameAndDepth& aB) const {
        return aA.mDepth == aB.mDepth;
      }
      bool LessThan(const FrameAndDepth& aA, const FrameAndDepth& aB) const {
        // Reverse depth! So '>' instead of '<'.
        return aA.mDepth > aB.mDepth;
      }
    };
  };
  // List of all known dirty roots, sorted by decreasing depths.
  nsTArray<FrameAndDepth> mList;
};

}  // namespace mozilla

#endif
