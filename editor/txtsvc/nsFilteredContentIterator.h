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

class nsIAtom;
class nsIDOMNode;
class nsIDOMRange;
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
  virtual nsresult Init(nsIDOMRange* aRange) override;
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

  // enum to give us the direction
  typedef enum {eDirNotSet, eForward, eBackward} eDirectionType;
  nsresult AdvanceNode(nsIDOMNode* aNode, nsIDOMNode*& aNewNode, eDirectionType aDir);
  void CheckAdvNode(nsIDOMNode* aNode, bool& aDidSkip, eDirectionType aDir);
  nsresult SwitchDirections(bool aChangeToForward);

  nsCOMPtr<nsIContentIterator> mCurrentIterator;
  nsCOMPtr<nsIContentIterator> mIterator;
  nsCOMPtr<nsIContentIterator> mPreIterator;

  nsCOMPtr<nsIAtom> mBlockQuoteAtom;
  nsCOMPtr<nsIAtom> mScriptAtom;
  nsCOMPtr<nsIAtom> mTextAreaAtom;
  nsCOMPtr<nsIAtom> mSelectAreaAtom;
  nsCOMPtr<nsIAtom> mMapAtom;

  nsCOMPtr<nsITextServicesFilter> mFilter;
  RefPtr<nsRange>               mRange;
  bool                            mDidSkip;
  bool                            mIsOutOfRange;
  eDirectionType                  mDirection;
};

#endif
