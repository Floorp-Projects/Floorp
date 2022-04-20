/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef JoinNodesTransaction_h
#define JoinNodesTransaction_h

#include "EditTransactionBase.h"  // for EditTransactionBase, etc.

#include "EditorDOMPoint.h"  // for EditorDOMPoint, etc.
#include "EditorForwards.h"

#include "nsCOMPtr.h"  // for nsCOMPtr
#include "nsCycleCollectionParticipant.h"
#include "nsID.h"    // for REFNSIID
#include "nscore.h"  // for NS_IMETHOD

class nsIContent;
class nsINode;

namespace mozilla {

/**
 * A transaction that joins two nodes E1 (left node) and E2 (right node) into a
 * single node E.  The children of E are the children of E1 followed by the
 * children of E2.  After DoTransaction() and RedoTransaction(), E1 is removed
 * from the content tree and E2 remains.
 */
class JoinNodesTransaction final : public EditTransactionBase {
 protected:
  JoinNodesTransaction(HTMLEditor& aHTMLEditor, nsIContent& aLeftContent,
                       nsIContent& aRightContent);

 public:
  /**
   * Creates a join node transaction.  This returns nullptr if cannot join the
   * nodes.
   *
   * @param aHTMLEditor     The provider of core editing operations.
   * @param aLeftContent    The first of two nodes to join.
   * @param aRightContent   The second of two nodes to join.
   */
  static already_AddRefed<JoinNodesTransaction> MaybeCreate(
      HTMLEditor& aHTMLEditor, nsIContent& aLeftContent,
      nsIContent& aRightContent);

  /**
   * CanDoIt() returns true if there are enough members and can join or
   * restore the nodes.  Otherwise, false.
   */
  bool CanDoIt() const;

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(JoinNodesTransaction,
                                           EditTransactionBase)
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;

  NS_DECL_EDITTRANSACTIONBASE
  NS_DECL_EDITTRANSACTIONBASE_GETASMETHODS_OVERRIDE(JoinNodesTransaction)

  MOZ_CAN_RUN_SCRIPT NS_IMETHOD RedoTransaction() override;

  /**
   * GetExistingContent() and GetRemovedContent() never returns nullptr
   * unless the cycle collector clears them out.
   */
  nsIContent* GetExistingContent() const { return mKeepingContent; }
  nsIContent* GetRemovedContent() const { return mRemovedContent; }
  nsINode* GetParentNode() const { return mParentNode; }

  template <typename EditorDOMPointType>
  EditorDOMPointType CreateJoinedPoint() const {
    if (MOZ_UNLIKELY(!mKeepingContent)) {
      return EditorDOMPointType();
    }
    return EditorDOMPointType(
        mKeepingContent, std::min(mJoinedOffset, mKeepingContent->Length()));
  }

  friend std::ostream& operator<<(std::ostream& aStream,
                                  const JoinNodesTransaction& aTransaction);

 protected:
  virtual ~JoinNodesTransaction() = default;

  enum class RedoingTransaction { No, Yes };
  MOZ_CAN_RUN_SCRIPT nsresult DoTransactionInternal(RedoingTransaction);

  RefPtr<HTMLEditor> mHTMLEditor;

  // The original parent of the left/right nodes.
  nsCOMPtr<nsINode> mParentNode;

  // Removed content after joined.
  nsCOMPtr<nsIContent> mRemovedContent;

  // The keeping content node which contains ex-children of mRemovedContent.
  nsCOMPtr<nsIContent> mKeepingContent;

  // Offset where the original first content is in mKeepingContent after
  // doing or redoing.
  uint32_t mJoinedOffset = 0u;
};

}  // namespace mozilla

#endif  // #ifndef JoinNodesTransaction_h
