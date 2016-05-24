/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHostObjectProtocolHandler.h"

#include "DOMMediaStream.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/MediaSource.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/Preferences.h"
#include "nsClassHashtable.h"
#include "nsContentUtils.h"
#include "nsError.h"
#include "nsHostObjectURI.h"
#include "nsIMemoryReporter.h"
#include "nsIPrincipal.h"
#include "nsIUUIDGenerator.h"
#include "nsNetUtil.h"

using mozilla::dom::BlobImpl;
using mozilla::ErrorResult;
using mozilla::net::LoadInfo;

// -----------------------------------------------------------------------
// Hash table
struct DataInfo
{
  // mObject is expected to be an nsIDOMBlob, DOMMediaStream, or MediaSource
  nsCOMPtr<nsISupports> mObject;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCString mStack;
};

static nsClassHashtable<nsCStringHashKey, DataInfo>* gDataTable;

// Memory reporting for the hash table.
namespace mozilla {

class HostObjectURLsReporter final : public nsIMemoryReporter
{
  ~HostObjectURLsReporter() {}

 public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    return MOZ_COLLECT_REPORT(
      "host-object-urls", KIND_OTHER, UNITS_COUNT,
      gDataTable ? gDataTable->Count() : 0,
      "The number of host objects stored for access via URLs "
      "(e.g. blobs passed to URL.createObjectURL).");
  }
};

NS_IMPL_ISUPPORTS(HostObjectURLsReporter, nsIMemoryReporter)

class BlobURLsReporter final : public nsIMemoryReporter
{
 public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aCallback,
                            nsISupports* aData, bool aAnonymize) override
  {
    if (!gDataTable) {
      return NS_OK;
    }

    nsDataHashtable<nsPtrHashKey<nsIDOMBlob>, uint32_t> refCounts;

    // Determine number of URLs per blob, to handle the case where it's > 1.
    for (auto iter = gDataTable->Iter(); !iter.Done(); iter.Next()) {
      nsCOMPtr<nsIDOMBlob> blob =
        do_QueryInterface(iter.UserData()->mObject);
      if (blob) {
        refCounts.Put(blob, refCounts.Get(blob) + 1);
      }
    }

    for (auto iter = gDataTable->Iter(); !iter.Done(); iter.Next()) {
      nsCStringHashKey::KeyType key = iter.Key();
      DataInfo* info = iter.UserData();

      nsCOMPtr<nsIDOMBlob> tmp = do_QueryInterface(info->mObject);
      RefPtr<mozilla::dom::Blob> blob =
        static_cast<mozilla::dom::Blob*>(tmp.get());

      if (blob) {
        NS_NAMED_LITERAL_CSTRING(desc,
          "A blob URL allocated with URL.createObjectURL; the referenced "
          "blob cannot be freed until all URLs for it have been explicitly "
          "invalidated with URL.revokeObjectURL.");
        nsAutoCString path, url, owner, specialDesc;
        nsCOMPtr<nsIURI> principalURI;
        uint64_t size = 0;
        uint32_t refCount = 1;
        DebugOnly<bool> blobWasCounted;

        blobWasCounted = refCounts.Get(blob, &refCount);
        MOZ_ASSERT(blobWasCounted);
        MOZ_ASSERT(refCount > 0);

        bool isMemoryFile = blob->IsMemoryFile();

        if (isMemoryFile) {
          ErrorResult rv;
          size = blob->GetSize(rv);
          if (NS_WARN_IF(rv.Failed())) {
            rv.SuppressException();
            size = 0;
          }
        }

        path = isMemoryFile ? "memory-blob-urls/" : "file-blob-urls/";
        BuildPath(path, key, info, aAnonymize);

        if (refCount > 1) {
          nsAutoCString addrStr;

          addrStr = "0x";
          addrStr.AppendInt((uint64_t)(nsIDOMBlob*)blob, 16);

          path += " ";
          path.AppendInt(refCount);
          path += "@";
          path += addrStr;

          specialDesc = desc;
          specialDesc += "\n\nNOTE: This blob (address ";
          specialDesc += addrStr;
          specialDesc += ") has ";
          specialDesc.AppendInt(refCount);
          specialDesc += " URLs.";
          if (isMemoryFile) {
            specialDesc += " Its size is divided ";
            specialDesc += refCount > 2 ? "among" : "between";
            specialDesc += " them in this report.";
          }
        }

        const nsACString& descString = specialDesc.IsEmpty()
            ? static_cast<const nsACString&>(desc)
            : static_cast<const nsACString&>(specialDesc);
        if (isMemoryFile) {
          aCallback->Callback(EmptyCString(),
              path,
              KIND_OTHER,
              UNITS_BYTES,
              size / refCount,
              descString,
              aData);
        } else {
          aCallback->Callback(EmptyCString(),
              path,
              KIND_OTHER,
              UNITS_COUNT,
              1,
              descString,
              aData);
        }
      } else {
        // Just report the path for the DOMMediaStream or MediaSource.
        nsCOMPtr<mozilla::dom::MediaSource>
          ms(do_QueryInterface(info->mObject));
        nsAutoCString path;
        path = ms ? "media-source-urls/" : "dom-media-stream-urls/";
        BuildPath(path, key, info, aAnonymize);

        NS_NAMED_LITERAL_CSTRING(desc,
          "An object URL allocated with URL.createObjectURL; the referenced "
          "data cannot be freed until all URLs for it have been explicitly "
          "invalidated with URL.revokeObjectURL.");

        aCallback->Callback(EmptyCString(),
            path,
            KIND_OTHER,
            UNITS_COUNT,
            1,
            desc,
            aData);
      }
    }

    return NS_OK;
  }

  // Initialize info->mStack to record JS stack info, if enabled.
  // The string generated here is used in ReportCallback, below.
  static void GetJSStackForBlob(DataInfo* aInfo)
  {
    nsCString& stack = aInfo->mStack;
    MOZ_ASSERT(stack.IsEmpty());
    const uint32_t maxFrames = Preferences::GetUint("memory.blob_report.stack_frames");

    if (maxFrames == 0) {
      return;
    }

    nsCOMPtr<nsIStackFrame> frame = dom::GetCurrentJSStack(maxFrames);

    nsAutoCString origin;
    nsCOMPtr<nsIURI> principalURI;
    if (NS_SUCCEEDED(aInfo->mPrincipal->GetURI(getter_AddRefs(principalURI)))
        && principalURI) {
      principalURI->GetPrePath(origin);
    }

    // If we got a frame, we better have a current JSContext.  This is cheating
    // a bit; ideally we'd have our caller pass in a JSContext, or have
    // GetCurrentJSStack() hand out the JSContext it found.
    JSContext* cx = frame ? nsContentUtils::GetCurrentJSContext() : nullptr;

    for (uint32_t i = 0; frame; ++i) {
      nsString fileNameUTF16;
      int32_t lineNumber = 0;

      frame->GetFilename(cx, fileNameUTF16);
      frame->GetLineNumber(cx, &lineNumber);

      if (!fileNameUTF16.IsEmpty()) {
        NS_ConvertUTF16toUTF8 fileName(fileNameUTF16);
        stack += "js(";
        if (!origin.IsEmpty()) {
          // Make the file name root-relative for conciseness if possible.
          const char* originData;
          uint32_t originLen;

          originLen = origin.GetData(&originData);
          // If fileName starts with origin + "/", cut up to that "/".
          if (fileName.Length() >= originLen + 1 &&
              memcmp(fileName.get(), originData, originLen) == 0 &&
              fileName[originLen] == '/') {
            fileName.Cut(0, originLen);
          }
        }
        fileName.ReplaceChar('/', '\\');
        stack += fileName;
        if (lineNumber > 0) {
          stack += ", line=";
          stack.AppendInt(lineNumber);
        }
        stack += ")/";
      }

      nsCOMPtr<nsIStackFrame> caller;
      nsresult rv = frame->GetCaller(cx, getter_AddRefs(caller));
      NS_ENSURE_SUCCESS_VOID(rv);
      caller.swap(frame);
    }
  }

 private:
  ~BlobURLsReporter() {}

  static void BuildPath(nsAutoCString& path,
                        nsCStringHashKey::KeyType aKey,
                        DataInfo* aInfo,
                        bool anonymize)
  {
    nsCOMPtr<nsIURI> principalURI;
    nsAutoCString url, owner;
    if (NS_SUCCEEDED(aInfo->mPrincipal->GetURI(getter_AddRefs(principalURI))) &&
        principalURI != nullptr &&
        NS_SUCCEEDED(principalURI->GetSpec(owner)) &&
        !owner.IsEmpty()) {
      owner.ReplaceChar('/', '\\');
      path += "owner(";
      if (anonymize) {
        path += "<anonymized>";
      } else {
        path += owner;
      }
      path += ")";
    } else {
      path += "owner unknown";
    }
    path += "/";
    if (anonymize) {
      path += "<anonymized-stack>";
    } else {
      path += aInfo->mStack;
    }
    url = aKey;
    url.ReplaceChar('/', '\\');
    if (anonymize) {
      path += "<anonymized-url>";
    } else {
      path += url;
    }
  }
};

NS_IMPL_ISUPPORTS(BlobURLsReporter, nsIMemoryReporter)

} // namespace mozilla

void
nsHostObjectProtocolHandler::Init(void)
{
  static bool initialized = false;

  if (!initialized) {
    initialized = true;
    RegisterStrongMemoryReporter(new mozilla::HostObjectURLsReporter());
    RegisterStrongMemoryReporter(new mozilla::BlobURLsReporter());
  }
}

nsHostObjectProtocolHandler::nsHostObjectProtocolHandler()
{
  Init();
}

nsresult
nsHostObjectProtocolHandler::AddDataEntry(const nsACString& aScheme,
                                          nsISupports* aObject,
                                          nsIPrincipal* aPrincipal,
                                          nsACString& aUri)
{
  Init();

  nsresult rv = GenerateURIString(aScheme, aPrincipal, aUri);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!gDataTable) {
    gDataTable = new nsClassHashtable<nsCStringHashKey, DataInfo>;
  }

  DataInfo* info = new DataInfo;

  info->mObject = aObject;
  info->mPrincipal = aPrincipal;
  mozilla::BlobURLsReporter::GetJSStackForBlob(info);

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
                                               nsIPrincipal* aPrincipal,
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

  aUri = aScheme;
  aUri.Append(':');

  if (aPrincipal) {
    nsAutoCString origin;
    rv = nsContentUtils::GetASCIIOrigin(aPrincipal, origin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    aUri.Append(origin);
    aUri.Append('/');
  }

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

  // Let's remove any fragment and query from this URI.
  int32_t hasFragmentPos = aUri.FindChar('#');
  int32_t hasQueryPos = aUri.FindChar('?');

  int32_t pos = -1;
  if (hasFragmentPos >= 0 && hasQueryPos >= 0) {
    pos = std::min(hasFragmentPos, hasQueryPos);
  } else if (hasFragmentPos >= 0) {
    pos = hasFragmentPos;
  } else {
    pos = hasQueryPos;
  }

  if (pos < 0) {
    gDataTable->Get(aUri, &res);
  } else {
    gDataTable->Get(StringHead(aUri, pos), &res);
  }

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
GetDataObjectForSpec(const nsACString& aSpec)
{
  DataInfo* info = GetDataInfo(aSpec);
  return info ? info->mObject : nullptr;
}

static nsISupports*
GetDataObject(nsIURI* aURI)
{
  nsCString spec;
  aURI->GetSpec(spec);
  return GetDataObjectForSpec(spec);
}

// -----------------------------------------------------------------------
// Protocol handler

NS_IMPL_ISUPPORTS(nsHostObjectProtocolHandler, nsIProtocolHandler)

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

  RefPtr<nsHostObjectURI> uri =
    new nsHostObjectURI(info ? info->mPrincipal.get() : nullptr);

  rv = uri->SetSpec(aSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_TryToSetImmutable(uri);
  uri.forget(aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsHostObjectProtocolHandler::NewChannel2(nsIURI* uri,
                                         nsILoadInfo* aLoadInfo,
                                         nsIChannel** result)
{
  *result = nullptr;

  nsCString spec;
  uri->GetSpec(spec);

  DataInfo* info = GetDataInfo(spec);

  if (!info) {
    return NS_ERROR_DOM_BAD_URI;
  }

  nsCOMPtr<BlobImpl> blob = do_QueryInterface(info->mObject);
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

  ErrorResult rv;
  nsCOMPtr<nsIInputStream> stream;
  blob->GetInternalStream(getter_AddRefs(stream), rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  nsAutoString contentType;
  blob->GetType(contentType);

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewInputStreamChannelInternal(getter_AddRefs(channel),
                                        uri,
                                        stream,
                                        NS_ConvertUTF16toUTF8(contentType),
                                        EmptyCString(), // aContentCharset
                                        aLoadInfo);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  nsString type;
  blob->GetType(type);

  if (blob->IsFile()) {
    nsString filename;
    blob->GetName(filename);
    channel->SetContentDispositionFilename(filename);
  }

  uint64_t size = blob->GetSize(rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  channel->SetOriginalURI(uri);
  channel->SetContentType(NS_ConvertUTF16toUTF8(type));
  channel->SetContentLength(size);

  channel.forget(result);

  return NS_OK;
}

NS_IMETHODIMP
nsHostObjectProtocolHandler::NewChannel(nsIURI* uri, nsIChannel* *result)
{
  return NewChannel2(uri, nullptr, result);
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
NS_GetBlobForBlobURI(nsIURI* aURI, BlobImpl** aBlob)
{
  NS_ASSERTION(IsBlobURI(aURI), "Only call this with blob URIs");

  *aBlob = nullptr;

  nsCOMPtr<BlobImpl> blob = do_QueryInterface(GetDataObject(aURI));
  if (!blob) {
    return NS_ERROR_DOM_BAD_URI;
  }

  blob.forget(aBlob);
  return NS_OK;
}

nsresult
NS_GetBlobForBlobURISpec(const nsACString& aSpec, BlobImpl** aBlob)
{
  *aBlob = nullptr;

  nsCOMPtr<BlobImpl> blob = do_QueryInterface(GetDataObjectForSpec(aSpec));
  if (!blob) {
    return NS_ERROR_DOM_BAD_URI;
  }

  blob.forget(aBlob);
  return NS_OK;
}

nsresult
NS_GetStreamForBlobURI(nsIURI* aURI, nsIInputStream** aStream)
{
  RefPtr<BlobImpl> blobImpl;
  ErrorResult rv;
  rv = NS_GetBlobForBlobURI(aURI, getter_AddRefs(blobImpl));
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  blobImpl->GetInternalStream(aStream, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  return NS_OK;
}

nsresult
NS_GetStreamForMediaStreamURI(nsIURI* aURI, mozilla::DOMMediaStream** aStream)
{
  NS_ASSERTION(IsMediaStreamURI(aURI), "Only call this with mediastream URIs");

  nsISupports* dataObject = GetDataObject(aURI);
  if (!dataObject) {
    return NS_ERROR_DOM_BAD_URI;
  }

  *aStream = nullptr;
  return CallQueryInterface(dataObject, aStream);
}

NS_IMETHODIMP
nsFontTableProtocolHandler::NewURI(const nsACString& aSpec,
                                   const char *aCharset,
                                   nsIURI *aBaseURI,
                                   nsIURI **aResult)
{
  RefPtr<nsIURI> uri;

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
    uri = new mozilla::net::nsSimpleURI();
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

#define NS_BLOBPROTOCOLHANDLER_CID \
{ 0xb43964aa, 0xa078, 0x44b2, \
  { 0xb0, 0x6b, 0xfd, 0x4d, 0x1b, 0x17, 0x2e, 0x66 } }

#define NS_MEDIASTREAMPROTOCOLHANDLER_CID \
{ 0x27d1fa24, 0x2b73, 0x4db3, \
  { 0xab, 0x48, 0xb9, 0x83, 0x83, 0x40, 0xe0, 0x81 } }

#define NS_MEDIASOURCEPROTOCOLHANDLER_CID \
{ 0x12ef31fc, 0xa8fb, 0x4661, \
  { 0x9a, 0x63, 0xfb, 0x61, 0x04,0x5d, 0xb8, 0x61 } }

#define NS_FONTTABLEPROTOCOLHANDLER_CID \
{ 0x3fc8f04e, 0xd719, 0x43ca, \
  { 0x9a, 0xd0, 0x18, 0xee, 0x32, 0x02, 0x11, 0xf2 } }

NS_GENERIC_FACTORY_CONSTRUCTOR(nsBlobProtocolHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMediaStreamProtocolHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMediaSourceProtocolHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFontTableProtocolHandler)

NS_DEFINE_NAMED_CID(NS_BLOBPROTOCOLHANDLER_CID);
NS_DEFINE_NAMED_CID(NS_MEDIASTREAMPROTOCOLHANDLER_CID);
NS_DEFINE_NAMED_CID(NS_MEDIASOURCEPROTOCOLHANDLER_CID);
NS_DEFINE_NAMED_CID(NS_FONTTABLEPROTOCOLHANDLER_CID);

static const mozilla::Module::CIDEntry kHostObjectProtocolHandlerCIDs[] = {
  { &kNS_BLOBPROTOCOLHANDLER_CID, false, nullptr, nsBlobProtocolHandlerConstructor },
  { &kNS_MEDIASTREAMPROTOCOLHANDLER_CID, false, nullptr, nsMediaStreamProtocolHandlerConstructor },
  { &kNS_MEDIASOURCEPROTOCOLHANDLER_CID, false, nullptr, nsMediaSourceProtocolHandlerConstructor },
  { &kNS_FONTTABLEPROTOCOLHANDLER_CID, false, nullptr, nsFontTableProtocolHandlerConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kHostObjectProtocolHandlerContracts[] = {
  { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX BLOBURI_SCHEME, &kNS_BLOBPROTOCOLHANDLER_CID },
  { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX MEDIASTREAMURI_SCHEME, &kNS_MEDIASTREAMPROTOCOLHANDLER_CID },
  { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX MEDIASOURCEURI_SCHEME, &kNS_MEDIASOURCEPROTOCOLHANDLER_CID },
  { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX FONTTABLEURI_SCHEME, &kNS_FONTTABLEPROTOCOLHANDLER_CID },
  { nullptr }
};

static const mozilla::Module kHostObjectProtocolHandlerModule = {
  mozilla::Module::kVersion,
  kHostObjectProtocolHandlerCIDs,
  kHostObjectProtocolHandlerContracts
};

NSMODULE_DEFN(HostObjectProtocolHandler) = &kHostObjectProtocolHandlerModule;
