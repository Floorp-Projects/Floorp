/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsILayoutHistoryState.h"
#include "nsHashtable.h"

class nsLayoutHistoryState : public nsILayoutHistoryState
{
public:
  nsLayoutHistoryState();
  virtual ~nsLayoutHistoryState();

  NS_DECL_ISUPPORTS

  // nsILayoutHistoryState
  NS_IMETHOD AddState(PRUint32 aContentID,
    nsISupports* aState,
    PRInt32 aStateType = NS_HISTORY_STATE_TYPE_NONE);
  NS_IMETHOD GetState(PRUint32 aContentID,
    nsISupports** aState,
    PRInt32 aStateType = NS_HISTORY_STATE_TYPE_NONE);

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
                               PRInt32 aStateType)
{
  return NS_OK;
}

NS_IMETHODIMP
nsLayoutHistoryState::GetState(PRUint32 aContentID,                                
                               nsISupports** aState,
                               PRInt32 aStateType)
{
  return NS_OK;
}
