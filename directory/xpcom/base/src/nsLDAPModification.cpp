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
 * The Original Code is the mozilla.org LDAP XPCOM SDK.
 * 
 * The Initial Developer of the Original Code is
 * Peter Van der Beken.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * Peter Van der Beken. All Rights Reserved.
 * 
 * Contributor(s):
 *   Peter Van der Beken <peterv@propagandism.org>
 *   Mark Banner <mark@standard8.demon.co.uk>
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

#include "nsLDAPModification.h"
#include "nsAutoLock.h"
#include "nsILDAPBERValue.h"
#include "nsISimpleEnumerator.h"
#include "nsServiceManagerUtils.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(nsLDAPModification, nsILDAPModification)

// constructor
//
nsLDAPModification::nsLDAPModification() : mValuesLock(nsnull)
{
}

// destructor
//
nsLDAPModification::~nsLDAPModification()
{
  if (mValuesLock) {
    PR_DestroyLock(mValuesLock);
  }
}

nsresult
nsLDAPModification::Init()
{
  if (!mValuesLock) {
    mValuesLock = PR_NewLock();
    if (!mValuesLock) {
      NS_ERROR("nsLDAPModification::Init: out of memory");
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLDAPModification::GetOperation(PRInt32 *aOperation)
{
  NS_ENSURE_ARG_POINTER(aOperation);

  *aOperation = mOperation;
  return NS_OK;
}

NS_IMETHODIMP nsLDAPModification::SetOperation(PRInt32 aOperation)
{
  mOperation = aOperation;
  return NS_OK;
}

NS_IMETHODIMP
nsLDAPModification::GetType(nsACString& aType)
{
  aType.Assign(mType);
  return NS_OK;
}

NS_IMETHODIMP
nsLDAPModification::SetType(const nsACString& aType)
{
  mType.Assign(aType);
  return NS_OK;
}

NS_IMETHODIMP
nsLDAPModification::GetValues(nsIArray** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  nsAutoLock lock(mValuesLock);

  if (!mValues)
    return NS_ERROR_NOT_INITIALIZED;

  NS_ADDREF(*aResult = mValues);

  return NS_OK;
}

NS_IMETHODIMP
nsLDAPModification::SetValues(nsIArray* aValues)
{
  NS_ENSURE_ARG_POINTER(aValues);

  nsAutoLock lock(mValuesLock);
  nsresult rv;

  if (!mValues)
    mValues = do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  else
    rv = mValues->Clear();

  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISimpleEnumerator> enumerator;
  rv = aValues->Enumerate(getter_AddRefs(enumerator));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMoreElements;
  rv = enumerator->HasMoreElements(&hasMoreElements);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupports> value;

  while (hasMoreElements)
  {
    rv = enumerator->GetNext(getter_AddRefs(value));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mValues->AppendElement(value, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = enumerator->HasMoreElements(&hasMoreElements);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLDAPModification::SetUpModification(PRInt32 aOperation,
                                      const nsACString &aType,
                                      nsIArray *aValues)
{
  // Set the values using our local function before entering lock
  // to avoid deadlocks due to holding the same lock twice.
  nsresult rv = SetValues(aValues);

  nsAutoLock lock(mValuesLock);

  mOperation = aOperation;
  mType.Assign(aType);

  return rv;
}

NS_IMETHODIMP
nsLDAPModification::SetUpModificationOneValue(PRInt32 aOperation,
                                              const nsACString &aType,
                                              nsILDAPBERValue *aValue)
{
  NS_ENSURE_ARG_POINTER(aValue);

  nsAutoLock lock(mValuesLock);

  mOperation = aOperation;
  mType.Assign(aType);

  nsresult rv;

  if (!mValues)
    mValues = do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  else
    rv = mValues->Clear();

  NS_ENSURE_SUCCESS(rv, rv);
  
  return mValues->AppendElement(aValue, PR_FALSE);
}
