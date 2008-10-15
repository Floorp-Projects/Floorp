/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __nsIContentIterator_h___
#define __nsIContentIterator_h___

#include "nsISupports.h"

class nsINode;
class nsIDOMRange;

#define NS_ICONTENTITERATOR_IID \
{ 0x716a396c, 0xdc4e, 0x4d10, \
  { 0xbd, 0x07, 0x27, 0xee, 0xae, 0x85, 0xe3, 0x86 } }

class nsIContentIterator : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICONTENTITERATOR_IID)

  /* Initializes an iterator for the subtree rooted by the node aRoot
   */
  virtual nsresult Init(nsINode* aRoot) = 0;

  /* Initializes an iterator for the subtree defined by the range aRange
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
  virtual PRBool IsDone() = 0;

  /** PositionAt will position the iterator to the supplied node
   */
  virtual nsresult PositionAt(nsINode* aCurNode) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIContentIterator, NS_ICONTENTITERATOR_IID)

#endif // __nsIContentIterator_h___

