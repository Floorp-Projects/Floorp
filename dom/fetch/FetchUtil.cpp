/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FetchUtil.h"

#include "zlib.h"

#include "js/friend/ErrorMessages.h"  // JSMSG_*
#include "nsCRT.h"
#include "nsError.h"
#include "nsIAsyncInputStream.h"
#include "nsICloneableInputStream.h"
#include "nsIHttpChannel.h"
#include "nsNetUtil.h"
#include "nsStreamUtils.h"
#include "nsString.h"
#include "js/BuildId.h"
#include "mozilla/dom/Document.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/InternalRequest.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/WorkerRef.h"

namespace mozilla::dom {

// static
nsresult FetchUtil::GetValidRequestMethod(const nsACString& aMethod,
                                          nsCString& outMethod) {
  nsAutoCString upperCaseMethod(aMethod);
  ToUpperCase(upperCaseMethod);
  if (!NS_IsValidHTTPToken(aMethod)) {
    outMethod.SetIsVoid(true);
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  if (upperCaseMethod.EqualsLiteral("CONNECT") ||
      upperCaseMethod.EqualsLiteral("TRACE") ||
      upperCaseMethod.EqualsLiteral("TRACK")) {
    outMethod.SetIsVoid(true);
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  if (upperCaseMethod.EqualsLiteral("DELETE") ||
      upperCaseMethod.EqualsLiteral("GET") ||
      upperCaseMethod.EqualsLiteral("HEAD") ||
      upperCaseMethod.EqualsLiteral("OPTIONS") ||
      upperCaseMethod.EqualsLiteral("POST") ||
      upperCaseMethod.EqualsLiteral("PUT")) {
    outMethod = upperCaseMethod;
  } else {
    outMethod = aMethod;  // Case unchanged for non-standard methods
  }
  return NS_OK;
}

static bool FindCRLF(nsACString::const_iterator& aStart,
                     nsACString::const_iterator& aEnd) {
  nsACString::const_iterator end(aEnd);
  return FindInReadable("\r\n"_ns, aStart, end);
}

// Reads over a CRLF and positions start after it.
static bool PushOverLine(nsACString::const_iterator& aStart,
                         const nsACString::const_iterator& aEnd) {
  if (*aStart == nsCRT::CR && (aEnd - aStart > 1) && *(++aStart) == nsCRT::LF) {
    ++aStart;  // advance to after CRLF
    return true;
  }

  return false;
}

// static
bool FetchUtil::ExtractHeader(nsACString::const_iterator& aStart,
                              nsACString::const_iterator& aEnd,
                              nsCString& aHeaderName, nsCString& aHeaderValue,
                              bool* aWasEmptyHeader) {
  MOZ_ASSERT(aWasEmptyHeader);
  // Set it to a valid value here so we don't forget later.
  *aWasEmptyHeader = false;

  const char* beginning = aStart.get();
  nsACString::const_iterator end(aEnd);
  if (!FindCRLF(aStart, end)) {
    return false;
  }

  if (aStart.get() == beginning) {
    *aWasEmptyHeader = true;
    return true;
  }

  nsAutoCString header(beginning, aStart.get() - beginning);

  nsACString::const_iterator headerStart, iter, headerEnd;
  header.BeginReading(headerStart);
  header.EndReading(headerEnd);
  iter = headerStart;
  if (!FindCharInReadable(':', iter, headerEnd)) {
    return false;
  }

  aHeaderName.Assign(StringHead(header, iter - headerStart));
  aHeaderName.CompressWhitespace();
  if (!NS_IsValidHTTPToken(aHeaderName)) {
    return false;
  }

  aHeaderValue.Assign(Substring(++iter, headerEnd));
  if (!NS_IsReasonableHTTPHeaderValue(aHeaderValue)) {
    return false;
  }
  aHeaderValue.CompressWhitespace();

  return PushOverLine(aStart, aEnd);
}

// static
nsresult FetchUtil::SetRequestReferrer(nsIPrincipal* aPrincipal, Document* aDoc,
                                       nsIHttpChannel* aChannel,
                                       InternalRequest& aRequest) {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = NS_OK;
  nsAutoString referrer;
  aRequest.GetReferrer(referrer);

  ReferrerPolicy policy = aRequest.ReferrerPolicy_();
  nsCOMPtr<nsIReferrerInfo> referrerInfo;
  if (referrer.IsEmpty()) {
    // This is the case request’s referrer is "no-referrer"
    referrerInfo = new ReferrerInfo(nullptr, ReferrerPolicy::No_referrer);
  } else if (referrer.EqualsLiteral(kFETCH_CLIENT_REFERRER_STR)) {
    referrerInfo = ReferrerInfo::CreateForFetch(aPrincipal, aDoc);
    // In the first step, we should use referrer info from requetInit
    referrerInfo = static_cast<ReferrerInfo*>(referrerInfo.get())
                       ->CloneWithNewPolicy(policy);
  } else {
    // From "Determine request's Referrer" step 3
    // "If request's referrer is a URL, let referrerSource be request's
    // referrer."
    nsCOMPtr<nsIURI> referrerURI;
    rv = NS_NewURI(getter_AddRefs(referrerURI), referrer);
    NS_ENSURE_SUCCESS(rv, rv);
    referrerInfo = new ReferrerInfo(referrerURI, policy);
  }

  rv = aChannel->SetReferrerInfoWithoutClone(referrerInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString computedReferrerSpec;
  referrerInfo = aChannel->GetReferrerInfo();
  if (referrerInfo) {
    Unused << referrerInfo->GetComputedReferrerSpec(computedReferrerSpec);
  }

  // Step 8 https://fetch.spec.whatwg.org/#main-fetch
  // If request’s referrer is not "no-referrer", set request’s referrer to
  // the result of invoking determine request’s referrer.
  aRequest.SetReferrer(computedReferrerSpec);

  return NS_OK;
}

class StoreOptimizedEncodingRunnable final : public Runnable {
  nsMainThreadPtrHandle<nsICacheInfoChannel> mCache;
  Vector<uint8_t> mBytes;

 public:
  StoreOptimizedEncodingRunnable(
      nsMainThreadPtrHandle<nsICacheInfoChannel>&& aCache,
      Vector<uint8_t>&& aBytes)
      : Runnable("StoreOptimizedEncodingRunnable"),
        mCache(std::move(aCache)),
        mBytes(std::move(aBytes)) {}

  NS_IMETHOD Run() override {
    nsresult rv;

    nsCOMPtr<nsIAsyncOutputStream> stream;
    rv = mCache->OpenAlternativeOutputStream(FetchUtil::WasmAltDataType,
                                             int64_t(mBytes.length()),
                                             getter_AddRefs(stream));
    if (NS_FAILED(rv)) {
      return rv;
    }

    auto closeStream = MakeScopeExit([&]() { stream->CloseWithStatus(rv); });

    uint32_t written;
    rv = stream->Write((char*)mBytes.begin(), mBytes.length(), &written);
    if (NS_FAILED(rv)) {
      return rv;
    }

    MOZ_RELEASE_ASSERT(mBytes.length() == written);
    return NS_OK;
  };
};

class WindowStreamOwner final : public nsIObserver,
                                public nsSupportsWeakReference {
  // Read from any thread but only set/cleared on the main thread. The lifecycle
  // of WindowStreamOwner prevents concurrent read/clear.
  nsCOMPtr<nsIAsyncInputStream> mStream;

  nsCOMPtr<nsIGlobalObject> mGlobal;

  ~WindowStreamOwner() {
    MOZ_ASSERT(NS_IsMainThread());

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(this, DOM_WINDOW_DESTROYED_TOPIC);
    }
  }

 public:
  NS_DECL_ISUPPORTS

  WindowStreamOwner(nsIAsyncInputStream* aStream, nsIGlobalObject* aGlobal)
      : mStream(aStream), mGlobal(aGlobal) {
    MOZ_DIAGNOSTIC_ASSERT(mGlobal);
    MOZ_ASSERT(NS_IsMainThread());
  }

  static already_AddRefed<WindowStreamOwner> Create(
      nsIAsyncInputStream* aStream, nsIGlobalObject* aGlobal) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (NS_WARN_IF(!os)) {
      return nullptr;
    }

    RefPtr<WindowStreamOwner> self = new WindowStreamOwner(aStream, aGlobal);

    // Holds nsIWeakReference to self.
    nsresult rv = os->AddObserver(self, DOM_WINDOW_DESTROYED_TOPIC, true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }

    return self.forget();
  }

  // nsIObserver:

  NS_IMETHOD
  Observe(nsISupports* aSubject, const char* aTopic,
          const char16_t* aData) override {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_DIAGNOSTIC_ASSERT(strcmp(aTopic, DOM_WINDOW_DESTROYED_TOPIC) == 0);

    if (!mStream) {
      return NS_OK;
    }

    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(mGlobal);
    if (!SameCOMIdentity(aSubject, window)) {
      return NS_OK;
    }

    // mStream->Close() will call JSStreamConsumer::OnInputStreamReady which may
    // then destory itself, dropping the last reference to 'this'.
    RefPtr<WindowStreamOwner> keepAlive(this);

    mStream->Close();
    mStream = nullptr;
    mGlobal = nullptr;
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(WindowStreamOwner, nsIObserver, nsISupportsWeakReference)

inline nsISupports* ToSupports(WindowStreamOwner* aObj) {
  return static_cast<nsIObserver*>(aObj);
}

class WorkerStreamOwner final {
 public:
  NS_INLINE_DECL_REFCOUNTING(WorkerStreamOwner)

  explicit WorkerStreamOwner(nsIAsyncInputStream* aStream,
                             nsCOMPtr<nsIEventTarget>&& target)
      : mStream(aStream), mOwningEventTarget(std::move(target)) {}

  static already_AddRefed<WorkerStreamOwner> Create(
      nsIAsyncInputStream* aStream, WorkerPrivate* aWorker,
      nsCOMPtr<nsIEventTarget>&& target) {
    RefPtr<WorkerStreamOwner> self =
        new WorkerStreamOwner(aStream, std::move(target));

    self->mWorkerRef =
        StrongWorkerRef::Create(aWorker, "JSStreamConsumer", [self]() {
          if (self->mStream) {
            // If this Close() calls JSStreamConsumer::OnInputStreamReady and
            // drops the last reference to the JSStreamConsumer, 'this' will not
            // be destroyed since ~JSStreamConsumer() only enqueues a release
            // proxy.
            self->mStream->Close();
            self->mStream = nullptr;
          }
        });

    if (!self->mWorkerRef) {
      return nullptr;
    }

    return self.forget();
  }

  static void ProxyRelease(already_AddRefed<WorkerStreamOwner> aDoomed) {
    RefPtr<WorkerStreamOwner> doomed = aDoomed;
    nsIEventTarget* target = doomed->mOwningEventTarget;
    NS_ProxyRelease("WorkerStreamOwner", target, doomed.forget(),
                    /* aAlwaysProxy = */ true);
  }

 private:
  ~WorkerStreamOwner() = default;

  // Read from any thread but only set/cleared on the worker thread. The
  // lifecycle of WorkerStreamOwner prevents concurrent read/clear.
  nsCOMPtr<nsIAsyncInputStream> mStream;
  RefPtr<StrongWorkerRef> mWorkerRef;
  nsCOMPtr<nsIEventTarget> mOwningEventTarget;
};

class JSStreamConsumer final : public nsIInputStreamCallback,
                               public JS::OptimizedEncodingListener {
  // A LengthPrefixType is stored at the start of the compressed optimized
  // encoding, allowing the decompressed buffer to be allocated to exactly
  // the right size.
  using LengthPrefixType = uint32_t;
  static const unsigned PrefixBytes = sizeof(LengthPrefixType);

  RefPtr<WindowStreamOwner> mWindowStreamOwner;
  RefPtr<WorkerStreamOwner> mWorkerStreamOwner;
  nsMainThreadPtrHandle<nsICacheInfoChannel> mCache;
  const bool mOptimizedEncoding;
  z_stream mZStream;
  bool mZStreamInitialized;
  Vector<uint8_t> mOptimizedEncodingBytes;
  JS::StreamConsumer* mConsumer;
  bool mConsumerAborted;

  JSStreamConsumer(already_AddRefed<WindowStreamOwner> aWindowStreamOwner,
                   nsIGlobalObject* aGlobal, JS::StreamConsumer* aConsumer,
                   nsMainThreadPtrHandle<nsICacheInfoChannel>&& aCache,
                   bool aOptimizedEncoding)
      : mWindowStreamOwner(aWindowStreamOwner),
        mCache(std::move(aCache)),
        mOptimizedEncoding(aOptimizedEncoding),
        mZStreamInitialized(false),
        mConsumer(aConsumer),
        mConsumerAborted(false) {
    MOZ_DIAGNOSTIC_ASSERT(mWindowStreamOwner);
    MOZ_DIAGNOSTIC_ASSERT(mConsumer);
  }

  JSStreamConsumer(RefPtr<WorkerStreamOwner> aWorkerStreamOwner,
                   nsIGlobalObject* aGlobal, JS::StreamConsumer* aConsumer,
                   nsMainThreadPtrHandle<nsICacheInfoChannel>&& aCache,
                   bool aOptimizedEncoding)
      : mWorkerStreamOwner(std::move(aWorkerStreamOwner)),
        mCache(std::move(aCache)),
        mOptimizedEncoding(aOptimizedEncoding),
        mZStreamInitialized(false),
        mConsumer(aConsumer),
        mConsumerAborted(false) {
    MOZ_DIAGNOSTIC_ASSERT(mWorkerStreamOwner);
    MOZ_DIAGNOSTIC_ASSERT(mConsumer);
  }

  ~JSStreamConsumer() {
    if (mZStreamInitialized) {
      inflateEnd(&mZStream);
    }

    // Both WindowStreamOwner and WorkerStreamOwner need to be destroyed on
    // their global's event target thread.

    if (mWindowStreamOwner) {
      MOZ_DIAGNOSTIC_ASSERT(!mWorkerStreamOwner);
      NS_ReleaseOnMainThread("JSStreamConsumer::mWindowStreamOwner",
                             mWindowStreamOwner.forget(),
                             /* aAlwaysProxy = */ true);
    } else {
      MOZ_DIAGNOSTIC_ASSERT(mWorkerStreamOwner);
      WorkerStreamOwner::ProxyRelease(mWorkerStreamOwner.forget());
    }

    // Bug 1733674: these annotations currently do nothing, because they are
    // member variables and the annotation mechanism only applies to locals. But
    // the analysis could be extended so that these could replace the big-hammer
    // ~JSStreamConsumer annotation and thus the analysis could check that
    // nothing is added that might GC for a different reason.
    JS_HAZ_VALUE_IS_GC_SAFE(mWindowStreamOwner);
    JS_HAZ_VALUE_IS_GC_SAFE(mWorkerStreamOwner);
  }

  static nsresult WriteSegment(nsIInputStream* aStream, void* aClosure,
                               const char* aFromSegment, uint32_t aToOffset,
                               uint32_t aCount, uint32_t* aWriteCount) {
    JSStreamConsumer* self = reinterpret_cast<JSStreamConsumer*>(aClosure);
    MOZ_DIAGNOSTIC_ASSERT(!self->mConsumerAborted);

    if (self->mOptimizedEncoding) {
      if (!self->mZStreamInitialized) {
        // mOptimizedEncodingBytes is used as temporary storage until we have
        // the full prefix.
        MOZ_ASSERT(self->mOptimizedEncodingBytes.length() < PrefixBytes);
        uint32_t remain = PrefixBytes - self->mOptimizedEncodingBytes.length();
        uint32_t consume = std::min(remain, aCount);

        if (!self->mOptimizedEncodingBytes.append(aFromSegment, consume)) {
          return NS_ERROR_UNEXPECTED;
        }

        if (consume == remain) {
          // Initialize zlib once all prefix bytes are loaded.
          LengthPrefixType length;
          memcpy(&length, self->mOptimizedEncodingBytes.begin(), PrefixBytes);

          if (!self->mOptimizedEncodingBytes.resizeUninitialized(length)) {
            return NS_ERROR_UNEXPECTED;
          }

          memset(&self->mZStream, 0, sizeof(self->mZStream));
          self->mZStream.avail_out = length;
          self->mZStream.next_out = self->mOptimizedEncodingBytes.begin();

          if (inflateInit(&self->mZStream) != Z_OK) {
            return NS_ERROR_UNEXPECTED;
          }
          self->mZStreamInitialized = true;
        }

        *aWriteCount = consume;
        return NS_OK;
      }

      // Zlib is initialized, overwrite the prefix with the inflated data.

      MOZ_DIAGNOSTIC_ASSERT(aCount > 0);
      self->mZStream.avail_in = aCount;
      self->mZStream.next_in = (uint8_t*)aFromSegment;

      int ret = inflate(&self->mZStream, Z_NO_FLUSH);

      MOZ_DIAGNOSTIC_ASSERT(ret == Z_OK || ret == Z_STREAM_END,
                            "corrupt optimized wasm cache file: data");
      MOZ_DIAGNOSTIC_ASSERT(self->mZStream.avail_in == 0,
                            "corrupt optimized wasm cache file: input");
      MOZ_DIAGNOSTIC_ASSERT_IF(ret == Z_STREAM_END,
                               self->mZStream.avail_out == 0);
      // Gracefully handle corruption in release.
      bool ok =
          (ret == Z_OK || ret == Z_STREAM_END) && self->mZStream.avail_in == 0;
      if (!ok) {
        return NS_ERROR_UNEXPECTED;
      }
    } else {
      // This callback can be called on any thread which is explicitly allowed
      // by this particular JS API call.
      if (!self->mConsumer->consumeChunk((const uint8_t*)aFromSegment,
                                         aCount)) {
        self->mConsumerAborted = true;
        return NS_ERROR_UNEXPECTED;
      }
    }

    *aWriteCount = aCount;
    return NS_OK;
  }

 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  static bool Start(nsCOMPtr<nsIInputStream> aStream, nsIGlobalObject* aGlobal,
                    WorkerPrivate* aMaybeWorker, JS::StreamConsumer* aConsumer,
                    nsMainThreadPtrHandle<nsICacheInfoChannel>&& aCache,
                    bool aOptimizedEncoding) {
    nsCOMPtr<nsIAsyncInputStream> asyncStream;
    nsresult rv = NS_MakeAsyncNonBlockingInputStream(
        aStream.forget(), getter_AddRefs(asyncStream));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }

    RefPtr<JSStreamConsumer> consumer;
    if (aMaybeWorker) {
      RefPtr<WorkerStreamOwner> owner = WorkerStreamOwner::Create(
          asyncStream, aMaybeWorker,
          aGlobal->EventTargetFor(TaskCategory::Other));
      if (!owner) {
        return false;
      }

      consumer = new JSStreamConsumer(std::move(owner), aGlobal, aConsumer,
                                      std::move(aCache), aOptimizedEncoding);
    } else {
      RefPtr<WindowStreamOwner> owner =
          WindowStreamOwner::Create(asyncStream, aGlobal);
      if (!owner) {
        return false;
      }

      consumer = new JSStreamConsumer(owner.forget(), aGlobal, aConsumer,
                                      std::move(aCache), aOptimizedEncoding);
    }

    // This AsyncWait() creates a ref-cycle between asyncStream and consumer:
    //
    //   asyncStream -> consumer -> (Window|Worker)StreamOwner -> asyncStream
    //
    // The cycle is broken when the stream completes or errors out and
    // asyncStream drops its reference to consumer.
    return NS_SUCCEEDED(asyncStream->AsyncWait(consumer, 0, 0, nullptr));
  }

  // nsIInputStreamCallback:

  NS_IMETHOD
  OnInputStreamReady(nsIAsyncInputStream* aStream) override {
    // Can be called on any stream. The JS API calls made below explicitly
    // support being called from any thread.
    MOZ_DIAGNOSTIC_ASSERT(!mConsumerAborted);

    nsresult rv;

    uint64_t available = 0;
    rv = aStream->Available(&available);
    if (NS_SUCCEEDED(rv) && available == 0) {
      rv = NS_BASE_STREAM_CLOSED;
    }

    if (rv == NS_BASE_STREAM_CLOSED) {
      if (mOptimizedEncoding) {
        // Gracefully handle corruption of compressed data stream in release.
        // From on investigations in bug 1738987, the incomplete data cases
        // mostly happen during shutdown. Some corruptions in the cache entry
        // can still happen and will be handled in the WriteSegment above.
        bool ok = mZStreamInitialized && mZStream.avail_out == 0;
        if (!ok) {
          mConsumer->streamError(size_t(NS_ERROR_UNEXPECTED));
          return NS_OK;
        }

        mConsumer->consumeOptimizedEncoding(mOptimizedEncodingBytes.begin(),
                                            mOptimizedEncodingBytes.length());
      } else {
        // If there is cache entry associated with this stream, then listen for
        // an optimized encoding so we can store it in the alt data. By JS API
        // contract, the compilation process will hold a refcount to 'this'
        // until it's done, optionally calling storeOptimizedEncoding().
        mConsumer->streamEnd(mCache ? this : nullptr);
      }
      return NS_OK;
    }

    if (NS_FAILED(rv)) {
      mConsumer->streamError(size_t(rv));
      return NS_OK;
    }

    // Check mConsumerAborted before NS_FAILED to avoid calling streamError()
    // if consumeChunk() returned false per JS API contract.
    uint32_t written = 0;
    rv = aStream->ReadSegments(WriteSegment, this, available, &written);
    if (mConsumerAborted) {
      return NS_OK;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mConsumer->streamError(size_t(rv));
      return NS_OK;
    }

    rv = aStream->AsyncWait(this, 0, 0, nullptr);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mConsumer->streamError(size_t(rv));
      return NS_OK;
    }

    return NS_OK;
  }

  // JS::OptimizedEncodingListener

  void storeOptimizedEncoding(const uint8_t* aSrcBytes,
                              size_t aSrcLength) override {
    MOZ_ASSERT(mCache, "we only listen if there's a cache entry");

    z_stream zstream;
    memset(&zstream, 0, sizeof(zstream));
    zstream.avail_in = aSrcLength;
    zstream.next_in = (uint8_t*)aSrcBytes;

    // The wins from increasing compression levels are tiny, while the time
    // to compress increases drastically. For example, for a 148mb alt-data
    // produced by a 40mb .wasm file, the level 2 takes 2.5s to get a 3.7x size
    // reduction while level 9 takes 22.5s to get a 4x size reduction. Read-time
    // wins from smaller compressed cache files are not found to be
    // significant, thus the fastest compression level is used. (On test
    // workloads, level 2 actually was faster *and* smaller than level 1.)
    const int COMPRESSION = 2;
    if (deflateInit(&zstream, COMPRESSION) != Z_OK) {
      return;
    }
    auto autoDestroy = MakeScopeExit([&]() { deflateEnd(&zstream); });

    Vector<uint8_t> dstBytes;
    if (!dstBytes.resizeUninitialized(PrefixBytes +
                                      deflateBound(&zstream, aSrcLength))) {
      return;
    }

    MOZ_RELEASE_ASSERT(LengthPrefixType(aSrcLength) == aSrcLength);
    LengthPrefixType srcLength = aSrcLength;
    memcpy(dstBytes.begin(), &srcLength, PrefixBytes);

    uint8_t* compressBegin = dstBytes.begin() + PrefixBytes;
    zstream.next_out = compressBegin;
    zstream.avail_out = dstBytes.length() - PrefixBytes;

    int ret = deflate(&zstream, Z_FINISH);
    if (ret == Z_MEM_ERROR) {
      return;
    }
    MOZ_RELEASE_ASSERT(ret == Z_STREAM_END);

    dstBytes.shrinkTo(zstream.next_out - dstBytes.begin());

    NS_DispatchToMainThread(new StoreOptimizedEncodingRunnable(
        std::move(mCache), std::move(dstBytes)));
  }
};

NS_IMPL_ISUPPORTS(JSStreamConsumer, nsIInputStreamCallback)

// static
const nsCString FetchUtil::WasmAltDataType;

// static
void FetchUtil::InitWasmAltDataType() {
  nsCString& type = const_cast<nsCString&>(WasmAltDataType);
  MOZ_ASSERT(type.IsEmpty());

  RunOnShutdown([]() {
    // Avoid nsStringBuffer leak tests failures.
    const_cast<nsCString&>(WasmAltDataType).Truncate();
  });

  type.Append(nsLiteralCString("wasm-"));

  JS::BuildIdCharVector buildId;
  if (!JS::GetOptimizedEncodingBuildId(&buildId)) {
    MOZ_CRASH("build id oom");
  }

  type.Append(buildId.begin(), buildId.length());
}

static bool ThrowException(JSContext* aCx, unsigned errorNumber) {
  JS_ReportErrorNumberASCII(aCx, js::GetErrorMessage, nullptr, errorNumber);
  return false;
}

// static
bool FetchUtil::StreamResponseToJS(JSContext* aCx, JS::Handle<JSObject*> aObj,
                                   JS::MimeType aMimeType,
                                   JS::StreamConsumer* aConsumer,
                                   WorkerPrivate* aMaybeWorker) {
  MOZ_ASSERT(!WasmAltDataType.IsEmpty());
  MOZ_ASSERT(!aMaybeWorker == NS_IsMainThread());

  RefPtr<Response> response;
  nsresult rv = UNWRAP_OBJECT(Response, aObj, response);
  if (NS_FAILED(rv)) {
    return ThrowException(aCx, JSMSG_WASM_BAD_RESPONSE_VALUE);
  }

  const char* requiredMimeType = nullptr;
  switch (aMimeType) {
    case JS::MimeType::Wasm:
      requiredMimeType = WASM_CONTENT_TYPE;
      break;
  }

  nsAutoCString mimeType;
  nsAutoCString mixedCaseMimeType;  // unused
  response->GetMimeType(mimeType, mixedCaseMimeType);

  if (!mimeType.EqualsASCII(requiredMimeType)) {
    JS_ReportErrorNumberASCII(aCx, js::GetErrorMessage, nullptr,
                              JSMSG_WASM_BAD_RESPONSE_MIME_TYPE, mimeType.get(),
                              requiredMimeType);
    return false;
  }

  if (response->Type() != ResponseType::Basic &&
      response->Type() != ResponseType::Cors &&
      response->Type() != ResponseType::Default) {
    return ThrowException(aCx, JSMSG_WASM_BAD_RESPONSE_CORS_SAME_ORIGIN);
  }

  if (!response->Ok()) {
    return ThrowException(aCx, JSMSG_WASM_BAD_RESPONSE_STATUS);
  }

  IgnoredErrorResult result;
  bool used = response->GetBodyUsed(result);
  if (NS_WARN_IF(result.Failed())) {
    return ThrowException(aCx, JSMSG_WASM_ERROR_CONSUMING_RESPONSE);
  }
  if (used) {
    return ThrowException(aCx, JSMSG_WASM_RESPONSE_ALREADY_CONSUMED);
  }

  switch (aMimeType) {
    case JS::MimeType::Wasm:
      nsAutoString url;
      response->GetUrl(url);

      nsCString sourceMapUrl;
      response->GetInternalHeaders()->Get("SourceMap"_ns, sourceMapUrl, result);
      if (NS_WARN_IF(result.Failed())) {
        return ThrowException(aCx, JSMSG_WASM_ERROR_CONSUMING_RESPONSE);
      }
      NS_ConvertUTF16toUTF8 urlUTF8(url);
      aConsumer->noteResponseURLs(
          urlUTF8.get(), sourceMapUrl.IsVoid() ? nullptr : sourceMapUrl.get());
      break;
  }

  SafeRefPtr<InternalResponse> ir = response->GetInternalResponse();
  if (NS_WARN_IF(!ir)) {
    return ThrowException(aCx, JSMSG_OUT_OF_MEMORY);
  }

  nsCOMPtr<nsIInputStream> stream;

  nsMainThreadPtrHandle<nsICacheInfoChannel> cache;
  bool optimizedEncoding = false;
  if (ir->HasCacheInfoChannel()) {
    cache = ir->TakeCacheInfoChannel();

    nsAutoCString altDataType;
    if (NS_SUCCEEDED(cache->GetAlternativeDataType(altDataType)) &&
        WasmAltDataType.Equals(altDataType)) {
      optimizedEncoding = true;
      rv = cache->GetAlternativeDataInputStream(getter_AddRefs(stream));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return ThrowException(aCx, JSMSG_OUT_OF_MEMORY);
      }
      if (ir->HasBeenCloned()) {
        // If `Response` is cloned, clone alternative data stream instance.
        // The cache entry does not clone automatically, and multiple
        // JSStreamConsumer instances will collide during read if not cloned.
        nsCOMPtr<nsICloneableInputStream> original = do_QueryInterface(stream);
        if (NS_WARN_IF(!original)) {
          return ThrowException(aCx, JSMSG_OUT_OF_MEMORY);
        }
        rv = original->Clone(getter_AddRefs(stream));
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return ThrowException(aCx, JSMSG_OUT_OF_MEMORY);
        }
      }
    }
  }

  if (!optimizedEncoding) {
    ir->GetUnfilteredBody(getter_AddRefs(stream));
    if (!stream) {
      aConsumer->streamEnd();
      return true;
    }
  }

  MOZ_ASSERT(stream);

  IgnoredErrorResult error;
  response->SetBodyUsed(aCx, error);
  if (NS_WARN_IF(error.Failed())) {
    return ThrowException(aCx, JSMSG_WASM_ERROR_CONSUMING_RESPONSE);
  }

  nsIGlobalObject* global = xpc::NativeGlobal(js::UncheckedUnwrap(aObj));

  if (!JSStreamConsumer::Start(stream, global, aMaybeWorker, aConsumer,
                               std::move(cache), optimizedEncoding)) {
    return ThrowException(aCx, JSMSG_OUT_OF_MEMORY);
  }

  return true;
}

// static
void FetchUtil::ReportJSStreamError(JSContext* aCx, size_t aErrorCode) {
  // For now, convert *all* errors into AbortError.

  RefPtr<DOMException> e = DOMException::Create(NS_ERROR_DOM_ABORT_ERR);

  JS::Rooted<JS::Value> value(aCx);
  if (!GetOrCreateDOMReflector(aCx, e, &value)) {
    return;
  }

  JS_SetPendingException(aCx, value);
}

}  // namespace mozilla::dom
