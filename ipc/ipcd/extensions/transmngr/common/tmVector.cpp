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

#include <stdlib.h>
#include "tmVector.h"

////////////////////////////////////////////////////////////////////////////
// Constructor(s) & Destructor

// can not be responsible for reclaiming memory pointed to by the void*s in
//   the collection - how would we reclaim, don't know how they were allocated
tmVector::~tmVector() {
  if (mElements)
    free((void*)mElements);
}

///////////////////////////////////////////////////////////////////////////////
// Public Member Functions

nsresult
tmVector::Init() {

  mElements = (void**) calloc (mCapacity, sizeof(void*));
  if (!mElements)
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// mutators

PRInt32
tmVector::Append(void *aElement){
  PR_ASSERT(aElement);

  // make sure there is room
  if (mNext == mCapacity)
    if (NS_FAILED(Grow()))
      return -1;

  // put the element in the array
  mElements[mNext] = aElement;
  mCount++;

  // encapsulates the index into a success value
  return mNext++; // post increment.
}

void
tmVector::Remove(void *aElement) {
  PR_ASSERT(aElement);

  for (PRUint32 index = 0; index < mNext; index++) {
    if (mElements[index] == aElement) {
      mElements[index] = nsnull;
      mCount--;
      if (index == mNext-1) {   // if we removed the last element
        mNext--;
        // don't test for success of the shrink
        Shrink();
      }
    }
  }
}

void
tmVector::RemoveAt(PRUint32 aIndex) {
  PR_ASSERT(aIndex < mNext);

  // remove the element if it isn't already nsnull
  if (mElements[aIndex] != nsnull) {
    mElements[aIndex] = nsnull;
    mCount--;
    if (aIndex == mNext-1) {   // if we removed the last element
      mNext--;
      // don't test for success of the shrink
      Shrink();
    }
  }
}

//void*
//tmVector::operator[](int index) {
//  if (index < mNext && index >= 0)
//    return mElements[index]; 
//  return nsnull;
//}

// Does not delete any of the data, merely removes references to them
void
tmVector::Clear(){
  memset(mElements, 0, mCapacity);
  mCount = 0;
  mNext = 0;
}

//////////////////////////////////////////////////////////////////////////////
// Protected Member Functions

// increases the capacity by the growth increment
nsresult
tmVector::Grow() {

  PRUint32 newcap = mCapacity + GROWTH_INC;
  mElements = (void**) realloc(mElements, (newcap * sizeof(void*)));
  if (mElements) {
    mCapacity = newcap;
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

// reduces the capacity by the growth increment. leaves room
//   for one more add before needing to Grow().
nsresult
tmVector::Shrink() {

  PRUint32 newcap = mCapacity - GROWTH_INC;
  if (mNext < newcap) {
    mElements = (void**) realloc(mElements, newcap * sizeof(void*));
    if (!mElements)
      return NS_ERROR_OUT_OF_MEMORY;
    mCapacity = newcap;
  }
  return NS_OK;
}
