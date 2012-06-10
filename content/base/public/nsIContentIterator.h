/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsIContentIterator_h___
#define __nsIContentIterator_h___

#include "nsISupports.h"
#include "nsCOMPtr.h"

class nsINode;
class nsIDOMRange;

#define NS_ICONTENTITERATOR_IID \
{ 0x2550078e, 0xae87, 0x4914, \
 { 0xb3, 0x04, 0xe4, 0xd1, 0x46, 0x19, 0x3d, 0x5f } }

class nsIContentIterator : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICONTENTITERATOR_IID)

  /* Initializes an iterator for the subtree rooted by the node aRoot
   */
  virtual nsresult Init(nsINode* aRoot) = 0;

  /* Initializes an iterator for the subtree defined by the range aRange
     Subclasses should make sure they implement both of these!
   */
  virtual nsresult Init(nsIDOMRange* aRange) = 0;

  /** First will reset the list.
   */
  virtual void First() = 0;

  /** Last will reset the list to the end.
   */
  virtual void Last() = 0;

  /** Next will advance the list.
   */
  virtual void Next() = 0;

  /** Prev will decrement the list.
   */
  virtual void Prev() = 0;

  /** CurrentItem will return the current item, or null if the list is empty
   *  @return the current node
   */
  virtual nsINode *GetCurrentNode() = 0;

  /** return if the collection is at the end. that is the beginning following a call to Prev
   *  and it is the end of the list following a call to next
   *  @return if the iterator is done.
   */
  virtual bool IsDone() = 0;

  /** PositionAt will position the iterator to the supplied node
   */
  virtual nsresult PositionAt(nsINode* aCurNode) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIContentIterator, NS_ICONTENTITERATOR_IID)

already_AddRefed<nsIContentIterator> NS_NewContentIterator();
already_AddRefed<nsIContentIterator> NS_NewPreContentIterator();
already_AddRefed<nsIContentIterator> NS_NewContentSubtreeIterator();

#endif // __nsIContentIterator_h___
