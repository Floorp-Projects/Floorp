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
 * The Original Code is Mozilla Transaction Manager.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Gaunt <jgaunt@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef _tmVector_H_
#define _tmVector_H_

#include "tmUtils.h"

#define GROWTH_INC 5

/**
  * A simple, clear, self-growing, collection of objects. typed independant
  *   basically a growing array. Useful in situations where you need an
  *   indexed collection but do not know the size in advance and need the
  *   ability for increase and decrease in size. Not optimized for anything
  *   in particular, or any size in particular.
  *
  * Is able to guarantee the index of an item will
  *   not change due to removals of a lower indexed item. The growing,
  *   and shrinking all happens to the end of the collection
  *
  * Does not backfill, adds to the end. At some point this should be 
  *   changed to make best use of space.
  */
class tmVector
{
public:

  ////////////////////////////////////////////////////////////////////////////
  // Constructor(s) & Destructor

  /**
    * Set some sane default values to set up the internal storage. Init()
    *   must be called after construction of the object to allcate the
    *   backing store.
    */
  tmVector() : mNext(0), mCount(0), mCapacity(10), mElements(nsnull) {;}

  /**
    * Reclaim the memory allocated in the Init() method. 
    */
  virtual ~tmVector();

  ////////////////////////////////////////////////////////////////////////////
  // Public Member Functions

  /**
    * Allocates the storage back-end
    *
    * @returns NS_OK if allocation succeeded
    * @returns NS_ERROR_OUT_OF_MEMORY if the allocation failed
    */
  nsresult Init();

  // mutators

  /**
    * @returns the index of the element added, if successful
    * @returns -1 if an error occured during allocation of space
    */
  PRInt32 Append(void *aElement);

  /**
    * This does not collapse the collection, it leaves holes. Note, it also
    *   doesn't delete the element, it merely removes it from the collection
    */
  void Remove(void *aElement);

  /**
    * This does not collapse the collection, it leaves holes. Note, it also
    *   doesn't delete the element, it merely removes it from the collection
    */
  void RemoveAt(PRUint32 aIndex);

  /**
    * Does not call delete on the elements since we have no idea how to
    *   reclaim the memory. Sets all array slots to 0.
    */
  void Clear();

  /**
    * @returns the element at the index indicated, including nsnull if the
    *          slot is empty.
    */
  void* operator[](PRUint32 index) { 
    PR_ASSERT(index < mNext);
    return mElements[index]; 
  }

  /**
    * @returns the number of elements stored
    */
  PRUint32 Count() { return mCount; }

  /**
    * Meant to be used as the conditional in a loop. |index < size| should
    *   reach all elements of the collection and not run out of bounds. If
    *   slots 0,1,4,5,6 contain elements Size() will return 7, Count() will
    *   return 5.
    *
    * @returns the number of slots in the array taken, irrespective of 
    *          holes in the collection.
    */
  PRUint32 Size() { return mNext; }

protected:

  nsresult Grow();     // mCapacity += GROWTH_INC - realloc()s
  nsresult Shrink();   // mCapacity -= GROWTH_INC - dumb, free, malloc

  ////////////////////////////////////////////////////////////////////////////
  // Protected Member Variables

  // bookkeeping variables
  PRUint32 mNext;             // next element insertion slot (0 based)
  PRUint32 mCount;            // how many elements in the Vector (1 based)
  PRUint32 mCapacity;         // current capacity of the Vector (1 based)

  // the actual array of objects being stored
  void **mElements;

private:

};

#endif
