/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsILayoutHistoryState.h"
#include "nsHashtable.h"
#include "nsIStatefulFrame.h" // Get StateType enum

MOZ_DECL_CTOR_COUNTER(HistoryKey);

class HistoryKey: public nsHashKey {
 private:
   PRUint32 itsHash;
   
 public:
   HistoryKey(PRUint32 aContentID, nsIStatefulFrame::StateType aStateType) {
     MOZ_COUNT_CTOR(HistoryKey);
     itsHash = aContentID * nsIStatefulFrame::eNumStateTypes + aStateType;
   }

   HistoryKey(PRUint32 aKey) {
     MOZ_COUNT_CTOR(HistoryKey);
     itsHash = aKey;
   }
   ~HistoryKey() {
     MOZ_COUNT_DTOR(HistoryKey);
   }

   PRUint32 HashValue(void) const {
     return itsHash;
   }
 
   PRBool Equals(const nsHashKey *aKey) const {
     return (itsHash == (((const HistoryKey *) aKey)->HashValue())) ? PR_TRUE : PR_FALSE;
   }
 
   nsHashKey *Clone(void) const {
     return new HistoryKey(itsHash);
   }
};

class nsLayoutHistoryState : public nsILayoutHistoryState
{
public:
  nsLayoutHistoryState();
  virtual ~nsLayoutHistoryState();

  NS_DECL_ISUPPORTS

  // nsILayoutHistoryState
  NS_IMETHOD AddState(PRUint32 aContentID,
    nsISupports* aState,
    nsIStatefulFrame::StateType aStateType = nsIStatefulFrame::eNoType);
  NS_IMETHOD GetState(PRUint32 aContentID,
    nsISupports** aState,
    nsIStatefulFrame::StateType aStateType = nsIStatefulFrame::eNoType);

private:
  nsSupportsHashtable mStates;
};


nsresult
NS_NewLayoutHistoryState(nsILayoutHistoryState** aState)
{
    NS_PRECONDITION(aState != nsnull, "null ptr");
    if (! aState)
        return NS_ERROR_NULL_POINTER;

    *aState = new nsLayoutHistoryState();
    if (! *aState)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aState);

    return NS_OK;
}

nsLayoutHistoryState::nsLayoutHistoryState()
{
  NS_INIT_REFCNT();
}

nsLayoutHistoryState::~nsLayoutHistoryState()
{
}

NS_IMPL_ISUPPORTS(nsLayoutHistoryState, 
                  nsILayoutHistoryState::GetIID());

NS_IMETHODIMP
nsLayoutHistoryState::AddState(PRUint32 aContentID,                                
                               nsISupports* aState, 
                               nsIStatefulFrame::StateType aStateType)
{
  nsresult rv = NS_OK;
  HistoryKey key(aContentID, aStateType);
  void * res = mStates.Put(&key, (void *) aState);
  /* nsHashTable seems to return null when it actually added
   * the element in to the table. If another element was already
   * present in the table for the key, it seems to return the 
   * element that was already present. Not sure if that was
   * the intended behavior
   */
  if (res)  {
    printf("nsLayoutHistoryState::AddState OOPS!. There was already a state in the hash table for the key\n");
    rv = NS_ERROR_UNEXPECTED;
  }

  return rv;
}

NS_IMETHODIMP
nsLayoutHistoryState::GetState(PRUint32 aContentID,                                
                               nsISupports** aState,
                               nsIStatefulFrame::StateType aStateType)
{
  nsresult rv = NS_OK;
  HistoryKey key(aContentID, aStateType);
  void * state = nsnull;
  state = mStates.Get(&key);
  if (state) {
    *aState = (nsISupports *)state;
  }
  else {
    printf("nsLayoutHistoryState::GetState, ERROR getting History state for the key\n");
    *aState = nsnull;
    rv = NS_ERROR_NULL_POINTER;
  }
  return rv;
}
