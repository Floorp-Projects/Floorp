/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHostObjectProtocolHandler.h"
#include "nsHostObjectURI.h"
#include "nsError.h"
#include "nsClassHashtable.h"
#include "nsNetUtil.h"
#include "nsIPrincipal.h"
#include "nsIDOMFile.h"
#include "nsIDOMMediaStream.h"
#include "mozilla/dom/MediaSource.h"
#include "nsIMemoryReporter.h"

// -----------------------------------------------------------------------
// Hash table
struct DataInfo
{
  // mObject is expected to be an nsIDOMBlob, nsIDOMMediaStream, or MediaSource
  nsCOMPtr<nsISupports> mObject;
  nsCOMPtr<nsIPrincipal> mPrincipal;
};

static nsClassHashtable<nsCStringHashKey, DataInfo>* gDataTable;

// Memory reporting for the hash table.
namespace mozilla {

class HostObjectURLsReporter MOZ_FINAL : public MemoryUniReporter
{
 public:
  HostObjectURLsReporter()
    : MemoryUniReporter("host-object-urls",
                        KIND_OTHER, UNITS_COUNT,
                        "The number of host objects stored for access via URLs "
                        "(e.g. blobs passed to URL.createObjectURL).")
    {}
 private:
  int64_t Amount() MOZ_OVERRIDE
  {
    return gDataTable ? gDataTable->Count() : 0;
  }
};

}

nsHostObjectProtocolHandler::nsHostObjectProtocolHandler()
{
  static bool initialized = false;

  if (!initialized) {
    initialized = true;
    RegisterStrongMemoryReporter(new mozilla::HostObjectURLsReporter());
  }
}

nsresult
nsHostObjectProtocolHandler::AddDataEntry(const nsACString& aScheme,
                                          nsISupports* aObject,
                                          nsIPrincipal* aPrincipal,
                                          nsACString& aUri)
{
  nsresult rv = GenerateURIString(aScheme, aUri);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!gDataTable) {
    gDataTable = new nsClassHashtable<nsCStringHashKey, DataInfo>;
  }

  DataInfo* info = new DataInfo;

  info->mObject = aObject;
  info->mPrincipal = aPrincipal;

  gDataTable->Put(aUri, info);
  return NS_OK;
}

void
nsHostObjectProtocolHandler::RemoveDataEntry(const nsACString& aUri)
{
  if (gDataTable) {
    nsCString uriIgnoringRef;
    int32_t hashPos = aUri.FindChar('#');
    if (hashPos < 0) {
      uriIgnoringRef = aUri;
    }
    else {
      uriIgnoringRef = StringHead(aUri, hashPos);
    }
    gDataTable->Remove(uriIgnoringRef);
    if (gDataTable->Count() == 0) {
      delete gDataTable;
      gDataTable = nullptr;
    }
  }
}

nsresult
nsHostObjectProtocolHandler::GenerateURIString(const nsACString &aScheme,
                                               nsACString& aUri)
{
  nsresult rv;
  nsCOMPtr<nsIUUIDGenerator> uuidgen =
    do_GetService("@mozilla.org/uuid-generator;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsID id;
  rv = uuidgen->GenerateUUIDInPlace(&id);
  NS_ENSURE_SUCCESS(rv, rv);

  char chars[NSID_LENGTH];
  id.ToProvidedString(chars);

  aUri += aScheme;
  aUri += NS_LITERAL_CSTRING(":");
  aUri += Substring(chars + 1, chars + NSID_LENGTH - 2);

  return NS_OK;
}

static DataInfo*
GetDataInfo(const nsACString& aUri)
{
  if (!gDataTable) {
    return nullptr;
  }

  DataInfo* res;
  nsCString uriIgnoringRef;
  int32_t hashPos = aUri.FindChar('#');
  if (hashPos < 0) {
    uriIgnoringRef = aUri;
  }
  else {
    uriIgnoringRef = StringHead(aUri, hashPos);
  }
  gDataTable->Get(uriIgnoringRef, &res);
  
  return res;
}

nsIPrincipal*
nsHostObjectProtocolHandler::GetDataEntryPrincipal(const nsACString& aUri)
{
  if (!gDataTable) {
    return nullptr;
  }

  DataInfo* res = GetDataInfo(aUri);

  if (!res) {
    return nullptr;
  }

  return res->mPrincipal;
}

void
nsHostObjectProtocolHandler::Traverse(const nsACString& aUri,
                                      nsCycleCollectionTraversalCallback& aCallback)
{
  if (!gDataTable) {
    return;
  }

  DataInfo* res;
  gDataTable->Get(aUri, &res);
  if (!res) {
    return;
  }

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCallback, "HostObjectProtocolHandler DataInfo.mObject");
  aCallback.NoteXPCOMChild(res->mObject);
}

static nsISupports*
GetDataObject(nsIURI* aURI)
{
  nsCString spec;
  aURI->GetSpec(spec);

  DataInfo* info = GetDataInfo(spec);
  return info ? info->mObject : nullptr;
}

// -----------------------------------------------------------------------
// Protocol handler

NS_IMPL_ISUPPORTS1(nsHostObjectProtocolHandler, nsIProtocolHandler)

NS_IMETHODIMP
nsHostObjectProtocolHandler::GetDefaultPort(int32_t *result)
{
  *result = -1;
  return NS_OK;
}

NS_IMETHODIMP
nsHostObjectProtocolHandler::GetProtocolFlags(uint32_t *result)
{
  *result = URI_NORELATIVE | URI_NOAUTH | URI_LOADABLE_BY_SUBSUMERS |
            URI_IS_LOCAL_RESOURCE | URI_NON_PERSISTABLE;
  return NS_OK;
}

NS_IMETHODIMP
nsHostObjectProtocolHandler::NewURI(const nsACString& aSpec,
                                    const char *aCharset,
                                    nsIURI *aBaseURI,
                                    nsIURI **aResult)
{
  *aResult = nullptr;
  nsresult rv;

  DataInfo* info = GetDataInfo(aSpec);

  nsRefPtr<nsHostObjectURI> uri =
    new nsHostObjectURI(info ? info->mPrincipal.get() : nullptr);

  rv = uri->SetSpec(aSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_TryToSetImmutable(uri);
  uri.forget(aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsHostObjectProtocolHandler::NewChannel(nsIURI* uri, nsIChannel* *result)
{
  *result = nullptr;

  nsCString spec;
  uri->GetSpec(spec);

  DataInfo* info = GetDataInfo(spec);

  if (!info) {
    return NS_ERROR_DOM_BAD_URI;
  }
  nsCOMPtr<nsIDOMBlob> blob = do_QueryInterface(info->mObject);
  if (!blob) {
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
  nsresult rv = blob->GetInternalStream(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewInputStreamChannel(getter_AddRefs(channel),
                                uri,
                                stream);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupports> owner = do_QueryInterface(info->mPrincipal);

  nsString type;
  rv = blob->GetType(type);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMFile> file = do_QueryInterface(info->mObject);
  if (file) {
    nsString filename;
    rv = file->GetName(filename);
    NS_ENSURE_SUCCESS(rv, rv);
    channel->SetContentDispositionFilename(filename);
  }

  uint64_t size;
  rv = blob->GetSize(&size);
  NS_ENSURE_SUCCESS(rv, rv);

  channel->SetOwner(owner);
  channel->SetOriginalURI(uri);
  channel->SetContentType(NS_ConvertUTF16toUTF8(type));
  channel->SetContentLength(size);

  channel.forget(result);

  return NS_OK;
}

NS_IMETHODIMP
nsHostObjectProtocolHandler::AllowPort(int32_t port, const char *scheme,
                                       bool *_retval)
{
  // don't override anything.
  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
nsBlobProtocolHandler::GetScheme(nsACString &result)
{
  result.AssignLiteral(BLOBURI_SCHEME);
  return NS_OK;
}

NS_IMETHODIMP
nsMediaStreamProtocolHandler::GetScheme(nsACString &result)
{
  result.AssignLiteral(MEDIASTREAMURI_SCHEME);
  return NS_OK;
}

NS_IMETHODIMP
nsMediaSourceProtocolHandler::GetScheme(nsACString &result)
{
  result.AssignLiteral(MEDIASOURCEURI_SCHEME);
  return NS_OK;
}

NS_IMETHODIMP
nsFontTableProtocolHandler::GetScheme(nsACString &result)
{
  result.AssignLiteral(FONTTABLEURI_SCHEME);
  return NS_OK;
}

nsresult
NS_GetStreamForBlobURI(nsIURI* aURI, nsIInputStream** aStream)
{
  NS_ASSERTION(IsBlobURI(aURI), "Only call this with blob URIs");

  *aStream = nullptr;

  nsCOMPtr<nsIDOMBlob> blob = do_QueryInterface(GetDataObject(aURI));
  if (!blob) {
    return NS_ERROR_DOM_BAD_URI;
  }

  return blob->GetInternalStream(aStream);
}

nsresult
NS_GetStreamForMediaStreamURI(nsIURI* aURI, nsIDOMMediaStream** aStream)
{
  NS_ASSERTION(IsMediaStreamURI(aURI), "Only call this with mediastream URIs");

  *aStream = nullptr;

  nsCOMPtr<nsIDOMMediaStream> stream = do_QueryInterface(GetDataObject(aURI));
  if (!stream) {
    return NS_ERROR_DOM_BAD_URI;
  }

  *aStream = stream;
  NS_ADDREF(*aStream);
  return NS_OK;
}

NS_IMETHODIMP
nsFontTableProtocolHandler::NewURI(const nsACString& aSpec,
                                   const char *aCharset,
                                   nsIURI *aBaseURI,
                                   nsIURI **aResult)
{
  nsRefPtr<nsIURI> uri;

  // Either you got here via a ref or a fonttable: uri
  if (aSpec.Length() && aSpec.CharAt(0) == '#') {
    nsresult rv = aBaseURI->CloneIgnoringRef(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    uri->SetRef(aSpec);
  } else {
    // Relative URIs (other than #ref) are not meaningful within the
    // fonttable: scheme.
    // If aSpec is a relative URI -other- than a bare #ref,
    // this will leave uri empty, and we'll return a failure code below.
    uri = new nsSimpleURI();
    uri->SetSpec(aSpec);
  }

  bool schemeIs;
  if (NS_FAILED(uri->SchemeIs(FONTTABLEURI_SCHEME, &schemeIs)) || !schemeIs) {
    NS_WARNING("Non-fonttable spec in nsFontTableProtocolHander");
    return NS_ERROR_NOT_AVAILABLE;
  }

  uri.forget(aResult);
  return NS_OK;
}

nsresult
NS_GetSourceForMediaSourceURI(nsIURI* aURI, mozilla::dom::MediaSource** aSource)
{
  NS_ASSERTION(IsMediaSourceURI(aURI), "Only call this with mediasource URIs");

  *aSource = nullptr;

  nsCOMPtr<mozilla::dom::MediaSource> source = do_QueryInterface(GetDataObject(aURI));
  if (!source) {
    return NS_ERROR_DOM_BAD_URI;
  }

  source.forget(aSource);
  return NS_OK;
}
