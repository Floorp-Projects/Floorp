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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsILayoutHistoryState.h"
#include "nsHashtable.h"
#include "nsIStatefulFrame.h" // Get StateType enum

MOZ_DECL_CTOR_COUNTER(HistoryKey)

class HistoryKey: public nsVoidKey {
 public:
   HistoryKey(PRUint32 aContentID, nsIStatefulFrame::StateType aStateType)
       : nsVoidKey((void*)(aContentID * nsIStatefulFrame::eNumStateTypes + aStateType)) {
   }

   HistoryKey(PRUint32 aKey) 
       : nsVoidKey((void*)aKey) {
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
    nsIPresState* aState,
    nsIStatefulFrame::StateType aStateType = nsIStatefulFrame::eNoType);
  NS_IMETHOD GetState(PRUint32 aContentID,
    nsIPresState** aState,
    nsIStatefulFrame::StateType aStateType = nsIStatefulFrame::eNoType);
  NS_IMETHOD RemoveState(PRUint32 aContentID,
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
                  NS_GET_IID(nsILayoutHistoryState));

NS_IMETHODIMP
nsLayoutHistoryState::AddState(PRUint32 aContentID,                                
                               nsIPresState* aState, 
                               nsIStatefulFrame::StateType aStateType)
{
  HistoryKey key(aContentID, aStateType);
  /*
   * nsSupportsHashtable::Put() returns false when no object has been
   * replaced when inserting the new one, true if it some one was.
   *
   */
  PRBool replaced = mStates.Put (&key, aState);
  if (replaced)
  {
          // done this way by indication of warren@netscape.com [ipg]
#if 0
      printf("nsLayoutHistoryState::AddState OOPS!. There was already a state in the hash table for the key\n");
#endif
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLayoutHistoryState::GetState(PRUint32 aContentID,                                
                               nsIPresState** aState,
                               nsIStatefulFrame::StateType aStateType)
{
  nsresult rv = NS_OK;
  HistoryKey key(aContentID, aStateType);
  nsISupports *state = nsnull;
  state = mStates.Get(&key);
  if (state) {
    *aState = (nsIPresState *)state;
  }
  else {
#if 0
      printf("nsLayoutHistoryState::GetState, ERROR getting History state for the key\n");
#endif
    *aState = nsnull;
    rv = NS_OK;
  }
  return rv;
}

NS_IMETHODIMP
nsLayoutHistoryState::RemoveState(PRUint32 aContentID,                                
                                  nsIStatefulFrame::StateType aStateType)
{
  nsresult rv = NS_OK;
  HistoryKey key(aContentID, aStateType);
  mStates.Remove(&key);
  return rv;
}
