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

#include "nsCOMPtr.h"
#include "nsIPresState.h"
#include "nsHashtable.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIComponentManager.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsLayoutErrors.h"



class nsPresState: public nsIPresState
{
  NS_DECL_ISUPPORTS

  NS_IMETHOD GetStatePropertyAsSupports(const nsAString& aName, nsISupports** aResult);
  NS_IMETHOD SetStatePropertyAsSupports(const nsAString& aName, nsISupports* aValue);

  NS_IMETHOD GetStateProperty(const nsAString& aProperty, nsAString& aResult);
  NS_IMETHOD SetStateProperty(const nsAString& aProperty, const nsAString& aValue);

  NS_IMETHOD RemoveStateProperty(const nsAString& aProperty);

public:
  nsPresState();
  virtual ~nsPresState();

// Static members

// Internal member functions
protected:
  
// MEMBER VARIABLES
protected:
  // A string table that holds property/value pairs.
  nsSupportsHashtable* mPropertyTable; 
};

// Static initialization


// Implementation /////////////////////////////////////////////////////////////////

// Implement our nsISupports methods
NS_IMPL_ISUPPORTS1(nsPresState, nsIPresState)

// Constructors/Destructors
nsPresState::nsPresState(void)
:mPropertyTable(nsnull)
{
}

nsPresState::~nsPresState(void)
{
  delete mPropertyTable;
}

// nsIPresState Interface ////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsPresState::GetStateProperty(const nsAString& aName,
                              nsAString& aResult)
{
  nsresult rv = NS_STATE_PROPERTY_NOT_THERE;
  aResult.Truncate();

  // Retrieve from hashtable.
  if (mPropertyTable) {
    const nsPromiseFlatString& flatString = PromiseFlatString(aName);   
    nsStringKey key(flatString);

    nsCOMPtr<nsISupportsCString> supportsStr =
            dont_AddRef(NS_STATIC_CAST(nsISupportsCString*,
                                       mPropertyTable->Get(&key)));

    if (supportsStr) {
      nsCAutoString data;
      supportsStr->GetData(data);

      CopyUTF8toUTF16(data, aResult);
      rv = NS_STATE_PROPERTY_EXISTS;
    }
  }

  return rv;
}

NS_IMETHODIMP
nsPresState::SetStateProperty(const nsAString& aName, const nsAString& aValue)
{
  if (!mPropertyTable) {
    mPropertyTable = new nsSupportsHashtable(8);
    NS_ENSURE_TRUE(mPropertyTable, NS_ERROR_OUT_OF_MEMORY);
  }

  // Add to hashtable
  const nsPromiseFlatString& flatString = PromiseFlatString(aName);   
  nsStringKey key(flatString);

  nsCOMPtr<nsISupportsCString> supportsStr(do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID));
  NS_ENSURE_TRUE(supportsStr, NS_ERROR_OUT_OF_MEMORY);

  supportsStr->SetData(NS_ConvertUCS2toUTF8(aValue));

  mPropertyTable->Put(&key, supportsStr);
  return NS_OK;
}

NS_IMETHODIMP
nsPresState::RemoveStateProperty(const nsAString& aName)
{
  if (!mPropertyTable)
    return NS_OK;

  const nsPromiseFlatString& flatString = PromiseFlatString(aName);   
  nsStringKey key(flatString);

  mPropertyTable->Remove(&key);
  return NS_OK;
}

NS_IMETHODIMP
nsPresState::GetStatePropertyAsSupports(const nsAString& aName, nsISupports** aResult)
{
  // Retrieve from hashtable.
  nsCOMPtr<nsISupports> supp;

  if (mPropertyTable) {
    const nsPromiseFlatString& flatString = PromiseFlatString(aName);   
    nsStringKey key(flatString);
    supp = dont_AddRef(NS_STATIC_CAST(nsISupports*, mPropertyTable->Get(&key)));
  }

  *aResult = supp;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsPresState::SetStatePropertyAsSupports(const nsAString& aName, nsISupports* aValue)
{
  if (!mPropertyTable) {
    mPropertyTable = new nsSupportsHashtable(8);
    NS_ENSURE_TRUE(mPropertyTable, NS_ERROR_OUT_OF_MEMORY);
  }

  // Add to hashtable
  const nsPromiseFlatString& flatString = PromiseFlatString(aName);   
  nsStringKey key(flatString);
  
  mPropertyTable->Put(&key, aValue);
  return NS_OK;
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewPresState(nsIPresState** aResult)
{
  *aResult = new nsPresState;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

