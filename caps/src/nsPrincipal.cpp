/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher A. Aillon <christopher@aillon.com>
 *   Giorgio Maone <g.maone@informaction.com>
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

#include "nscore.h"
#include "nsScriptSecurityManager.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "plstr.h"
#include "nsCRT.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsJSPrincipals.h"
#include "nsVoidArray.h"
#include "nsHashtable.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIClassInfoImpl.h"

#include "nsPrincipal.h"


// Static member variables
PRInt32 nsPrincipal::sCapabilitiesOrdinal = 0;
const char nsPrincipal::sInvalid[] = "Invalid";


NS_IMPL_QUERY_INTERFACE2_CI(nsPrincipal,
                            nsIPrincipal,
                            nsISerializable)
NS_IMPL_CI_INTERFACE_GETTER2(nsPrincipal,
                             nsIPrincipal,
                             nsISerializable)

NS_IMETHODIMP_(nsrefcnt)
nsPrincipal::AddRef()
{
  NS_PRECONDITION(PRInt32(mJSPrincipals.refcount) >= 0, "illegal refcnt");
  // XXXcaa does this need to be threadsafe?  See bug 143559.
  nsrefcnt count = PR_AtomicIncrement((PRInt32 *)&mJSPrincipals.refcount);
  NS_LOG_ADDREF(this, count, "nsPrincipal", sizeof(*this));
  return count;
}

NS_IMETHODIMP_(nsrefcnt)
nsPrincipal::Release()
{
  NS_PRECONDITION(0 != mJSPrincipals.refcount, "dup release");
  nsrefcnt count = PR_AtomicDecrement((PRInt32 *)&mJSPrincipals.refcount);
  NS_LOG_RELEASE(this, count, "nsPrincipal");
  if (count == 0) {
    NS_DELETEXPCOM(this);
  }

  return count;
}

nsPrincipal::nsPrincipal()
  : mCapabilities(7),
    mSecurityPolicy(nsnull),
    mTrusted(PR_FALSE),
    mInitialized(PR_FALSE)
{
}

nsresult
nsPrincipal::Init(const nsACString& aCertFingerprint,
                  const nsACString& aSubjectName,
                  const nsACString& aPrettyName,
                  nsISupports* aCert,
                  nsIURI *aCodebase)
{
  NS_ENSURE_STATE(!mInitialized);
  NS_ENSURE_ARG(!aCertFingerprint.IsEmpty() || aCodebase); // better have one of these.

  mInitialized = PR_TRUE;

  mCodebase = aCodebase;

  // Invalidate our cached origin
  mOrigin = nsnull;

  nsresult rv;
  if (!aCertFingerprint.IsEmpty()) {
    rv = SetCertificate(aCertFingerprint, aSubjectName, aPrettyName, aCert);
    if (NS_SUCCEEDED(rv)) {
      rv = mJSPrincipals.Init(this, mCert->fingerprint.get());
    }
  }
  else {
    nsCAutoString spec;
    rv = mCodebase->GetSpec(spec);
    if (NS_SUCCEEDED(rv)) {
      rv = mJSPrincipals.Init(this, spec.get());
    }
  }

  NS_ASSERTION(NS_SUCCEEDED(rv), "nsPrincipal::Init() failed");

  return rv;
}


PR_STATIC_CALLBACK(PRBool)
deleteElement(void* aElement, void *aData)
{
  nsHashtable *ht = (nsHashtable *) aElement;
  delete ht;
  return PR_TRUE;
}

nsPrincipal::~nsPrincipal(void)
{
  mAnnotations.EnumerateForwards(deleteElement, nsnull);
  SetSecurityPolicy(nsnull); 
}

NS_IMETHODIMP
nsPrincipal::GetJSPrincipals(JSContext *cx, JSPrincipals **jsprin)
{
  NS_PRECONDITION(mJSPrincipals.nsIPrincipalPtr, "mJSPrincipals is uninitialized!");

  JSPRINCIPALS_HOLD(cx, &mJSPrincipals);
  *jsprin = &mJSPrincipals;
  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::GetOrigin(char **aOrigin)
{
  *aOrigin = nsnull;

  if (!mOrigin) {
    nsIURI* uri = mDomain ? mDomain : mCodebase;
    if (uri) {
      mOrigin = NS_GetInnermostURI(uri);
    }
  }
  
  if (!mOrigin) {
    NS_ASSERTION(mCert, "No Domain or Codebase for a non-cert principal");
    return NS_ERROR_FAILURE;
  }

  nsCAutoString hostPort;

  // chrome: URLs don't have a meaningful origin, so make
  // sure we just get the full spec for them.
  // XXX this should be removed in favor of the solution in
  // bug 160042.
  PRBool isChrome;
  nsresult rv = mOrigin->SchemeIs("chrome", &isChrome);
  if (NS_SUCCEEDED(rv) && !isChrome) {
    rv = mOrigin->GetHostPort(hostPort);
  }

  if (NS_SUCCEEDED(rv) && !isChrome) {
    nsCAutoString scheme;
    rv = mOrigin->GetScheme(scheme);
    NS_ENSURE_SUCCESS(rv, rv);
    *aOrigin = ToNewCString(scheme + NS_LITERAL_CSTRING("://") + hostPort);
  }
  else {
    // Some URIs (e.g., nsSimpleURI) don't support host. Just
    // get the full spec.
    nsCAutoString spec;
    rv = mOrigin->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);
    *aOrigin = ToNewCString(spec);
  }

  return *aOrigin ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsPrincipal::GetSecurityPolicy(void** aSecurityPolicy)
{
  if (mSecurityPolicy && mSecurityPolicy->IsInvalid()) 
    SetSecurityPolicy(nsnull);
  
  *aSecurityPolicy = (void *) mSecurityPolicy;
  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::SetSecurityPolicy(void* aSecurityPolicy)
{
  DomainPolicy *newPolicy = NS_REINTERPRET_CAST(
                              DomainPolicy *, aSecurityPolicy);
  if (newPolicy)
    newPolicy->Hold();
 
  if (mSecurityPolicy)
    mSecurityPolicy->Drop();
  
  mSecurityPolicy = newPolicy;
  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::Equals(nsIPrincipal *aOther, PRBool *aResult)
{
  *aResult = PR_FALSE;

  if (!aOther) {
    NS_WARNING("Need a principal to compare this to!");
    return NS_OK;
  }

  if (this != aOther) {
    if (mCert) {
      PRBool otherHasCert;
      aOther->GetHasCertificate(&otherHasCert);
      if (!otherHasCert) {
        return NS_OK;
      }

      nsCAutoString str;
      aOther->GetFingerprint(str);
      *aResult = str.Equals(mCert->fingerprint);

      // If either subject name is empty, just let the result stand (so that
      // nsScriptSecurityManager::SetCanEnableCapability works), but if they're
      // both non-empty, only claim equality if they're equal.
      if (*aResult && !mCert->subjectName.IsEmpty()) {
        // Check the other principal's subject name
        aOther->GetSubjectName(str);
        *aResult = str.Equals(mCert->subjectName) || str.IsEmpty();
      }
        
      return NS_OK;
    }

    // Codebases are equal if they have the same origin.
    *aResult =
      NS_SUCCEEDED(nsScriptSecurityManager::GetScriptSecurityManager()
                   ->CheckSameOriginPrincipal(this, aOther));
    return NS_OK;
  }

  *aResult = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::Subsumes(nsIPrincipal *aOther, PRBool *aResult)
{
  return Equals(aOther, aResult);
}

NS_IMETHODIMP
nsPrincipal::CanEnableCapability(const char *capability, PRInt16 *result)
{
  // If this principal is marked invalid, can't enable any capabilities
  nsCStringKey invalidKey(sInvalid);
  if (mCapabilities.Exists(&invalidKey)) {
    *result = nsIPrincipal::ENABLE_DENIED;

    return NS_OK;
  }

  if (!mCert && !mTrusted) {
    NS_ASSERTION(mInitialized, "Trying to enable a capability on an "
                               "uninitialized principal");

    // If we are a non-trusted codebase principal, capabilities can not
    // be enabled if the user has not set the pref allowing scripts to
    // request enhanced capabilities; however, the file: and resource:
    // schemes are special and may be able to get extra capabilities
    // even with the pref disabled.

    static const char pref[] = "signed.applets.codebase_principal_support";
    nsCOMPtr<nsIPrefBranch> prefBranch =
      do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefBranch) {
      PRBool mightEnable;
      nsresult rv = prefBranch->GetBoolPref(pref, &mightEnable);
      if (NS_FAILED(rv) || !mightEnable) {
        rv = mCodebase->SchemeIs("file", &mightEnable);
        if (NS_FAILED(rv) || !mightEnable) {
          rv = mCodebase->SchemeIs("resource", &mightEnable);
          if (NS_FAILED(rv) || !mightEnable) {
            *result = nsIPrincipal::ENABLE_DENIED;

            return NS_OK;
          }
        }
      }
    }
  }

  const char *start = capability;
  *result = nsIPrincipal::ENABLE_GRANTED;
  for(;;) {
    const char *space = PL_strchr(start, ' ');
    PRInt32 len = space ? space - start : strlen(start);
    nsCAutoString capString(start, len);
    nsCStringKey key(capString);
    PRInt16 value = (PRInt16)NS_PTR_TO_INT32(mCapabilities.Get(&key));
    if (value == 0 || value == nsIPrincipal::ENABLE_UNKNOWN) {
      // We don't know whether we can enable this capability,
      // so we should ask the user.
      value = nsIPrincipal::ENABLE_WITH_USER_PERMISSION;
    }

    if (value < *result) {
      *result = value;
    }

    if (!space) {
      break;
    }

    start = space + 1;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::SetCanEnableCapability(const char *capability,
                                    PRInt16 canEnable)
{
  // If this principal is marked invalid, can't enable any capabilities

  nsCStringKey invalidKey(sInvalid);
  if (mCapabilities.Exists(&invalidKey)) {
    return NS_OK;
  }

  if (PL_strcmp(capability, sInvalid) == 0) {
    mCapabilities.Reset();
  }

  const char *start = capability;
  for(;;) {
    const char *space = PL_strchr(start, ' ');
    int len = space ? space - start : strlen(start);
    nsCAutoString capString(start, len);
    nsCStringKey key(capString);
    mCapabilities.Put(&key, NS_INT32_TO_PTR(canEnable));
    if (!space) {
      break;
    }

    start = space + 1;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::IsCapabilityEnabled(const char *capability, void *annotation,
                                 PRBool *result)
{
  *result = PR_FALSE;
  nsHashtable *ht = (nsHashtable *) annotation;
  if (!ht) {
    return NS_OK;
  }
  const char *start = capability;
  for(;;) {
    const char *space = PL_strchr(start, ' ');
    int len = space ? space - start : strlen(start);
    nsCAutoString capString(start, len);
    nsCStringKey key(capString);
    *result = (ht->Get(&key) == (void *) AnnotationEnabled);
    if (!*result) {
      // If any single capability is not enabled, then return false.
      return NS_OK;
    }

    if (!space) {
      return NS_OK;
    }

    start = space + 1;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::EnableCapability(const char *capability, void **annotation)
{
  return SetCapability(capability, annotation, AnnotationEnabled);
}

NS_IMETHODIMP
nsPrincipal::DisableCapability(const char *capability, void **annotation)
{
  return SetCapability(capability, annotation, AnnotationDisabled);
}

NS_IMETHODIMP
nsPrincipal::RevertCapability(const char *capability, void **annotation)
{
  if (*annotation) {
    nsHashtable *ht = (nsHashtable *) *annotation;
    const char *start = capability;
    for(;;) {
      const char *space = PL_strchr(start, ' ');
      int len = space ? space - start : strlen(start);
      nsCAutoString capString(start, len);
      nsCStringKey key(capString);
      ht->Remove(&key);
      if (!space) {
        return NS_OK;
      }

      start = space + 1;
    }
  }
  return NS_OK;
}

nsresult
nsPrincipal::SetCapability(const char *capability, void **annotation,
                           AnnotationValue value)
{
  if (*annotation == nsnull) {
    *annotation = new nsHashtable(5);
    if (!*annotation) {
       return NS_ERROR_OUT_OF_MEMORY;
     }

    // This object owns its annotations. Save them so we can release
    // them when we destroy this object.
    mAnnotations.AppendElement(*annotation);
  }

  const char *start = capability;
  for(;;) {
    const char *space = PL_strchr(start, ' ');
    int len = space ? space - start : strlen(start);
    nsCAutoString capString(start, len);
    nsCStringKey key(capString);
    nsHashtable *ht = (nsHashtable *) *annotation;
    ht->Put(&key, (void *) value);
    if (!space) {
      break;
    }

    start = space + 1;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::GetHasCertificate(PRBool* aResult)
{
  *aResult = (mCert != nsnull);

  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::GetURI(nsIURI** aURI)
{
  if (!mCodebase) {
    *aURI = nsnull;
    return NS_OK;
  }

  return NS_EnsureSafeToReturn(mCodebase, aURI);
}

void
nsPrincipal::SetURI(nsIURI* aURI)
{
  mCodebase = NS_TryToMakeImmutable(aURI);

  // Invalidate our cached origin
  mOrigin = nsnull;
}


nsresult
nsPrincipal::SetCertificate(const nsACString& aFingerprint,
                            const nsACString& aSubjectName,
                            const nsACString& aPrettyName,
                            nsISupports* aCert)
{
  NS_ENSURE_STATE(!mCert);

  if (aFingerprint.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  mCert = new Certificate(aFingerprint, aSubjectName, aPrettyName, aCert);
  if (!mCert) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::GetFingerprint(nsACString& aFingerprint)
{
  NS_ENSURE_STATE(mCert);

  aFingerprint = mCert->fingerprint;

  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::GetPrettyName(nsACString& aName)
{
  NS_ENSURE_STATE(mCert);

  aName = mCert->prettyName;

  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::GetSubjectName(nsACString& aName)
{
  NS_ENSURE_STATE(mCert);

  aName = mCert->subjectName;

  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::GetCertificate(nsISupports** aCertificate)
{
  if (mCert) {
    NS_IF_ADDREF(*aCertificate = mCert->cert);
  }
  else {
    *aCertificate = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::GetHashValue(PRUint32* aValue)
{
  NS_PRECONDITION(mCert || mCodebase, "Need a cert or codebase");

  // If there is a certificate, it takes precendence over the codebase.
  if (mCert) {
    *aValue = nsCRT::HashCode(mCert->fingerprint.get(), nsnull);
  }
  else {
    nsCAutoString str;
    mCodebase->GetSpec(str);
    *aValue = nsCRT::HashCode(str.get(), nsnull);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::GetDomain(nsIURI** aDomain)
{
  if (!mDomain) {
    *aDomain = nsnull;
    return NS_OK;
  }

  return NS_EnsureSafeToReturn(mDomain, aDomain);
}

NS_IMETHODIMP
nsPrincipal::SetDomain(nsIURI* aDomain)
{
  mDomain = NS_TryToMakeImmutable(aDomain);
  // Domain has changed, forget cached security policy
  SetSecurityPolicy(nsnull);

  // Invalidate our cached origin
  mOrigin = nsnull;

  return NS_OK;
}

nsresult
nsPrincipal::InitFromPersistent(const char* aPrefName,
                                const nsCString& aToken,
                                const nsCString& aSubjectName,
                                const nsACString& aPrettyName,
                                const char* aGrantedList,
                                const char* aDeniedList,
                                nsISupports* aCert,
                                PRBool aIsCert,
                                PRBool aTrusted)
{
  NS_PRECONDITION(mCapabilities.Count() == 0,
                  "mCapabilities was already initialized?");
  NS_PRECONDITION(mAnnotations.Count() == 0,
                  "mAnnotations was already initialized?");
  NS_PRECONDITION(!mInitialized, "We were already initialized?");

  mInitialized = PR_TRUE;

  nsresult rv;
  if (aIsCert) {
    rv = SetCertificate(aToken, aSubjectName, aPrettyName, aCert);
    
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  else {
    rv = NS_NewURI(getter_AddRefs(mCodebase), aToken, nsnull);
    if (NS_FAILED(rv)) {
      NS_ERROR("Malformed URI in capability.principal preference.");
      return rv;
    }

    mTrusted = aTrusted;

    // Invalidate our cached origin
    mOrigin = nsnull;
  }

  rv = mJSPrincipals.Init(this, aToken.get());
  NS_ENSURE_SUCCESS(rv, rv);

  //-- Save the preference name
  mPrefName = aPrefName;

  const char* ordinalBegin = PL_strpbrk(aPrefName, "1234567890");
  if (ordinalBegin) {
    PRIntn n = atoi(ordinalBegin);
    if (sCapabilitiesOrdinal <= n) {
      sCapabilitiesOrdinal = n + 1;
    }
  }

  //-- Store the capabilities
  rv = NS_OK;
  if (aGrantedList) {
    rv = SetCanEnableCapability(aGrantedList, nsIPrincipal::ENABLE_GRANTED);
  }

  if (NS_SUCCEEDED(rv) && aDeniedList) {
    rv = SetCanEnableCapability(aDeniedList, nsIPrincipal::ENABLE_DENIED);
  }

  return rv;
}

nsresult
nsPrincipal::EnsureCertData(const nsACString& aSubjectName,
                            const nsACString& aPrettyName,
                            nsISupports* aCert)
{
  NS_ENSURE_STATE(mCert);

  if (!mCert->subjectName.IsEmpty() &&
      !mCert->subjectName.Equals(aSubjectName)) {
    return NS_ERROR_INVALID_ARG;
  }

  mCert->subjectName = aSubjectName;
  mCert->prettyName = aPrettyName;
  mCert->cert = aCert;
  return NS_OK;
}

struct CapabilityList
{
  nsCString* granted;
  nsCString* denied;
};

PR_STATIC_CALLBACK(PRBool)
AppendCapability(nsHashKey *aKey, void *aData, void *capListPtr)
{
  CapabilityList* capList = (CapabilityList*)capListPtr;
  PRInt16 value = (PRInt16)NS_PTR_TO_INT32(aData);
  nsCStringKey* key = (nsCStringKey *)aKey;
  if (value == nsIPrincipal::ENABLE_GRANTED) {
    capList->granted->Append(key->GetString(), key->GetStringLength());
    capList->granted->Append(' ');
  }
  else if (value == nsIPrincipal::ENABLE_DENIED) {
    capList->denied->Append(key->GetString(), key->GetStringLength());
    capList->denied->Append(' ');
  }

  return PR_TRUE;
}

NS_IMETHODIMP
nsPrincipal::GetPreferences(char** aPrefName, char** aID,
                            char** aSubjectName,
                            char** aGrantedList, char** aDeniedList,
                            PRBool* aIsTrusted)
{
  if (mPrefName.IsEmpty()) {
    if (mCert) {
      mPrefName.Assign("capability.principal.certificate.p");
    }
    else {
      mPrefName.Assign("capability.principal.codebase.p");
    }

    mPrefName.AppendInt(sCapabilitiesOrdinal++);
    mPrefName.Append(".id");
  }

  *aPrefName = nsnull;
  *aID = nsnull;
  *aSubjectName = nsnull;
  *aGrantedList = nsnull;
  *aDeniedList = nsnull;
  *aIsTrusted = mTrusted;

  char *prefName = nsnull;
  char *id = nsnull;
  char *subjectName = nsnull;
  char *granted = nsnull;
  char *denied = nsnull;

  //-- Preference name
  prefName = ToNewCString(mPrefName);
  if (!prefName) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  //-- ID
  nsresult rv = NS_OK;
  if (mCert) {
    id = ToNewCString(mCert->fingerprint);
    if (!id) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }
  else {
    rv = GetOrigin(&id);
  }

  if (NS_FAILED(rv)) {
    nsMemory::Free(prefName);
    return rv;
  }

  if (mCert) {
    subjectName = ToNewCString(mCert->subjectName);
  } else {
    subjectName = ToNewCString(EmptyCString());
  }

  if (!subjectName) {
    nsMemory::Free(prefName);
    nsMemory::Free(id);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  //-- Capabilities
  nsCAutoString grantedListStr, deniedListStr;
  CapabilityList capList = CapabilityList();
  capList.granted = &grantedListStr;
  capList.denied = &deniedListStr;
  mCapabilities.Enumerate(AppendCapability, (void*)&capList);

  if (!grantedListStr.IsEmpty()) {
    grantedListStr.Truncate(grantedListStr.Length() - 1);
    granted = ToNewCString(grantedListStr);
    if (!granted) {
      nsMemory::Free(prefName);
      nsMemory::Free(id);
      nsMemory::Free(subjectName);
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  if (!deniedListStr.IsEmpty()) {
    deniedListStr.Truncate(deniedListStr.Length() - 1);
    denied = ToNewCString(deniedListStr);
    if (!denied) {
      nsMemory::Free(prefName);
      nsMemory::Free(id);
      nsMemory::Free(subjectName);
      if (granted) {
        nsMemory::Free(granted);
      }
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  *aPrefName = prefName;
  *aID = id;
  *aSubjectName = subjectName;
  *aGrantedList = granted;
  *aDeniedList = denied;

  return NS_OK;
}

PR_STATIC_CALLBACK(nsresult)
ReadAnnotationEntry(nsIObjectInputStream* aStream, nsHashKey** aKey,
                    void** aData)
{
  nsresult rv;
  nsCStringKey* key = new nsCStringKey(aStream, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  PRUint32 value;
  rv = aStream->Read32(&value);
  if (NS_FAILED(rv)) {
    delete key;
    return rv;
  }

  *aKey = key;
  *aData = (void*) value;
  return NS_OK;
}

PR_STATIC_CALLBACK(void)
FreeAnnotationEntry(nsIObjectInputStream* aStream, nsHashKey* aKey,
                    void* aData)
{
  delete aKey;
}

NS_IMETHODIMP
nsPrincipal::Read(nsIObjectInputStream* aStream)
{
  PRUint32 annotationCount;
  nsresult rv = aStream->Read32(&annotationCount);
  if (NS_FAILED(rv)) {
    return rv;
  }

  for (PRInt32 i = 0, n = PRInt32(annotationCount); i < n; i++) {
    nsHashtable *ht = new nsHashtable(aStream,
                                      ReadAnnotationEntry,
                                      FreeAnnotationEntry,
                                      &rv);
    if (!ht) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (NS_FAILED(rv)) {
      delete ht;
      return rv;
    }

    if (!mAnnotations.InsertElementAt(NS_REINTERPRET_CAST(void*, ht), i)) {
      delete ht;
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  PRBool hasCapabilities;
  rv = aStream->ReadBoolean(&hasCapabilities);
  if (NS_SUCCEEDED(rv) && hasCapabilities) {
    mCapabilities = nsHashtable(aStream,
                                ReadAnnotationEntry,
                                FreeAnnotationEntry,
                                &rv);
  }

  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = NS_ReadOptionalCString(aStream, mPrefName);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

PR_STATIC_CALLBACK(nsresult)
WriteScalarValue(nsIObjectOutputStream* aStream, void* aData)
{
  PRUint32 value = NS_PTR_TO_INT32(aData);

  return aStream->Write32(value);
}

NS_IMETHODIMP
nsPrincipal::Write(nsIObjectOutputStream* aStream)
{
  PRUint32 annotationCount = PRUint32(mAnnotations.Count());
  nsresult rv = aStream->Write32(annotationCount);
  if (NS_FAILED(rv)) {
    return rv;
  }

  for (PRInt32 i = 0, n = PRInt32(annotationCount); i < n; i++) {
    nsHashtable *ht = NS_REINTERPRET_CAST(nsHashtable *, mAnnotations[i]);
    rv = ht->Write(aStream, WriteScalarValue);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  PRBool hasCapabilities = (mCapabilities.Count() > 0);
  rv = aStream->WriteBoolean(hasCapabilities);
  if (NS_SUCCEEDED(rv) && hasCapabilities) {
    rv = mCapabilities.Write(aStream, WriteScalarValue);
  }

  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = NS_WriteOptionalStringZ(aStream, mPrefName.get());
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}
