/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Fetch.h"

#include "nsIDocument.h"
#include "nsIGlobalObject.h"
#include "nsIStreamLoader.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsIUnicodeDecoder.h"
#include "nsIUnicodeEncoder.h"

#include "nsCharSeparatedTokenizer.h"
#include "nsDOMString.h"
#include "nsNetUtil.h"
#include "nsReadableUtils.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/FetchDriver.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/Headers.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/URLSearchParams.h"
#include "mozilla/Telemetry.h"

#include "InternalRequest.h"
#include "InternalResponse.h"

#include "nsFormData.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"
#include "Workers.h"

namespace mozilla {
namespace dom {

using namespace workers;

class WorkerFetchResolver final : public FetchDriverObserver,
                                  public WorkerFeature
{
  friend class MainThreadFetchRunnable;
  friend class WorkerFetchResponseEndRunnable;
  friend class WorkerFetchResponseRunnable;

  workers::WorkerPrivate* mWorkerPrivate;

  Mutex mCleanUpLock;
  bool mCleanedUp;
  // The following are initialized and used exclusively on the worker thread.
  nsRefPtr<Promise> mFetchPromise;
  nsRefPtr<Response> mResponse;
public:

  WorkerFetchResolver(workers::WorkerPrivate* aWorkerPrivate, Promise* aPromise)
    : mWorkerPrivate(aWorkerPrivate)
    , mCleanUpLock("WorkerFetchResolver")
    , mCleanedUp(false)
    , mFetchPromise(aPromise)
  {
  }

  void
  OnResponseAvailable(InternalResponse* aResponse) override;

  void
  OnResponseEnd() override;

  bool
  Notify(JSContext* aCx, Status aStatus) override
  {
    if (aStatus > Running) {
      CleanUp(aCx);
    }
    return true;
  }

  void
  CleanUp(JSContext* aCx)
  {
    MutexAutoLock lock(mCleanUpLock);

    if (mCleanedUp) {
      return;
    }

    MOZ_ASSERT(mWorkerPrivate);
    mWorkerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(mWorkerPrivate->GetJSContext() == aCx);

    mWorkerPrivate->RemoveFeature(aCx, this);
    CleanUpUnchecked();
  }

  void
  CleanUpUnchecked()
  {
    mResponse = nullptr;
    if (mFetchPromise) {
      mFetchPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
      mFetchPromise = nullptr;
    }
    mCleanedUp = true;
  }

  workers::WorkerPrivate*
  GetWorkerPrivate() const
  {
    // It's ok to race on |mCleanedUp|, because it will never cause us to fire
    // the assertion when we should not.
    MOZ_ASSERT(!mCleanedUp);
    return mWorkerPrivate;
  }

private:
  ~WorkerFetchResolver()
  {
    MOZ_ASSERT(mCleanedUp);
    MOZ_ASSERT(!mFetchPromise);
  }
};

class MainThreadFetchResolver final : public FetchDriverObserver
{
  nsRefPtr<Promise> mPromise;
  nsRefPtr<Response> mResponse;

  NS_DECL_OWNINGTHREAD
public:
  explicit MainThreadFetchResolver(Promise* aPromise);

  void
  OnResponseAvailable(InternalResponse* aResponse) override;

private:
  ~MainThreadFetchResolver();
};

class MainThreadFetchRunnable : public nsRunnable
{
  nsRefPtr<WorkerFetchResolver> mResolver;
  nsRefPtr<InternalRequest> mRequest;

public:
  MainThreadFetchRunnable(WorkerPrivate* aWorkerPrivate,
                          Promise* aPromise,
                          InternalRequest* aRequest)
    : mResolver(new WorkerFetchResolver(aWorkerPrivate, aPromise))
    , mRequest(aRequest)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
    if (!aWorkerPrivate->AddFeature(aWorkerPrivate->GetJSContext(), mResolver)) {
      NS_WARNING("Could not add WorkerFetchResolver feature to worker");
      mResolver->CleanUpUnchecked();
      mResolver = nullptr;
    }
  }

  NS_IMETHODIMP
  Run()
  {
    AssertIsOnMainThread();
    // AddFeature() call failed, don't bother running.
    if (!mResolver) {
      return NS_OK;
    }

    nsCOMPtr<nsIPrincipal> principal = mResolver->GetWorkerPrivate()->GetPrincipal();
    nsCOMPtr<nsILoadGroup> loadGroup = mResolver->GetWorkerPrivate()->GetLoadGroup();
    nsRefPtr<FetchDriver> fetch = new FetchDriver(mRequest, principal, loadGroup);
    nsIDocument* doc = mResolver->GetWorkerPrivate()->GetDocument();
    if (doc) {
      fetch->SetDocument(doc);
    }

    nsresult rv = fetch->Fetch(mResolver);
    // Right now we only support async fetch, which should never directly fail.
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }
};

already_AddRefed<Promise>
FetchRequest(nsIGlobalObject* aGlobal, const RequestOrUSVString& aInput,
             const RequestInit& aInit, ErrorResult& aRv)
{
  nsRefPtr<Promise> p = Promise::Create(aGlobal, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  AutoJSAPI jsapi;
  jsapi.Init(aGlobal);
  JSContext* cx = jsapi.cx();

  JS::Rooted<JSObject*> jsGlobal(cx, aGlobal->GetGlobalJSObject());
  GlobalObject global(cx, jsGlobal);

  nsRefPtr<Request> request = Request::Constructor(global, aInput, aInit, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsRefPtr<InternalRequest> r = request->GetInternalRequest();

  aRv = UpdateRequestReferrer(aGlobal, r);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (NS_IsMainThread()) {
    nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal);
    if (!window) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
    if (!doc) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    Telemetry::Accumulate(Telemetry::FETCH_IS_MAINTHREAD, 1);

    nsRefPtr<MainThreadFetchResolver> resolver = new MainThreadFetchResolver(p);
    nsCOMPtr<nsILoadGroup> loadGroup = doc->GetDocumentLoadGroup();
    nsRefPtr<FetchDriver> fetch =
      new FetchDriver(r, doc->NodePrincipal(), loadGroup);
    fetch->SetDocument(doc);
    aRv = fetch->Fetch(resolver);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  } else {
    WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(worker);

    Telemetry::Accumulate(Telemetry::FETCH_IS_MAINTHREAD, 0);

    if (worker->IsServiceWorker()) {
      r->SetSkipServiceWorker();
    }

    nsRefPtr<MainThreadFetchRunnable> run = new MainThreadFetchRunnable(worker, p, r);
    if (NS_FAILED(NS_DispatchToMainThread(run))) {
      NS_WARNING("MainThreadFetchRunnable dispatch failed!");
    }
  }

  return p.forget();
}

MainThreadFetchResolver::MainThreadFetchResolver(Promise* aPromise)
  : mPromise(aPromise)
{
}

void
MainThreadFetchResolver::OnResponseAvailable(InternalResponse* aResponse)
{
  NS_ASSERT_OWNINGTHREAD(MainThreadFetchResolver);
  AssertIsOnMainThread();

  if (aResponse->Type() != ResponseType::Error) {
    nsCOMPtr<nsIGlobalObject> go = mPromise->GetParentObject();
    mResponse = new Response(go, aResponse);
    mPromise->MaybeResolve(mResponse);
  } else {
    ErrorResult result;
    result.ThrowTypeError(MSG_FETCH_FAILED);
    mPromise->MaybeReject(result);
  }
}

MainThreadFetchResolver::~MainThreadFetchResolver()
{
  NS_ASSERT_OWNINGTHREAD(MainThreadFetchResolver);
}

class WorkerFetchResponseRunnable final : public WorkerRunnable
{
  nsRefPtr<WorkerFetchResolver> mResolver;
  // Passed from main thread to worker thread after being initialized.
  nsRefPtr<InternalResponse> mInternalResponse;
public:
  WorkerFetchResponseRunnable(WorkerFetchResolver* aResolver, InternalResponse* aResponse)
    : WorkerRunnable(aResolver->GetWorkerPrivate(), WorkerThreadModifyBusyCount)
    , mResolver(aResolver)
    , mInternalResponse(aResponse)
  {
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(aWorkerPrivate == mResolver->GetWorkerPrivate());

    nsRefPtr<Promise> promise = mResolver->mFetchPromise.forget();

    if (mInternalResponse->Type() != ResponseType::Error) {
      nsRefPtr<nsIGlobalObject> global = aWorkerPrivate->GlobalScope();
      mResolver->mResponse = new Response(global, mInternalResponse);

      promise->MaybeResolve(mResolver->mResponse);
    } else {
      ErrorResult result;
      result.ThrowTypeError(MSG_FETCH_FAILED);
      promise->MaybeReject(result);
    }
    return true;
  }
};

class WorkerFetchResponseEndRunnable final : public WorkerRunnable
{
  nsRefPtr<WorkerFetchResolver> mResolver;
public:
  explicit WorkerFetchResponseEndRunnable(WorkerFetchResolver* aResolver)
    : WorkerRunnable(aResolver->GetWorkerPrivate(), WorkerThreadModifyBusyCount)
    , mResolver(aResolver)
  {
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(aWorkerPrivate == mResolver->GetWorkerPrivate());

    mResolver->CleanUp(aCx);
    return true;
  }
};

void
WorkerFetchResolver::OnResponseAvailable(InternalResponse* aResponse)
{
  AssertIsOnMainThread();

  MutexAutoLock lock(mCleanUpLock);
  if (mCleanedUp) {
    return;
  }

  nsRefPtr<WorkerFetchResponseRunnable> r =
    new WorkerFetchResponseRunnable(this, aResponse);

  AutoSafeJSContext cx;
  if (!r->Dispatch(cx)) {
    NS_WARNING("Could not dispatch fetch resolve");
  }
}

void
WorkerFetchResolver::OnResponseEnd()
{
  AssertIsOnMainThread();
  MutexAutoLock lock(mCleanUpLock);
  if (mCleanedUp) {
    return;
  }

  nsRefPtr<WorkerFetchResponseEndRunnable> r =
    new WorkerFetchResponseEndRunnable(this);

  AutoSafeJSContext cx;
  if (!r->Dispatch(cx)) {
    NS_WARNING("Could not dispatch fetch resolve end");
  }
}

// This method sets the request's referrerURL, as specified by the "determine
// request's referrer" steps from Referrer Policy [1].
// The actual referrer policy and stripping is dealt with by HttpBaseChannel,
// this always sets the full API referrer URL of the relevant global if it is
// not already a url or no-referrer.
// [1]: https://w3c.github.io/webappsec/specs/referrer-policy/#determine-requests-referrer
nsresult
UpdateRequestReferrer(nsIGlobalObject* aGlobal, InternalRequest* aRequest)
{
  nsAutoString originalReferrer;
  aRequest->GetReferrer(originalReferrer);
  // If it is no-referrer ("") or a URL, don't modify.
  if (!originalReferrer.EqualsLiteral(kFETCH_CLIENT_REFERRER_STR)) {
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal);
  if (window) {
    nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
    if (doc) {
      nsAutoString referrer;
      doc->GetReferrer(referrer);
      aRequest->SetReferrer(referrer);
    }
  } else {
    WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(worker);
    worker->AssertIsOnWorkerThread();
    WorkerPrivate::LocationInfo& info = worker->GetLocationInfo();
    aRequest->SetReferrer(NS_ConvertUTF8toUTF16(info.mHref));
  }

  return NS_OK;
}

namespace {
nsresult
ExtractFromArrayBuffer(const ArrayBuffer& aBuffer,
                       nsIInputStream** aStream)
{
  aBuffer.ComputeLengthAndData();
  //XXXnsm reinterpret_cast<> is used in DOMParser, should be ok.
  return NS_NewByteInputStream(aStream,
                               reinterpret_cast<char*>(aBuffer.Data()),
                               aBuffer.Length(), NS_ASSIGNMENT_COPY);
}

nsresult
ExtractFromArrayBufferView(const ArrayBufferView& aBuffer,
                           nsIInputStream** aStream)
{
  aBuffer.ComputeLengthAndData();
  //XXXnsm reinterpret_cast<> is used in DOMParser, should be ok.
  return NS_NewByteInputStream(aStream,
                               reinterpret_cast<char*>(aBuffer.Data()),
                               aBuffer.Length(), NS_ASSIGNMENT_COPY);
}

nsresult
ExtractFromBlob(const File& aFile, nsIInputStream** aStream,
                nsCString& aContentType)
{
  nsRefPtr<FileImpl> impl = aFile.Impl();
  nsresult rv = impl->GetInternalStream(aStream);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoString type;
  impl->GetType(type);
  aContentType = NS_ConvertUTF16toUTF8(type);
  return NS_OK;
}

nsresult
ExtractFromFormData(nsFormData& aFormData, nsIInputStream** aStream,
                    nsCString& aContentType)
{
  uint64_t unusedContentLength;
  nsAutoCString unusedCharset;
  return aFormData.GetSendInfo(aStream, &unusedContentLength,
                               aContentType, unusedCharset);
}

nsresult
ExtractFromUSVString(const nsString& aStr,
                     nsIInputStream** aStream,
                     nsCString& aContentType)
{
  nsCOMPtr<nsIUnicodeEncoder> encoder = EncodingUtils::EncoderForEncoding("UTF-8");
  if (!encoder) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  int32_t destBufferLen;
  nsresult rv = encoder->GetMaxLength(aStr.get(), aStr.Length(), &destBufferLen);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString encoded;
  if (!encoded.SetCapacity(destBufferLen, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  char* destBuffer = encoded.BeginWriting();
  int32_t srcLen = (int32_t) aStr.Length();
  int32_t outLen = destBufferLen;
  rv = encoder->Convert(aStr.get(), &srcLen, destBuffer, &outLen);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(outLen <= destBufferLen);
  encoded.SetLength(outLen);

  aContentType = NS_LITERAL_CSTRING("text/plain;charset=UTF-8");

  return NS_NewCStringInputStream(aStream, encoded);
}

nsresult
ExtractFromURLSearchParams(const URLSearchParams& aParams,
                           nsIInputStream** aStream,
                           nsCString& aContentType)
{
  nsAutoString serialized;
  aParams.Stringify(serialized);
  aContentType = NS_LITERAL_CSTRING("application/x-www-form-urlencoded;charset=UTF-8");
  return NS_NewStringInputStream(aStream, serialized);
}

void
FillFormData(const nsString& aName, const nsString& aValue, void* aFormData)
{
  MOZ_ASSERT(aFormData);
  nsFormData* fd = static_cast<nsFormData*>(aFormData);
  fd->Append(aName, aValue);
}

/**
 * A simple multipart/form-data parser as defined in RFC 2388 and RFC 2046.
 * This does not respect any encoding specified per entry, using UTF-8
 * throughout. This is as the Fetch spec states in the consume body algorithm.
 * Borrows some things from Necko's nsMultiMixedConv, but is simpler since
 * unlike Necko we do not have to deal with receiving incomplete chunks of data.
 *
 * This parser will fail the entire parse on any invalid entry, so it will
 * never return a partially filled FormData.
 * The content-disposition header is used to figure out the name and filename
 * entries. The inclusion of the filename parameter decides if the entry is
 * inserted into the nsFormData as a string or a File.
 *
 * File blobs are copies of the underlying data string since we cannot adopt
 * char* chunks embedded within the larger body without significant effort.
 * FIXME(nsm): Bug 1127552 - We should add telemetry to calls to formData() and
 * friends to figure out if Fetch ends up copying big blobs to see if this is
 * worth optimizing.
 */
class MOZ_STACK_CLASS FormDataParser
{
private:
  nsRefPtr<nsFormData> mFormData;
  nsCString mMimeType;
  nsCString mData;

  // Entry state, reset in START_PART.
  nsCString mName;
  nsCString mFilename;
  nsCString mContentType;

  enum
  {
    START_PART,
    PARSE_HEADER,
    PARSE_BODY,
  } mState;

  nsIGlobalObject* mParentObject;

  // Reads over a boundary and sets start to the position after the end of the
  // boundary. Returns false if no boundary is found immediately.
  bool
  PushOverBoundary(const nsACString& aBoundaryString,
                   nsACString::const_iterator& aStart,
                   nsACString::const_iterator& aEnd)
  {
    // We copy the end iterator to keep the original pointing to the real end
    // of the string.
    nsACString::const_iterator end(aEnd);
    const char* beginning = aStart.get();
    if (FindInReadable(aBoundaryString, aStart, end)) {
      // We either should find the body immediately, or after 2 chars with the
      // 2 chars being '-', everything else is failure.
      if ((aStart.get() - beginning) == 0) {
        aStart.advance(aBoundaryString.Length());
        return true;
      }

      if ((aStart.get() - beginning) == 2) {
        if (*(--aStart) == '-' && *(--aStart) == '-') {
          aStart.advance(aBoundaryString.Length() + 2);
          return true;
        }
      }
    }

    return false;
  }

  // Reads over a CRLF and positions start after it.
  bool
  PushOverLine(nsACString::const_iterator& aStart)
  {
    if (*aStart == nsCRT::CR && (aStart.size_forward() > 1) && *(++aStart) == nsCRT::LF) {
      ++aStart; // advance to after CRLF
      return true;
    }

    return false;
  }

  bool
  FindCRLF(nsACString::const_iterator& aStart,
           nsACString::const_iterator& aEnd)
  {
    nsACString::const_iterator end(aEnd);
    return FindInReadable(NS_LITERAL_CSTRING("\r\n"), aStart, end);
  }

  bool
  ParseHeader(nsACString::const_iterator& aStart,
              nsACString::const_iterator& aEnd,
              bool* aWasEmptyHeader)
  {
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

    nsACString::const_iterator headerStart, headerEnd;
    header.BeginReading(headerStart);
    header.EndReading(headerEnd);
    if (!FindCharInReadable(':', headerStart, headerEnd)) {
      return false;
    }

    nsAutoCString headerName(StringHead(header, headerStart.size_backward()));
    headerName.CompressWhitespace();
    if (!NS_IsValidHTTPToken(headerName)) {
      return false;
    }

    nsAutoCString headerValue(Substring(++headerStart, headerEnd));
    if (!NS_IsReasonableHTTPHeaderValue(headerValue)) {
      return false;
    }
    headerValue.CompressWhitespace();

    if (headerName.LowerCaseEqualsLiteral("content-disposition")) {
      nsCCharSeparatedTokenizer tokenizer(headerValue, ';');
      bool seenFormData = false;
      while (tokenizer.hasMoreTokens()) {
        const nsDependentCSubstring& token = tokenizer.nextToken();
        if (token.IsEmpty()) {
          continue;
        }

        if (token.EqualsLiteral("form-data")) {
          seenFormData = true;
          continue;
        }

        if (seenFormData &&
            StringBeginsWith(token, NS_LITERAL_CSTRING("name="))) {
          mName = StringTail(token, token.Length() - 5);
          mName.Trim(" \"");
          continue;
        }

        if (seenFormData &&
            StringBeginsWith(token, NS_LITERAL_CSTRING("filename="))) {
          mFilename = StringTail(token, token.Length() - 9);
          mFilename.Trim(" \"");
          continue;
        }
      }

      if (mName.IsVoid()) {
        // Could not parse a valid entry name.
        return false;
      }
    } else if (headerName.LowerCaseEqualsLiteral("content-type")) {
      mContentType = headerValue;
    }

    return true;
  }

  // The end of a body is marked by a CRLF followed by the boundary. So the
  // CRLF is part of the boundary and not the body, but any prior CRLFs are
  // part of the body. This will position the iterator at the beginning of the
  // boundary (after the CRLF).
  bool
  ParseBody(const nsACString& aBoundaryString,
            nsACString::const_iterator& aStart,
            nsACString::const_iterator& aEnd)
  {
    const char* beginning = aStart.get();

    // Find the boundary marking the end of the body.
    nsACString::const_iterator end(aEnd);
    if (!FindInReadable(aBoundaryString, aStart, end)) {
      return false;
    }

    // We found a boundary, strip the just prior CRLF, and consider
    // everything else the body section.
    if (aStart.get() - beginning < 2) {
      // Only the first entry can have a boundary right at the beginning. Even
      // an empty body will have a CRLF before the boundary. So this is
      // a failure.
      return false;
    }

    // Check that there is a CRLF right before the boundary.
    aStart.advance(-2);

    // Skip optional hyphens.
    if (*aStart == '-' && *(aStart.get()+1) == '-') {
      if (aStart.get() - beginning < 2) {
        return false;
      }

      aStart.advance(-2);
    }

    if (*aStart != nsCRT::CR || *(aStart.get()+1) != nsCRT::LF) {
      return false;
    }

    nsAutoCString body(beginning, aStart.get() - beginning);

    // Restore iterator to after the \r\n as we promised.
    // We do not need to handle the extra hyphens case since our boundary
    // parser in PushOverBoundary()
    aStart.advance(2);

    if (!mFormData) {
      mFormData = new nsFormData();
    }

    NS_ConvertUTF8toUTF16 name(mName);

    if (mFilename.IsVoid()) {
      mFormData->Append(name, NS_ConvertUTF8toUTF16(body));
    } else {
      // Unfortunately we've to copy the data first since all our strings are
      // going to free it. We also need fallible alloc, so we can't just use
      // ToNewCString().
      char* copy = static_cast<char*>(moz_xmalloc(body.Length()));
      if (!copy) {
        NS_WARNING("Failed to copy File entry body.");
        return false;
      }
      nsCString::const_iterator bodyIter, bodyEnd;
      body.BeginReading(bodyIter);
      body.EndReading(bodyEnd);
      char *p = copy;
      while (bodyIter != bodyEnd) {
        *p++ = *bodyIter++;
      }
      p = nullptr;

      nsRefPtr<File> file =
        File::CreateMemoryFile(mParentObject,
                               reinterpret_cast<void *>(copy), body.Length(),
                               NS_ConvertUTF8toUTF16(mFilename),
                               NS_ConvertUTF8toUTF16(mContentType), /* aLastModifiedDate */ 0);
      Optional<nsAString> dummy;
      mFormData->Append(name, *file, dummy);
    }

    return true;
  }

public:
  FormDataParser(const nsACString& aMimeType, const nsACString& aData, nsIGlobalObject* aParent)
    : mMimeType(aMimeType), mData(aData), mState(START_PART), mParentObject(aParent)
  {
  }

  bool
  Parse()
  {
    // Determine boundary from mimetype.
    const char* boundaryId = nullptr;
    boundaryId = strstr(mMimeType.BeginWriting(), "boundary");
    if (!boundaryId) {
      return false;
    }

    boundaryId = strchr(boundaryId, '=');
    if (!boundaryId) {
      return false;
    }

    // Skip over '='.
    boundaryId++;

    char *attrib = (char *) strchr(boundaryId, ';');
    if (attrib) *attrib = '\0';

    nsAutoCString boundaryString(boundaryId);
    if (attrib) *attrib = ';';

    boundaryString.Trim(" \"");

    if (boundaryString.Length() == 0) {
      return false;
    }

    nsACString::const_iterator start, end;
    mData.BeginReading(start);
    // This should ALWAYS point to the end of data.
    // Helpers make copies.
    mData.EndReading(end);

    while (start != end) {
      switch(mState) {
        case START_PART:
          mName.SetIsVoid(true);
          mFilename.SetIsVoid(true);
          mContentType = NS_LITERAL_CSTRING("text/plain");

          // MUST start with boundary.
          if (!PushOverBoundary(boundaryString, start, end)) {
            return false;
          }

          if (start != end && *start == '-') {
            // End of data.
            if (!mFormData) {
              mFormData = new nsFormData();
            }
            return true;
          }

          if (!PushOverLine(start)) {
            return false;
          }
          mState = PARSE_HEADER;
          break;

        case PARSE_HEADER:
          bool emptyHeader;
          if (!ParseHeader(start, end, &emptyHeader)) {
            return false;
          }

          if (!PushOverLine(start)) {
            return false;
          }

          mState = emptyHeader ? PARSE_BODY : PARSE_HEADER;
          break;

        case PARSE_BODY:
          if (mName.IsVoid()) {
            NS_WARNING("No content-disposition header with a valid name was "
                       "found. Failing at body parse.");
            return false;
          }

          if (!ParseBody(boundaryString, start, end)) {
            return false;
          }

          mState = START_PART;
          break;

        default:
          MOZ_CRASH("Invalid case");
      }
    }

    NS_NOTREACHED("Should never reach here.");
    return false;
  }

  already_AddRefed<nsFormData> FormData()
  {
    return mFormData.forget();
  }
};
} // anonymous namespace

nsresult
ExtractByteStreamFromBody(const OwningArrayBufferOrArrayBufferViewOrBlobOrFormDataOrUSVStringOrURLSearchParams& aBodyInit,
                          nsIInputStream** aStream,
                          nsCString& aContentType)
{
  MOZ_ASSERT(aStream);

  if (aBodyInit.IsArrayBuffer()) {
    const ArrayBuffer& buf = aBodyInit.GetAsArrayBuffer();
    return ExtractFromArrayBuffer(buf, aStream);
  } else if (aBodyInit.IsArrayBufferView()) {
    const ArrayBufferView& buf = aBodyInit.GetAsArrayBufferView();
    return ExtractFromArrayBufferView(buf, aStream);
  } else if (aBodyInit.IsBlob()) {
    const File& blob = aBodyInit.GetAsBlob();
    return ExtractFromBlob(blob, aStream, aContentType);
  } else if (aBodyInit.IsFormData()) {
    nsFormData& form = aBodyInit.GetAsFormData();
    return ExtractFromFormData(form, aStream, aContentType);
  } else if (aBodyInit.IsUSVString()) {
    nsAutoString str;
    str.Assign(aBodyInit.GetAsUSVString());
    return ExtractFromUSVString(str, aStream, aContentType);
  } else if (aBodyInit.IsURLSearchParams()) {
    URLSearchParams& params = aBodyInit.GetAsURLSearchParams();
    return ExtractFromURLSearchParams(params, aStream, aContentType);
  }

  NS_NOTREACHED("Should never reach here");
  return NS_ERROR_FAILURE;
}

nsresult
ExtractByteStreamFromBody(const ArrayBufferOrArrayBufferViewOrBlobOrFormDataOrUSVStringOrURLSearchParams& aBodyInit,
                          nsIInputStream** aStream,
                          nsCString& aContentType)
{
  MOZ_ASSERT(aStream);

  if (aBodyInit.IsArrayBuffer()) {
    const ArrayBuffer& buf = aBodyInit.GetAsArrayBuffer();
    return ExtractFromArrayBuffer(buf, aStream);
  } else if (aBodyInit.IsArrayBufferView()) {
    const ArrayBufferView& buf = aBodyInit.GetAsArrayBufferView();
    return ExtractFromArrayBufferView(buf, aStream);
  } else if (aBodyInit.IsBlob()) {
    const File& blob = aBodyInit.GetAsBlob();
    return ExtractFromBlob(blob, aStream, aContentType);
  } else if (aBodyInit.IsFormData()) {
    nsFormData& form = aBodyInit.GetAsFormData();
    return ExtractFromFormData(form, aStream, aContentType);
  } else if (aBodyInit.IsUSVString()) {
    nsAutoString str;
    str.Assign(aBodyInit.GetAsUSVString());
    return ExtractFromUSVString(str, aStream, aContentType);
  } else if (aBodyInit.IsURLSearchParams()) {
    URLSearchParams& params = aBodyInit.GetAsURLSearchParams();
    return ExtractFromURLSearchParams(params, aStream, aContentType);
  }

  NS_NOTREACHED("Should never reach here");
  return NS_ERROR_FAILURE;
}

namespace {
class StreamDecoder final
{
  nsCOMPtr<nsIUnicodeDecoder> mDecoder;
  nsString mDecoded;

public:
  StreamDecoder()
    : mDecoder(EncodingUtils::DecoderForEncoding("UTF-8"))
  {
    MOZ_ASSERT(mDecoder);
  }

  nsresult
  AppendText(const char* aSrcBuffer, uint32_t aSrcBufferLen)
  {
    int32_t destBufferLen;
    nsresult rv =
      mDecoder->GetMaxLength(aSrcBuffer, aSrcBufferLen, &destBufferLen);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!mDecoded.SetCapacity(mDecoded.Length() + destBufferLen, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    char16_t* destBuffer = mDecoded.BeginWriting() + mDecoded.Length();
    int32_t totalChars = mDecoded.Length();

    int32_t srcLen = (int32_t) aSrcBufferLen;
    int32_t outLen = destBufferLen;
    rv = mDecoder->Convert(aSrcBuffer, &srcLen, destBuffer, &outLen);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    totalChars += outLen;
    mDecoded.SetLength(totalChars);

    return NS_OK;
  }

  nsString&
  GetText()
  {
    return mDecoded;
  }
};

/*
 * Called on successfully reading the complete stream.
 */
template <class Derived>
class ContinueConsumeBodyRunnable final : public WorkerRunnable
{
  // This has been addrefed before this runnable is dispatched,
  // released in WorkerRun().
  FetchBody<Derived>* mFetchBody;
  nsresult mStatus;
  uint32_t mLength;
  uint8_t* mResult;

public:
  ContinueConsumeBodyRunnable(FetchBody<Derived>* aFetchBody, nsresult aStatus,
                              uint32_t aLength, uint8_t* aResult)
    : WorkerRunnable(aFetchBody->mWorkerPrivate, WorkerThreadModifyBusyCount)
    , mFetchBody(aFetchBody)
    , mStatus(aStatus)
    , mLength(aLength)
    , mResult(aResult)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    mFetchBody->ContinueConsumeBody(mStatus, mLength, mResult);
    return true;
  }
};

// OnStreamComplete always adopts the buffer, utility class to release it in
// a couple of places.
class MOZ_STACK_CLASS AutoFreeBuffer final {
  uint8_t* mBuffer;

public:
  explicit AutoFreeBuffer(uint8_t* aBuffer)
    : mBuffer(aBuffer)
  {}

  ~AutoFreeBuffer()
  {
    free(mBuffer);
  }

  void
  Reset()
  {
    mBuffer= nullptr;
  }
};

template <class Derived>
class FailConsumeBodyWorkerRunnable : public MainThreadWorkerControlRunnable
{
  FetchBody<Derived>* mBody;
public:
  explicit FailConsumeBodyWorkerRunnable(FetchBody<Derived>* aBody)
    : MainThreadWorkerControlRunnable(aBody->mWorkerPrivate)
    , mBody(aBody)
  {
    AssertIsOnMainThread();
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    mBody->ContinueConsumeBody(NS_ERROR_FAILURE, 0, nullptr);
    return true;
  }
};

/*
 * In case of failure to create a stream pump or dispatch stream completion to
 * worker, ensure we cleanup properly. Thread agnostic.
 */
template <class Derived>
class MOZ_STACK_CLASS AutoFailConsumeBody final
{
  FetchBody<Derived>* mBody;
public:
  explicit AutoFailConsumeBody(FetchBody<Derived>* aBody)
    : mBody(aBody)
  { }

  ~AutoFailConsumeBody()
  {
    AssertIsOnMainThread();
    if (mBody) {
      if (mBody->mWorkerPrivate) {
        nsRefPtr<FailConsumeBodyWorkerRunnable<Derived>> r =
          new FailConsumeBodyWorkerRunnable<Derived>(mBody);
        AutoSafeJSContext cx;
        if (!r->Dispatch(cx)) {
          MOZ_CRASH("We are going to leak");
        }
      } else {
        mBody->ContinueConsumeBody(NS_ERROR_FAILURE, 0, nullptr);
      }
    }
  }

  void
  DontFail()
  {
    mBody = nullptr;
  }
};

template <class Derived>
class ConsumeBodyDoneObserver : public nsIStreamLoaderObserver
{
  FetchBody<Derived>* mFetchBody;

public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit ConsumeBodyDoneObserver(FetchBody<Derived>* aFetchBody)
    : mFetchBody(aFetchBody)
  { }

  NS_IMETHOD
  OnStreamComplete(nsIStreamLoader* aLoader,
                   nsISupports* aCtxt,
                   nsresult aStatus,
                   uint32_t aResultLength,
                   const uint8_t* aResult) override
  {
    MOZ_ASSERT(NS_IsMainThread());

    // If the binding requested cancel, we don't need to call
    // ContinueConsumeBody, since that is the originator.
    if (aStatus == NS_BINDING_ABORTED) {
      return NS_OK;
    }

    uint8_t* nonconstResult = const_cast<uint8_t*>(aResult);
    if (mFetchBody->mWorkerPrivate) {
      // This way if the runnable dispatch fails, the body is still released.
      AutoFailConsumeBody<Derived> autoFail(mFetchBody);
      nsRefPtr<ContinueConsumeBodyRunnable<Derived>> r =
        new ContinueConsumeBodyRunnable<Derived>(mFetchBody,
                                        aStatus,
                                        aResultLength,
                                        nonconstResult);
      AutoSafeJSContext cx;
      if (r->Dispatch(cx)) {
        autoFail.DontFail();
      } else {
        NS_WARNING("Could not dispatch ConsumeBodyRunnable");
        // Return failure so that aResult is freed.
        return NS_ERROR_FAILURE;
      }
    } else {
      mFetchBody->ContinueConsumeBody(aStatus, aResultLength, nonconstResult);
    }

    // FetchBody is responsible for data.
    return NS_SUCCESS_ADOPTED_DATA;
  }

private:
  virtual ~ConsumeBodyDoneObserver()
  { }
};

template <class Derived>
NS_IMPL_ADDREF(ConsumeBodyDoneObserver<Derived>)
template <class Derived>
NS_IMPL_RELEASE(ConsumeBodyDoneObserver<Derived>)
template <class Derived>
NS_INTERFACE_MAP_BEGIN(ConsumeBodyDoneObserver<Derived>)
  NS_INTERFACE_MAP_ENTRY(nsIStreamLoaderObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStreamLoaderObserver)
NS_INTERFACE_MAP_END

template <class Derived>
class BeginConsumeBodyRunnable final : public nsRunnable
{
  FetchBody<Derived>* mFetchBody;
public:
  explicit BeginConsumeBodyRunnable(FetchBody<Derived>* aBody)
    : mFetchBody(aBody)
  { }

  NS_IMETHOD
  Run() override
  {
    mFetchBody->BeginConsumeBodyMainThread();
    return NS_OK;
  }
};

template <class Derived>
class CancelPumpRunnable final : public WorkerMainThreadRunnable
{
  FetchBody<Derived>* mBody;
public:
  explicit CancelPumpRunnable(FetchBody<Derived>* aBody)
    : WorkerMainThreadRunnable(aBody->mWorkerPrivate)
    , mBody(aBody)
  { }

  bool
  MainThreadRun() override
  {
    mBody->CancelPump();
    return true;
  }
};
} // anonymous namespace

template <class Derived>
class FetchBodyFeature final : public workers::WorkerFeature
{
  // This is addrefed before the feature is created, and is released in ContinueConsumeBody()
  // so we can hold a rawptr.
  FetchBody<Derived>* mBody;

public:
  explicit FetchBodyFeature(FetchBody<Derived>* aBody)
    : mBody(aBody)
  { }

  ~FetchBodyFeature()
  { }

  bool Notify(JSContext* aCx, workers::Status aStatus) override
  {
    MOZ_ASSERT(aStatus > workers::Running);
    mBody->ContinueConsumeBody(NS_BINDING_ABORTED, 0, nullptr);
    return true;
  }
};

template <class Derived>
FetchBody<Derived>::FetchBody()
  : mFeature(nullptr)
  , mBodyUsed(false)
  , mReadDone(false)
{
  if (!NS_IsMainThread()) {
    mWorkerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(mWorkerPrivate);
  } else {
    mWorkerPrivate = nullptr;
  }
}

template
FetchBody<Request>::FetchBody();

template
FetchBody<Response>::FetchBody();

template <class Derived>
FetchBody<Derived>::~FetchBody()
{
}

// Returns true if addref succeeded.
// Always succeeds on main thread.
// May fail on worker if RegisterFeature() fails. In that case, it will release
// the object before returning false.
template <class Derived>
bool
FetchBody<Derived>::AddRefObject()
{
  AssertIsOnTargetThread();
  DerivedClass()->AddRef();

  if (mWorkerPrivate && !mFeature) {
    if (!RegisterFeature()) {
      ReleaseObject();
      return false;
    }
  }
  return true;
}

template <class Derived>
void
FetchBody<Derived>::ReleaseObject()
{
  AssertIsOnTargetThread();

  if (mWorkerPrivate && mFeature) {
    UnregisterFeature();
  }

  DerivedClass()->Release();
}

template <class Derived>
bool
FetchBody<Derived>::RegisterFeature()
{
  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(!mFeature);
  mFeature = new FetchBodyFeature<Derived>(this);

  if (!mWorkerPrivate->AddFeature(mWorkerPrivate->GetJSContext(), mFeature)) {
    NS_WARNING("Failed to add feature");
    mFeature = nullptr;
    return false;
  }

  return true;
}

template <class Derived>
void
FetchBody<Derived>::UnregisterFeature()
{
  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(mFeature);

  mWorkerPrivate->RemoveFeature(mWorkerPrivate->GetJSContext(), mFeature);
  mFeature = nullptr;
}

template <class Derived>
void
FetchBody<Derived>::CancelPump()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mConsumeBodyPump);
  mConsumeBodyPump->Cancel(NS_BINDING_ABORTED);
}

// Return value is used by ConsumeBody to bubble the error code up to WebIDL so
// mConsumePromise doesn't have to be rejected on early exit.
template <class Derived>
nsresult
FetchBody<Derived>::BeginConsumeBody()
{
  AssertIsOnTargetThread();
  MOZ_ASSERT(!mFeature);
  MOZ_ASSERT(mConsumePromise);

  // The FetchBody is not thread-safe refcounted. We addref it here and release
  // it once the stream read is finished.
  if (!AddRefObject()) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIRunnable> r = new BeginConsumeBodyRunnable<Derived>(this);
  nsresult rv = NS_DispatchToMainThread(r);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ReleaseObject();
    return rv;
  }
  return NS_OK;
}

/*
 * BeginConsumeBodyMainThread() will automatically reject the consume promise
 * and clean up on any failures, so there is no need for callers to do so,
 * reflected in a lack of error return code.
 */
template <class Derived>
void
FetchBody<Derived>::BeginConsumeBodyMainThread()
{
  AssertIsOnMainThread();
  AutoFailConsumeBody<Derived> autoReject(DerivedClass());
  nsresult rv;
  nsCOMPtr<nsIInputStream> stream;
  DerivedClass()->GetBody(getter_AddRefs(stream));
  if (!stream) {
    rv = NS_NewCStringInputStream(getter_AddRefs(stream), EmptyCString());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  }

  nsCOMPtr<nsIInputStreamPump> pump;
  rv = NS_NewInputStreamPump(getter_AddRefs(pump),
                             stream);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsRefPtr<ConsumeBodyDoneObserver<Derived>> p = new ConsumeBodyDoneObserver<Derived>(this);
  nsCOMPtr<nsIStreamLoader> loader;
  rv = NS_NewStreamLoader(getter_AddRefs(loader), p);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  rv = pump->AsyncRead(loader, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  // Now that everything succeeded, we can assign the pump to a pointer that
  // stays alive for the lifetime of the FetchBody.
  mConsumeBodyPump = new nsMainThreadPtrHolder<nsIInputStreamPump>(pump);
  // It is ok for retargeting to fail and reads to happen on the main thread.
  autoReject.DontFail();

  // Try to retarget, otherwise fall back to main thread.
  nsCOMPtr<nsIThreadRetargetableRequest> rr = do_QueryInterface(pump);
  if (rr) {
    nsCOMPtr<nsIEventTarget> sts = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
    rv = rr->RetargetDeliveryTo(sts);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      NS_WARNING("Retargeting failed");
    }
  }
}

template <class Derived>
void
FetchBody<Derived>::ContinueConsumeBody(nsresult aStatus, uint32_t aResultLength, uint8_t* aResult)
{
  AssertIsOnTargetThread();
  // Just a precaution to ensure ContinueConsumeBody is not called out of
  // sync with a body read.
  MOZ_ASSERT(mBodyUsed);
  MOZ_ASSERT(!mReadDone);
  MOZ_ASSERT_IF(mWorkerPrivate, mFeature);
  mReadDone = true;

  AutoFreeBuffer autoFree(aResult);

  MOZ_ASSERT(mConsumePromise);
  nsRefPtr<Promise> localPromise = mConsumePromise.forget();

  nsRefPtr<Derived> kungfuDeathGrip = DerivedClass();
  ReleaseObject();

  if (NS_WARN_IF(NS_FAILED(aStatus))) {
    localPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);

    // If binding aborted, cancel the pump. We can't assert mConsumeBodyPump.
    // In the (admittedly rare) situation that BeginConsumeBodyMainThread()
    // context switches out, and the worker thread gets canceled before the
    // pump is setup, mConsumeBodyPump will be null.
    // We've to use the !! form since non-main thread pointer access on
    // a nsMainThreadPtrHandle is not permitted.
    if (aStatus == NS_BINDING_ABORTED && !!mConsumeBodyPump) {
      if (NS_IsMainThread()) {
        CancelPump();
      } else {
        MOZ_ASSERT(mWorkerPrivate);
        // In case of worker thread, we block the worker while the request is
        // canceled on the main thread. This ensures that OnStreamComplete has
        // a valid FetchBody around to call CancelPump and we don't release the
        // FetchBody on the main thread.
        nsRefPtr<CancelPumpRunnable<Derived>> r =
          new CancelPumpRunnable<Derived>(this);
        if (!r->Dispatch(mWorkerPrivate->GetJSContext())) {
          NS_WARNING("Could not dispatch CancelPumpRunnable. Nothing we can do here");
        }
      }
    }
  }

  // Release the pump and then early exit if there was an error.
  // Uses NS_ProxyRelease internally, so this is safe.
  mConsumeBodyPump = nullptr;

  // Don't warn here since we warned above.
  if (NS_FAILED(aStatus)) {
    return;
  }

  // Finish successfully consuming body according to type.
  MOZ_ASSERT(aResult);

  AutoJSAPI jsapi;
  jsapi.Init(DerivedClass()->GetParentObject());
  JSContext* cx = jsapi.cx();

  switch (mConsumeType) {
    case CONSUME_ARRAYBUFFER: {
      JS::Rooted<JSObject*> arrayBuffer(cx);
      arrayBuffer = JS_NewArrayBufferWithContents(cx, aResultLength, reinterpret_cast<void *>(aResult));
      if (!arrayBuffer) {
        JS_ClearPendingException(cx);
        localPromise->MaybeReject(NS_ERROR_DOM_UNKNOWN_ERR);
        NS_WARNING("OUT OF MEMORY");
        return;
      }

      JS::Rooted<JS::Value> val(cx);
      val.setObjectOrNull(arrayBuffer);
      localPromise->MaybeResolve(cx, val);
      // ArrayBuffer takes over ownership.
      autoFree.Reset();
      return;
    }
    case CONSUME_BLOB: {
      nsRefPtr<File> blob =
        File::CreateMemoryFile(DerivedClass()->GetParentObject(),
                               reinterpret_cast<void *>(aResult), aResultLength, NS_ConvertUTF8toUTF16(mMimeType));

      if (!blob) {
        localPromise->MaybeReject(NS_ERROR_DOM_UNKNOWN_ERR);
        return;
      }

      localPromise->MaybeResolve(blob);
      // File takes over ownership.
      autoFree.Reset();
      return;
    }
    case CONSUME_FORMDATA: {
      nsCString data;
      data.Adopt(reinterpret_cast<char*>(aResult), aResultLength);
      autoFree.Reset();

      NS_NAMED_LITERAL_CSTRING(formDataMimeType, NS_LITERAL_CSTRING("multipart/form-data"));

      // Allow semicolon separated boundary/encoding suffix like multipart/form-data; boundary=
      // but disallow multipart/form-datafoobar.
      bool isValidFormDataMimeType = StringBeginsWith(mMimeType, formDataMimeType);

      if (isValidFormDataMimeType && mMimeType.Length() > formDataMimeType.Length()) {
        isValidFormDataMimeType = mMimeType[formDataMimeType.Length()] == ';';
      }

      if (isValidFormDataMimeType) {
        FormDataParser parser(mMimeType, data, DerivedClass()->GetParentObject());
        if (!parser.Parse()) {
          ErrorResult result;
          result.ThrowTypeError(MSG_BAD_FORMDATA);
          localPromise->MaybeReject(result);
          return;
        }

        nsRefPtr<nsFormData> fd = parser.FormData();
        MOZ_ASSERT(fd);
        localPromise->MaybeResolve(fd);
      } else {
        NS_NAMED_LITERAL_CSTRING(urlDataMimeType, NS_LITERAL_CSTRING("application/x-www-form-urlencoded"));
        bool isValidUrlEncodedMimeType = StringBeginsWith(mMimeType, urlDataMimeType);

        if (isValidUrlEncodedMimeType && mMimeType.Length() > urlDataMimeType.Length()) {
          isValidUrlEncodedMimeType = mMimeType[urlDataMimeType.Length()] == ';';
        }

        if (isValidUrlEncodedMimeType) {
          nsRefPtr<URLSearchParams> params = new URLSearchParams();
          params->ParseInput(data, /* aObserver */ nullptr);

          nsRefPtr<nsFormData> fd = new nsFormData(DerivedClass()->GetParentObject());
          params->ForEach(FillFormData, static_cast<void*>(fd));
          localPromise->MaybeResolve(fd);
        } else {
          ErrorResult result;
          result.ThrowTypeError(MSG_BAD_FORMDATA);
          localPromise->MaybeReject(result);
        }
      }
      return;
    }
    case CONSUME_TEXT:
      // fall through handles early exit.
    case CONSUME_JSON: {
      StreamDecoder decoder;
      decoder.AppendText(reinterpret_cast<char*>(aResult), aResultLength);

      nsString& decoded = decoder.GetText();
      if (mConsumeType == CONSUME_TEXT) {
        localPromise->MaybeResolve(decoded);
        return;
      }

      AutoForceSetExceptionOnContext forceExn(cx);
      JS::Rooted<JS::Value> json(cx);
      if (!JS_ParseJSON(cx, decoded.get(), decoded.Length(), &json)) {
        if (!JS_IsExceptionPending(cx)) {
          localPromise->MaybeReject(NS_ERROR_DOM_UNKNOWN_ERR);
          return;
        }

        JS::Rooted<JS::Value> exn(cx);
        DebugOnly<bool> gotException = JS_GetPendingException(cx, &exn);
        MOZ_ASSERT(gotException);

        JS_ClearPendingException(cx);
        localPromise->MaybeReject(cx, exn);
        return;
      }

      localPromise->MaybeResolve(cx, json);
      return;
    }
  }

  NS_NOTREACHED("Unexpected consume body type");
}

template <class Derived>
already_AddRefed<Promise>
FetchBody<Derived>::ConsumeBody(ConsumeType aType, ErrorResult& aRv)
{
  mConsumeType = aType;
  if (BodyUsed()) {
    aRv.ThrowTypeError(MSG_FETCH_BODY_CONSUMED_ERROR);
    return nullptr;
  }

  SetBodyUsed();

  mConsumePromise = Promise::Create(DerivedClass()->GetParentObject(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  aRv = BeginConsumeBody();
  if (NS_WARN_IF(aRv.Failed())) {
    mConsumePromise = nullptr;
    return nullptr;
  }

  nsRefPtr<Promise> promise = mConsumePromise;
  return promise.forget();
}

template
already_AddRefed<Promise>
FetchBody<Request>::ConsumeBody(ConsumeType aType, ErrorResult& aRv);

template
already_AddRefed<Promise>
FetchBody<Response>::ConsumeBody(ConsumeType aType, ErrorResult& aRv);

template <class Derived>
void
FetchBody<Derived>::SetMimeType()
{
  // Extract mime type.
  ErrorResult result;
  nsTArray<nsCString> contentTypeValues;
  MOZ_ASSERT(DerivedClass()->GetInternalHeaders());
  DerivedClass()->GetInternalHeaders()->GetAll(NS_LITERAL_CSTRING("Content-Type"),
                                               contentTypeValues, result);
  MOZ_ALWAYS_TRUE(!result.Failed());

  // HTTP ABNF states Content-Type may have only one value.
  // This is from the "parse a header value" of the fetch spec.
  if (contentTypeValues.Length() == 1) {
    mMimeType = contentTypeValues[0];
    ToLowerCase(mMimeType);
  }
}

template
void
FetchBody<Request>::SetMimeType();

template
void
FetchBody<Response>::SetMimeType();
} // namespace dom
} // namespace mozilla
