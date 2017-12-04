/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHostObjectProtocolHandler.h"

#include "DOMMediaStream.h"
#include "mozilla/dom/ChromeUtils.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/BlobImpl.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/dom/MediaSource.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/SystemGroup.h"
#include "nsClassHashtable.h"
#include "nsContentUtils.h"
#include "nsError.h"
#include "nsHostObjectURI.h"
#include "nsIAsyncShutdown.h"
#include "nsIMemoryReporter.h"
#include "nsIPrincipal.h"
#include "nsIUUIDGenerator.h"
#include "nsNetUtil.h"

#define RELEASING_TIMER 5000

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;

// -----------------------------------------------------------------------
// Hash table
struct DataInfo
{
  enum ObjectType {
    eBlobImpl,
    eMediaStream,
    eMediaSource
  };

  DataInfo(BlobImpl* aBlobImpl, nsIPrincipal* aPrincipal)
    : mObjectType(eBlobImpl)
    , mBlobImpl(aBlobImpl)
    , mPrincipal(aPrincipal)
    , mRevoked(false)
  {}

  DataInfo(DOMMediaStream* aMediaStream, nsIPrincipal* aPrincipal)
    : mObjectType(eMediaStream)
    , mMediaStream(aMediaStream)
    , mPrincipal(aPrincipal)
    , mRevoked(false)
  {}

  DataInfo(MediaSource* aMediaSource, nsIPrincipal* aPrincipal)
    : mObjectType(eMediaSource)
    , mMediaSource(aMediaSource)
    , mPrincipal(aPrincipal)
    , mRevoked(false)
  {}

  ObjectType mObjectType;

  RefPtr<BlobImpl> mBlobImpl;
  RefPtr<DOMMediaStream> mMediaStream;
  RefPtr<MediaSource> mMediaSource;

  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCString mStack;

  // WeakReferences of nsHostObjectURI objects.
  nsTArray<nsWeakPtr> mURIs;

  // When a blobURL is revoked, we keep it alive for RELEASING_TIMER
  // milliseconds in order to support pending operations such as navigation,
  // download and so on.
  bool mRevoked;
};

static nsClassHashtable<nsCStringHashKey, DataInfo>* gDataTable;

static DataInfo*
GetDataInfo(const nsACString& aUri, bool aAlsoIfRevoked = false)
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

  if (!aAlsoIfRevoked && res && res->mRevoked) {
    return nullptr;
  }

  return res;
}

static DataInfo*
GetDataInfoFromURI(nsIURI* aURI)
{
  if (!aURI) {
    return nullptr;
  }

  nsCString spec;
  nsresult rv = aURI->GetSpec(spec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  return GetDataInfo(spec);
}

// Memory reporting for the hash table.
namespace mozilla {

void
BroadcastBlobURLRegistration(const nsACString& aURI,
                             BlobImpl* aBlobImpl,
                             nsIPrincipal* aPrincipal)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aBlobImpl);

  if (XRE_IsParentProcess()) {
    dom::ContentParent::BroadcastBlobURLRegistration(aURI, aBlobImpl,
                                                     aPrincipal);
    return;
  }

  dom::ContentChild* cc = dom::ContentChild::GetSingleton();

  IPCBlob ipcBlob;
  nsresult rv = IPCBlobUtils::Serialize(aBlobImpl, cc, ipcBlob);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  Unused << NS_WARN_IF(!cc->SendStoreAndBroadcastBlobURLRegistration(
    nsCString(aURI), ipcBlob, IPC::Principal(aPrincipal)));
}

void
BroadcastBlobURLUnregistration(const nsCString& aURI)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (XRE_IsParentProcess()) {
    dom::ContentParent::BroadcastBlobURLUnregistration(aURI);
    return;
  }

  dom::ContentChild* cc = dom::ContentChild::GetSingleton();
  Unused << NS_WARN_IF(!cc->SendUnstoreAndBroadcastBlobURLUnregistration(aURI));
}

class HostObjectURLsReporter final : public nsIMemoryReporter
{
  ~HostObjectURLsReporter() {}

 public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    MOZ_COLLECT_REPORT(
      "host-object-urls", KIND_OTHER, UNITS_COUNT,
      gDataTable ? gDataTable->Count() : 0,
      "The number of host objects stored for access via URLs "
      "(e.g. blobs passed to URL.createObjectURL).");

    return NS_OK;
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

    nsDataHashtable<nsPtrHashKey<BlobImpl>, uint32_t> refCounts;

    // Determine number of URLs per BlobImpl, to handle the case where it's > 1.
    for (auto iter = gDataTable->Iter(); !iter.Done(); iter.Next()) {
      if (iter.UserData()->mObjectType != DataInfo::eBlobImpl) {
        continue;
      }

      BlobImpl* blobImpl = iter.UserData()->mBlobImpl;
      MOZ_ASSERT(blobImpl);

      refCounts.Put(blobImpl, refCounts.Get(blobImpl) + 1);
    }

    for (auto iter = gDataTable->Iter(); !iter.Done(); iter.Next()) {
      nsCStringHashKey::KeyType key = iter.Key();
      DataInfo* info = iter.UserData();

      if (iter.UserData()->mObjectType == DataInfo::eBlobImpl) {
        BlobImpl* blobImpl = iter.UserData()->mBlobImpl;
        MOZ_ASSERT(blobImpl);

        NS_NAMED_LITERAL_CSTRING(desc,
          "A blob URL allocated with URL.createObjectURL; the referenced "
          "blob cannot be freed until all URLs for it have been explicitly "
          "invalidated with URL.revokeObjectURL.");
        nsAutoCString path, url, owner, specialDesc;
        uint64_t size = 0;
        uint32_t refCount = 1;
        DebugOnly<bool> blobImplWasCounted;

        blobImplWasCounted = refCounts.Get(blobImpl, &refCount);
        MOZ_ASSERT(blobImplWasCounted);
        MOZ_ASSERT(refCount > 0);

        bool isMemoryFile = blobImpl->IsMemoryFile();

        if (isMemoryFile) {
          ErrorResult rv;
          size = blobImpl->GetSize(rv);
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
          addrStr.AppendInt((uint64_t)(BlobImpl*)blobImpl, 16);

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
        continue;
      }

      // Just report the path for the DOMMediaStream or MediaSource.
      nsAutoCString path;
      path = iter.UserData()->mObjectType == DataInfo::eMediaSource
               ? "media-source-urls/" : "dom-media-stream-urls/";
      BuildPath(path, key, info, aAnonymize);

      NS_NAMED_LITERAL_CSTRING(desc,
        "An object URL allocated with URL.createObjectURL; the referenced "
        "data cannot be freed until all URLs for it have been explicitly "
        "invalidated with URL.revokeObjectURL.");

      aCallback->Callback(EmptyCString(), path, KIND_OTHER, UNITS_COUNT, 1,
                          desc, aData);
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

class ReleasingTimerHolder final : public nsITimerCallback
                                 , public nsINamed
                                 , public nsIAsyncShutdownBlocker
{
public:
  NS_DECL_ISUPPORTS

  static void
  Create(const nsACString& aURI, bool aBroadcastToOtherProcesses)
  {
    MOZ_ASSERT(NS_IsMainThread());

    RefPtr<ReleasingTimerHolder> holder =
      new ReleasingTimerHolder(aURI, aBroadcastToOtherProcesses);

    auto raii = mozilla::MakeScopeExit([&] {
      holder->CancelTimerAndRevokeURI();
    });

    nsresult rv = NS_NewTimerWithCallback(getter_AddRefs(holder->mTimer),
                                          holder, RELEASING_TIMER,
                                          nsITimer::TYPE_ONE_SHOT,
                                          SystemGroup::EventTargetFor(TaskCategory::Other));
    NS_ENSURE_SUCCESS_VOID(rv);

    nsCOMPtr<nsIAsyncShutdownClient> phase = GetShutdownPhase();
    NS_ENSURE_TRUE_VOID(!!phase);

    rv = phase->AddBlocker(holder, NS_LITERAL_STRING(__FILE__), __LINE__,
                           NS_LITERAL_STRING("ReleasingTimerHolder shutdown"));
    NS_ENSURE_SUCCESS_VOID(rv);

    raii.release();
  }

  // nsITimerCallback interface

  NS_IMETHOD
  Notify(nsITimer* aTimer) override
  {
    RevokeURI(mBroadcastToOtherProcesses);
    return NS_OK;
  }

  // nsINamed interface

  NS_IMETHOD
  GetName(nsACString& aName) override
  {
    aName.AssignLiteral("ReleasingTimerHolder");
    return NS_OK;
  }

  // nsIAsyncShutdownBlocker interface

  NS_IMETHOD
  GetName(nsAString& aName) override
  {
    aName.AssignLiteral("ReleasingTimerHolder");
    return NS_OK;
  }

  NS_IMETHOD
  BlockShutdown(nsIAsyncShutdownClient* aClient) override
  {
    CancelTimerAndRevokeURI();
    return NS_OK;
  }

  NS_IMETHOD
  GetState(nsIPropertyBag**) override
  {
    return NS_OK;
  }

private:
  ReleasingTimerHolder(const nsACString& aURI, bool aBroadcastToOtherProcesses)
    : mURI(aURI)
    , mBroadcastToOtherProcesses(aBroadcastToOtherProcesses)
  {}

  ~ReleasingTimerHolder()
  {}

  void
  RevokeURI(bool aBroadcastToOtherProcesses)
  {
    // Remove the shutting down blocker
    nsCOMPtr<nsIAsyncShutdownClient> phase = GetShutdownPhase();
    if (phase) {
      phase->RemoveBlocker(this);
    }

    // If we have to broadcast the unregistration, let's do it now.
    if (aBroadcastToOtherProcesses) {
      BroadcastBlobURLUnregistration(mURI);
    }

    DataInfo* info = GetDataInfo(mURI, true /* We care about revoked dataInfo */);
    if (!info) {
      // Already gone!
      return;
    }

    MOZ_ASSERT(info->mRevoked);

    for (uint32_t i = 0; i < info->mURIs.Length(); ++i) {
      nsCOMPtr<nsIURI> uri = do_QueryReferent(info->mURIs[i]);
      if (uri) {
        static_cast<nsHostObjectURI*>(uri.get())->ForgetBlobImpl();
      }
    }

    gDataTable->Remove(mURI);
    if (gDataTable->Count() == 0) {
      delete gDataTable;
      gDataTable = nullptr;
    }
  }

  void
  CancelTimerAndRevokeURI()
  {
    if (mTimer) {
      mTimer->Cancel();
      mTimer = nullptr;
    }

    RevokeURI(false /* aBroadcastToOtherProcesses */);
  }

  static nsCOMPtr<nsIAsyncShutdownClient>
  GetShutdownPhase()
  {
    nsCOMPtr<nsIAsyncShutdownService> svc = services::GetAsyncShutdown();
    NS_ENSURE_TRUE(!!svc, nullptr);

    nsCOMPtr<nsIAsyncShutdownClient> phase;
    nsresult rv = svc->GetXpcomWillShutdown(getter_AddRefs(phase));
    NS_ENSURE_SUCCESS(rv, nullptr);

    return Move(phase);
  }

  nsCString mURI;
  bool mBroadcastToOtherProcesses;

  nsCOMPtr<nsITimer> mTimer;
};

NS_IMPL_ISUPPORTS(ReleasingTimerHolder, nsITimerCallback, nsINamed,
                  nsIAsyncShutdownBlocker)

} // namespace mozilla

template<typename T>
static nsresult
AddDataEntryInternal(const nsACString& aURI, T aObject,
                     nsIPrincipal* aPrincipal)
{
  if (!gDataTable) {
    gDataTable = new nsClassHashtable<nsCStringHashKey, DataInfo>;
  }

  DataInfo* info = new DataInfo(aObject, aPrincipal);
  mozilla::BlobURLsReporter::GetJSStackForBlob(info);

  gDataTable->Put(aURI, info);
  return NS_OK;
}

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

/* static */ nsresult
nsHostObjectProtocolHandler::AddDataEntry(BlobImpl* aBlobImpl,
                                          nsIPrincipal* aPrincipal,
                                          nsACString& aUri)
{
  Init();

  nsresult rv = GenerateURIStringForBlobURL(aPrincipal, aUri);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddDataEntryInternal(aUri, aBlobImpl, aPrincipal);
  NS_ENSURE_SUCCESS(rv, rv);

  BroadcastBlobURLRegistration(aUri, aBlobImpl, aPrincipal);
  return NS_OK;
}

/* static */ nsresult
nsHostObjectProtocolHandler::AddDataEntry(DOMMediaStream* aMediaStream,
                                          nsIPrincipal* aPrincipal,
                                          nsACString& aUri)
{
  Init();

  nsresult rv = GenerateURIStringForBlobURL(aPrincipal, aUri);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddDataEntryInternal(aUri, aMediaStream, aPrincipal);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

/* static */ nsresult
nsHostObjectProtocolHandler::AddDataEntry(MediaSource* aMediaSource,
                                          nsIPrincipal* aPrincipal,
                                          nsACString& aUri)
{
  Init();

  nsresult rv = GenerateURIStringForBlobURL(aPrincipal, aUri);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddDataEntryInternal(aUri, aMediaSource, aPrincipal);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

/* static */ nsresult
nsHostObjectProtocolHandler::AddDataEntry(const nsACString& aURI,
                                          nsIPrincipal* aPrincipal,
                                          BlobImpl* aBlobImpl)
{
  return AddDataEntryInternal(aURI, aBlobImpl, aPrincipal);
}

/* static */ bool
nsHostObjectProtocolHandler::GetAllBlobURLEntries(
  nsTArray<BlobURLRegistrationData>& aRegistrations,
  ContentParent* aCP)
{
  MOZ_ASSERT(aCP);

  if (!gDataTable) {
    return true;
  }

  for (auto iter = gDataTable->ConstIter(); !iter.Done(); iter.Next()) {
    DataInfo* info = iter.UserData();
    MOZ_ASSERT(info);

    if (info->mObjectType != DataInfo::eBlobImpl) {
      continue;
    }

    MOZ_ASSERT(info->mBlobImpl);

    IPCBlob ipcBlob;
    nsresult rv = IPCBlobUtils::Serialize(info->mBlobImpl, aCP, ipcBlob);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }

    aRegistrations.AppendElement(BlobURLRegistrationData(
      nsCString(iter.Key()), ipcBlob, IPC::Principal(info->mPrincipal),
                info->mRevoked));
  }

  return true;
}

/*static */ void
nsHostObjectProtocolHandler::RemoveDataEntry(const nsACString& aUri,
                                             bool aBroadcastToOtherProcesses)
{
  if (!gDataTable) {
    return;
  }

  DataInfo* info = GetDataInfo(aUri);
  if (!info) {
    return;
  }

  info->mRevoked = true;

  // The timer will take care of removing the entry for real after
  // RELEASING_TIMER milliseconds. In the meantime, the DataInfo, marked as
  // revoked, will not be exposed.
  ReleasingTimerHolder::Create(aUri,
                               aBroadcastToOtherProcesses &&
                                 info->mObjectType == DataInfo::eBlobImpl);
}

/* static */ void
nsHostObjectProtocolHandler::RemoveDataEntries()
{
  if (!gDataTable) {
    return;
  }

  gDataTable->Clear();
  delete gDataTable;
  gDataTable = nullptr;
}

/* static */ bool
nsHostObjectProtocolHandler::HasDataEntry(const nsACString& aUri)
{
  return !!GetDataInfo(aUri);
}

/* static */ nsresult
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

/* static */ nsresult
nsHostObjectProtocolHandler::GenerateURIStringForBlobURL(nsIPrincipal* aPrincipal,
                                                         nsACString& aUri)
{
  return
    GenerateURIString(NS_LITERAL_CSTRING(BLOBURI_SCHEME), aPrincipal, aUri);
}

/* static */ nsIPrincipal*
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

/* static */ void
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

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCallback, "HostObjectProtocolHandler DataInfo.mBlobImpl");
  aCallback.NoteXPCOMChild(res->mBlobImpl);

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCallback, "HostObjectProtocolHandler DataInfo.mMediaSource");
  aCallback.NoteXPCOMChild(res->mMediaSource);

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCallback, "HostObjectProtocolHandler DataInfo.mMediaStream");
  aCallback.NoteXPCOMChild(res->mMediaStream);
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
            URI_NON_PERSISTABLE;
  return NS_OK;
}

NS_IMETHODIMP
nsHostObjectProtocolHandler::GetFlagsForURI(nsIURI *aURI, uint32_t *aResult)
{
  Unused << nsHostObjectProtocolHandler::GetProtocolFlags(aResult);
  if (IsFontTableURI(aURI) || IsBlobURI(aURI)) {
    *aResult |= URI_IS_LOCAL_RESOURCE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBlobProtocolHandler::GetProtocolFlags(uint32_t *result)
{
  Unused << nsHostObjectProtocolHandler::GetProtocolFlags(result);
  *result |= URI_IS_LOCAL_RESOURCE;
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

  RefPtr<nsHostObjectURI> uri;
  if (info && info->mObjectType == DataInfo::eBlobImpl) {
    MOZ_ASSERT(info->mBlobImpl);
    uri = new nsHostObjectURI(info->mPrincipal, info->mBlobImpl);
  } else {
    uri = new nsHostObjectURI(nullptr, nullptr);
  }

  rv = uri->SetSpec(aSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_TryToSetImmutable(uri);
  uri.forget(aResult);

  if (info && info->mObjectType == DataInfo::eBlobImpl) {
    info->mURIs.AppendElement(do_GetWeakReference(*aResult));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHostObjectProtocolHandler::NewChannel2(nsIURI* uri,
                                         nsILoadInfo* aLoadInfo,
                                         nsIChannel** result)
{
  *result = nullptr;

  nsCOMPtr<nsIURIWithBlobImpl> uriBlobImpl = do_QueryInterface(uri);
  if (!uriBlobImpl) {
    return NS_ERROR_DOM_BAD_URI;
  }

  nsCOMPtr<nsISupports> tmp;
  MOZ_ALWAYS_SUCCEEDS(uriBlobImpl->GetBlobImpl(getter_AddRefs(tmp)));
  nsCOMPtr<BlobImpl> blobImpl = do_QueryInterface(tmp);
  if (!blobImpl) {
    return NS_ERROR_DOM_BAD_URI;
  }

  nsCOMPtr<nsIURIWithPrincipal> uriPrinc = do_QueryInterface(uri);
  if (!uriPrinc) {
    return NS_ERROR_DOM_BAD_URI;
  }

  nsCOMPtr<nsIPrincipal> principal;
  nsresult rv = uriPrinc->GetPrincipal(getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
  // Info can be null, in case this blob URL has been revoked already.
  DataInfo* info = GetDataInfoFromURI(uri);
  MOZ_ASSERT_IF(info, info->mPrincipal == principal);
#endif

  // We want to be sure that we stop the creation of the channel if the blob URL
  // is copy-and-pasted on a different context (ex. private browsing or
  // containers).
  if (aLoadInfo &&
      !ChromeUtils::IsOriginAttributesEqualIgnoringFPD(aLoadInfo->GetOriginAttributes(),
                                                         BasePrincipal::Cast(principal)->OriginAttributesRef())) {
    return NS_ERROR_DOM_BAD_URI;
  }

  ErrorResult error;
  nsCOMPtr<nsIInputStream> stream;
  blobImpl->CreateInputStream(getter_AddRefs(stream), error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  nsAutoString contentType;
  blobImpl->GetType(contentType);

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewInputStreamChannelInternal(getter_AddRefs(channel),
                                        uri,
                                        stream.forget(),
                                        NS_ConvertUTF16toUTF8(contentType),
                                        EmptyCString(), // aContentCharset
                                        aLoadInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (blobImpl->IsFile()) {
    nsString filename;
    blobImpl->GetName(filename);
    channel->SetContentDispositionFilename(filename);
  }

  uint64_t size = blobImpl->GetSize(error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  channel->SetOriginalURI(uri);
  channel->SetContentType(NS_ConvertUTF16toUTF8(contentType));
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
nsFontTableProtocolHandler::GetProtocolFlags(uint32_t *result)
{
  Unused << nsHostObjectProtocolHandler::GetProtocolFlags(result);
  *result |= URI_IS_LOCAL_RESOURCE;
  return NS_OK;
}

NS_IMETHODIMP
nsFontTableProtocolHandler::GetScheme(nsACString &result)
{
  result.AssignLiteral(FONTTABLEURI_SCHEME);
  return NS_OK;
}

/* static */ void
nsHostObjectProtocolHandler::StoreClonedURI(const nsACString& aSpec,
                                            nsIURI* aURI)
{
  MOZ_ASSERT(aURI);

  DataInfo* info = GetDataInfo(aSpec);
  if (info) {
    info->mURIs.AppendElement(do_GetWeakReference(aURI));
  }
}

nsresult
NS_GetBlobForBlobURI(nsIURI* aURI, BlobImpl** aBlob)
{
  *aBlob = nullptr;

  DataInfo* info = GetDataInfoFromURI(aURI);
  if (!info || info->mObjectType != DataInfo::eBlobImpl) {
    return NS_ERROR_DOM_BAD_URI;
  }

  RefPtr<BlobImpl> blob = info->mBlobImpl;
  blob.forget(aBlob);
  return NS_OK;
}

nsresult
NS_GetBlobForBlobURISpec(const nsACString& aSpec, BlobImpl** aBlob)
{
  *aBlob = nullptr;

  DataInfo* info = GetDataInfo(aSpec);
  if (!info || info->mObjectType != DataInfo::eBlobImpl) {
    return NS_ERROR_DOM_BAD_URI;
  }

  RefPtr<BlobImpl> blob = info->mBlobImpl;
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

  blobImpl->CreateInputStream(aStream, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  return NS_OK;
}

nsresult
NS_GetStreamForMediaStreamURI(nsIURI* aURI, DOMMediaStream** aStream)
{
  DataInfo* info = GetDataInfoFromURI(aURI);
  if (!info || info->mObjectType != DataInfo::eMediaStream) {
    return NS_ERROR_DOM_BAD_URI;
  }

  RefPtr<DOMMediaStream> mediaStream = info->mMediaStream;
  mediaStream.forget(aStream);
  return NS_OK;
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
    nsresult rv = uri->SetSpec(aSpec);
    NS_ENSURE_SUCCESS(rv, rv);
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
NS_GetSourceForMediaSourceURI(nsIURI* aURI, MediaSource** aSource)
{
  *aSource = nullptr;

  DataInfo* info = GetDataInfoFromURI(aURI);
  if (!info || info->mObjectType != DataInfo::eMediaSource) {
    return NS_ERROR_DOM_BAD_URI;
  }

  RefPtr<MediaSource> mediaSource = info->mMediaSource;
  mediaSource.forget(aSource);
  return NS_OK;
}

#define NS_BLOBPROTOCOLHANDLER_CID \
{ 0xb43964aa, 0xa078, 0x44b2, \
  { 0xb0, 0x6b, 0xfd, 0x4d, 0x1b, 0x17, 0x2e, 0x66 } }

#define NS_FONTTABLEPROTOCOLHANDLER_CID \
{ 0x3fc8f04e, 0xd719, 0x43ca, \
  { 0x9a, 0xd0, 0x18, 0xee, 0x32, 0x02, 0x11, 0xf2 } }

NS_GENERIC_FACTORY_CONSTRUCTOR(nsBlobProtocolHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFontTableProtocolHandler)

NS_DEFINE_NAMED_CID(NS_BLOBPROTOCOLHANDLER_CID);
NS_DEFINE_NAMED_CID(NS_FONTTABLEPROTOCOLHANDLER_CID);

static const mozilla::Module::CIDEntry kHostObjectProtocolHandlerCIDs[] = {
  { &kNS_BLOBPROTOCOLHANDLER_CID, false, nullptr, nsBlobProtocolHandlerConstructor },
  { &kNS_FONTTABLEPROTOCOLHANDLER_CID, false, nullptr, nsFontTableProtocolHandlerConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kHostObjectProtocolHandlerContracts[] = {
  { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX BLOBURI_SCHEME, &kNS_BLOBPROTOCOLHANDLER_CID },
  { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX FONTTABLEURI_SCHEME, &kNS_FONTTABLEPROTOCOLHANDLER_CID },
  { nullptr }
};

static const mozilla::Module kHostObjectProtocolHandlerModule = {
  mozilla::Module::kVersion,
  kHostObjectProtocolHandlerCIDs,
  kHostObjectProtocolHandlerContracts
};

NSMODULE_DEFN(HostObjectProtocolHandler) = &kHostObjectProtocolHandlerModule;

bool IsType(nsIURI* aUri, DataInfo::ObjectType aType)
{
  DataInfo* info = GetDataInfoFromURI(aUri);
  if (!info) {
    return false;
  }

  return info->mObjectType == aType;
}

bool IsBlobURI(nsIURI* aUri)
{
  return IsType(aUri, DataInfo::eBlobImpl);
}

bool IsMediaStreamURI(nsIURI* aUri)
{
  return IsType(aUri, DataInfo::eMediaStream);
}

bool IsMediaSourceURI(nsIURI* aUri)
{
  return IsType(aUri, DataInfo::eMediaSource);
}
