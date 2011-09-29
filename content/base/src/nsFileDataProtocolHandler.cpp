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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsFileDataProtocolHandler.h"
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
nsFileDataProtocolHandler::AddFileDataEntry(nsACString& aUri,
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
nsFileDataProtocolHandler::RemoveFileDataEntry(nsACString& aUri)
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
nsFileDataProtocolHandler::GetFileDataEntryPrincipal(nsACString& aUri)
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
                                NS_LITERAL_CSTRING(FILEDATA_SCHEME ":")),
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

#define NS_FILEDATAURI_CID \
{ 0xf5475c51, 0x59a7, 0x4757, \
  { 0xb3, 0xd9, 0xe2, 0x11, 0xa9, 0x41, 0x08, 0x72 } }

static NS_DEFINE_CID(kFILEDATAURICID, NS_FILEDATAURI_CID);

class nsFileDataURI : public nsSimpleURI,
                      public nsIURIWithPrincipal
{
public:
  nsFileDataURI(nsIPrincipal* aPrincipal) :
      nsSimpleURI(), mPrincipal(aPrincipal)
  {}
  virtual ~nsFileDataURI() {}

  // For use only from deserialization
  nsFileDataURI() : nsSimpleURI() {}

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

  // Override StartClone to hand back a nsFileDataURI
  virtual nsSimpleURI* StartClone(RefHandlingEnum /* unused */)
  { return new nsFileDataURI(); }

  nsCOMPtr<nsIPrincipal> mPrincipal;
};

static NS_DEFINE_CID(kThisSimpleURIImplementationCID,
                     NS_THIS_SIMPLEURI_IMPLEMENTATION_CID);

NS_IMPL_ADDREF_INHERITED(nsFileDataURI, nsSimpleURI)
NS_IMPL_RELEASE_INHERITED(nsFileDataURI, nsSimpleURI)

NS_INTERFACE_MAP_BEGIN(nsFileDataURI)
  NS_INTERFACE_MAP_ENTRY(nsIURIWithPrincipal)
  if (aIID.Equals(kFILEDATAURICID))
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
nsFileDataURI::GetPrincipal(nsIPrincipal** aPrincipal)
{
  NS_IF_ADDREF(*aPrincipal = mPrincipal);

  return NS_OK;
}

NS_IMETHODIMP
nsFileDataURI::GetPrincipalUri(nsIURI** aUri)
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
nsFileDataURI::Read(nsIObjectInputStream* aStream)
{
  nsresult rv = nsSimpleURI::Read(aStream);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_ReadOptionalObject(aStream, PR_TRUE, getter_AddRefs(mPrincipal));
}

NS_IMETHODIMP
nsFileDataURI::Write(nsIObjectOutputStream* aStream)
{
  nsresult rv = nsSimpleURI::Write(aStream);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_WriteOptionalCompoundObject(aStream, mPrincipal,
                                        NS_GET_IID(nsIPrincipal),
                                        PR_TRUE);
}

// nsIURI methods:
nsresult
nsFileDataURI::CloneInternal(nsSimpleURI::RefHandlingEnum aRefHandlingMode,
                             nsIURI** aClone)
{
  nsCOMPtr<nsIURI> simpleClone;
  nsresult rv =
    nsSimpleURI::CloneInternal(aRefHandlingMode, getter_AddRefs(simpleClone));
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
  nsRefPtr<nsFileDataURI> uriCheck;
  rv = simpleClone->QueryInterface(kFILEDATAURICID, getter_AddRefs(uriCheck));
  NS_ABORT_IF_FALSE(NS_SUCCEEDED(rv) && uriCheck,
		    "Unexpected!");
#endif

  nsFileDataURI* fileDataURI = static_cast<nsFileDataURI*>(simpleClone.get());

  fileDataURI->mPrincipal = mPrincipal;

  simpleClone.forget(aClone);
  return NS_OK;
}

/* virtual */ nsresult
nsFileDataURI::EqualsInternal(nsIURI* aOther,
                              nsSimpleURI::RefHandlingEnum aRefHandlingMode,
                              bool* aResult)
{
  if (!aOther) {
    *aResult = PR_FALSE;
    return NS_OK;
  }
  
  nsRefPtr<nsFileDataURI> otherFileDataUri;
  aOther->QueryInterface(kFILEDATAURICID, getter_AddRefs(otherFileDataUri));
  if (!otherFileDataUri) {
    *aResult = PR_FALSE;
    return NS_OK;
  }

  // Compare the member data that our base class knows about.
  if (!nsSimpleURI::EqualsInternal(otherFileDataUri, aRefHandlingMode)) {
    *aResult = PR_FALSE;
    return NS_OK;
   }

  // Compare the piece of additional member data that we add to base class.
  if (mPrincipal && otherFileDataUri->mPrincipal) {
    // Both of us have mPrincipals. Compare them.
    return mPrincipal->Equals(otherFileDataUri->mPrincipal, aResult);
  }
  // else, at least one of us lacks a principal; only equal if *both* lack it.
  *aResult = (!mPrincipal && !otherFileDataUri->mPrincipal);
  return NS_OK;
}

// nsIClassInfo methods:
NS_IMETHODIMP 
nsFileDataURI::GetInterfaces(PRUint32 *count, nsIID * **array)
{
  *count = 0;
  *array = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsFileDataURI::GetHelperForLanguage(PRUint32 language, nsISupports **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsFileDataURI::GetContractID(char * *aContractID)
{
  // Make sure to modify any subclasses as needed if this ever
  // changes.
  *aContractID = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsFileDataURI::GetClassDescription(char * *aClassDescription)
{
  *aClassDescription = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsFileDataURI::GetClassID(nsCID * *aClassID)
{
  // Make sure to modify any subclasses as needed if this ever
  // changes to not call the virtual GetClassIDNoAlloc.
  *aClassID = (nsCID*) nsMemory::Alloc(sizeof(nsCID));
  NS_ENSURE_TRUE(*aClassID, NS_ERROR_OUT_OF_MEMORY);

  return GetClassIDNoAlloc(*aClassID);
}

NS_IMETHODIMP 
nsFileDataURI::GetImplementationLanguage(PRUint32 *aImplementationLanguage)
{
  *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
  return NS_OK;
}

NS_IMETHODIMP 
nsFileDataURI::GetFlags(PRUint32 *aFlags)
{
  *aFlags = nsIClassInfo::MAIN_THREAD_ONLY;
  return NS_OK;
}

NS_IMETHODIMP 
nsFileDataURI::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
  *aClassIDNoAlloc = kFILEDATAURICID;
  return NS_OK;
}

// -----------------------------------------------------------------------
// Protocol handler

NS_IMPL_ISUPPORTS1(nsFileDataProtocolHandler, nsIProtocolHandler)

NS_IMETHODIMP
nsFileDataProtocolHandler::GetScheme(nsACString &result)
{
  result.AssignLiteral(FILEDATA_SCHEME);
  return NS_OK;
}

NS_IMETHODIMP
nsFileDataProtocolHandler::GetDefaultPort(PRInt32 *result)
{
  *result = -1;
  return NS_OK;
}

NS_IMETHODIMP
nsFileDataProtocolHandler::GetProtocolFlags(PRUint32 *result)
{
  *result = URI_NORELATIVE | URI_NOAUTH | URI_LOADABLE_BY_SUBSUMERS |
            URI_IS_LOCAL_RESOURCE | URI_NON_PERSISTABLE;
  return NS_OK;
}

NS_IMETHODIMP
nsFileDataProtocolHandler::NewURI(const nsACString& aSpec,
                                  const char *aCharset,
                                  nsIURI *aBaseURI,
                                  nsIURI **aResult)
{
  *aResult = nsnull;
  nsresult rv;

  FileDataInfo* info =
    GetFileDataInfo(aSpec);

  nsRefPtr<nsFileDataURI> uri =
    new nsFileDataURI(info ? info->mPrincipal.get() : nsnull);

  rv = uri->SetSpec(aSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_TryToSetImmutable(uri);
  uri.forget(aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsFileDataProtocolHandler::NewChannel(nsIURI* uri, nsIChannel* *result)
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
nsFileDataProtocolHandler::AllowPort(PRInt32 port, const char *scheme,
                                     bool *_retval)
{
    // don't override anything.  
    *_retval = PR_FALSE;
    return NS_OK;
}
