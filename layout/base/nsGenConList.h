/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class for nsCounterList and nsQuoteList */

#ifndef nsGenConList_h___
#define nsGenConList_h___

#include "mozilla/LinkedList.h"
#include "nsIFrame.h"
#include "nsStyleStruct.h"
#include "nsCSSPseudoElements.h"
#include "nsTextNode.h"

class nsGenConList;

struct nsGenConNode : public mozilla::LinkedListElement<nsGenConNode> {
  using StyleContentType = mozilla::StyleContentType;

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
  //  * counter nodes for bullets (mPseudoFrame->IsBulletFrame()).
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

  virtual ~nsGenConNode() {}  // XXX Avoid, perhaps?

 protected:
  void CheckFrameAssertions() {
    NS_ASSERTION(
        mContentIndex < int32_t(mPseudoFrame->StyleContent()->ContentCount()) ||
            // Special-case for the use node created for the legacy markers,
            // which don't use the content property.
            (mPseudoFrame->IsBulletFrame() && mContentIndex == 0 &&
             mPseudoFrame->Style()->GetPseudoType() ==
                 mozilla::PseudoStyleType::marker &&
             !mPseudoFrame->StyleContent()->ContentCount()),
        "index out of range");
    // We allow negative values of mContentIndex for 'counter-reset' and
    // 'counter-increment'.

    NS_ASSERTION(mContentIndex < 0 ||
                     mPseudoFrame->Style()->GetPseudoType() ==
                         mozilla::PseudoStyleType::before ||
                     mPseudoFrame->Style()->GetPseudoType() ==
                         mozilla::PseudoStyleType::after ||
                     mPseudoFrame->Style()->GetPseudoType() ==
                         mozilla::PseudoStyleType::marker,
                 "not CSS generated content and not counter change");
    NS_ASSERTION(mContentIndex < 0 ||
                     mPseudoFrame->GetStateBits() & NS_FRAME_GENERATED_CONTENT,
                 "not generated content and not counter change");
  }
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

  // Return true if |aNode1| is after |aNode2|.
  static bool NodeAfter(const nsGenConNode* aNode1, const nsGenConNode* aNode2);

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
  nsDataHashtable<nsPtrHashKey<nsIFrame>, nsGenConNode*> mNodes;

  // A weak pointer to the node most recently inserted, used to avoid repeated
  // list traversals in Insert().
  nsGenConNode* mLastInserted;
};

#endif /* nsGenConList_h___ */
