/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Mike Calmus
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

#include "nsPasswordManager.h"
#include "nsPassword.h"
#include "singsign.h"
#include "nsReadableUtils.h"

////////////////////////////////////////////////////////////////////////////////

class nsPasswordManagerEnumerator : public nsISimpleEnumerator
{
  public:

    NS_DECL_ISUPPORTS

    nsPasswordManagerEnumerator() : mHostCount(0), mUserCount(0)
    {
    }

    NS_IMETHOD HasMoreElements(PRBool *result) 
    {
      *result = SINGSIGN_HostCount() > mHostCount;
      return NS_OK;
    }

    NS_IMETHOD GetNext(nsISupports **result) 
    {
      char * host;
      PRUnichar * user;
      PRUnichar * pswd;
      nsresult rv = SINGSIGN_Enumerate(mHostCount, mUserCount++, &host, &user, &pswd);
      if (NS_FAILED(rv)) {
        mUserCount = 0;
        mHostCount++;
        return rv;
      }
      if (mUserCount == SINGSIGN_UserCount(mHostCount)) {
        mUserCount = 0;
        mHostCount++;
      }
      nsIPassword *password = new nsPassword(host, user, pswd);
      // note that memory is handed off to "new nsPassword" in a non-xpcom fashion 
      if (password == nsnull) {
        nsMemory::Free(host);
        nsMemory::Free(user);
        nsMemory::Free(pswd);
        return NS_ERROR_OUT_OF_MEMORY;
      }
      *result = password;
      NS_ADDREF(*result);
      return NS_OK;
    }

    virtual ~nsPasswordManagerEnumerator() 
    {
    }

  protected:
    PRInt32 mHostCount;
    PRInt32 mUserCount;
};

NS_IMPL_ISUPPORTS1(nsPasswordManagerEnumerator, nsISimpleEnumerator)

////////////////////////////////////////////////////////////////////////////////

class nsPasswordManagerRejectEnumerator : public nsISimpleEnumerator
{
  public:

    NS_DECL_ISUPPORTS

    nsPasswordManagerRejectEnumerator() : mRejectCount(0)
    {
    }

    NS_IMETHOD HasMoreElements(PRBool *result) 
    {
      *result = SINGSIGN_RejectCount() > mRejectCount;
      return NS_OK;
    }

    NS_IMETHOD GetNext(nsISupports **result) 
    {
      char * host;
      nsresult rv = SINGSIGN_RejectEnumerate(mRejectCount++, &host);
      if (NS_FAILED(rv)) {
        return rv;
      }

      nsIPassword *password = new nsPassword(host, nsnull, nsnull); /* only first argument used */
      if (password == nsnull) {
        nsMemory::Free(host);
        return NS_ERROR_OUT_OF_MEMORY;
      }
      *result = password;
      NS_ADDREF(*result);
      return NS_OK;
    }

    virtual ~nsPasswordManagerRejectEnumerator() 
    {
    }

  protected:
    PRInt32 mRejectCount;
};

NS_IMPL_ISUPPORTS1(nsPasswordManagerRejectEnumerator, nsISimpleEnumerator)

////////////////////////////////////////////////////////////////////////////////
// nsPasswordManager Implementation

NS_IMPL_ISUPPORTS3(nsPasswordManager, nsIPasswordManager, nsIPasswordManagerInternal, nsISupportsWeakReference)
 
nsPasswordManager::nsPasswordManager()
{
}

nsPasswordManager::~nsPasswordManager(void)
{
}

nsresult nsPasswordManager::Init()
{
  return NS_OK;
}

NS_IMETHODIMP nsPasswordManager::GetEnumerator(nsISimpleEnumerator * *entries)
{
  *entries = nsnull;
  nsPasswordManagerEnumerator* enumerator = new nsPasswordManagerEnumerator();
  if (enumerator == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(enumerator);
  *entries = enumerator;
  return NS_OK;
}

NS_IMETHODIMP nsPasswordManager::AddUser(const nsACString& aHost, const nsAString& aUser, const nsAString& aPwd) {
  SINGSIGN_StorePassword(PromiseFlatCString(aHost).get(),
                         PromiseFlatString(aUser).get(),
                         PromiseFlatString(aPwd).get());
  return NS_OK;
}

NS_IMETHODIMP nsPasswordManager::RemoveUser(const nsACString& aHost, const nsAString& aUser)
{
  return ::SINGSIGN_RemoveUser(PromiseFlatCString(aHost).get(),
                               PromiseFlatString(aUser).get(), PR_TRUE);
}

NS_IMETHODIMP nsPasswordManager::GetRejectEnumerator(nsISimpleEnumerator * *entries)
{
  *entries = nsnull;
  nsPasswordManagerRejectEnumerator* enumerator = new nsPasswordManagerRejectEnumerator();
  if (enumerator == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(enumerator);
  *entries = enumerator;
  return NS_OK;
}

NS_IMETHODIMP nsPasswordManager::RemoveReject(const nsACString& aHost)
{
  return ::SINGSIGN_RemoveReject(PromiseFlatCString(aHost).get());
}

NS_IMETHODIMP
nsPasswordManager::FindPasswordEntry
  (const nsACString& aHostURI, const nsAString& aUsername, const nsAString& aPassword,
   nsACString& aHostURIFound, nsAString& aUsernameFound, nsAString& aPasswordFound)
{
  nsresult rv;
  nsCOMPtr<nsIPassword> passwordElem;

  nsCOMPtr<nsISimpleEnumerator> enumerator;
  rv = GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(rv)) {
    return rv; // could not unlock the database
  }

  PRBool hasMoreElements = PR_FALSE;
  enumerator->HasMoreElements(&hasMoreElements);

  // Emumerate through set of saved logins
  while (hasMoreElements) {
    rv = enumerator->GetNext(getter_AddRefs(passwordElem));
    if (NS_FAILED(rv))
      return rv;

    if (passwordElem) {

      // Get the contents of this saved login
      nsCAutoString hostURI;
      nsAutoString username;
      nsAutoString password;

      passwordElem->GetHost(hostURI);
      passwordElem->GetUser(username);
      passwordElem->GetPassword(password);

      // Check for a match with the input parameters, treating null input values as
      // wild cards
      PRBool hostURIOK  = aHostURI.IsEmpty()  || hostURI.Equals(aHostURI);
      PRBool usernameOK = aUsername.IsEmpty() || username.Equals(aUsername);
      PRBool passwordOK = aPassword.IsEmpty() || password.Equals(aPassword);

      // If a match is found, return success
      if (hostURIOK && usernameOK && passwordOK) {
        aHostURIFound = hostURI;
        aUsernameFound = username;
        aPasswordFound = password;
        return NS_OK;
      }
    }
    enumerator->HasMoreElements(&hasMoreElements);
  }

  // no match found
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsPasswordManager::AddReject(const nsACString& host)
{
  return ::SINGSIGN_AddReject(PromiseFlatCString(host).get());
}

NS_IMETHODIMP
nsPasswordManager::AddUserFull(const nsACString& aKey,
                               const nsAString& aUser,
                               const nsAString& aPassword,
                               const nsAString& aUserFieldName,
                               const nsAString& aPassFieldName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPasswordManager::ReadPasswords(nsIFile* aPasswordFile)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

