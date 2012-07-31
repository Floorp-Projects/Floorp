/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nscore.h"
#include "nsScriptSecurityManager.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "plstr.h"
#include "nsCRT.h"
#include "nsIURI.h"
#include "nsIFileURL.h"
#include "nsIProtocolHandler.h"
#include "nsNetUtil.h"
#include "nsJSPrincipals.h"
#include "nsVoidArray.h"
#include "nsHashtable.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIClassInfoImpl.h"
#include "nsDOMError.h"
#include "nsIContentSecurityPolicy.h"
#include "nsContentUtils.h"
#include "jswrapper.h"

#include "nsPrincipal.h"

#include "mozilla/Preferences.h"
#include "mozilla/HashFunctions.h"

using namespace mozilla;

static bool gCodeBasePrincipalSupport = false;
static bool gIsObservingCodeBasePrincipalSupport = false;

static bool URIIsImmutable(nsIURI* aURI)
{
  nsCOMPtr<nsIMutable> mutableObj(do_QueryInterface(aURI));
  bool isMutable;
  return
    mutableObj &&
    NS_SUCCEEDED(mutableObj->GetMutable(&isMutable)) &&
    !isMutable;                               
}

// Static member variables
PRInt32 nsBasePrincipal::sCapabilitiesOrdinal = 0;
const char nsBasePrincipal::sInvalid[] = "Invalid";

NS_IMETHODIMP_(nsrefcnt)
nsBasePrincipal::AddRef()
{
  NS_PRECONDITION(PRInt32(refcount) >= 0, "illegal refcnt");
  // XXXcaa does this need to be threadsafe?  See bug 143559.
  nsrefcnt count = PR_ATOMIC_INCREMENT(&refcount);
  NS_LOG_ADDREF(this, count, "nsBasePrincipal", sizeof(*this));
  return count;
}

NS_IMETHODIMP_(nsrefcnt)
nsBasePrincipal::Release()
{
  NS_PRECONDITION(0 != refcount, "dup release");
  nsrefcnt count = PR_ATOMIC_DECREMENT(&refcount);
  NS_LOG_RELEASE(this, count, "nsBasePrincipal");
  if (count == 0) {
    delete this;
  }

  return count;
}

nsBasePrincipal::nsBasePrincipal()
  : mCapabilities(nullptr),
    mSecurityPolicy(nullptr),
    mTrusted(false)
{
  if (!gIsObservingCodeBasePrincipalSupport) {
    nsresult rv =
      Preferences::AddBoolVarCache(&gCodeBasePrincipalSupport,
                                   "signed.applets.codebase_principal_support",
                                   false);
    gIsObservingCodeBasePrincipalSupport = NS_SUCCEEDED(rv);
    NS_WARN_IF_FALSE(gIsObservingCodeBasePrincipalSupport,
                     "Installing gCodeBasePrincipalSupport failed!");
  }
}

nsBasePrincipal::~nsBasePrincipal(void)
{
  SetSecurityPolicy(nullptr); 
  delete mCapabilities;
}

NS_IMETHODIMP
nsBasePrincipal::GetSecurityPolicy(void** aSecurityPolicy)
{
  if (mSecurityPolicy && mSecurityPolicy->IsInvalid()) 
    SetSecurityPolicy(nullptr);
  
  *aSecurityPolicy = (void *) mSecurityPolicy;
  return NS_OK;
}

NS_IMETHODIMP
nsBasePrincipal::SetSecurityPolicy(void* aSecurityPolicy)
{
  DomainPolicy *newPolicy = reinterpret_cast<DomainPolicy *>(aSecurityPolicy);
  if (newPolicy)
    newPolicy->Hold();
 
  if (mSecurityPolicy)
    mSecurityPolicy->Drop();
  
  mSecurityPolicy = newPolicy;
  return NS_OK;
}

bool
nsBasePrincipal::CertificateEquals(nsIPrincipal *aOther)
{
  bool otherHasCert;
  aOther->GetHasCertificate(&otherHasCert);
  if (otherHasCert != (mCert != nullptr)) {
    // One has a cert while the other doesn't.  Not equal.
    return false;
  }

  if (!mCert)
    return true;

  nsCAutoString str;
  aOther->GetFingerprint(str);
  if (!str.Equals(mCert->fingerprint))
    return false;

  // If either subject name is empty, just let the result stand (so that
  // nsScriptSecurityManager::SetCanEnableCapability works), but if they're
  // both non-empty, only claim equality if they're equal.
  if (!mCert->subjectName.IsEmpty()) {
    // Check the other principal's subject name
    aOther->GetSubjectName(str);
    return str.Equals(mCert->subjectName) || str.IsEmpty();
  }

  return true;
}

NS_IMETHODIMP
nsBasePrincipal::CanEnableCapability(const char *capability, PRInt16 *result)
{
  // If this principal is marked invalid, can't enable any capabilities
  if (mCapabilities) {
    nsCStringKey invalidKey(sInvalid);
    if (mCapabilities->Exists(&invalidKey)) {
      *result = nsIPrincipal::ENABLE_DENIED;

      return NS_OK;
    }
  }

  if (!mCert && !mTrusted) {
    // If we are a non-trusted codebase principal, capabilities can not
    // be enabled if the user has not set the pref allowing scripts to
    // request enhanced capabilities; however, the file: and resource:
    // schemes are special and may be able to get extra capabilities
    // even with the pref disabled.
    nsCOMPtr<nsIURI> codebase;
    GetURI(getter_AddRefs(codebase));
    if (!gCodeBasePrincipalSupport && codebase) {
      bool mightEnable = false;     
      nsresult rv = codebase->SchemeIs("file", &mightEnable);
      if (NS_FAILED(rv) || !mightEnable) {
        rv = codebase->SchemeIs("resource", &mightEnable);
        if (NS_FAILED(rv) || !mightEnable) {
          *result = nsIPrincipal::ENABLE_DENIED;
          return NS_OK;
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
    PRInt16 value =
      mCapabilities ? (PRInt16)NS_PTR_TO_INT32(mCapabilities->Get(&key)) : 0;
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

nsresult
nsBasePrincipal::SetCanEnableCapability(const char *capability,
                                        PRInt16 canEnable)
{
  // If this principal is marked invalid, can't enable any capabilities
  if (!mCapabilities) {
    mCapabilities = new nsHashtable(7);  // XXXbz gets bumped up to 16 anyway
    NS_ENSURE_TRUE(mCapabilities, NS_ERROR_OUT_OF_MEMORY);
  }

  nsCStringKey invalidKey(sInvalid);
  if (mCapabilities->Exists(&invalidKey)) {
    return NS_OK;
  }

  if (PL_strcmp(capability, sInvalid) == 0) {
    mCapabilities->Reset();
  }

  const char *start = capability;
  for(;;) {
    const char *space = PL_strchr(start, ' ');
    int len = space ? space - start : strlen(start);
    nsCAutoString capString(start, len);
    nsCStringKey key(capString);
    mCapabilities->Put(&key, NS_INT32_TO_PTR(canEnable));
    if (!space) {
      break;
    }

    start = space + 1;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBasePrincipal::IsCapabilityEnabled(const char *capability, void *annotation,
                                     bool *result)
{
  *result = false;
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
nsBasePrincipal::EnableCapability(const char *capability, void **annotation)
{
  return SetCapability(capability, annotation, AnnotationEnabled);
}

nsresult
nsBasePrincipal::SetCapability(const char *capability, void **annotation,
                               AnnotationValue value)
{
  if (*annotation == nullptr) {
    nsHashtable* ht = new nsHashtable(5);

    if (!ht) {
       return NS_ERROR_OUT_OF_MEMORY;
     }

    // This object owns its annotations. Save them so we can release
    // them when we destroy this object.
    if (!mAnnotations.AppendElement(ht)) {
      delete ht;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    *annotation = ht;
  }

  const char *start = capability;
  for(;;) {
    const char *space = PL_strchr(start, ' ');
    int len = space ? space - start : strlen(start);
    nsCAutoString capString(start, len);
    nsCStringKey key(capString);
    nsHashtable *ht = static_cast<nsHashtable *>(*annotation);
    ht->Put(&key, (void *) value);
    if (!space) {
      break;
    }

    start = space + 1;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBasePrincipal::GetHasCertificate(bool* aResult)
{
  *aResult = (mCert != nullptr);

  return NS_OK;
}

nsresult
nsBasePrincipal::SetCertificate(const nsACString& aFingerprint,
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
nsBasePrincipal::GetFingerprint(nsACString& aFingerprint)
{
  NS_ENSURE_STATE(mCert);

  aFingerprint = mCert->fingerprint;

  return NS_OK;
}

NS_IMETHODIMP
nsBasePrincipal::GetPrettyName(nsACString& aName)
{
  NS_ENSURE_STATE(mCert);

  aName = mCert->prettyName;

  return NS_OK;
}

NS_IMETHODIMP
nsBasePrincipal::GetSubjectName(nsACString& aName)
{
  NS_ENSURE_STATE(mCert);

  aName = mCert->subjectName;

  return NS_OK;
}

NS_IMETHODIMP
nsBasePrincipal::GetCertificate(nsISupports** aCertificate)
{
  if (mCert) {
    NS_IF_ADDREF(*aCertificate = mCert->cert);
  }
  else {
    *aCertificate = nullptr;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBasePrincipal::GetCsp(nsIContentSecurityPolicy** aCsp)
{
  NS_IF_ADDREF(*aCsp = mCSP);
  return NS_OK;
}

NS_IMETHODIMP
nsBasePrincipal::SetCsp(nsIContentSecurityPolicy* aCsp)
{
  // If CSP was already set, it should not be destroyed!  Instead, it should
  // get set anew when a new principal is created.
  if (mCSP)
    return NS_ERROR_ALREADY_INITIALIZED;

  mCSP = aCsp;
  return NS_OK;
}

nsresult
nsBasePrincipal::EnsureCertData(const nsACString& aSubjectName,
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

static bool
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

  return true;
}

NS_IMETHODIMP
nsBasePrincipal::GetPreferences(char** aPrefName, char** aID,
                                char** aSubjectName,
                                char** aGrantedList, char** aDeniedList,
                                bool* aIsTrusted)
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

  *aPrefName = nullptr;
  *aID = nullptr;
  *aSubjectName = nullptr;
  *aGrantedList = nullptr;
  *aDeniedList = nullptr;
  *aIsTrusted = mTrusted;

  char *prefName = nullptr;
  char *id = nullptr;
  char *subjectName = nullptr;
  char *granted = nullptr;
  char *denied = nullptr;

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
  if (mCapabilities) {
    CapabilityList capList = CapabilityList();
    capList.granted = &grantedListStr;
    capList.denied = &deniedListStr;
    mCapabilities->Enumerate(AppendCapability, (void*)&capList);
  }

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

static nsresult
ReadAnnotationEntry(nsIObjectInputStream* aStream, nsHashKey** aKey,
                    void** aData)
{
  nsresult rv;
  nsCStringKey* key = new nsCStringKey(aStream, &rv);
  if (!key)
    return NS_ERROR_OUT_OF_MEMORY;

  if (NS_FAILED(rv)) {
    delete key;
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

static void
FreeAnnotationEntry(nsIObjectInputStream* aStream, nsHashKey* aKey,
                    void* aData)
{
  delete aKey;
}

#ifdef DEBUG
void nsPrincipal::dumpImpl()
{
  nsCAutoString str;
  GetScriptLocation(str);
  fprintf(stderr, "nsPrincipal (%p) = %s\n", static_cast<void*>(this), str.get());
}
#endif 

NS_IMPL_CLASSINFO(nsPrincipal, NULL, nsIClassInfo::MAIN_THREAD_ONLY,
                  NS_PRINCIPAL_CID)
NS_IMPL_QUERY_INTERFACE2_CI(nsPrincipal,
                            nsIPrincipal,
                            nsISerializable)
NS_IMPL_CI_INTERFACE_GETTER2(nsPrincipal,
                             nsIPrincipal,
                             nsISerializable)
NS_IMPL_ADDREF_INHERITED(nsPrincipal, nsBasePrincipal)
NS_IMPL_RELEASE_INHERITED(nsPrincipal, nsBasePrincipal)

nsPrincipal::nsPrincipal()
  : mAppId(nsIScriptSecurityManager::UNKNOWN_APP_ID)
  , mInMozBrowser(false)
  , mCodebaseImmutable(false)
  , mDomainImmutable(false)
  , mInitialized(false)
{ }

nsPrincipal::~nsPrincipal()
{ }

nsresult
nsPrincipal::Init(const nsACString& aCertFingerprint,
                  const nsACString& aSubjectName,
                  const nsACString& aPrettyName,
                  nsISupports* aCert,
                  nsIURI *aCodebase,
                  PRUint32 aAppId,
                  bool aInMozBrowser)
{
  NS_ENSURE_STATE(!mInitialized);
  NS_ENSURE_ARG(!aCertFingerprint.IsEmpty() || aCodebase); // better have one of these.

  mInitialized = true;

  mCodebase = NS_TryToMakeImmutable(aCodebase);
  mCodebaseImmutable = URIIsImmutable(mCodebase);

  mAppId = aAppId;
  mInMozBrowser = aInMozBrowser;

  if (aCertFingerprint.IsEmpty())
    return NS_OK;

  return SetCertificate(aCertFingerprint, aSubjectName, aPrettyName, aCert);
}

void
nsPrincipal::GetScriptLocation(nsACString &aStr)
{
  if (mCert) {
    aStr.Assign(mCert->fingerprint);
  } else {
    mCodebase->GetSpec(aStr);
  }
}

/* static */ nsresult
nsPrincipal::GetOriginForURI(nsIURI* aURI, char **aOrigin)
{
  if (!aURI) {
    return NS_ERROR_FAILURE;
  }

  *aOrigin = nullptr;

  nsCOMPtr<nsIURI> origin = NS_GetInnermostURI(aURI);
  if (!origin) {
    return NS_ERROR_FAILURE;
  }

  nsCAutoString hostPort;

  // chrome: URLs don't have a meaningful origin, so make
  // sure we just get the full spec for them.
  // XXX this should be removed in favor of the solution in
  // bug 160042.
  bool isChrome;
  nsresult rv = origin->SchemeIs("chrome", &isChrome);
  if (NS_SUCCEEDED(rv) && !isChrome) {
    rv = origin->GetAsciiHost(hostPort);
    // Some implementations return an empty string, treat it as no support
    // for asciiHost by that implementation.
    if (hostPort.IsEmpty()) {
      rv = NS_ERROR_FAILURE;
    }
  }

  PRInt32 port;
  if (NS_SUCCEEDED(rv) && !isChrome) {
    rv = origin->GetPort(&port);
  }

  if (NS_SUCCEEDED(rv) && !isChrome) {
    if (port != -1) {
      hostPort.AppendLiteral(":");
      hostPort.AppendInt(port, 10);
    }

    nsCAutoString scheme;
    rv = origin->GetScheme(scheme);
    NS_ENSURE_SUCCESS(rv, rv);

    *aOrigin = ToNewCString(scheme + NS_LITERAL_CSTRING("://") + hostPort);
  }
  else {
    // Some URIs (e.g., nsSimpleURI) don't support asciiHost. Just
    // get the full spec.
    nsCAutoString spec;
    // XXX nsMozIconURI and nsJARURI don't implement this correctly, they
    // both fall back to GetSpec.  That needs to be fixed.
    rv = origin->GetAsciiSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);

    *aOrigin = ToNewCString(spec);
  }

  return *aOrigin ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsPrincipal::GetOrigin(char **aOrigin)
{
  return GetOriginForURI(mCodebase, aOrigin);
}

NS_IMETHODIMP
nsPrincipal::Equals(nsIPrincipal *aOther, bool *aResult)
{
  if (!aOther) {
    NS_WARNING("Need a principal to compare this to!");
    *aResult = false;
    return NS_OK;
  }

  if (this != aOther) {
    if (!CertificateEquals(aOther)) {
      *aResult = false;
      return NS_OK;
    }

    if (mCert) {
      // If either principal has no URI, it's the saved principal from
      // preferences; in that case, test true.  Do NOT test true if the two
      // principals have URIs with different codebases.
      nsCOMPtr<nsIURI> otherURI;
      nsresult rv = aOther->GetURI(getter_AddRefs(otherURI));
      if (NS_FAILED(rv)) {
        *aResult = false;
        return rv;
      }

      if (!otherURI || !mCodebase) {
        *aResult = true;
        return NS_OK;
      }

      // Fall through to the codebase comparison.
    }

    // Codebases are equal if they have the same origin.
    *aResult =
      NS_SUCCEEDED(nsScriptSecurityManager::CheckSameOriginPrincipal(this,
                                                                     aOther));
    return NS_OK;
  }

  *aResult = true;
  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::EqualsIgnoringDomain(nsIPrincipal *aOther, bool *aResult)
{
  if (this == aOther) {
    *aResult = true;
    return NS_OK;
  }

  *aResult = false;
  if (!CertificateEquals(aOther)) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> otherURI;
  nsresult rv = aOther->GetURI(getter_AddRefs(otherURI));
  if (NS_FAILED(rv)) {
    return rv;
  }

  NS_ASSERTION(mCodebase,
               "shouldn't be calling this on principals from preferences");

  // Compare codebases.
  *aResult = nsScriptSecurityManager::SecurityCompareURIs(mCodebase,
                                                          otherURI);
  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::Subsumes(nsIPrincipal *aOther, bool *aResult)
{
  return Equals(aOther, aResult);
}

NS_IMETHODIMP
nsPrincipal::SubsumesIgnoringDomain(nsIPrincipal *aOther, bool *aResult)
{
  return EqualsIgnoringDomain(aOther, aResult);
}

NS_IMETHODIMP
nsPrincipal::GetURI(nsIURI** aURI)
{
  if (mCodebaseImmutable) {
    NS_ADDREF(*aURI = mCodebase);
    return NS_OK;
  }

  if (!mCodebase) {
    *aURI = nullptr;
    return NS_OK;
  }

  return NS_EnsureSafeToReturn(mCodebase, aURI);
}

static bool
URIIsLocalFile(nsIURI *aURI)
{
  bool isFile;
  nsCOMPtr<nsINetUtil> util = do_GetNetUtil();

  return util && NS_SUCCEEDED(util->ProtocolHasFlags(aURI,
                                nsIProtocolHandler::URI_IS_LOCAL_FILE,
                                &isFile)) &&
         isFile;
}

NS_IMETHODIMP
nsPrincipal::CheckMayLoad(nsIURI* aURI, bool aReport)
{
  if (!nsScriptSecurityManager::SecurityCompareURIs(mCodebase, aURI)) {
    if (nsScriptSecurityManager::GetStrictFileOriginPolicy() &&
        URIIsLocalFile(aURI)) {
      nsCOMPtr<nsIFileURL> fileURL(do_QueryInterface(aURI));

      if (!URIIsLocalFile(mCodebase)) {
        // If the codebase is not also a file: uri then forget it
        // (don't want resource: principals in a file: doc)
        //
        // note: we're not de-nesting jar: uris here, we want to
        // keep archive content bottled up in its own little island

        if (aReport) {
          nsScriptSecurityManager::ReportError(
            nullptr, NS_LITERAL_STRING("CheckSameOriginError"), mCodebase, aURI);
        }

        return NS_ERROR_DOM_BAD_URI;
      }

      //
      // pull out the internal files
      //
      nsCOMPtr<nsIFileURL> codebaseFileURL(do_QueryInterface(mCodebase));
      nsCOMPtr<nsIFile> targetFile;
      nsCOMPtr<nsIFile> codebaseFile;
      bool targetIsDir;

      // Make sure targetFile is not a directory (bug 209234)
      // and that it exists w/out unescaping (bug 395343)

      if (!codebaseFileURL || !fileURL ||
          NS_FAILED(fileURL->GetFile(getter_AddRefs(targetFile))) ||
          NS_FAILED(codebaseFileURL->GetFile(getter_AddRefs(codebaseFile))) ||
          !targetFile || !codebaseFile ||
          NS_FAILED(targetFile->Normalize()) ||
          NS_FAILED(codebaseFile->Normalize()) ||
          NS_FAILED(targetFile->IsDirectory(&targetIsDir)) ||
          targetIsDir) {
        if (aReport) {
          nsScriptSecurityManager::ReportError(
            nullptr, NS_LITERAL_STRING("CheckSameOriginError"), mCodebase, aURI);
        }

        return NS_ERROR_DOM_BAD_URI;
      }

      //
      // If the file to be loaded is in a subdirectory of the codebase
      // (or same-dir if codebase is not a directory) then it will
      // inherit its codebase principal and be scriptable by that codebase.
      //
      bool codebaseIsDir;
      bool contained = false;
      nsresult rv = codebaseFile->IsDirectory(&codebaseIsDir);
      if (NS_SUCCEEDED(rv) && codebaseIsDir) {
        rv = codebaseFile->Contains(targetFile, true, &contained);
      }
      else {
        nsCOMPtr<nsIFile> codebaseParent;
        rv = codebaseFile->GetParent(getter_AddRefs(codebaseParent));
        if (NS_SUCCEEDED(rv) && codebaseParent) {
          rv = codebaseParent->Contains(targetFile, true, &contained);
        }
      }

      if (NS_SUCCEEDED(rv) && contained) {
        return NS_OK;
      }
    }

    if (aReport) {
      nsScriptSecurityManager::ReportError(
        nullptr, NS_LITERAL_STRING("CheckSameOriginError"), mCodebase, aURI);
    }
    
    return NS_ERROR_DOM_BAD_URI;
  }

  return NS_OK;
}

void
nsPrincipal::SetURI(nsIURI* aURI)
{
  mCodebase = NS_TryToMakeImmutable(aURI);
  mCodebaseImmutable = URIIsImmutable(mCodebase);
}

NS_IMETHODIMP
nsPrincipal::GetHashValue(PRUint32* aValue)
{
  NS_PRECONDITION(mCert || mCodebase, "Need a cert or codebase");

  // If there is a certificate, it takes precendence over the codebase.
  if (mCert) {
    *aValue = HashString(mCert->fingerprint);
  }
  else {
    *aValue = nsScriptSecurityManager::HashPrincipalByOrigin(this);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::GetDomain(nsIURI** aDomain)
{
  if (!mDomain) {
    *aDomain = nullptr;
    return NS_OK;
  }

  if (mDomainImmutable) {
    NS_ADDREF(*aDomain = mDomain);
    return NS_OK;
  }

  return NS_EnsureSafeToReturn(mDomain, aDomain);
}

NS_IMETHODIMP
nsPrincipal::SetDomain(nsIURI* aDomain)
{
  mDomain = NS_TryToMakeImmutable(aDomain);
  mDomainImmutable = URIIsImmutable(mDomain);
  
  // Domain has changed, forget cached security policy
  SetSecurityPolicy(nullptr);

  // Recompute all wrappers between compartments using this principal and other
  // non-chrome compartments.
  JSContext *cx = nsContentUtils::GetSafeJSContext();
  NS_ENSURE_TRUE(cx, NS_ERROR_FAILURE);
  JSPrincipals *principals = nsJSPrincipals::get(static_cast<nsIPrincipal*>(this));
  bool success = js::RecomputeWrappers(cx, js::ContentCompartmentsOnly(),
                                       js::CompartmentsWithPrincipals(principals));
  NS_ENSURE_TRUE(success, NS_ERROR_FAILURE);
  success = js::RecomputeWrappers(cx, js::CompartmentsWithPrincipals(principals),
                                  js::ContentCompartmentsOnly());
  NS_ENSURE_TRUE(success, NS_ERROR_FAILURE);

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
                                bool aIsCert,
                                bool aTrusted,
                                PRUint32 aAppId,
                                bool aInMozBrowser)
{
  NS_PRECONDITION(!mCapabilities || mCapabilities->Count() == 0,
                  "mCapabilities was already initialized?");
  NS_PRECONDITION(mAnnotations.Length() == 0,
                  "mAnnotations was already initialized?");
  NS_PRECONDITION(!mInitialized, "We were already initialized?");

  mInitialized = true;

  mAppId = aAppId;
  mInMozBrowser = aInMozBrowser;

  nsresult rv;
  if (aIsCert) {
    rv = SetCertificate(aToken, aSubjectName, aPrettyName, aCert);
    
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  else {
    rv = NS_NewURI(getter_AddRefs(mCodebase), aToken, nullptr);
    if (NS_FAILED(rv)) {
      NS_ERROR("Malformed URI in capability.principal preference.");
      return rv;
    }

    NS_TryToSetImmutable(mCodebase);
    mCodebaseImmutable = URIIsImmutable(mCodebase);

    mTrusted = aTrusted;
  }

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

NS_IMETHODIMP
nsPrincipal::GetExtendedOrigin(nsACString& aExtendedOrigin)
{
  MOZ_ASSERT(mAppId != nsIScriptSecurityManager::UNKNOWN_APP_ID);

  mozilla::GetExtendedOrigin(mCodebase, mAppId, mInMozBrowser, aExtendedOrigin);
  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::GetAppStatus(PRUint16* aAppStatus)
{
  MOZ_ASSERT(mAppId != nsIScriptSecurityManager::UNKNOWN_APP_ID);

  *aAppStatus = GetAppStatus();
  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::GetAppId(PRUint32* aAppId)
{
  if (mAppId == nsIScriptSecurityManager::UNKNOWN_APP_ID) {
    MOZ_ASSERT(false);
    *aAppId = nsIScriptSecurityManager::NO_APP_ID;
    return NS_OK;
  }

  *aAppId = mAppId;
  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::GetIsInBrowserElement(bool* aIsInBrowserElement)
{
  *aIsInBrowserElement = mInMozBrowser;
  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::Read(nsIObjectInputStream* aStream)
{
  bool hasCapabilities;
  nsresult rv = aStream->ReadBoolean(&hasCapabilities);
  if (NS_SUCCEEDED(rv) && hasCapabilities) {
    mCapabilities = new nsHashtable(aStream, ReadAnnotationEntry,
                                    FreeAnnotationEntry, &rv);
    NS_ENSURE_TRUE(mCapabilities, NS_ERROR_OUT_OF_MEMORY);
  }

  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = NS_ReadOptionalCString(aStream, mPrefName);
  if (NS_FAILED(rv)) {
    return rv;
  }

  const char* ordinalBegin = PL_strpbrk(mPrefName.get(), "1234567890");
  if (ordinalBegin) {
    PRIntn n = atoi(ordinalBegin);
    if (sCapabilitiesOrdinal <= n) {
      sCapabilitiesOrdinal = n + 1;
    }
  }

  bool haveCert;
  rv = aStream->ReadBoolean(&haveCert);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCString fingerprint;
  nsCString subjectName;
  nsCString prettyName;
  nsCOMPtr<nsISupports> cert;
  if (haveCert) {
    rv = NS_ReadOptionalCString(aStream, fingerprint);
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = NS_ReadOptionalCString(aStream, subjectName);
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = NS_ReadOptionalCString(aStream, prettyName);
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = aStream->ReadObject(true, getter_AddRefs(cert));
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  nsCOMPtr<nsIURI> codebase;
  rv = NS_ReadOptionalObject(aStream, true, getter_AddRefs(codebase));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIURI> domain;
  rv = NS_ReadOptionalObject(aStream, true, getter_AddRefs(domain));
  if (NS_FAILED(rv)) {
    return rv;
  }

  PRUint32 appId;
  rv = aStream->Read32(&appId);
  NS_ENSURE_SUCCESS(rv, rv);

  bool inMozBrowser;
  rv = aStream->ReadBoolean(&inMozBrowser);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = Init(fingerprint, subjectName, prettyName, cert, codebase, appId, inMozBrowser);
  NS_ENSURE_SUCCESS(rv, rv);

  SetDomain(domain);

  rv = aStream->ReadBoolean(&mTrusted);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

static nsresult
WriteScalarValue(nsIObjectOutputStream* aStream, void* aData)
{
  PRUint32 value = NS_PTR_TO_INT32(aData);

  return aStream->Write32(value);
}

NS_IMETHODIMP
nsPrincipal::Write(nsIObjectOutputStream* aStream)
{
  NS_ENSURE_STATE(mCert || mCodebase);
  
  // mAnnotations is transient data associated to specific JS stack frames.  We
  // don't want to serialize that.
  
  bool hasCapabilities = (mCapabilities && mCapabilities->Count() > 0);
  nsresult rv = aStream->WriteBoolean(hasCapabilities);
  if (NS_SUCCEEDED(rv) && hasCapabilities) {
    rv = mCapabilities->Write(aStream, WriteScalarValue);
  }

  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = NS_WriteOptionalStringZ(aStream, mPrefName.get());
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = aStream->WriteBoolean(mCert != nullptr);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (mCert) {
    NS_ENSURE_STATE(mCert->cert);
    
    rv = NS_WriteOptionalStringZ(aStream, mCert->fingerprint.get());
    if (NS_FAILED(rv)) {
      return rv;
    }
    
    rv = NS_WriteOptionalStringZ(aStream, mCert->subjectName.get());
    if (NS_FAILED(rv)) {
      return rv;
    }
    
    rv = NS_WriteOptionalStringZ(aStream, mCert->prettyName.get());
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = aStream->WriteCompoundObject(mCert->cert, NS_GET_IID(nsISupports),
                                      true);
    if (NS_FAILED(rv)) {
      return rv;
    }    
  }
  
  // mSecurityPolicy is an optimization; it'll get looked up again as needed.
  // Don't bother saving and restoring it, esp. since it might change if
  // preferences change.

  rv = NS_WriteOptionalCompoundObject(aStream, mCodebase, NS_GET_IID(nsIURI),
                                      true);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = NS_WriteOptionalCompoundObject(aStream, mDomain, NS_GET_IID(nsIURI),
                                      true);
  if (NS_FAILED(rv)) {
    return rv;
  }

  aStream->Write32(mAppId);
  aStream->WriteBoolean(mInMozBrowser);

  rv = aStream->Write8(mTrusted);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // mCodebaseImmutable and mDomainImmutable will be recomputed based
  // on the deserialized URIs in Read().

  return NS_OK;
}

PRUint16
nsPrincipal::GetAppStatus()
{
  MOZ_ASSERT(mAppId != nsIScriptSecurityManager::UNKNOWN_APP_ID);

  // Installed apps have a valid app id (not NO_APP_ID or UNKNOWN_APP_ID)
  // and they are not inside a mozbrowser.
  return mAppId != nsIScriptSecurityManager::NO_APP_ID &&
         mAppId != nsIScriptSecurityManager::UNKNOWN_APP_ID && !mInMozBrowser
          ? nsIPrincipal::APP_STATUS_INSTALLED
          : nsIPrincipal::APP_STATUS_NOT_INSTALLED;
}

/************************************************************************************************************************/

static const char EXPANDED_PRINCIPAL_SPEC[] = "[Expanded Principal]";

NS_IMPL_CLASSINFO(nsExpandedPrincipal, NULL, nsIClassInfo::MAIN_THREAD_ONLY,
                  NS_EXPANDEDPRINCIPAL_CID)
NS_IMPL_QUERY_INTERFACE2_CI(nsExpandedPrincipal,
                            nsIPrincipal,
                            nsIExpandedPrincipal)
NS_IMPL_CI_INTERFACE_GETTER2(nsExpandedPrincipal,
                             nsIPrincipal,
                             nsIExpandedPrincipal)
NS_IMPL_ADDREF_INHERITED(nsExpandedPrincipal, nsBasePrincipal)
NS_IMPL_RELEASE_INHERITED(nsExpandedPrincipal, nsBasePrincipal)

nsExpandedPrincipal::nsExpandedPrincipal(nsTArray<nsCOMPtr <nsIPrincipal> > &aWhiteList)
{
  mPrincipals.AppendElements(aWhiteList);
}

nsExpandedPrincipal::~nsExpandedPrincipal()
{ }

NS_IMETHODIMP 
nsExpandedPrincipal::GetDomain(nsIURI** aDomain)
{
  *aDomain = nullptr;
  return NS_OK;
}

NS_IMETHODIMP 
nsExpandedPrincipal::SetDomain(nsIURI* aDomain)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsExpandedPrincipal::GetOrigin(char** aOrigin)
{
  *aOrigin = ToNewCString(NS_LITERAL_CSTRING(EXPANDED_PRINCIPAL_SPEC));
  return *aOrigin ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

typedef nsresult (NS_STDCALL nsIPrincipal::*nsIPrincipalMemFn)(nsIPrincipal* aOther,
                                                               bool* aResult);
#define CALL_MEMBER_FUNCTION(THIS,MEM_FN)  ((THIS)->*(MEM_FN))

// nsExpandedPrincipal::Equals and nsExpandedPrincipal::EqualsIgnoringDomain
// shares the same logic. The difference only that Equals requires 'this' 
// and 'aOther' to Subsume each other while EqualsIgnoringDomain requires 
// bidirectional SubsumesIgnoringDomain.
static nsresult 
Equals(nsExpandedPrincipal* aThis, nsIPrincipalMemFn aFn, nsIPrincipal* aOther,
       bool* aResult)
{
  // If (and only if) 'aThis' and 'aOther' both Subsume/SubsumesIgnoringDomain
  // each other, then they are Equal.
  *aResult = false;
  // Calling the corresponding subsume function on this (aFn).
  nsresult rv = CALL_MEMBER_FUNCTION(aThis, aFn)(aOther, aResult);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!*aResult)
    return NS_OK;

  // Calling the corresponding subsume function on aOther (aFn).
  rv = CALL_MEMBER_FUNCTION(aOther, aFn)(aThis, aResult);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

NS_IMETHODIMP
nsExpandedPrincipal::Equals(nsIPrincipal* aOther, bool* aResult)
{
  return ::Equals(this, &nsIPrincipal::Subsumes, aOther, aResult);
}

NS_IMETHODIMP
nsExpandedPrincipal::EqualsIgnoringDomain(nsIPrincipal* aOther, bool* aResult)
{
  return ::Equals(this, &nsIPrincipal::SubsumesIgnoringDomain, aOther, aResult);
}

// nsExpandedPrincipal::Subsumes and nsExpandedPrincipal::SubsumesIgnoringDomain
// shares the same logic. The difference only that Subsumes calls are replaced
//with SubsumesIgnoringDomain calls in the second case.
static nsresult 
Subsumes(nsExpandedPrincipal* aThis, nsIPrincipalMemFn aFn, nsIPrincipal* aOther, 
         bool* aResult)
{
  nsresult rv;
  nsCOMPtr<nsIExpandedPrincipal> expanded = do_QueryInterface(aOther);  
  if (expanded) {
    // If aOther is an ExpandedPrincipal too, check if all of its 
    // principals are subsumed.
    nsTArray< nsCOMPtr<nsIPrincipal> >* otherList;
    expanded->GetWhiteList(&otherList);
    for (uint32_t i = 0; i < otherList->Length(); ++i){
      rv = CALL_MEMBER_FUNCTION(aThis, aFn)((*otherList)[i], aResult);
      NS_ENSURE_SUCCESS(rv, rv);
      if (!*aResult) {
        // If we don't subsume at least one principal of aOther, return false.
        return NS_OK;    
      }
    }
  } else {
    // For a regular aOther, one of our principals must subsume it.
    nsTArray< nsCOMPtr<nsIPrincipal> >* list;
    aThis->GetWhiteList(&list);
    for (uint32_t i = 0; i < list->Length(); ++i){
      rv = CALL_MEMBER_FUNCTION((*list)[i], aFn)(aOther, aResult);
      NS_ENSURE_SUCCESS(rv, rv);
      if (*aResult) {
        // If one of our principal subsumes it, return true.
        return NS_OK;
      }
    }
  }
  return NS_OK;
}

#undef CALL_MEMBER_FUNCTION

NS_IMETHODIMP
nsExpandedPrincipal::Subsumes(nsIPrincipal* aOther, bool* aResult)
{
  return ::Subsumes(this, &nsIPrincipal::Subsumes, aOther, aResult);
}

NS_IMETHODIMP
nsExpandedPrincipal::SubsumesIgnoringDomain(nsIPrincipal* aOther, bool* aResult)
{
  return ::Subsumes(this, &nsIPrincipal::SubsumesIgnoringDomain, aOther, aResult);
}

NS_IMETHODIMP
nsExpandedPrincipal::CheckMayLoad(nsIURI* uri, bool aReport)
{
  nsresult rv;
  for (uint32_t i = 0; i < mPrincipals.Length(); ++i){
    rv = mPrincipals[i]->CheckMayLoad(uri, aReport);
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  return NS_ERROR_DOM_BAD_URI;
}

NS_IMETHODIMP
nsExpandedPrincipal::GetHashValue(PRUint32* result)
{
  MOZ_NOT_REACHED("extended principal should never be used as key in a hash map");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsExpandedPrincipal::GetURI(nsIURI** aURI)
{
  *aURI = nullptr;
  return NS_OK;
}

NS_IMETHODIMP 
nsExpandedPrincipal::GetWhiteList(nsTArray<nsCOMPtr<nsIPrincipal> >** aWhiteList)
{
  *aWhiteList = &mPrincipals;
  return NS_OK;
}

NS_IMETHODIMP
nsExpandedPrincipal::GetExtendedOrigin(nsACString& aExtendedOrigin)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsExpandedPrincipal::GetAppStatus(PRUint16* aAppStatus)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsExpandedPrincipal::GetAppId(PRUint32* aAppId)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsExpandedPrincipal::GetIsInBrowserElement(bool* aIsInBrowserElement)
{
  return NS_ERROR_NOT_AVAILABLE;
}

void
nsExpandedPrincipal::GetScriptLocation(nsACString& aStr)
{
  if (mCert) {
    aStr.Assign(mCert->fingerprint);
  } else {
    // Is that a good idea to list it's principals?
    aStr.Assign(EXPANDED_PRINCIPAL_SPEC);
  }
}

#ifdef DEBUG
void nsExpandedPrincipal::dumpImpl()
{
  fprintf(stderr, "nsExpandedPrincipal (%p)\n", static_cast<void*>(this));
}
#endif 

//////////////////////////////////////////
// Methods implementing nsISerializable //
//////////////////////////////////////////

NS_IMETHODIMP
nsExpandedPrincipal::Read(nsIObjectInputStream* aStream)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsExpandedPrincipal::Write(nsIObjectOutputStream* aStream)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
