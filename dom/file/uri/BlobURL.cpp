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
  if (aIID.Equals(kHOSTOBJECTURICID))
    foundInterface = static_cast<nsIURI*>(this);
  else if (aIID.Equals(kThisSimpleURIImplementationCID)) {
    // Need to return explicitly here, because if we just set foundInterface
    // to null the NS_INTERFACE_MAP_END_INHERITING will end up calling into
    // nsSimplURI::QueryInterface and finding something for this CID.
    *aInstancePtr = nullptr;
    return NS_NOINTERFACE;
  } else
NS_INTERFACE_MAP_END_INHERITING(mozilla::net::nsSimpleURI)

BlobURL::BlobURL() : mRevoked(false) {}

// nsISerializable methods:

NS_IMETHODIMP
BlobURL::Read(nsIObjectInputStream* aStream) {
  MOZ_ASSERT_UNREACHABLE("Use nsIURIMutator.read() instead");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult BlobURL::ReadPrivate(nsIObjectInputStream* aStream) {
  nsresult rv = mozilla::net::nsSimpleURI::ReadPrivate(aStream);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStream->ReadBoolean(&mRevoked);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
BlobURL::Write(nsIObjectOutputStream* aStream) {
  nsresult rv = mozilla::net::nsSimpleURI::Write(aStream);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStream->WriteBoolean(mRevoked);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP_(void)
BlobURL::Serialize(mozilla::ipc::URIParams& aParams) {
  using namespace mozilla::ipc;

  HostObjectURIParams hostParams;
  URIParams simpleParams;

  mozilla::net::nsSimpleURI::Serialize(simpleParams);
  hostParams.simpleParams() = simpleParams;

  hostParams.revoked() = mRevoked;

  aParams = hostParams;
}

bool BlobURL::Deserialize(const mozilla::ipc::URIParams& aParams) {
  using namespace mozilla::ipc;

  if (aParams.type() != URIParams::THostObjectURIParams) {
    NS_ERROR("Received unknown parameters from the other process!");
    return false;
  }

  const HostObjectURIParams& hostParams = aParams.get_HostObjectURIParams();

  if (!mozilla::net::nsSimpleURI::Deserialize(hostParams.simpleParams())) {
    return false;
  }

  mRevoked = hostParams.revoked();
  return true;
}

nsresult BlobURL::SetScheme(const nsACString& aScheme) {
  // Disallow setting the scheme, since that could cause us to be associated
  // with a different protocol handler.
  return NS_ERROR_FAILURE;
}

// nsIURI methods:
nsresult BlobURL::CloneInternal(
    mozilla::net::nsSimpleURI::RefHandlingEnum aRefHandlingMode,
    const nsACString& newRef, nsIURI** aClone) {
  nsCOMPtr<nsIURI> simpleClone;
  nsresult rv = mozilla::net::nsSimpleURI::CloneInternal(
      aRefHandlingMode, newRef, getter_AddRefs(simpleClone));
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
  RefPtr<BlobURL> uriCheck;
  rv = simpleClone->QueryInterface(kHOSTOBJECTURICID, getter_AddRefs(uriCheck));
  MOZ_ASSERT(NS_SUCCEEDED(rv) && uriCheck);
#endif

  BlobURL* u = static_cast<BlobURL*>(simpleClone.get());
  u->mRevoked = mRevoked;

  simpleClone.forget(aClone);
  return NS_OK;
}

/* virtual */
nsresult BlobURL::EqualsInternal(
    nsIURI* aOther, mozilla::net::nsSimpleURI::RefHandlingEnum aRefHandlingMode,
    bool* aResult) {
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
  *aResult =
      mozilla::net::nsSimpleURI::EqualsInternal(otherUri, aRefHandlingMode);

  // We don't want to compare the revoked flag.
  return NS_OK;
}

// Queries this list of interfaces. If none match, it queries mURI.
NS_IMPL_NSIURIMUTATOR_ISUPPORTS(BlobURL::Mutator, nsIURISetters, nsIURIMutator,
                                nsISerializable, nsIBlobURLMutator)

NS_IMETHODIMP
BlobURL::Mutate(nsIURIMutator** aMutator) {
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
BlobURL::GetInterfaces(nsTArray<nsIID>& array) {
  array.Clear();
  return NS_OK;
}

NS_IMETHODIMP
BlobURL::GetScriptableHelper(nsIXPCScriptable** _retval) {
  *_retval = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
BlobURL::GetContractID(nsACString& aContractID) {
  // Make sure to modify any subclasses as needed if this ever
  // changes.
  aContractID.SetIsVoid(true);
  return NS_OK;
}

NS_IMETHODIMP
BlobURL::GetClassDescription(nsACString& aClassDescription) {
  aClassDescription.SetIsVoid(true);
  return NS_OK;
}

NS_IMETHODIMP
BlobURL::GetClassID(nsCID** aClassID) {
  // Make sure to modify any subclasses as needed if this ever
  // changes to not call the virtual GetClassIDNoAlloc.
  *aClassID = (nsCID*)moz_xmalloc(sizeof(nsCID));

  return GetClassIDNoAlloc(*aClassID);
}

NS_IMETHODIMP
BlobURL::GetFlags(uint32_t* aFlags) {
  *aFlags = nsIClassInfo::MAIN_THREAD_ONLY;
  return NS_OK;
}

NS_IMETHODIMP
BlobURL::GetClassIDNoAlloc(nsCID* aClassIDNoAlloc) {
  *aClassIDNoAlloc = kHOSTOBJECTURICID;
  return NS_OK;
}
