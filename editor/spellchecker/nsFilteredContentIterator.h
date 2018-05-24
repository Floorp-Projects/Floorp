/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFilteredContentIterator_h__
#define nsFilteredContentIterator_h__

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIContentIterator.h"
#include "nsISupportsImpl.h"
#include "nscore.h"

class nsAtom;
class nsINode;
class nsITextServicesFilter;
class nsRange;

class nsFilteredContentIterator final : public nsIContentIterator
{
public:

  // nsISupports interface...
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsFilteredContentIterator)

  explicit nsFilteredContentIterator(nsITextServicesFilter* aFilter);

  /* nsIContentIterator */
  virtual nsresult Init(nsINode* aRoot) override;
  virtual nsresult Init(nsRange* aRange) override;
  virtual nsresult Init(nsINode* aStartContainer, uint32_t aStartOffset,
                        nsINode* aEndContainer, uint32_t aEndOffset) override;
  virtual nsresult Init(const mozilla::RawRangeBoundary& aStart,
                        const mozilla::RawRangeBoundary& aEnd) override;
  virtual void First() override;
  virtual void Last() override;
  virtual void Next() override;
  virtual void Prev() override;
  virtual nsINode *GetCurrentNode() override;
  virtual bool IsDone() override;
  virtual nsresult PositionAt(nsINode* aCurNode) override;

  /* Helpers */
  bool DidSkip()      { return mDidSkip; }
  void         ClearDidSkip() {  mDidSkip = false; }

protected:
  nsFilteredContentIterator() : mDidSkip(false), mIsOutOfRange(false) { }

  virtual ~nsFilteredContentIterator();

  /**
   * Callers must guarantee that mRange isn't nullptr and it's positioned.
   */
  nsresult InitWithRange();

  // enum to give us the direction
  typedef enum {eDirNotSet, eForward, eBackward} eDirectionType;
  nsresult AdvanceNode(nsINode* aNode, nsINode*& aNewNode, eDirectionType aDir);
  void CheckAdvNode(nsINode* aNode, bool& aDidSkip, eDirectionType aDir);
  nsresult SwitchDirections(bool aChangeToForward);

  nsCOMPtr<nsIContentIterator> mCurrentIterator;
  nsCOMPtr<nsIContentIterator> mIterator;
  nsCOMPtr<nsIContentIterator> mPreIterator;

  RefPtr<nsAtom> mBlockQuoteAtom;
  RefPtr<nsAtom> mScriptAtom;
  RefPtr<nsAtom> mTextAreaAtom;
  RefPtr<nsAtom> mSelectAreaAtom;
  RefPtr<nsAtom> mMapAtom;

  nsCOMPtr<nsITextServicesFilter> mFilter;
  RefPtr<nsRange>               mRange;
  bool                            mDidSkip;
  bool                            mIsOutOfRange;
  eDirectionType                  mDirection;
};

#endif
