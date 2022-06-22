/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class for nsCounterList and nsQuoteList */

#ifndef nsGenConList_h___
#define nsGenConList_h___

#include "mozilla/FunctionRef.h"
#include "mozilla/LinkedList.h"
#include "nsStyleStruct.h"
#include "nsCSSPseudoElements.h"
#include "nsTextNode.h"
#include <functional>

class nsGenConList;
class nsIFrame;

struct nsGenConNode : public mozilla::LinkedListElement<nsGenConNode> {
  using StyleContentType = mozilla::StyleContentItem::Tag;

  // The wrapper frame for all of the pseudo-element's content.  This
  // frame generally has useful style data and has the
  // NS_FRAME_GENERATED_CONTENT bit set (so we use it to track removal),
  // but does not necessarily for |nsCounterChangeNode|s.
  nsIFrame* mPseudoFrame;

  // Index within the list of things specified by the 'content' property,
  // which is needed to do 'content: open-quote open-quote' correctly,
  // and needed for similar cases for counters.
  const int32_t mContentIndex;

  // null for:
  //  * content: no-open-quote / content: no-close-quote
  //  * counter nodes for increments and resets
  RefPtr<nsTextNode> mText;

  explicit nsGenConNode(int32_t aContentIndex)
      : mPseudoFrame(nullptr), mContentIndex(aContentIndex) {}

  /**
   * Finish initializing the generated content node once we know the
   * relevant text frame. This must be called just after
   * the textframe has been initialized. This need not be called at all
   * for nodes that don't generate text. This will generally set the
   * mPseudoFrame, insert the node into aList, and set aTextFrame up
   * with the correct text.
   * @param aList the list the node belongs to
   * @param aPseudoFrame the :before or :after frame
   * @param aTextFrame the textframe where the node contents will render
   * @return true iff this marked the list dirty
   */
  virtual bool InitTextFrame(nsGenConList* aList, nsIFrame* aPseudoFrame,
                             nsIFrame* aTextFrame) {
    mPseudoFrame = aPseudoFrame;
    CheckFrameAssertions();
    return false;
  }

  virtual ~nsGenConNode() = default;  // XXX Avoid, perhaps?

 protected:
  void CheckFrameAssertions();
};

class nsGenConList {
 protected:
  mozilla::LinkedList<nsGenConNode> mList;
  uint32_t mSize;

 public:
  nsGenConList() : mSize(0), mLastInserted(nullptr) {}
  ~nsGenConList() { Clear(); }
  void Clear();
  static nsGenConNode* Next(nsGenConNode* aNode) {
    MOZ_ASSERT(aNode, "aNode cannot be nullptr!");
    return aNode->getNext();
  }
  static nsGenConNode* Prev(nsGenConNode* aNode) {
    MOZ_ASSERT(aNode, "aNode cannot be nullptr!");
    return aNode->getPrevious();
  }
  void Insert(nsGenConNode* aNode);

  // Destroy all nodes with aFrame as parent. Returns true if some nodes
  // have been destroyed; otherwise false.
  bool DestroyNodesFor(nsIFrame* aFrame);

  // Return the first node for aFrame on this list, or nullptr.
  nsGenConNode* GetFirstNodeFor(nsIFrame* aFrame) const {
    return mNodes.Get(aFrame);
  }

  // Return true if |aNode1| is after |aNode2|.
  static bool NodeAfter(const nsGenConNode* aNode1, const nsGenConNode* aNode2);

  // Find the first element in the list for which the given comparator returns
  // true. This does a binary search on the list contents.
  nsGenConNode* BinarySearch(
      const mozilla::FunctionRef<bool(nsGenConNode*)>& aIsAfter);

  nsGenConNode* GetLast() { return mList.getLast(); }

  bool IsFirst(nsGenConNode* aNode) {
    MOZ_ASSERT(aNode, "aNode cannot be nullptr!");
    return aNode == mList.getFirst();
  }

  bool IsLast(nsGenConNode* aNode) {
    MOZ_ASSERT(aNode, "aNode cannot be nullptr!");
    return aNode == mList.getLast();
  }

 private:
  void Destroy(nsGenConNode* aNode) {
    MOZ_ASSERT(aNode, "aNode cannot be nullptr!");
    delete aNode;
    mSize--;
  }

  // Map from frame to the first nsGenConNode of it in the list.
  nsTHashMap<nsPtrHashKey<nsIFrame>, nsGenConNode*> mNodes;

  // A weak pointer to the node most recently inserted, used to avoid repeated
  // list traversals in Insert().
  nsGenConNode* mLastInserted;
};

#endif /* nsGenConList_h___ */
