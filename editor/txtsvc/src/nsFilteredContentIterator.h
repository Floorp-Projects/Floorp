/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFilteredContentIterator_h__
#define nsFilteredContentIterator_h__

#include "nsIContentIterator.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsITextServicesFilter.h"
#include "nsRange.h"
#include "nsCycleCollectionParticipant.h"

/**
 * 
 */
class nsFilteredContentIterator : public nsIContentIterator
{
public:

  // nsISupports interface...
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsFilteredContentIterator)

  nsFilteredContentIterator(nsITextServicesFilter* aFilter);

  virtual ~nsFilteredContentIterator();

  /* nsIContentIterator */
  virtual nsresult Init(nsINode* aRoot);
  virtual nsresult Init(nsIDOMRange* aRange);
  virtual void First();
  virtual void Last();
  virtual void Next();
  virtual void Prev();
  virtual nsINode *GetCurrentNode();
  virtual bool IsDone();
  virtual nsresult PositionAt(nsINode* aCurNode);

  /* Helpers */
  bool DidSkip()      { return mDidSkip; }
  void         ClearDidSkip() {  mDidSkip = false; }

protected:
  nsFilteredContentIterator() { }

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
  nsCOMPtr<nsIDOMRange>           mRange;
  bool                            mDidSkip;
  bool                            mIsOutOfRange;
  eDirectionType                  mDirection;
};

#endif
