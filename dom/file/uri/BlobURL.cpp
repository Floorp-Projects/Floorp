/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"

#include "mozilla/dom/BlobURL.h"
#include "mozilla/dom/BlobURLProtocolHandler.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/URIUtils.h"

using namespace mozilla::dom;

static NS_DEFINE_CID(kHOSTOBJECTURICID, NS_HOSTOBJECTURI_CID);

static NS_DEFINE_CID(kThisSimpleURIImplementationCID,
                     NS_THIS_SIMPLEURI_IMPLEMENTATION_CID);

NS_IMPL_ADDREF_INHERITED(BlobURL, mozilla::net::nsSimpleURI)
NS_IMPL_RELEASE_INHERITED(BlobURL, mozilla::net::nsSimpleURI)

NS_INTERFACE_MAP_BEGIN(BlobURL)
  NS_INTERFACE_MAP_ENTRY(nsIURIWithPrincipal)
  if (aIID.Equals(kHOSTOBJECTURICID))
    foundInterface = static_cast<nsIURI*>(this);
  else if (aIID.Equals(kThisSimpleURIImplementationCID)) {
    // Need to return explicitly here, because if we just set foundInterface
    // to null the NS_INTERFACE_MAP_END_INHERITING will end up calling into
    // nsSimplURI::QueryInterface and finding something for this CID.
    *aInstancePtr = nullptr;
    return NS_NOINTERFACE;
  }
  else
NS_INTERFACE_MAP_END_INHERITING(mozilla::net::nsSimpleURI)

// nsIURIWithPrincipal methods:

NS_IMETHODIMP
BlobURL::GetPrincipal(nsIPrincipal** aPrincipal)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIPrincipal> principal = mPrincipal.get();
  principal.forget(aPrincipal);

  return NS_OK;
}

NS_IMETHODIMP
BlobURL::GetPrincipalUri(nsIURI** aUri)
{
  if (mPrincipal) {
    mPrincipal->GetURI(aUri);
  }
  else {
    *aUri = nullptr;
  }

  return NS_OK;
}

// nsISerializable methods:

NS_IMETHODIMP
BlobURL::Read(nsIObjectInputStream* aStream)
{
  MOZ_ASSERT_UNREACHABLE("Use nsIURIMutator.read() instead");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
BlobURL::ReadPrivate(nsIObjectInputStream *aStream)
{
  nsresult rv = mozilla::net::nsSimpleURI::ReadPrivate(aStream);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupports> supports;
  rv = NS_ReadOptionalObject(aStream, true, getter_AddRefs(supports));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrincipal> principal = do_QueryInterface(supports, &rv);
  mPrincipal = new nsMainThreadPtrHolder<nsIPrincipal>("nsIPrincipal", principal, false);
  return rv;
}

NS_IMETHODIMP
BlobURL::Write(nsIObjectOutputStream* aStream)
{
  nsresult rv = mozilla::net::nsSimpleURI::Write(aStream);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrincipal> principal = mPrincipal.get();
  return NS_WriteOptionalCompoundObject(aStream, principal,
                                        NS_GET_IID(nsIPrincipal),
                                        true);
}

// nsIIPCSerializableURI methods:
void
BlobURL::Serialize(mozilla::ipc::URIParams& aParams)
{
  using namespace mozilla::ipc;

  HostObjectURIParams hostParams;
  URIParams simpleParams;

  mozilla::net::nsSimpleURI::Serialize(simpleParams);
  hostParams.simpleParams() = simpleParams;

  nsCOMPtr<nsIPrincipal> principal = mPrincipal.get();
  if (principal) {
    PrincipalInfo info;
    nsresult rv = PrincipalToPrincipalInfo(principal, &info);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    hostParams.principal() = info;
  } else {
    hostParams.principal() = mozilla::void_t();
  }

  aParams = hostParams;
}

bool
BlobURL::Deserialize(const mozilla::ipc::URIParams& aParams)
{
  using namespace mozilla::ipc;

  if (aParams.type() != URIParams::THostObjectURIParams) {
      NS_ERROR("Received unknown parameters from the other process!");
      return false;
  }

  const HostObjectURIParams& hostParams = aParams.get_HostObjectURIParams();

  if (!mozilla::net::nsSimpleURI::Deserialize(hostParams.simpleParams())) {
    return false;
  }

  if (hostParams.principal().type() == OptionalPrincipalInfo::Tvoid_t) {
    return true;
  }

  nsCOMPtr<nsIPrincipal> principal = PrincipalInfoToPrincipal(hostParams.principal().get_PrincipalInfo());
  if (!principal) {
    return false;
  }
  mPrincipal = new nsMainThreadPtrHolder<nsIPrincipal>("nsIPrincipal", principal, false);

  return true;
}

nsresult
BlobURL::SetScheme(const nsACString& aScheme)
{
  // Disallow setting the scheme, since that could cause us to be associated
  // with a different protocol handler that doesn't expect us to be carrying
  // around a principal with nsIURIWithPrincipal.
  return NS_ERROR_FAILURE;
}

// nsIURI methods:
nsresult
BlobURL::CloneInternal(mozilla::net::nsSimpleURI::RefHandlingEnum aRefHandlingMode,
                       const nsACString& newRef,
                       nsIURI** aClone)
{
  nsCOMPtr<nsIURI> simpleClone;
  nsresult rv =
    mozilla::net::nsSimpleURI::CloneInternal(aRefHandlingMode, newRef, getter_AddRefs(simpleClone));
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
  RefPtr<BlobURL> uriCheck;
  rv = simpleClone->QueryInterface(kHOSTOBJECTURICID, getter_AddRefs(uriCheck));
  MOZ_ASSERT(NS_SUCCEEDED(rv) && uriCheck);
#endif

  BlobURL* u = static_cast<BlobURL*>(simpleClone.get());

  u->mPrincipal = mPrincipal;

  simpleClone.forget(aClone);
  return NS_OK;
}

/* virtual */ nsresult
BlobURL::EqualsInternal(nsIURI* aOther,
                        mozilla::net::nsSimpleURI::RefHandlingEnum aRefHandlingMode,
                        bool* aResult)
{
  if (!aOther) {
    *aResult = false;
    return NS_OK;
  }

  RefPtr<BlobURL> otherUri;
  aOther->QueryInterface(kHOSTOBJECTURICID, getter_AddRefs(otherUri));
  if (!otherUri) {
    *aResult = false;
    return NS_OK;
  }

  // Compare the member data that our base class knows about.
  if (!mozilla::net::nsSimpleURI::EqualsInternal(otherUri, aRefHandlingMode)) {
    *aResult = false;
    return NS_OK;
  }

  if (mPrincipal && otherUri->mPrincipal) {
    // Both of us have mPrincipals. Compare them.
    return mPrincipal->Equals(otherUri->mPrincipal, aResult);
  }
  // else, at least one of us lacks a principal; only equal if *both* lack it.
  *aResult = (!mPrincipal && !otherUri->mPrincipal);
  return NS_OK;
}

// Queries this list of interfaces. If none match, it queries mURI.
NS_IMPL_NSIURIMUTATOR_ISUPPORTS(BlobURL::Mutator,
                                nsIURISetters,
                                nsIURIMutator,
                                nsIPrincipalURIMutator,
                                nsISerializable)

NS_IMETHODIMP
BlobURL::Mutate(nsIURIMutator** aMutator)
{
    RefPtr<BlobURL::Mutator> mutator = new BlobURL::Mutator();
    nsresult rv = mutator->InitFromURI(this);
    if (NS_FAILED(rv)) {
        return rv;
    }
    mutator.forget(aMutator);
    return NS_OK;
}

// nsIClassInfo methods:
NS_IMETHODIMP
BlobURL::GetInterfaces(uint32_t *count, nsIID * **array)
{
  *count = 0;
  *array = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
BlobURL::GetScriptableHelper(nsIXPCScriptable **_retval)
{
  *_retval = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
BlobURL::GetContractID(nsACString& aContractID)
{
  // Make sure to modify any subclasses as needed if this ever
  // changes.
  aContractID.SetIsVoid(true);
  return NS_OK;
}

NS_IMETHODIMP
BlobURL::GetClassDescription(nsACString& aClassDescription)
{
  aClassDescription.SetIsVoid(true);
  return NS_OK;
}

NS_IMETHODIMP
BlobURL::GetClassID(nsCID * *aClassID)
{
  // Make sure to modify any subclasses as needed if this ever
  // changes to not call the virtual GetClassIDNoAlloc.
  *aClassID = (nsCID*) moz_xmalloc(sizeof(nsCID));
  NS_ENSURE_TRUE(*aClassID, NS_ERROR_OUT_OF_MEMORY);

  return GetClassIDNoAlloc(*aClassID);
}

NS_IMETHODIMP
BlobURL::GetFlags(uint32_t *aFlags)
{
  *aFlags = nsIClassInfo::MAIN_THREAD_ONLY;
  return NS_OK;
}

NS_IMETHODIMP
BlobURL::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
  *aClassIDNoAlloc = kHOSTOBJECTURICID;
  return NS_OK;
}
