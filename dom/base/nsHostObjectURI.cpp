/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHostObjectURI.h"

#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"

#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/URIUtils.h"

static NS_DEFINE_CID(kHOSTOBJECTURICID, NS_HOSTOBJECTURI_CID);

static NS_DEFINE_CID(kThisSimpleURIImplementationCID,
                     NS_THIS_SIMPLEURI_IMPLEMENTATION_CID);

NS_IMPL_ADDREF_INHERITED(nsHostObjectURI, mozilla::net::nsSimpleURI)
NS_IMPL_RELEASE_INHERITED(nsHostObjectURI, mozilla::net::nsSimpleURI)

NS_INTERFACE_MAP_BEGIN(nsHostObjectURI)
  NS_INTERFACE_MAP_ENTRY(nsIURIWithBlobImpl)
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

// nsIURIWithBlobImpl methods:

NS_IMETHODIMP
nsHostObjectURI::GetBlobImpl(nsISupports** aBlobImpl)
{
  RefPtr<BlobImpl> blobImpl(mBlobImpl);
  blobImpl.forget(aBlobImpl);
  return NS_OK;
}

// nsIURIWithPrincipal methods:

NS_IMETHODIMP
nsHostObjectURI::GetPrincipal(nsIPrincipal** aPrincipal)
{
  NS_IF_ADDREF(*aPrincipal = mPrincipal);

  return NS_OK;
}

NS_IMETHODIMP
nsHostObjectURI::GetPrincipalUri(nsIURI** aUri)
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
nsHostObjectURI::Read(nsIObjectInputStream* aStream)
{
  nsresult rv = mozilla::net::nsSimpleURI::Read(aStream);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupports> supports;
  rv = NS_ReadOptionalObject(aStream, true, getter_AddRefs(supports));
  NS_ENSURE_SUCCESS(rv, rv);

  mPrincipal = do_QueryInterface(supports, &rv);
  return rv;
}

NS_IMETHODIMP
nsHostObjectURI::Write(nsIObjectOutputStream* aStream)
{
  nsresult rv = mozilla::net::nsSimpleURI::Write(aStream);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_WriteOptionalCompoundObject(aStream, mPrincipal,
                                        NS_GET_IID(nsIPrincipal),
                                        true);
}

// nsIIPCSerializableURI methods:
void
nsHostObjectURI::Serialize(mozilla::ipc::URIParams& aParams)
{
  using namespace mozilla::ipc;

  HostObjectURIParams hostParams;
  URIParams simpleParams;

  mozilla::net::nsSimpleURI::Serialize(simpleParams);
  hostParams.simpleParams() = simpleParams;

  if (mPrincipal) {
    PrincipalInfo info;
    nsresult rv = PrincipalToPrincipalInfo(mPrincipal, &info);
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
nsHostObjectURI::Deserialize(const mozilla::ipc::URIParams& aParams)
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

  // XXXbaku: when we will have shared blobURL maps, we can populate mBlobImpl
  // here asll well.

  if (hostParams.principal().type() == OptionalPrincipalInfo::Tvoid_t) {
    return true;
  }

  mPrincipal = PrincipalInfoToPrincipal(hostParams.principal().get_PrincipalInfo());
  return mPrincipal != nullptr;
}

NS_IMETHODIMP
nsHostObjectURI::SetScheme(const nsACString& aScheme)
{
  // Disallow setting the scheme, since that could cause us to be associated
  // with a different protocol handler that doesn't expect us to be carrying
  // around a principal with nsIURIWithPrincipal.
  return NS_ERROR_FAILURE;
}

// nsIURI methods:
nsresult
nsHostObjectURI::CloneInternal(mozilla::net::nsSimpleURI::RefHandlingEnum aRefHandlingMode,
                               nsIURI** aClone)
{
  nsCOMPtr<nsIURI> simpleClone;
  nsresult rv =
    mozilla::net::nsSimpleURI::CloneInternal(aRefHandlingMode, getter_AddRefs(simpleClone));
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
  RefPtr<nsHostObjectURI> uriCheck;
  rv = simpleClone->QueryInterface(kHOSTOBJECTURICID, getter_AddRefs(uriCheck));
  MOZ_ASSERT(NS_SUCCEEDED(rv) && uriCheck);
#endif

  nsHostObjectURI* u = static_cast<nsHostObjectURI*>(simpleClone.get());

  u->mPrincipal = mPrincipal;
  u->mBlobImpl = mBlobImpl;

  simpleClone.forget(aClone);
  return NS_OK;
}

/* virtual */ nsresult
nsHostObjectURI::EqualsInternal(nsIURI* aOther,
                                mozilla::net::nsSimpleURI::RefHandlingEnum aRefHandlingMode,
                                bool* aResult)
{
  if (!aOther) {
    *aResult = false;
    return NS_OK;
  }
  
  RefPtr<nsHostObjectURI> otherUri;
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

  // Compare the piece of additional member data that we add to base class,
  // but we cannot compare BlobImpl. This should not be a problem, because we
  // don't support changing the underlying mBlobImpl.

  if (mPrincipal && otherUri->mPrincipal) {
    // Both of us have mPrincipals. Compare them.
    return mPrincipal->Equals(otherUri->mPrincipal, aResult);
  }
  // else, at least one of us lacks a principal; only equal if *both* lack it.
  *aResult = (!mPrincipal && !otherUri->mPrincipal);
  return NS_OK;
}

// nsIClassInfo methods:
NS_IMETHODIMP 
nsHostObjectURI::GetInterfaces(uint32_t *count, nsIID * **array)
{
  *count = 0;
  *array = nullptr;
  return NS_OK;
}

NS_IMETHODIMP 
nsHostObjectURI::GetScriptableHelper(nsIXPCScriptable **_retval)
{
  *_retval = nullptr;
  return NS_OK;
}

NS_IMETHODIMP 
nsHostObjectURI::GetContractID(char * *aContractID)
{
  // Make sure to modify any subclasses as needed if this ever
  // changes.
  *aContractID = nullptr;
  return NS_OK;
}

NS_IMETHODIMP 
nsHostObjectURI::GetClassDescription(char * *aClassDescription)
{
  *aClassDescription = nullptr;
  return NS_OK;
}

NS_IMETHODIMP 
nsHostObjectURI::GetClassID(nsCID * *aClassID)
{
  // Make sure to modify any subclasses as needed if this ever
  // changes to not call the virtual GetClassIDNoAlloc.
  *aClassID = (nsCID*) moz_xmalloc(sizeof(nsCID));
  NS_ENSURE_TRUE(*aClassID, NS_ERROR_OUT_OF_MEMORY);

  return GetClassIDNoAlloc(*aClassID);
}

NS_IMETHODIMP 
nsHostObjectURI::GetFlags(uint32_t *aFlags)
{
  *aFlags = nsIClassInfo::MAIN_THREAD_ONLY;
  return NS_OK;
}

NS_IMETHODIMP 
nsHostObjectURI::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
  *aClassIDNoAlloc = kHOSTOBJECTURICID;
  return NS_OK;
}
