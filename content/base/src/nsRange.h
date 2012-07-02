/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of the DOM nsIDOMRange object.
 */

#ifndef nsRange_h___
#define nsRange_h___

#include "nsIDOMRange.h"
#include "nsCOMPtr.h"
#include "nsIDOMDocumentFragment.h"
#include "nsINode.h"
#include "nsIDOMNode.h"
#include "prmon.h"
#include "nsStubMutationObserver.h"

class nsRange : public nsIDOMRange,
                public nsStubMutationObserver
{
public:
  nsRange()
    : mRoot(nsnull)
    , mStartOffset(0)
    , mEndOffset(0)
    , mIsPositioned(false)
    , mIsDetached(false)
    , mMaySpanAnonymousSubtrees(false)
    , mInSelection(false)
  {}
  virtual ~nsRange();

  static nsresult CreateRange(nsIDOMNode* aStartParent, PRInt32 aStartOffset,
                              nsIDOMNode* aEndParent, PRInt32 aEndOffset,
                              nsRange** aRange);
  static nsresult CreateRange(nsIDOMNode* aStartParent, PRInt32 aStartOffset,
                              nsIDOMNode* aEndParent, PRInt32 aEndOffset,
                              nsIDOMRange** aRange);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsRange, nsIDOMRange)

  // nsIDOMRange interface
  NS_DECL_NSIDOMRANGE
  
  nsINode* GetRoot() const
  {
    return mRoot;
  }

  nsINode* GetStartParent() const
  {
    return mStartParent;
  }

  nsINode* GetEndParent() const
  {
    return mEndParent;
  }

  PRInt32 StartOffset() const
  {
    return mStartOffset;
  }

  PRInt32 EndOffset() const
  {
    return mEndOffset;
  }
  
  bool IsPositioned() const
  {
    return mIsPositioned;
  }

  bool Collapsed() const
  {
    return mIsPositioned && mStartParent == mEndParent &&
           mStartOffset == mEndOffset;
  }

  void SetMaySpanAnonymousSubtrees(bool aMaySpanAnonymousSubtrees)
  {
    mMaySpanAnonymousSubtrees = aMaySpanAnonymousSubtrees;
  }
  
  /**
   * Return true iff this range is part of at least one Selection object
   * and isn't detached.
   */
  bool IsInSelection() const
  {
    return mInSelection;
  }

  /**
   * Called when the range is added/removed from a Selection.
   */
  void SetInSelection(bool aInSelection)
  {
    if (mInSelection == aInSelection) {
      return;
    }
    mInSelection = aInSelection;
    nsINode* commonAncestor = GetCommonAncestor();
    NS_ASSERTION(commonAncestor, "unexpected disconnected nodes");
    if (mInSelection) {
      RegisterCommonAncestor(commonAncestor);
    } else {
      UnregisterCommonAncestor(commonAncestor);
    }
  }

  nsINode* GetCommonAncestor() const;
  void Reset();
  nsresult SetStart(nsINode* aParent, PRInt32 aOffset);
  nsresult SetEnd(nsINode* aParent, PRInt32 aOffset);
  already_AddRefed<nsRange> CloneRange() const;

  nsresult Set(nsINode* aStartParent, PRInt32 aStartOffset,
               nsINode* aEndParent, PRInt32 aEndOffset)
  {
    // If this starts being hot, we may be able to optimize this a bit,
    // but for now just set start and end separately.
    nsresult rv = SetStart(aStartParent, aStartOffset);
    NS_ENSURE_SUCCESS(rv, rv);

    return SetEnd(aEndParent, aEndOffset);
  }

  NS_IMETHOD GetUsedFontFaces(nsIDOMFontFaceList** aResult);

  // nsIMutationObserver methods
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
  NS_DECL_NSIMUTATIONOBSERVER_PARENTCHAINCHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED

private:
  // no copy's or assigns
  nsRange(const nsRange&);
  nsRange& operator=(const nsRange&);

  /**
   * Cut or delete the range's contents.
   *
   * @param aFragment nsIDOMDocumentFragment containing the nodes.
   *                  May be null to indicate the caller doesn't want a fragment.
   */
  nsresult CutContents(nsIDOMDocumentFragment** frag);

  static nsresult CloneParentsBetween(nsIDOMNode *aAncestor,
                                      nsIDOMNode *aNode,
                                      nsIDOMNode **aClosestAncestor,
                                      nsIDOMNode **aFarthestAncestor);

public:
/******************************************************************************
 *  Utility routine to detect if a content node starts before a range and/or 
 *  ends after a range.  If neither it is contained inside the range.
 *  
 *  XXX - callers responsibility to ensure node in same doc as range!
 *
 *****************************************************************************/
  static nsresult CompareNodeToRange(nsINode* aNode, nsRange* aRange,
                                     bool *outNodeBefore,
                                     bool *outNodeAfter);

  static bool IsNodeSelected(nsINode* aNode, PRUint32 aStartOffset,
                             PRUint32 aEndOffset);

  typedef nsTHashtable<nsPtrHashKey<nsRange> > RangeHashTable;
protected:
  void RegisterCommonAncestor(nsINode* aNode);
  void UnregisterCommonAncestor(nsINode* aNode);
  nsINode* IsValidBoundary(nsINode* aNode);

  // CharacterDataChanged set aNotInsertedYet to true to disable an assertion
  // and suppress re-registering a range common ancestor node since
  // the new text node of a splitText hasn't been inserted yet.
  // CharacterDataChanged does the re-registering when needed.
  void DoSetRange(nsINode* aStartN, PRInt32 aStartOffset,
                  nsINode* aEndN, PRInt32 aEndOffset,
                  nsINode* aRoot, bool aNotInsertedYet = false);

  /**
   * For a range for which IsInSelection() is true, return the common
   * ancestor for the range.  This method uses the selection bits and
   * nsGkAtoms::range property on the nodes to quickly find the ancestor.
   * That is, it's a faster version of GetCommonAncestor that only works
   * for ranges in a Selection.  The method will assert and the behavior
   * is undefined if called on a range where IsInSelection() is false.
   */
  nsINode* GetRegisteredCommonAncestor();

  struct NS_STACK_CLASS AutoInvalidateSelection
  {
    AutoInvalidateSelection(nsRange* aRange) : mRange(aRange)
    {
#ifdef DEBUG
      mWasInSelection = mRange->IsInSelection();
#endif
      if (!mRange->IsInSelection() || mIsNested) {
        return;
      }
      mIsNested = true;
      mCommonAncestor = mRange->GetRegisteredCommonAncestor();
    }
    ~AutoInvalidateSelection();
    nsRange* mRange;
    nsRefPtr<nsINode> mCommonAncestor;
#ifdef DEBUG
    bool mWasInSelection;
#endif
    static bool mIsNested;
  };
  
  nsCOMPtr<nsINode> mRoot;
  nsCOMPtr<nsINode> mStartParent;
  nsCOMPtr<nsINode> mEndParent;
  PRInt32 mStartOffset;
  PRInt32 mEndOffset;

  bool mIsPositioned;
  bool mIsDetached;
  bool mMaySpanAnonymousSubtrees;
  bool mInSelection;
};

#endif /* nsRange_h___ */
