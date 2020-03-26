/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BlobURLProtocolHandler.h"
#include "BlobURLChannel.h"
#include "mozilla/dom/BlobURL.h"

#include "mozilla/dom/ChromeUtils.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/BlobImpl.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/dom/MediaSource.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/Preferences.h"
#include "mozilla/SystemGroup.h"
#include "nsClassHashtable.h"
#include "nsContentUtils.h"
#include "nsError.h"
#include "nsIAsyncShutdown.h"
#include "nsIException.h"  // for nsIStackFrame
#include "nsIMemoryReporter.h"
#include "nsIPrincipal.h"
#include "nsIUUIDGenerator.h"
#include "nsNetUtil.h"

#define RELEASING_TIMER 5000

namespace mozilla {

using namespace ipc;

namespace dom {

// -----------------------------------------------------------------------
// Hash table
struct DataInfo {
  enum ObjectType { eBlobImpl, eMediaSource };

  DataInfo(BlobImpl* aBlobImpl, nsIPrincipal* aPrincipal)
      : mObjectType(eBlobImpl),
        mBlobImpl(aBlobImpl),
        mPrincipal(aPrincipal),
        mRevoked(false) {
    MOZ_ASSERT(aPrincipal);
  }

  DataInfo(MediaSource* aMediaSource, nsIPrincipal* aPrincipal)
      : mObjectType(eMediaSource),
        mMediaSource(aMediaSource),
        mPrincipal(aPrincipal),
        mRevoked(false) {
    MOZ_ASSERT(aPrincipal);
  }

  ObjectType mObjectType;

  RefPtr<BlobImpl> mBlobImpl;
  RefPtr<MediaSource> mMediaSource;

  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCString mStack;

  // When a blobURL is revoked, we keep it alive for RELEASING_TIMER
  // milliseconds in order to support pending operations such as navigation,
  // download and so on.
  bool mRevoked;
};

// The mutex is locked whenever gDataTable is changed, or if gDataTable
// is accessed off-main-thread.
static StaticMutex sMutex;

// All changes to gDataTable must happen on the main thread, while locking
// sMutex. Reading from gDataTable on the main thread may happen without
// locking, since no changes are possible. Reading it from another thread
// must also lock sMutex to prevent data races.
static nsClassHashtable<nsCStringHashKey, DataInfo>* gDataTable;

static DataInfo* GetDataInfo(const nsACString& aUri,
                             bool aAlsoIfRevoked = false) {
  if (!gDataTable) {
    return nullptr;
  }

  // Let's remove any fragment from this URI.
  int32_t fragmentPos = aUri.FindChar('#');

  DataInfo* res;
  if (fragmentPos < 0) {
    res = gDataTable->Get(aUri);
  } else {
    res = gDataTable->Get(StringHead(aUri, fragmentPos));
  }

  if (!aAlsoIfRevoked && res && res->mRevoked) {
    return nullptr;
  }

  return res;
}

static DataInfo* GetDataInfoFromURI(nsIURI* aURI, bool aAlsoIfRevoked = false) {
  if (!aURI) {
    return nullptr;
  }

  nsCString spec;
  nsresult rv = aURI->GetSpec(spec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  return GetDataInfo(spec, aAlsoIfRevoked);
}

// Memory reporting for the hash table.
void BroadcastBlobURLRegistration(const nsACString& aURI, BlobImpl* aBlobImpl,
                                  nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aBlobImpl);
  MOZ_ASSERT(aPrincipal);

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

void BroadcastBlobURLUnregistration(const nsCString& aURI,
                                    nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(NS_IsMainThread());

  if (XRE_IsParentProcess()) {
    dom::ContentParent::BroadcastBlobURLUnregistration(aURI, aPrincipal);
    return;
  }

  dom::ContentChild* cc = dom::ContentChild::GetSingleton();
  Unused << NS_WARN_IF(!cc->SendUnstoreAndBroadcastBlobURLUnregistration(
      aURI, IPC::Principal(aPrincipal)));
}

class BlobURLsReporter final : public nsIMemoryReporter {
 public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aCallback,
                            nsISupports* aData, bool aAnonymize) override {
    MOZ_ASSERT(NS_IsMainThread(),
               "without locking gDataTable is main-thread only");
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

        NS_NAMED_LITERAL_CSTRING(
            desc,
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

        const nsACString& descString =
            specialDesc.IsEmpty() ? static_cast<const nsACString&>(desc)
                                  : static_cast<const nsACString&>(specialDesc);
        if (isMemoryFile) {
          aCallback->Callback(EmptyCString(), path, KIND_OTHER, UNITS_BYTES,
                              size / refCount, descString, aData);
        } else {
          aCallback->Callback(EmptyCString(), path, KIND_OTHER, UNITS_COUNT, 1,
                              descString, aData);
        }
        continue;
      }

      // Just report the path for the MediaSource.
      nsAutoCString path;
      path = "media-source-urls/";
      BuildPath(path, key, info, aAnonymize);

      NS_NAMED_LITERAL_CSTRING(
          desc,
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
  static void GetJSStackForBlob(DataInfo* aInfo) {
    nsCString& stack = aInfo->mStack;
    MOZ_ASSERT(stack.IsEmpty());
    const uint32_t maxFrames =
        Preferences::GetUint("memory.blob_report.stack_frames");

    if (maxFrames == 0) {
      return;
    }

    nsCOMPtr<nsIStackFrame> frame = dom::GetCurrentJSStack(maxFrames);

    nsAutoCString origin;

    aInfo->mPrincipal->GetPrepath(origin);

    // If we got a frame, we better have a current JSContext.  This is cheating
    // a bit; ideally we'd have our caller pass in a JSContext, or have
    // GetCurrentJSStack() hand out the JSContext it found.
    JSContext* cx = frame ? nsContentUtils::GetCurrentJSContext() : nullptr;

    for (uint32_t i = 0; frame; ++i) {
      nsString fileNameUTF16;
      frame->GetFilename(cx, fileNameUTF16);

      int32_t lineNumber = frame->GetLineNumber(cx);

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

      frame = frame->GetCaller(cx);
    }
  }

 private:
  ~BlobURLsReporter() = default;

  static void BuildPath(nsAutoCString& path, nsCStringHashKey::KeyType aKey,
                        DataInfo* aInfo, bool anonymize) {
    nsAutoCString url, owner;
    aInfo->mPrincipal->GetAsciiSpec(owner);
    if (!owner.IsEmpty()) {
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

class ReleasingTimerHolder final : public Runnable,
                                   public nsITimerCallback,
                                   public nsIAsyncShutdownBlocker {
 public:
  NS_DECL_ISUPPORTS_INHERITED

  static void Create(const nsACString& aURI) {
    MOZ_ASSERT(NS_IsMainThread());

    RefPtr<ReleasingTimerHolder> holder = new ReleasingTimerHolder(aURI);

    auto raii = MakeScopeExit([holder] { holder->CancelTimerAndRevokeURI(); });

    nsresult rv = SystemGroup::EventTargetFor(TaskCategory::Other)
                      ->Dispatch(holder.forget());
    NS_ENSURE_SUCCESS_VOID(rv);

    raii.release();
  }

  // Runnable interface

  NS_IMETHOD
  Run() override {
    RefPtr<ReleasingTimerHolder> self = this;
    auto raii = MakeScopeExit([self] { self->CancelTimerAndRevokeURI(); });

    nsresult rv = NS_NewTimerWithCallback(
        getter_AddRefs(mTimer), this, RELEASING_TIMER, nsITimer::TYPE_ONE_SHOT,
        SystemGroup::EventTargetFor(TaskCategory::Other));
    NS_ENSURE_SUCCESS(rv, NS_OK);

    nsCOMPtr<nsIAsyncShutdownClient> phase = GetShutdownPhase();
    NS_ENSURE_TRUE(!!phase, NS_OK);

    rv = phase->AddBlocker(this, NS_LITERAL_STRING(__FILE__), __LINE__,
                           NS_LITERAL_STRING("ReleasingTimerHolder shutdown"));
    NS_ENSURE_SUCCESS(rv, NS_OK);

    raii.release();
    return NS_OK;
  }

  // nsITimerCallback interface

  NS_IMETHOD
  Notify(nsITimer* aTimer) override {
    RevokeURI();
    return NS_OK;
  }

#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
  using nsINamed::GetName;
#endif

  // nsIAsyncShutdownBlocker interface

  NS_IMETHOD
  GetName(nsAString& aName) override {
    aName.AssignLiteral("ReleasingTimerHolder for blobURL: ");
    aName.Append(NS_ConvertUTF8toUTF16(mURI));
    return NS_OK;
  }

  NS_IMETHOD
  BlockShutdown(nsIAsyncShutdownClient* aClient) override {
    CancelTimerAndRevokeURI();
    return NS_OK;
  }

  NS_IMETHOD
  GetState(nsIPropertyBag**) override { return NS_OK; }

 private:
  explicit ReleasingTimerHolder(const nsACString& aURI)
      : Runnable("ReleasingTimerHolder"), mURI(aURI) {}

  ~ReleasingTimerHolder() = default;

  void RevokeURI() {
    // Remove the shutting down blocker
    nsCOMPtr<nsIAsyncShutdownClient> phase = GetShutdownPhase();
    if (phase) {
      phase->RemoveBlocker(this);
    }

    MOZ_ASSERT(NS_IsMainThread(),
               "without locking gDataTable is main-thread only");
    DataInfo* info =
        GetDataInfo(mURI, true /* We care about revoked dataInfo */);
    if (!info) {
      // Already gone!
      return;
    }

    MOZ_ASSERT(info->mRevoked);

    StaticMutexAutoLock lock(sMutex);
    gDataTable->Remove(mURI);
    if (gDataTable->Count() == 0) {
      delete gDataTable;
      gDataTable = nullptr;
    }
  }

  void CancelTimerAndRevokeURI() {
    if (mTimer) {
      mTimer->Cancel();
      mTimer = nullptr;
    }

    RevokeURI();
  }

  static nsCOMPtr<nsIAsyncShutdownClient> GetShutdownPhase() {
    nsCOMPtr<nsIAsyncShutdownService> svc = services::GetAsyncShutdown();
    NS_ENSURE_TRUE(!!svc, nullptr);

    nsCOMPtr<nsIAsyncShutdownClient> phase;
    nsresult rv = svc->GetXpcomWillShutdown(getter_AddRefs(phase));
    NS_ENSURE_SUCCESS(rv, nullptr);

    return phase;
  }

  nsCString mURI;
  nsCOMPtr<nsITimer> mTimer;
};

NS_IMPL_ISUPPORTS_INHERITED(ReleasingTimerHolder, Runnable, nsITimerCallback,
                            nsIAsyncShutdownBlocker)

template <typename T>
static void AddDataEntryInternal(const nsACString& aURI, T aObject,
                                 nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(NS_IsMainThread(), "changing gDataTable is main-thread only");
  StaticMutexAutoLock lock(sMutex);
  if (!gDataTable) {
    gDataTable = new nsClassHashtable<nsCStringHashKey, DataInfo>;
  }

  DataInfo* info = new DataInfo(aObject, aPrincipal);
  BlobURLsReporter::GetJSStackForBlob(info);

  gDataTable->Put(aURI, info);
}

void BlobURLProtocolHandler::Init(void) {
  static bool initialized = false;

  if (!initialized) {
    initialized = true;
    RegisterStrongMemoryReporter(new BlobURLsReporter());
  }
}

BlobURLProtocolHandler::BlobURLProtocolHandler() { Init(); }

BlobURLProtocolHandler::~BlobURLProtocolHandler() = default;

/* static */
nsresult BlobURLProtocolHandler::AddDataEntry(BlobImpl* aBlobImpl,
                                              nsIPrincipal* aPrincipal,
                                              nsACString& aUri) {
  MOZ_ASSERT(aBlobImpl);
  MOZ_ASSERT(aPrincipal);

  Init();

  nsresult rv = GenerateURIString(aPrincipal, aUri);
  NS_ENSURE_SUCCESS(rv, rv);

  AddDataEntryInternal(aUri, aBlobImpl, aPrincipal);

  BroadcastBlobURLRegistration(aUri, aBlobImpl, aPrincipal);
  return NS_OK;
}

/* static */
nsresult BlobURLProtocolHandler::AddDataEntry(MediaSource* aMediaSource,
                                              nsIPrincipal* aPrincipal,
                                              nsACString& aUri) {
  MOZ_ASSERT(aMediaSource);
  MOZ_ASSERT(aPrincipal);

  Init();

  nsresult rv = GenerateURIString(aPrincipal, aUri);
  NS_ENSURE_SUCCESS(rv, rv);

  AddDataEntryInternal(aUri, aMediaSource, aPrincipal);
  return NS_OK;
}

/* static */
void BlobURLProtocolHandler::AddDataEntry(const nsACString& aURI,
                                          nsIPrincipal* aPrincipal,
                                          BlobImpl* aBlobImpl) {
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aBlobImpl);
  AddDataEntryInternal(aURI, aBlobImpl, aPrincipal);
}

/* static */
bool BlobURLProtocolHandler::ForEachBlobURL(
    std::function<bool(BlobImpl*, nsIPrincipal*, const nsACString&,
                       bool aRevoked)>&& aCb) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!gDataTable) {
    return false;
  }

  for (auto iter = gDataTable->ConstIter(); !iter.Done(); iter.Next()) {
    DataInfo* info = iter.UserData();
    MOZ_ASSERT(info);

    if (info->mObjectType != DataInfo::eBlobImpl) {
      continue;
    }

    MOZ_ASSERT(info->mBlobImpl);
    if (!aCb(info->mBlobImpl, info->mPrincipal, iter.Key(), info->mRevoked)) {
      return false;
    }
  }

  return true;
}

/*static */
void BlobURLProtocolHandler::RemoveDataEntry(const nsACString& aUri,
                                             bool aBroadcastToOtherProcesses) {
  MOZ_ASSERT(NS_IsMainThread(), "changing gDataTable is main-thread only");
  if (!gDataTable) {
    return;
  }
  DataInfo* info = GetDataInfo(aUri);
  if (!info) {
    return;
  }

  {
    StaticMutexAutoLock lock(sMutex);
    info->mRevoked = true;
  }

  if (aBroadcastToOtherProcesses && info->mObjectType == DataInfo::eBlobImpl) {
    BroadcastBlobURLUnregistration(nsCString(aUri), info->mPrincipal);
  }

  // The timer will take care of removing the entry for real after
  // RELEASING_TIMER milliseconds. In the meantime, the DataInfo, marked as
  // revoked, will not be exposed.
  ReleasingTimerHolder::Create(aUri);
}

/* static */
void BlobURLProtocolHandler::RemoveDataEntries() {
  MOZ_ASSERT(NS_IsMainThread(), "changing gDataTable is main-thread only");
  StaticMutexAutoLock lock(sMutex);
  if (!gDataTable) {
    return;
  }

  gDataTable->Clear();
  delete gDataTable;
  gDataTable = nullptr;
}

/* static */
bool BlobURLProtocolHandler::HasDataEntry(const nsACString& aUri) {
  MOZ_ASSERT(NS_IsMainThread(),
             "without locking gDataTable is main-thread only");
  return !!GetDataInfo(aUri);
}

/* static */
nsresult BlobURLProtocolHandler::GenerateURIString(nsIPrincipal* aPrincipal,
                                                   nsACString& aUri) {
  nsresult rv;
  nsCOMPtr<nsIUUIDGenerator> uuidgen =
      do_GetService("@mozilla.org/uuid-generator;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsID id;
  rv = uuidgen->GenerateUUIDInPlace(&id);
  NS_ENSURE_SUCCESS(rv, rv);

  char chars[NSID_LENGTH];
  id.ToProvidedString(chars);

  aUri.AssignLiteral(BLOBURI_SCHEME);
  aUri.Append(':');

  if (aPrincipal) {
    nsAutoCString origin;
    rv = aPrincipal->GetAsciiOrigin(origin);
    if (NS_FAILED(rv)) {
      origin.AssignLiteral("null");
    }

    aUri.Append(origin);
    aUri.Append('/');
  }

  aUri += Substring(chars + 1, chars + NSID_LENGTH - 2);

  return NS_OK;
}

/* static */
nsIPrincipal* BlobURLProtocolHandler::GetDataEntryPrincipal(
    const nsACString& aUri) {
  MOZ_ASSERT(NS_IsMainThread(),
             "without locking gDataTable is main-thread only");
  if (!gDataTable) {
    return nullptr;
  }

  DataInfo* res = GetDataInfo(aUri);

  if (!res) {
    return nullptr;
  }

  return res->mPrincipal;
}

/* static */
void BlobURLProtocolHandler::Traverse(
    const nsACString& aUri, nsCycleCollectionTraversalCallback& aCallback) {
  MOZ_ASSERT(NS_IsMainThread(),
             "without locking gDataTable is main-thread only");
  if (!gDataTable) {
    return;
  }

  DataInfo* res;
  gDataTable->Get(aUri, &res);
  if (!res) {
    return;
  }

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(
      aCallback, "BlobURLProtocolHandler DataInfo.mBlobImpl");
  aCallback.NoteXPCOMChild(res->mBlobImpl);

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(
      aCallback, "BlobURLProtocolHandler DataInfo.mMediaSource");
  aCallback.NoteXPCOMChild(res->mMediaSource);
}

NS_IMPL_ISUPPORTS(BlobURLProtocolHandler, nsIProtocolHandler,
                  nsISupportsWeakReference)

NS_IMETHODIMP
BlobURLProtocolHandler::GetDefaultPort(int32_t* result) {
  *result = -1;
  return NS_OK;
}

NS_IMETHODIMP
BlobURLProtocolHandler::GetProtocolFlags(uint32_t* result) {
  *result = URI_NORELATIVE | URI_NOAUTH | URI_LOADABLE_BY_SUBSUMERS |
            URI_NON_PERSISTABLE | URI_IS_LOCAL_RESOURCE;
  return NS_OK;
}

NS_IMETHODIMP
BlobURLProtocolHandler::GetFlagsForURI(nsIURI* aURI, uint32_t* aResult) {
  Unused << BlobURLProtocolHandler::GetProtocolFlags(aResult);
  if (IsBlobURI(aURI)) {
    *aResult |= URI_IS_LOCAL_RESOURCE;
  }

  return NS_OK;
}

/* static */ nsresult BlobURLProtocolHandler::CreateNewURI(
    const nsACString& aSpec, const char* aCharset, nsIURI* aBaseURI,
    nsIURI** aResult) {
  *aResult = nullptr;

  // This method can be called on any thread, which is why we lock the mutex
  // for read access to gDataTable.
  bool revoked = true;
  {
    StaticMutexAutoLock lock(sMutex);
    DataInfo* info = GetDataInfo(aSpec);
    if (info && info->mObjectType == DataInfo::eBlobImpl) {
      revoked = info->mRevoked;
    }
  }

  return NS_MutateURI(new BlobURL::Mutator())
      .SetSpec(aSpec)
      .Apply(NS_MutatorMethod(&nsIBlobURLMutator::SetRevoked, revoked))
      .Finalize(aResult);
}

NS_IMETHODIMP
BlobURLProtocolHandler::NewChannel(nsIURI* aURI, nsILoadInfo* aLoadInfo,
                                   nsIChannel** aResult) {
  RefPtr<BlobURLChannel> channel = new BlobURLChannel(aURI, aLoadInfo);

  auto raii = MakeScopeExit([&] {
    channel->InitFailed();
    channel.forget(aResult);
  });

  RefPtr<BlobURL> blobURL;
  nsresult rv =
      aURI->QueryInterface(kHOSTOBJECTURICID, getter_AddRefs(blobURL));
  if (NS_FAILED(rv) || !blobURL) {
    return NS_OK;
  }

  MOZ_ASSERT(NS_IsMainThread(),
             "without locking gDataTable is main-thread only");
  DataInfo* info = GetDataInfoFromURI(aURI, true /*aAlsoIfRevoked */);
  if (!info || info->mObjectType != DataInfo::eBlobImpl || !info->mBlobImpl) {
    return NS_OK;
  }

  if (blobURL->Revoked()) {
    return NS_OK;
  }

  // We want to be sure that we stop the creation of the channel if the blob URL
  // is copy-and-pasted on a different context (ex. private browsing or
  // containers).
  //
  // We also allow the system principal to create the channel regardless of the
  // OriginAttributes.  This is primarily for the benefit of mechanisms like
  // the Download API that explicitly create a channel with the system
  // principal and which is never mutated to have a non-zero mPrivateBrowsingId
  // or container.
  if (aLoadInfo &&
      (!aLoadInfo->LoadingPrincipal() ||
       !aLoadInfo->LoadingPrincipal()->IsSystemPrincipal()) &&
      !ChromeUtils::IsOriginAttributesEqualIgnoringFPD(
          aLoadInfo->GetOriginAttributes(),
          BasePrincipal::Cast(info->mPrincipal)->OriginAttributesRef())) {
    return NS_OK;
  }

  raii.release();

  channel->Initialize(info->mBlobImpl);
  channel.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
BlobURLProtocolHandler::AllowPort(int32_t port, const char* scheme,
                                  bool* _retval) {
  // don't override anything.
  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
BlobURLProtocolHandler::GetScheme(nsACString& result) {
  result.AssignLiteral(BLOBURI_SCHEME);
  return NS_OK;
}

/* static */
bool BlobURLProtocolHandler::GetBlobURLPrincipal(nsIURI* aURI,
                                                 nsIPrincipal** aPrincipal) {
  MOZ_ASSERT(aURI);
  MOZ_ASSERT(aPrincipal);

  RefPtr<BlobURL> blobURL;
  nsresult rv =
      aURI->QueryInterface(kHOSTOBJECTURICID, getter_AddRefs(blobURL));
  if (NS_FAILED(rv) || !blobURL) {
    return false;
  }

  MOZ_ASSERT(NS_IsMainThread(),
             "without locking gDataTable is main-thread only");
  DataInfo* info = GetDataInfoFromURI(aURI, true /*aAlsoIfRevoked */);
  if (!info || info->mObjectType != DataInfo::eBlobImpl || !info->mBlobImpl) {
    return false;
  }

  nsCOMPtr<nsIPrincipal> principal;

  if (blobURL->Revoked()) {
    principal = NullPrincipal::Create(
        BasePrincipal::Cast(info->mPrincipal)->OriginAttributesRef());
  } else {
    principal = info->mPrincipal;
  }

  principal.forget(aPrincipal);
  return true;
}

}  // namespace dom
}  // namespace mozilla

nsresult NS_GetBlobForBlobURI(nsIURI* aURI, BlobImpl** aBlob) {
  *aBlob = nullptr;
  MOZ_ASSERT(NS_IsMainThread(),
             "without locking gDataTable is main-thread only");
  DataInfo* info = GetDataInfoFromURI(aURI, false /* aAlsoIfRevoked */);
  if (!info || info->mObjectType != DataInfo::eBlobImpl) {
    return NS_ERROR_DOM_BAD_URI;
  }

  RefPtr<BlobImpl> blob = info->mBlobImpl;
  blob.forget(aBlob);
  return NS_OK;
}

nsresult NS_GetBlobForBlobURISpec(const nsACString& aSpec, BlobImpl** aBlob) {
  *aBlob = nullptr;
  MOZ_ASSERT(NS_IsMainThread(),
             "without locking gDataTable is main-thread only");

  DataInfo* info = GetDataInfo(aSpec);
  if (!info || info->mObjectType != DataInfo::eBlobImpl) {
    return NS_ERROR_DOM_BAD_URI;
  }

  RefPtr<BlobImpl> blob = info->mBlobImpl;
  blob.forget(aBlob);
  return NS_OK;
}

nsresult NS_GetSourceForMediaSourceURI(nsIURI* aURI, MediaSource** aSource) {
  *aSource = nullptr;

  MOZ_ASSERT(NS_IsMainThread(),
             "without locking gDataTable is main-thread only");
  DataInfo* info = GetDataInfoFromURI(aURI);
  if (!info || info->mObjectType != DataInfo::eMediaSource) {
    return NS_ERROR_DOM_BAD_URI;
  }

  RefPtr<MediaSource> mediaSource = info->mMediaSource;
  mediaSource.forget(aSource);
  return NS_OK;
}

namespace mozilla {
namespace dom {

bool IsType(nsIURI* aUri, DataInfo::ObjectType aType) {
  // We lock because this may be called off-main-thread
  StaticMutexAutoLock lock(sMutex);
  DataInfo* info = GetDataInfoFromURI(aUri);
  if (!info) {
    return false;
  }

  return info->mObjectType == aType;
}

bool IsBlobURI(nsIURI* aUri) { return IsType(aUri, DataInfo::eBlobImpl); }

bool IsMediaSourceURI(nsIURI* aUri) {
  return IsType(aUri, DataInfo::eMediaSource);
}

}  // namespace dom
}  // namespace mozilla
