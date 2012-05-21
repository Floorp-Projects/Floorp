/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBlobProtocolHandler.h"
#include "nsSimpleURI.h"
#include "nsDOMError.h"
#include "nsCOMPtr.h"
#include "nsClassHashtable.h"
#include "nsNetUtil.h"
#include "nsIURIWithPrincipal.h"
#include "nsIPrincipal.h"
#include "nsIDOMFile.h"
#include "nsISerializable.h"
#include "nsIClassInfo.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIProgrammingLanguage.h"

// -----------------------------------------------------------------------
// Hash table
struct FileDataInfo
{
  nsCOMPtr<nsIDOMBlob> mFile;
  nsCOMPtr<nsIPrincipal> mPrincipal;
};

static nsClassHashtable<nsCStringHashKey, FileDataInfo>* gFileDataTable;

void
nsBlobProtocolHandler::AddFileDataEntry(nsACString& aUri,
					    nsIDOMBlob* aFile,
                                            nsIPrincipal* aPrincipal)
{
  if (!gFileDataTable) {
    gFileDataTable = new nsClassHashtable<nsCStringHashKey, FileDataInfo>;
    gFileDataTable->Init();
  }

  FileDataInfo* info = new FileDataInfo;

  info->mFile = aFile;
  info->mPrincipal = aPrincipal;

  gFileDataTable->Put(aUri, info);
}

void
nsBlobProtocolHandler::RemoveFileDataEntry(nsACString& aUri)
{
  if (gFileDataTable) {
    gFileDataTable->Remove(aUri);
    if (gFileDataTable->Count() == 0) {
      delete gFileDataTable;
      gFileDataTable = nsnull;
    }
  }
}

nsIPrincipal*
nsBlobProtocolHandler::GetFileDataEntryPrincipal(nsACString& aUri)
{
  if (!gFileDataTable) {
    return nsnull;
  }
  
  FileDataInfo* res;
  gFileDataTable->Get(aUri, &res);
  if (!res) {
    return nsnull;
  }

  return res->mPrincipal;
}

static FileDataInfo*
GetFileDataInfo(const nsACString& aUri)
{
  NS_ASSERTION(StringBeginsWith(aUri,
                                NS_LITERAL_CSTRING(BLOBURI_SCHEME ":")),
               "Bad URI");
  
  if (!gFileDataTable) {
    return nsnull;
  }
  
  FileDataInfo* res;
  gFileDataTable->Get(aUri, &res);
  return res;
}

// -----------------------------------------------------------------------
// Uri

#define NS_BLOBURI_CID \
{ 0xf5475c51, 0x59a7, 0x4757, \
  { 0xb3, 0xd9, 0xe2, 0x11, 0xa9, 0x41, 0x08, 0x72 } }

static NS_DEFINE_CID(kBLOBURICID, NS_BLOBURI_CID);

class nsBlobURI : public nsSimpleURI,
                      public nsIURIWithPrincipal
{
public:
  nsBlobURI(nsIPrincipal* aPrincipal) :
      nsSimpleURI(), mPrincipal(aPrincipal)
  {}
  virtual ~nsBlobURI() {}

  // For use only from deserialization
  nsBlobURI() : nsSimpleURI() {}

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIURIWITHPRINCIPAL
  NS_DECL_NSISERIALIZABLE
  NS_DECL_NSICLASSINFO

  // Override CloneInternal() and EqualsInternal()
  virtual nsresult CloneInternal(RefHandlingEnum aRefHandlingMode,
                                 nsIURI** aClone);
  virtual nsresult EqualsInternal(nsIURI* aOther,
                                  RefHandlingEnum aRefHandlingMode,
                                  bool* aResult);

  // Override StartClone to hand back a nsBlobURI
  virtual nsSimpleURI* StartClone(RefHandlingEnum /* unused */)
  { return new nsBlobURI(); }

  nsCOMPtr<nsIPrincipal> mPrincipal;
};

static NS_DEFINE_CID(kThisSimpleURIImplementationCID,
                     NS_THIS_SIMPLEURI_IMPLEMENTATION_CID);

NS_IMPL_ADDREF_INHERITED(nsBlobURI, nsSimpleURI)
NS_IMPL_RELEASE_INHERITED(nsBlobURI, nsSimpleURI)

NS_INTERFACE_MAP_BEGIN(nsBlobURI)
  NS_INTERFACE_MAP_ENTRY(nsIURIWithPrincipal)
  if (aIID.Equals(kBLOBURICID))
    foundInterface = static_cast<nsIURI*>(this);
  else if (aIID.Equals(kThisSimpleURIImplementationCID)) {
    // Need to return explicitly here, because if we just set foundInterface
    // to null the NS_INTERFACE_MAP_END_INHERITING will end up calling into
    // nsSimplURI::QueryInterface and finding something for this CID.
    *aInstancePtr = nsnull;
    return NS_NOINTERFACE;
  }
  else
NS_INTERFACE_MAP_END_INHERITING(nsSimpleURI)

// nsIURIWithPrincipal methods:

NS_IMETHODIMP
nsBlobURI::GetPrincipal(nsIPrincipal** aPrincipal)
{
  NS_IF_ADDREF(*aPrincipal = mPrincipal);

  return NS_OK;
}

NS_IMETHODIMP
nsBlobURI::GetPrincipalUri(nsIURI** aUri)
{
  if (mPrincipal) {
    mPrincipal->GetURI(aUri);
  }
  else {
    *aUri = nsnull;
  }

  return NS_OK;
}

// nsISerializable methods:

NS_IMETHODIMP
nsBlobURI::Read(nsIObjectInputStream* aStream)
{
  nsresult rv = nsSimpleURI::Read(aStream);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_ReadOptionalObject(aStream, true, getter_AddRefs(mPrincipal));
}

NS_IMETHODIMP
nsBlobURI::Write(nsIObjectOutputStream* aStream)
{
  nsresult rv = nsSimpleURI::Write(aStream);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_WriteOptionalCompoundObject(aStream, mPrincipal,
                                        NS_GET_IID(nsIPrincipal),
                                        true);
}

// nsIURI methods:
nsresult
nsBlobURI::CloneInternal(nsSimpleURI::RefHandlingEnum aRefHandlingMode,
                             nsIURI** aClone)
{
  nsCOMPtr<nsIURI> simpleClone;
  nsresult rv =
    nsSimpleURI::CloneInternal(aRefHandlingMode, getter_AddRefs(simpleClone));
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
  nsRefPtr<nsBlobURI> uriCheck;
  rv = simpleClone->QueryInterface(kBLOBURICID, getter_AddRefs(uriCheck));
  NS_ABORT_IF_FALSE(NS_SUCCEEDED(rv) && uriCheck,
		    "Unexpected!");
#endif

  nsBlobURI* blobURI = static_cast<nsBlobURI*>(simpleClone.get());

  blobURI->mPrincipal = mPrincipal;

  simpleClone.forget(aClone);
  return NS_OK;
}

/* virtual */ nsresult
nsBlobURI::EqualsInternal(nsIURI* aOther,
                              nsSimpleURI::RefHandlingEnum aRefHandlingMode,
                              bool* aResult)
{
  if (!aOther) {
    *aResult = false;
    return NS_OK;
  }
  
  nsRefPtr<nsBlobURI> otherBlobUri;
  aOther->QueryInterface(kBLOBURICID, getter_AddRefs(otherBlobUri));
  if (!otherBlobUri) {
    *aResult = false;
    return NS_OK;
  }

  // Compare the member data that our base class knows about.
  if (!nsSimpleURI::EqualsInternal(otherBlobUri, aRefHandlingMode)) {
    *aResult = false;
    return NS_OK;
   }

  // Compare the piece of additional member data that we add to base class.
  if (mPrincipal && otherBlobUri->mPrincipal) {
    // Both of us have mPrincipals. Compare them.
    return mPrincipal->Equals(otherBlobUri->mPrincipal, aResult);
  }
  // else, at least one of us lacks a principal; only equal if *both* lack it.
  *aResult = (!mPrincipal && !otherBlobUri->mPrincipal);
  return NS_OK;
}

// nsIClassInfo methods:
NS_IMETHODIMP 
nsBlobURI::GetInterfaces(PRUint32 *count, nsIID * **array)
{
  *count = 0;
  *array = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsBlobURI::GetHelperForLanguage(PRUint32 language, nsISupports **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsBlobURI::GetContractID(char * *aContractID)
{
  // Make sure to modify any subclasses as needed if this ever
  // changes.
  *aContractID = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsBlobURI::GetClassDescription(char * *aClassDescription)
{
  *aClassDescription = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsBlobURI::GetClassID(nsCID * *aClassID)
{
  // Make sure to modify any subclasses as needed if this ever
  // changes to not call the virtual GetClassIDNoAlloc.
  *aClassID = (nsCID*) nsMemory::Alloc(sizeof(nsCID));
  NS_ENSURE_TRUE(*aClassID, NS_ERROR_OUT_OF_MEMORY);

  return GetClassIDNoAlloc(*aClassID);
}

NS_IMETHODIMP 
nsBlobURI::GetImplementationLanguage(PRUint32 *aImplementationLanguage)
{
  *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
  return NS_OK;
}

NS_IMETHODIMP 
nsBlobURI::GetFlags(PRUint32 *aFlags)
{
  *aFlags = nsIClassInfo::MAIN_THREAD_ONLY;
  return NS_OK;
}

NS_IMETHODIMP 
nsBlobURI::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
  *aClassIDNoAlloc = kBLOBURICID;
  return NS_OK;
}

// -----------------------------------------------------------------------
// Protocol handler

NS_IMPL_ISUPPORTS1(nsBlobProtocolHandler, nsIProtocolHandler)

NS_IMETHODIMP
nsBlobProtocolHandler::GetScheme(nsACString &result)
{
  result.AssignLiteral(BLOBURI_SCHEME);
  return NS_OK;
}

NS_IMETHODIMP
nsBlobProtocolHandler::GetDefaultPort(PRInt32 *result)
{
  *result = -1;
  return NS_OK;
}

NS_IMETHODIMP
nsBlobProtocolHandler::GetProtocolFlags(PRUint32 *result)
{
  *result = URI_NORELATIVE | URI_NOAUTH | URI_LOADABLE_BY_SUBSUMERS |
            URI_IS_LOCAL_RESOURCE | URI_NON_PERSISTABLE;
  return NS_OK;
}

NS_IMETHODIMP
nsBlobProtocolHandler::NewURI(const nsACString& aSpec,
                                  const char *aCharset,
                                  nsIURI *aBaseURI,
                                  nsIURI **aResult)
{
  *aResult = nsnull;
  nsresult rv;

  FileDataInfo* info =
    GetFileDataInfo(aSpec);

  nsRefPtr<nsBlobURI> uri =
    new nsBlobURI(info ? info->mPrincipal.get() : nsnull);

  rv = uri->SetSpec(aSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_TryToSetImmutable(uri);
  uri.forget(aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsBlobProtocolHandler::NewChannel(nsIURI* uri, nsIChannel* *result)
{
  *result = nsnull;

  nsCString spec;
  uri->GetSpec(spec);

  FileDataInfo* info =
    GetFileDataInfo(spec);

  if (!info) {
    return NS_ERROR_DOM_BAD_URI;
  }

#ifdef DEBUG
  {
    nsCOMPtr<nsIURIWithPrincipal> uriPrinc = do_QueryInterface(uri);
    nsCOMPtr<nsIPrincipal> principal;
    uriPrinc->GetPrincipal(getter_AddRefs(principal));
    NS_ASSERTION(info->mPrincipal == principal, "Wrong principal!");
  }
#endif

  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = info->mFile->GetInternalStream(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewInputStreamChannel(getter_AddRefs(channel),
				uri,
				stream);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupports> owner = do_QueryInterface(info->mPrincipal);

  nsAutoString type;
  rv = info->mFile->GetType(type);
  NS_ENSURE_SUCCESS(rv, rv);

  channel->SetOwner(owner);
  channel->SetOriginalURI(uri);
  channel->SetContentType(NS_ConvertUTF16toUTF8(type));
  channel.forget(result);
  
  return NS_OK;
}

NS_IMETHODIMP 
nsBlobProtocolHandler::AllowPort(PRInt32 port, const char *scheme,
                                     bool *_retval)
{
    // don't override anything.  
    *_retval = false;
    return NS_OK;
}
