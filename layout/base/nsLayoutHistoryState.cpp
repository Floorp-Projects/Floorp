/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

/*
 * container for information saved in session history when the document
 * is not
 */

#include "nsILayoutHistoryState.h"
#include "nsWeakReference.h"
#include "nsClassHashtable.h"
#include "nsPresState.h"

class nsLayoutHistoryState : public nsILayoutHistoryState,
                             public nsSupportsWeakReference
{
public:
  NS_HIDDEN_(nsresult) Init();

  NS_DECL_ISUPPORTS

  // nsILayoutHistoryState
  NS_IMETHOD AddState(const nsCString& aKey, nsPresState* aState);
  NS_IMETHOD GetState(const nsCString& aKey, nsPresState** aState);
  NS_IMETHOD RemoveState(const nsCString& aKey);
  NS_IMETHOD_(bool) HasStates() const;
  NS_IMETHOD SetScrollPositionOnly(const bool aFlag);


private:
  ~nsLayoutHistoryState() {}
  bool mScrollPositionOnly;

  nsClassHashtable<nsCStringHashKey,nsPresState> mStates;
};


nsresult
NS_NewLayoutHistoryState(nsILayoutHistoryState** aState)
{
  nsLayoutHistoryState *state;

  *aState = nsnull;
  state = new nsLayoutHistoryState();

  NS_ADDREF(state);
  nsresult rv = state->Init();
  if (NS_SUCCEEDED(rv))
    *aState = state;
  else
    NS_RELEASE(state);

  return rv;
}

NS_IMPL_ISUPPORTS2(nsLayoutHistoryState,
                   nsILayoutHistoryState,
                   nsISupportsWeakReference)

nsresult
nsLayoutHistoryState::Init()
{
  mScrollPositionOnly = PR_FALSE;
  return mStates.Init() ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLayoutHistoryState::AddState(const nsCString& aStateKey, nsPresState* aState)
{
  return mStates.Put(aStateKey, aState) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsLayoutHistoryState::GetState(const nsCString& aKey, nsPresState** aState)
{
  bool entryExists = mStates.Get(aKey, aState);

  if (entryExists && mScrollPositionOnly) {
    // Ensure any state that shouldn't be restored is removed
    (*aState)->ClearNonScrollState();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLayoutHistoryState::RemoveState(const nsCString& aKey)
{
  mStates.Remove(aKey);
  return NS_OK;
}

NS_IMETHODIMP_(bool)
nsLayoutHistoryState::HasStates() const
{
  return mStates.Count() != 0;
}

NS_IMETHODIMP
nsLayoutHistoryState::SetScrollPositionOnly(const bool aFlag)
{
  mScrollPositionOnly = aFlag;
  return NS_OK;
}
