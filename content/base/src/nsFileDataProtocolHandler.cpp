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
#include "nsNetCID.h"
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

static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);

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


// Use an extra base object to avoid having to manually retype all the
// nsIURI methods.  I wish we could just inherit from nsSimpleURI instead.
class nsFileDataURI_base : public nsIURI,
                           public nsIMutable
{
public:
  nsFileDataURI_base(nsIURI* aSimpleURI) :
    mSimpleURI(aSimpleURI)
  {
    mMutable = do_QueryInterface(mSimpleURI);
    NS_ASSERTION(aSimpleURI && mMutable, "This isn't going to work out");
  }
  virtual ~nsFileDataURI_base() {}

  // For use only from deserialization
  nsFileDataURI_base() {}
  
  NS_FORWARD_NSIURI(mSimpleURI->)
  NS_FORWARD_NSIMUTABLE(mMutable->)

protected:
  nsCOMPtr<nsIURI> mSimpleURI;
  nsCOMPtr<nsIMutable> mMutable;
};

class nsFileDataURI : public nsFileDataURI_base,
                      public nsIURIWithPrincipal,
                      public nsISerializable,
                      public nsIClassInfo
{
public:
  nsFileDataURI(nsIPrincipal* aPrincipal, nsIURI* aSimpleURI) :
      nsFileDataURI_base(aSimpleURI), mPrincipal(aPrincipal)
  {}
  virtual ~nsFileDataURI() {}

  // For use only from deserialization
  nsFileDataURI() : nsFileDataURI_base() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIURIWITHPRINCIPAL
  NS_DECL_NSISERIALIZABLE
  NS_DECL_NSICLASSINFO

  // Override Clone() and Equals()
  NS_IMETHOD Clone(nsIURI** aClone);
  NS_IMETHOD Equals(nsIURI* aOther, PRBool *aResult);

  nsCOMPtr<nsIPrincipal> mPrincipal;
};

NS_IMPL_ADDREF(nsFileDataURI)
NS_IMPL_RELEASE(nsFileDataURI)
NS_INTERFACE_MAP_BEGIN(nsFileDataURI)
  NS_INTERFACE_MAP_ENTRY(nsIURI)
  NS_INTERFACE_MAP_ENTRY(nsIURIWithPrincipal)
  NS_INTERFACE_MAP_ENTRY(nsISerializable)
  NS_INTERFACE_MAP_ENTRY(nsIClassInfo)
  NS_INTERFACE_MAP_ENTRY(nsIMutable)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIURI)
  if (aIID.Equals(kFILEDATAURICID))
      foundInterface = static_cast<nsIURI*>(this);
  else
NS_INTERFACE_MAP_END

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
  nsresult rv = aStream->ReadObject(PR_TRUE, getter_AddRefs(mSimpleURI));
  NS_ENSURE_SUCCESS(rv, rv);

  mMutable = do_QueryInterface(mSimpleURI);
  NS_ENSURE_TRUE(mMutable, NS_ERROR_UNEXPECTED);

  return NS_ReadOptionalObject(aStream, PR_TRUE, getter_AddRefs(mPrincipal));
}

NS_IMETHODIMP
nsFileDataURI::Write(nsIObjectOutputStream* aStream)
{
  nsresult rv = aStream->WriteCompoundObject(mSimpleURI, NS_GET_IID(nsIURI),
                                             PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_WriteOptionalCompoundObject(aStream, mPrincipal,
                                        NS_GET_IID(nsIPrincipal),
                                        PR_TRUE);
}

// nsIURI methods:

NS_IMETHODIMP
nsFileDataURI::Clone(nsIURI** aClone)
{
  nsCOMPtr<nsIURI> simpleClone;
  nsresult rv = mSimpleURI->Clone(getter_AddRefs(simpleClone));
  NS_ENSURE_SUCCESS(rv, rv);

  nsIURI* newURI = new nsFileDataURI(mPrincipal, simpleClone);
  NS_ENSURE_TRUE(newURI, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aClone = newURI);
  return NS_OK;
}

NS_IMETHODIMP
nsFileDataURI::Equals(nsIURI* aOther, PRBool *aResult)
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

  nsresult rv = mPrincipal->Equals(otherFileDataUri->mPrincipal, aResult);
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (!*aResult) {
    return NS_OK;
  }

  return mSimpleURI->Equals(otherFileDataUri->mSimpleURI, aResult);
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

  nsCOMPtr<nsIURI> inner = do_CreateInstance(kSimpleURICID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = inner->SetSpec(aSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<nsFileDataURI> uri =
    new nsFileDataURI(info ? info->mPrincipal.get() : nsnull, inner);

  NS_TryToSetImmutable(uri);
  *aResult = uri.forget().get();

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

  channel->SetOwner(owner);
  channel->SetOriginalURI(uri);
  channel.forget(result);
  
  return NS_OK;
}

NS_IMETHODIMP 
nsFileDataProtocolHandler::AllowPort(PRInt32 port, const char *scheme,
                                     PRBool *_retval)
{
    // don't override anything.  
    *_retval = PR_FALSE;
    return NS_OK;
}
