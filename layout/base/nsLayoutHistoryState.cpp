/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsILayoutHistoryState.h"
#include "nsWeakReference.h"
#include "nsHashtable.h"

class nsLayoutHistoryState : public nsILayoutHistoryState,
                             public nsSupportsWeakReference
{
public:
  nsLayoutHistoryState();
  virtual ~nsLayoutHistoryState();

  NS_DECL_ISUPPORTS

  // nsILayoutHistoryState
  NS_IMETHOD AddState(const nsCString& aKey, nsIPresState* aState);
  NS_IMETHOD GetState(const nsCString& aKey, nsIPresState** aState);
  NS_IMETHOD RemoveState(const nsCString& aKey);


private:
  nsSupportsHashtable mStates;
};


nsresult
NS_NewLayoutHistoryState(nsILayoutHistoryState** aState)
{
    NS_ENSURE_ARG_POINTER(aState);
    if (! aState)
        return NS_ERROR_NULL_POINTER;

    nsLayoutHistoryState *state = new nsLayoutHistoryState();
    if (!state)
        return NS_ERROR_OUT_OF_MEMORY;

    *aState = NS_STATIC_CAST(nsILayoutHistoryState *, state);
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

NS_IMPL_ISUPPORTS2(nsLayoutHistoryState,
                   nsILayoutHistoryState,
                   nsISupportsWeakReference);

NS_IMETHODIMP
nsLayoutHistoryState::AddState(const nsCString& aStateKey,
                               nsIPresState* aState)
{
  nsCStringKey key(aStateKey);

  /*
   * nsSupportsHashtable::Put() returns false when no object was
   * replaced when inserting the new one, true if one was.
   */
#ifdef DEBUG_pollmann
  PRBool replaced =
#endif

  mStates.Put (&key, aState);

#ifdef DEBUG_pollmann
  NS_ASSERTION(!replaced, 
               "nsLayoutHistoryState::AddState OOPS!. There was already a state in the hash table for the key\n");
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsLayoutHistoryState::GetState(const nsCString& aKey,
                               nsIPresState** aState)
{
  nsresult rv = NS_OK;
  nsCStringKey key(aKey);
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
nsLayoutHistoryState::RemoveState(const nsCString& aKey)
{
  nsresult rv = NS_OK;
  nsCStringKey key(aKey);
  mStates.Remove(&key);
  return rv;
}
