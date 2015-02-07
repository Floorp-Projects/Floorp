/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHostObjectURI.h"

#include "nsAutoPtr.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIProgrammingLanguage.h"

static NS_DEFINE_CID(kHOSTOBJECTURICID, NS_HOSTOBJECTURI_CID);

static NS_DEFINE_CID(kThisSimpleURIImplementationCID,
                     NS_THIS_SIMPLEURI_IMPLEMENTATION_CID);

NS_IMPL_ADDREF_INHERITED(nsHostObjectURI, nsSimpleURI)
NS_IMPL_RELEASE_INHERITED(nsHostObjectURI, nsSimpleURI)

NS_INTERFACE_MAP_BEGIN(nsHostObjectURI)
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
NS_INTERFACE_MAP_END_INHERITING(nsSimpleURI)

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
  nsresult rv = nsSimpleURI::Read(aStream);
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
  nsresult rv = nsSimpleURI::Write(aStream);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_WriteOptionalCompoundObject(aStream, mPrincipal,
                                        NS_GET_IID(nsIPrincipal),
                                        true);
}

// nsIURI methods:
nsresult
nsHostObjectURI::CloneInternal(nsSimpleURI::RefHandlingEnum aRefHandlingMode,
                               nsIURI** aClone)
{
  nsCOMPtr<nsIURI> simpleClone;
  nsresult rv =
    nsSimpleURI::CloneInternal(aRefHandlingMode, getter_AddRefs(simpleClone));
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
  nsRefPtr<nsHostObjectURI> uriCheck;
  rv = simpleClone->QueryInterface(kHOSTOBJECTURICID, getter_AddRefs(uriCheck));
  NS_ABORT_IF_FALSE(NS_SUCCEEDED(rv) && uriCheck,
		    "Unexpected!");
#endif

  nsHostObjectURI* u = static_cast<nsHostObjectURI*>(simpleClone.get());

  u->mPrincipal = mPrincipal;

  simpleClone.forget(aClone);
  return NS_OK;
}

/* virtual */ nsresult
nsHostObjectURI::EqualsInternal(nsIURI* aOther,
                                nsSimpleURI::RefHandlingEnum aRefHandlingMode,
                                bool* aResult)
{
  if (!aOther) {
    *aResult = false;
    return NS_OK;
  }
  
  nsRefPtr<nsHostObjectURI> otherUri;
  aOther->QueryInterface(kHOSTOBJECTURICID, getter_AddRefs(otherUri));
  if (!otherUri) {
    *aResult = false;
    return NS_OK;
  }

  // Compare the member data that our base class knows about.
  if (!nsSimpleURI::EqualsInternal(otherUri, aRefHandlingMode)) {
    *aResult = false;
    return NS_OK;
  }

  // Compare the piece of additional member data that we add to base class.
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
nsHostObjectURI::GetHelperForLanguage(uint32_t language, nsISupports **_retval)
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
  *aClassID = (nsCID*) nsMemory::Alloc(sizeof(nsCID));
  NS_ENSURE_TRUE(*aClassID, NS_ERROR_OUT_OF_MEMORY);

  return GetClassIDNoAlloc(*aClassID);
}

NS_IMETHODIMP 
nsHostObjectURI::GetImplementationLanguage(uint32_t *aImplementationLanguage)
{
  *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
  return NS_OK;
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
