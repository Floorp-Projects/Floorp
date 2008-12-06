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
 * a piece of state that is stored in session history when the document
 * is not
 */

#include "nsCOMPtr.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIComponentManager.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsLayoutErrors.h"
#include "nsPresState.h"
#include "nsString.h"
// Implementation /////////////////////////////////////////////////////////////////

nsresult
nsPresState::Init()
{
  return mPropertyTable.Init(8) ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
nsPresState::GetStateProperty(const nsAString& aName, nsAString& aResult)
{
  nsresult rv = NS_STATE_PROPERTY_NOT_THERE;
  aResult.Truncate();

  // Retrieve from hashtable.
  nsISupports *data = mPropertyTable.GetWeak(aName);

  // Strings are stored in the table as UTF-8, to save space.
  // XXX minimize conversions here...

  nsCOMPtr<nsISupportsCString> supportsStr = do_QueryInterface(data);
  if (supportsStr) {
    nsCAutoString data;
    supportsStr->GetData(data);

    CopyUTF8toUTF16(data, aResult);
    aResult.SetIsVoid(data.IsVoid());
    rv = NS_STATE_PROPERTY_EXISTS;
  }

  return rv;
}

nsresult
nsPresState::SetStateProperty(const nsAString& aName, const nsAString& aValue)
{
  // Add to hashtable
  nsCOMPtr<nsISupportsCString> supportsStr(do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID));
  NS_ENSURE_TRUE(supportsStr, NS_ERROR_OUT_OF_MEMORY);
  NS_ConvertUTF16toUTF8 data(aValue);
  data.SetIsVoid(aValue.IsVoid());
  supportsStr->SetData(data);

  mPropertyTable.Put(aName, supportsStr);
  return NS_OK;
}

nsresult
nsPresState::RemoveStateProperty(const nsAString& aName)
{
  mPropertyTable.Remove(aName);
  return NS_OK;
}

nsresult
nsPresState::GetStatePropertyAsSupports(const nsAString& aName,
                                        nsISupports** aResult)
{
  // Retrieve from hashtable.
  if (mPropertyTable.Get(aName, aResult))
    return NS_STATE_PROPERTY_EXISTS;

  return NS_STATE_PROPERTY_NOT_THERE;
}

nsresult
nsPresState::SetStatePropertyAsSupports(const nsAString& aName,
                                        nsISupports* aValue)
{
  mPropertyTable.Put(aName, aValue);
  return NS_OK;
}

nsresult
nsPresState::SetScrollState(const nsRect& aRect)
{
  if (!mScrollState) {
    mScrollState = new nsRect();
    if (!mScrollState)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  *mScrollState = aRect;
  return NS_OK;
}

nsRect
nsPresState::GetScrollState()
{
  if (!mScrollState) {
    nsRect empty(0,0,0,0);
    return empty;  
  }

  return *mScrollState;
}

void
nsPresState::ClearNonScrollState()
{
  mPropertyTable.Clear();
}

nsresult
NS_NewPresState(nsPresState** aState)
{
  nsPresState *state;

  *aState = nsnull;
  state = new nsPresState();
  if (!state)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = state->Init();
  if (NS_SUCCEEDED(rv))
    *aState = state;
  else
    delete state;

  return rv;
}

  
