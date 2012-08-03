/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBlobProtocolHandler.h"
#include "nsBlobURI.h"
#include "nsDOMError.h"
#include "nsClassHashtable.h"
#include "nsNetUtil.h"
#include "nsIPrincipal.h"
#include "nsIDOMFile.h"

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
      gFileDataTable = nullptr;
    }
  }
}

nsIPrincipal*
nsBlobProtocolHandler::GetFileDataEntryPrincipal(nsACString& aUri)
{
  if (!gFileDataTable) {
    return nullptr;
  }
  
  FileDataInfo* res;
  gFileDataTable->Get(aUri, &res);
  if (!res) {
    return nullptr;
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
    return nullptr;
  }
  
  FileDataInfo* res;
  gFileDataTable->Get(aUri, &res);
  return res;
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
  *aResult = nullptr;
  nsresult rv;

  FileDataInfo* info =
    GetFileDataInfo(aSpec);

  nsRefPtr<nsBlobURI> uri =
    new nsBlobURI(info ? info->mPrincipal.get() : nullptr);

  rv = uri->SetSpec(aSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_TryToSetImmutable(uri);
  uri.forget(aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsBlobProtocolHandler::NewChannel(nsIURI* uri, nsIChannel* *result)
{
  *result = nullptr;

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
