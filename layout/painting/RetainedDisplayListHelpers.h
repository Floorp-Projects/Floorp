/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RETAINEDDISPLAYLISTHELPERS_H_
#define RETAINEDDISPLAYLISTHELPERS_H_

#include "PLDHashTable.h"

struct DisplayItemKey {
  bool operator==(const DisplayItemKey& aOther) const {
    return mFrame == aOther.mFrame && mPerFrameKey == aOther.mPerFrameKey;
  }

  nsIFrame* mFrame;
  uint32_t mPerFrameKey;
};

class DisplayItemHashEntry : public PLDHashEntryHdr {
 public:
  typedef DisplayItemKey KeyType;
  typedef const DisplayItemKey* KeyTypePointer;

  explicit DisplayItemHashEntry(KeyTypePointer aKey) : mKey(*aKey) {}
  DisplayItemHashEntry(DisplayItemHashEntry&&) = default;

  ~DisplayItemHashEntry() = default;

  KeyType GetKey() const { return mKey; }
  bool KeyEquals(KeyTypePointer aKey) const { return mKey == *aKey; }

  static KeyTypePointer KeyToPointer(KeyType& aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    if (!aKey) return 0;

    return mozilla::HashGeneric(aKey->mFrame, aKey->mPerFrameKey);
  }
  enum { ALLOW_MEMMOVE = true };

  DisplayItemKey mKey;
};

template <typename T>
bool SpanContains(mozilla::Span<const T>& aSpan, T aItem) {
  for (const T& i : aSpan) {
    if (i == aItem) {
      return true;
    }
  }
  return false;
}

class OldListUnits {};
class MergedListUnits {};

template <typename Units>
struct Index {
  Index() : val(0) {}
  explicit Index(size_t aVal) : val(aVal) {
    MOZ_RELEASE_ASSERT(aVal < std::numeric_limits<uint32_t>::max(),
                       "List index overflowed");
  }

  bool operator==(const Index<Units>& aOther) const {
    return val == aOther.val;
  }

  uint32_t val;
};
typedef Index<OldListUnits> OldListIndex;
typedef Index<MergedListUnits> MergedListIndex;

template <typename T>
class DirectedAcyclicGraph {
 public:
  DirectedAcyclicGraph() = default;
  DirectedAcyclicGraph(DirectedAcyclicGraph&& aOther)
      : mNodesInfo(std::move(aOther.mNodesInfo)),
        mDirectPredecessorList(std::move(aOther.mDirectPredecessorList)) {}

  DirectedAcyclicGraph& operator=(DirectedAcyclicGraph&& aOther) {
    mNodesInfo = std::move(aOther.mNodesInfo);
    mDirectPredecessorList = std::move(aOther.mDirectPredecessorList);
    return *this;
  }

  Index<T> AddNode(
      mozilla::Span<const Index<T>> aDirectPredecessors,
      const mozilla::Maybe<Index<T>>& aExtraPredecessor = mozilla::Nothing()) {
    size_t index = mNodesInfo.Length();
    mNodesInfo.AppendElement(NodeInfo(mDirectPredecessorList.Length(),
                                      aDirectPredecessors.Length()));
    if (aExtraPredecessor &&
        !SpanContains(aDirectPredecessors, aExtraPredecessor.value())) {
      mNodesInfo.LastElement().mDirectPredecessorCount++;
      mDirectPredecessorList.SetCapacity(mDirectPredecessorList.Length() +
                                         aDirectPredecessors.Length() + 1);
      mDirectPredecessorList.AppendElements(aDirectPredecessors);
      mDirectPredecessorList.AppendElement(aExtraPredecessor.value());
    } else {
      mDirectPredecessorList.AppendElements(aDirectPredecessors);
    }
    return Index<T>(index);
  }

  size_t Length() { return mNodesInfo.Length(); }

  mozilla::Span<Index<T>> GetDirectPredecessors(Index<T> aNodeIndex) {
    NodeInfo& node = mNodesInfo[aNodeIndex.val];
    return mozilla::MakeSpan(mDirectPredecessorList)
        .Subspan(node.mIndexInDirectPredecessorList,
                 node.mDirectPredecessorCount);
  }

  template <typename OtherUnits>
  void EnsureCapacityFor(const DirectedAcyclicGraph<OtherUnits>& aOther) {
    mNodesInfo.SetCapacity(aOther.mNodesInfo.Length());
    mDirectPredecessorList.SetCapacity(aOther.mDirectPredecessorList.Length());
  }

  void Clear() {
    mNodesInfo.Clear();
    mDirectPredecessorList.Clear();
  }

  struct NodeInfo {
    NodeInfo(size_t aIndexInDirectPredecessorList,
             size_t aDirectPredecessorCount)
        : mIndexInDirectPredecessorList(aIndexInDirectPredecessorList),
          mDirectPredecessorCount(aDirectPredecessorCount) {}
    size_t mIndexInDirectPredecessorList;
    size_t mDirectPredecessorCount;
  };

  nsTArray<NodeInfo> mNodesInfo;
  nsTArray<Index<T>> mDirectPredecessorList;
};

struct RetainedDisplayListBuilder;
class nsDisplayItem;

struct OldItemInfo {
  explicit OldItemInfo(nsDisplayItem* aItem);

  void AddedToMergedList(MergedListIndex aIndex) {
    MOZ_ASSERT(!IsUsed());
    mUsed = true;
    mIndex = aIndex;
    mItem = nullptr;
  }

  void AddedMatchToMergedList(RetainedDisplayListBuilder* aBuilder,
                              MergedListIndex aIndex);
  void Discard(RetainedDisplayListBuilder* aBuilder,
               nsTArray<MergedListIndex>&& aDirectPredecessors);
  bool IsUsed() { return mUsed; }

  bool IsDiscarded() {
    MOZ_ASSERT(IsUsed());
    return mDiscarded;
  }

  bool IsChanged();

  nsDisplayItem* mItem;
  nsTArray<MergedListIndex> mDirectPredecessors;
  MergedListIndex mIndex;
  bool mUsed;
  bool mDiscarded;
  bool mOwnsItem;
};

bool AnyContentAncestorModified(nsIFrame* aFrame,
                                nsIFrame* aStopAtFrame = nullptr);

#endif  // RETAINEDDISPLAYLISTHELPERS_H_
