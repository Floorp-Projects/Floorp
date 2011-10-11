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
 * the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Boris Zbarsky <bzbarsky@mit.edu> (Original author)
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

/**
 * This is the principal that has no rights and can't be accessed by
 * anything other than itself and chrome; null principals are not
 * same-origin with anything but themselves.
 */

#include "mozilla/Util.h"

#include "nsNullPrincipal.h"
#include "nsNullPrincipalURI.h"
#include "nsMemory.h"
#include "nsIUUIDGenerator.h"
#include "nsID.h"
#include "nsNetUtil.h"
#include "nsIClassInfoImpl.h"
#include "nsNetCID.h"
#include "nsDOMError.h"
#include "nsScriptSecurityManager.h"

using namespace mozilla;

NS_IMPL_CLASSINFO(nsNullPrincipal, NULL, nsIClassInfo::MAIN_THREAD_ONLY,
                  NS_NULLPRINCIPAL_CID)
NS_IMPL_QUERY_INTERFACE2_CI(nsNullPrincipal,
                            nsIPrincipal,
                            nsISerializable)
NS_IMPL_CI_INTERFACE_GETTER2(nsNullPrincipal,
                             nsIPrincipal,
                             nsISerializable)

NS_IMETHODIMP_(nsrefcnt) 
nsNullPrincipal::AddRef()
{
  NS_PRECONDITION(PRInt32(mJSPrincipals.refcount) >= 0, "illegal refcnt");
  nsrefcnt count = PR_ATOMIC_INCREMENT(&mJSPrincipals.refcount);
  NS_LOG_ADDREF(this, count, "nsNullPrincipal", sizeof(*this));
  return count;
}

NS_IMETHODIMP_(nsrefcnt)
nsNullPrincipal::Release()
{
  NS_PRECONDITION(0 != mJSPrincipals.refcount, "dup release");
  nsrefcnt count = PR_ATOMIC_DECREMENT(&mJSPrincipals.refcount);
  NS_LOG_RELEASE(this, count, "nsNullPrincipal");
  if (count == 0) {
    delete this;
  }

  return count;
}

nsNullPrincipal::nsNullPrincipal()
{
}

nsNullPrincipal::~nsNullPrincipal()
{
}

#define NS_NULLPRINCIPAL_PREFIX NS_NULLPRINCIPAL_SCHEME ":"

nsresult
nsNullPrincipal::Init()
{
  // FIXME: bug 327161 -- make sure the uuid generator is reseeding-resistant.
  nsresult rv;
  nsCOMPtr<nsIUUIDGenerator> uuidgen =
    do_GetService("@mozilla.org/uuid-generator;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsID id;
  rv = uuidgen->GenerateUUIDInPlace(&id);
  NS_ENSURE_SUCCESS(rv, rv);

  char chars[NSID_LENGTH];
  id.ToProvidedString(chars);

  PRUint32 suffixLen = NSID_LENGTH - 1;
  PRUint32 prefixLen = ArrayLength(NS_NULLPRINCIPAL_PREFIX) - 1;

  // Use an nsCString so we only do the allocation once here and then share
  // with nsJSPrincipals
  nsCString str;
  str.SetCapacity(prefixLen + suffixLen);

  str.Append(NS_NULLPRINCIPAL_PREFIX);
  str.Append(chars);

  if (str.Length() != prefixLen + suffixLen) {
    NS_WARNING("Out of memory allocating null-principal URI");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mURI = new nsNullPrincipalURI(str);
  NS_ENSURE_TRUE(mURI, NS_ERROR_OUT_OF_MEMORY);

  return mJSPrincipals.Init(this, str);
}

/**
 * nsIPrincipal implementation
 */

NS_IMETHODIMP
nsNullPrincipal::GetPreferences(char** aPrefName, char** aID,
                                char** aSubjectName,
                                char** aGrantedList, char** aDeniedList,
                                bool* aIsTrusted)
{
  // The null principal should never be written to preferences.
  *aPrefName = nsnull;
  *aID = nsnull;
  *aSubjectName = nsnull;
  *aGrantedList = nsnull;
  *aDeniedList = nsnull;
  *aIsTrusted = PR_FALSE;

  return NS_ERROR_FAILURE; 
}

NS_IMETHODIMP
nsNullPrincipal::Equals(nsIPrincipal *aOther, bool *aResult)
{
  // Just equal to ourselves.  Note that nsPrincipal::Equals will return false
  // for us since we have a unique domain/origin/etc.
  *aResult = (aOther == this);
  return NS_OK;
}

NS_IMETHODIMP
nsNullPrincipal::EqualsIgnoringDomain(nsIPrincipal *aOther, bool *aResult)
{
  return Equals(aOther, aResult);
}

NS_IMETHODIMP
nsNullPrincipal::GetHashValue(PRUint32 *aResult)
{
  *aResult = (NS_PTR_TO_INT32(this) >> 2);
  return NS_OK;
}

NS_IMETHODIMP
nsNullPrincipal::GetJSPrincipals(JSContext *cx, JSPrincipals **aJsprin)
{
  NS_PRECONDITION(mJSPrincipals.nsIPrincipalPtr,
                  "mJSPrincipals is uninitalized!");

  JSPRINCIPALS_HOLD(cx, &mJSPrincipals);
  *aJsprin = &mJSPrincipals;
  return NS_OK;
}

NS_IMETHODIMP
nsNullPrincipal::GetSecurityPolicy(void** aSecurityPolicy)
{
  // We don't actually do security policy caching.  And it's not like anyone
  // can set a security policy for us anyway.
  *aSecurityPolicy = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsNullPrincipal::SetSecurityPolicy(void* aSecurityPolicy)
{
  // We don't actually do security policy caching.  And it's not like anyone
  // can set a security policy for us anyway.
  return NS_OK;
}

NS_IMETHODIMP 
nsNullPrincipal::CanEnableCapability(const char *aCapability, 
                                     PRInt16 *aResult)
{
  // Null principal can enable no capabilities.
  *aResult = nsIPrincipal::ENABLE_DENIED;
  return NS_OK;
}

NS_IMETHODIMP 
nsNullPrincipal::SetCanEnableCapability(const char *aCapability, 
                                        PRInt16 aCanEnable)
{
  return NS_ERROR_NOT_AVAILABLE;
}


NS_IMETHODIMP 
nsNullPrincipal::IsCapabilityEnabled(const char *aCapability, 
                                     void *aAnnotation, 
                                     bool *aResult)
{
  // Nope.  No capabilities, I say!
  *aResult = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP 
nsNullPrincipal::EnableCapability(const char *aCapability, void **aAnnotation)
{
  NS_NOTREACHED("Didn't I say it?  NO CAPABILITIES!");
  *aAnnotation = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsNullPrincipal::RevertCapability(const char *aCapability, void **aAnnotation)
{
    *aAnnotation = nsnull;
    return NS_OK;
}

NS_IMETHODIMP 
nsNullPrincipal::DisableCapability(const char *aCapability, void **aAnnotation)
{
  // Just a no-op.  They're all disabled anyway.
  *aAnnotation = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsNullPrincipal::GetURI(nsIURI** aURI)
{
  return NS_EnsureSafeToReturn(mURI, aURI);
}

NS_IMETHODIMP
nsNullPrincipal::GetCsp(nsIContentSecurityPolicy** aCsp)
{
  // CSP on a null principal makes no sense
  *aCsp = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsNullPrincipal::SetCsp(nsIContentSecurityPolicy* aCsp)
{
  // CSP on a null principal makes no sense
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsNullPrincipal::GetDomain(nsIURI** aDomain)
{
  return NS_EnsureSafeToReturn(mURI, aDomain);
}

NS_IMETHODIMP
nsNullPrincipal::SetDomain(nsIURI* aDomain)
{
  // I think the right thing to do here is to just throw...  Silently failing
  // seems counterproductive.
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP 
nsNullPrincipal::GetOrigin(char** aOrigin)
{
  *aOrigin = nsnull;
  
  nsCAutoString str;
  nsresult rv = mURI->GetSpec(str);
  NS_ENSURE_SUCCESS(rv, rv);

  *aOrigin = ToNewCString(str);
  NS_ENSURE_TRUE(*aOrigin, NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

NS_IMETHODIMP 
nsNullPrincipal::GetHasCertificate(bool* aResult)
{
  *aResult = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP 
nsNullPrincipal::GetFingerprint(nsACString& aID)
{
    return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP 
nsNullPrincipal::GetPrettyName(nsACString& aName)
{
    return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsNullPrincipal::Subsumes(nsIPrincipal *aOther, bool *aResult)
{
  // We don't subsume anything except ourselves.  Note that nsPrincipal::Equals
  // will return false for us, since we're not about:blank and not Equals to
  // reasonable nsPrincipals.
  *aResult = (aOther == this);
  return NS_OK;
}

NS_IMETHODIMP
nsNullPrincipal::CheckMayLoad(nsIURI* aURI, bool aReport)
{
  if (aReport) {
    nsScriptSecurityManager::ReportError(
      nsnull, NS_LITERAL_STRING("CheckSameOriginError"), mURI, aURI);
  }

  return NS_ERROR_DOM_BAD_URI;
}

NS_IMETHODIMP 
nsNullPrincipal::GetSubjectName(nsACString& aName)
{
    return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsNullPrincipal::GetCertificate(nsISupports** aCertificate)
{
    *aCertificate = nsnull;
    return NS_OK;
}

/**
 * nsISerializable implementation
 */
NS_IMETHODIMP
nsNullPrincipal::Read(nsIObjectInputStream* aStream)
{
  // no-op: CID is sufficient to create a useful nsNullPrincipal, since the URI
  // is not really relevant.
  return NS_OK;
}

NS_IMETHODIMP
nsNullPrincipal::Write(nsIObjectOutputStream* aStream)
{
  // no-op: CID is sufficient to create a useful nsNullPrincipal, since the URI
  // is not really relevant.
  return NS_OK;
}

