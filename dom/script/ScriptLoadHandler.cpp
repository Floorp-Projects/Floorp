/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScriptLoadHandler.h"

#include <stdlib.h>
#include <utility>
#include "ScriptCompression.h"
#include "ScriptLoader.h"
#include "ScriptTrace.h"
#include "js/Transcoding.h"
#include "js/loader/ScriptLoadRequest.h"
#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Encoding.h"
#include "mozilla/Logging.h"
#include "mozilla/NotNull.h"
#include "mozilla/PerfStats.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/Utf8.h"
#include "mozilla/Vector.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/SRICheck.h"
#include "mozilla/dom/ScriptDecoding.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsICacheInfoChannel.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIRequest.h"
#include "nsIScriptElement.h"
#include "nsIURI.h"
#include "nsJSUtils.h"
#include "nsMimeTypes.h"
#include "nsString.h"
#include "nsTArray.h"
#include "zlib.h"

namespace mozilla::dom {

#undef LOG
#define LOG(args) \
  MOZ_LOG(ScriptLoader::gScriptLoaderLog, mozilla::LogLevel::Debug, args)

#define LOG_ENABLED() \
  MOZ_LOG_TEST(ScriptLoader::gScriptLoaderLog, mozilla::LogLevel::Debug)

ScriptDecoder::ScriptDecoder(const Encoding* aEncoding,
                             ScriptDecoder::BOMHandling handleBOM) {
  if (handleBOM == BOMHandling::Ignore) {
    mDecoder = aEncoding->NewDecoderWithoutBOMHandling();
  } else {
    mDecoder = aEncoding->NewDecoderWithBOMRemoval();
  }
  MOZ_ASSERT(mDecoder);
}

template <typename Unit>
nsresult ScriptDecoder::DecodeRawDataHelper(
    JS::loader::ScriptLoadRequest* aRequest, const uint8_t* aData,
    uint32_t aDataLength, bool aEndOfStream) {
  CheckedInt<size_t> needed =
      ScriptDecoding<Unit>::MaxBufferLength(mDecoder, aDataLength);
  if (!needed.isValid()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Reference to the script source buffer which we will update.
  JS::loader::ScriptLoadRequest::ScriptTextBuffer<Unit>& scriptText =
      aRequest->ScriptText<Unit>();

  uint32_t haveRead = scriptText.length();

  CheckedInt<uint32_t> capacity = haveRead;
  capacity += needed.value();

  if (!capacity.isValid() || !scriptText.resize(capacity.value())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  size_t written = ScriptDecoding<Unit>::DecodeInto(
      mDecoder, Span(aData, aDataLength),
      Span(scriptText.begin() + haveRead, needed.value()), aEndOfStream);
  MOZ_ASSERT(written <= needed.value());

  haveRead += written;
  MOZ_ASSERT(haveRead <= capacity.value(),
             "mDecoder produced more data than expected");
  MOZ_ALWAYS_TRUE(scriptText.resize(haveRead));
  aRequest->SetReceivedScriptTextLength(scriptText.length());

  return NS_OK;
}

nsresult ScriptDecoder::DecodeRawData(JS::loader::ScriptLoadRequest* aRequest,
                                      const uint8_t* aData,
                                      uint32_t aDataLength, bool aEndOfStream) {
  if (aRequest->IsUTF16Text()) {
    return DecodeRawDataHelper<char16_t>(aRequest, aData, aDataLength,
                                         aEndOfStream);
  }

  return DecodeRawDataHelper<Utf8Unit>(aRequest, aData, aDataLength,
                                       aEndOfStream);
}

ScriptLoadHandler::ScriptLoadHandler(
    ScriptLoader* aScriptLoader, JS::loader::ScriptLoadRequest* aRequest,
    UniquePtr<SRICheckDataVerifier>&& aSRIDataVerifier)
    : mScriptLoader(aScriptLoader),
      mRequest(aRequest),
      mSRIDataVerifier(std::move(aSRIDataVerifier)),
      mSRIStatus(NS_OK) {
  MOZ_ASSERT(aRequest->IsUnknownDataType());
  MOZ_ASSERT(aRequest->IsFetching());
}

ScriptLoadHandler::~ScriptLoadHandler() = default;

NS_IMPL_ISUPPORTS(ScriptLoadHandler, nsIIncrementalStreamLoaderObserver)

NS_IMETHODIMP
ScriptLoadHandler::OnIncrementalData(nsIIncrementalStreamLoader* aLoader,
                                     nsISupports* aContext,
                                     uint32_t aDataLength, const uint8_t* aData,
                                     uint32_t* aConsumedLength) {
  nsCOMPtr<nsIRequest> channelRequest;
  aLoader->GetRequest(getter_AddRefs(channelRequest));

  auto firstTime = !mPreloadStartNotified;
  if (!mPreloadStartNotified) {
    mPreloadStartNotified = true;
    mRequest->GetScriptLoadContext()->NotifyStart(channelRequest);
  }

  if (mRequest->IsCanceled()) {
    // If request cancelled, ignore any incoming data.
    *aConsumedLength = aDataLength;
    return NS_OK;
  }

  nsresult rv = NS_OK;
  if (mRequest->IsUnknownDataType()) {
    rv = EnsureKnownDataType(aLoader);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (mRequest->IsBytecode() && firstTime) {
    PerfStats::RecordMeasurementStart(PerfStats::Metric::JSBC_IO_Read);
  }

  if (mRequest->IsTextSource()) {
    if (!EnsureDecoder(aLoader, aData, aDataLength,
                       /* aEndOfStream = */ false)) {
      return NS_OK;
    }

    // Below we will/shall consume entire data chunk.
    *aConsumedLength = aDataLength;

    // Decoder has already been initialized. -- trying to decode all loaded
    // bytes.
    rv = mDecoder->DecodeRawData(mRequest, aData, aDataLength,
                                 /* aEndOfStream = */ false);
    NS_ENSURE_SUCCESS(rv, rv);

    // If SRI is required for this load, appending new bytes to the hash.
    if (mSRIDataVerifier && NS_SUCCEEDED(mSRIStatus)) {
      mSRIStatus = mSRIDataVerifier->Update(aDataLength, aData);
    }
  } else {
    MOZ_ASSERT(mRequest->IsBytecode());
    if (!mRequest->SRIAndBytecode().append(aData, aDataLength)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    *aConsumedLength = aDataLength;
    uint32_t sriLength = 0;
    rv = MaybeDecodeSRI(&sriLength);
    if (NS_FAILED(rv)) {
      return channelRequest->Cancel(mScriptLoader->RestartLoad(mRequest));
    }
    if (sriLength) {
      mRequest->SetSRILength(sriLength);
    }
  }

  return rv;
}

bool ScriptLoadHandler::TrySetDecoder(nsIIncrementalStreamLoader* aLoader,
                                      const uint8_t* aData,
                                      uint32_t aDataLength, bool aEndOfStream) {
  MOZ_ASSERT(mDecoder == nullptr,
             "can't have a decoder already if we're trying to set one");

  // JavaScript modules are always UTF-8.
  if (mRequest->IsModuleRequest()) {
    mDecoder = MakeUnique<ScriptDecoder>(UTF_8_ENCODING,
                                         ScriptDecoder::BOMHandling::Remove);
    return true;
  }

  // Determine if BOM check should be done.  This occurs either
  // if end-of-stream has been reached, or at least 3 bytes have
  // been read from input.
  if (!aEndOfStream && (aDataLength < 3)) {
    return false;
  }

  // Do BOM detection.
  const Encoding* encoding;
  std::tie(encoding, std::ignore) = Encoding::ForBOM(Span(aData, aDataLength));
  if (encoding) {
    mDecoder =
        MakeUnique<ScriptDecoder>(encoding, ScriptDecoder::BOMHandling::Remove);
    return true;
  }

  // BOM detection failed, check content stream for charset.
  nsCOMPtr<nsIRequest> req;
  nsresult rv = aLoader->GetRequest(getter_AddRefs(req));
  NS_ASSERTION(req, "StreamLoader's request went away prematurely");
  NS_ENSURE_SUCCESS(rv, false);

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(req);

  if (channel) {
    nsAutoCString label;
    if (NS_SUCCEEDED(channel->GetContentCharset(label)) &&
        (encoding = Encoding::ForLabel(label))) {
      mDecoder = MakeUnique<ScriptDecoder>(encoding,
                                           ScriptDecoder::BOMHandling::Ignore);
      return true;
    }
  }

  // Check the hint charset from the script element or preload
  // request.
  nsAutoString hintCharset;
  if (!mRequest->GetScriptLoadContext()->IsPreload()) {
    mRequest->GetScriptLoadContext()->GetScriptElement()->GetScriptCharset(
        hintCharset);
  } else {
    nsTArray<ScriptLoader::PreloadInfo>::index_type i =
        mScriptLoader->mPreloads.IndexOf(
            mRequest, 0, ScriptLoader::PreloadRequestComparator());

    NS_ASSERTION(i != mScriptLoader->mPreloads.NoIndex,
                 "Incorrect preload bookkeeping");
    hintCharset = mScriptLoader->mPreloads[i].mCharset;
  }

  if ((encoding = Encoding::ForLabel(hintCharset))) {
    mDecoder =
        MakeUnique<ScriptDecoder>(encoding, ScriptDecoder::BOMHandling::Ignore);
    return true;
  }

  // Get the charset from the charset of the document.
  if (mScriptLoader->mDocument) {
    encoding = mScriptLoader->mDocument->GetDocumentCharacterSet();
    mDecoder =
        MakeUnique<ScriptDecoder>(encoding, ScriptDecoder::BOMHandling::Ignore);
    return true;
  }

  // Curiously, there are various callers that don't pass aDocument. The
  // fallback in the old code was ISO-8859-1, which behaved like
  // windows-1252.
  mDecoder = MakeUnique<ScriptDecoder>(WINDOWS_1252_ENCODING,
                                       ScriptDecoder::BOMHandling::Ignore);
  return true;
}

nsresult ScriptLoadHandler::MaybeDecodeSRI(uint32_t* sriLength) {
  *sriLength = 0;

  if (!mSRIDataVerifier || mSRIDataVerifier->IsComplete() ||
      NS_FAILED(mSRIStatus)) {
    return NS_OK;
  }

  // Skip until the content is large enough to be decoded.
  JS::TranscodeBuffer& receivedData = mRequest->SRIAndBytecode();
  if (receivedData.length() <= mSRIDataVerifier->DataSummaryLength()) {
    return NS_OK;
  }

  mSRIStatus = mSRIDataVerifier->ImportDataSummary(receivedData.length(),
                                                   receivedData.begin());

  if (NS_FAILED(mSRIStatus)) {
    // We are unable to decode the hash contained in the alternate data which
    // contains the bytecode, or it does not use the same algorithm.
    LOG(
        ("ScriptLoadHandler::MaybeDecodeSRI, failed to decode SRI, restart "
         "request"));
    return mSRIStatus;
  }

  *sriLength = mSRIDataVerifier->DataSummaryLength();
  MOZ_ASSERT(*sriLength > 0);
  return NS_OK;
}

nsresult ScriptLoadHandler::EnsureKnownDataType(
    nsIIncrementalStreamLoader* aLoader) {
  MOZ_ASSERT(mRequest->IsUnknownDataType());
  MOZ_ASSERT(mRequest->IsFetching());

  nsCOMPtr<nsIRequest> req;
  nsresult rv = aLoader->GetRequest(getter_AddRefs(req));
  MOZ_ASSERT(req, "StreamLoader's request went away prematurely");
  NS_ENSURE_SUCCESS(rv, rv);

  if (mRequest->mFetchSourceOnly) {
    mRequest->SetTextSource(mRequest->mLoadContext.get());
    TRACE_FOR_TEST(mRequest->GetScriptLoadContext()->GetScriptElement(),
                   "scriptloader_load_source");
    return NS_OK;
  }

  nsCOMPtr<nsICacheInfoChannel> cic(do_QueryInterface(req));
  if (cic) {
    nsAutoCString altDataType;
    cic->GetAlternativeDataType(altDataType);
    if (altDataType.Equals(ScriptLoader::BytecodeMimeTypeFor(mRequest))) {
      mRequest->SetBytecode();
      TRACE_FOR_TEST(mRequest->GetScriptLoadContext()->GetScriptElement(),
                     "scriptloader_load_bytecode");
      return NS_OK;
    }
    MOZ_ASSERT(altDataType.IsEmpty());
  }

  mRequest->SetTextSource(mRequest->mLoadContext.get());
  TRACE_FOR_TEST(mRequest->GetScriptLoadContext()->GetScriptElement(),
                 "scriptloader_load_source");

  MOZ_ASSERT(!mRequest->IsUnknownDataType());
  MOZ_ASSERT(mRequest->IsFetching());
  return NS_OK;
}

NS_IMETHODIMP
ScriptLoadHandler::OnStreamComplete(nsIIncrementalStreamLoader* aLoader,
                                    nsISupports* aContext, nsresult aStatus,
                                    uint32_t aDataLength,
                                    const uint8_t* aData) {
  nsresult rv = NS_OK;
  if (LOG_ENABLED()) {
    nsAutoCString url;
    mRequest->mURI->GetAsciiSpec(url);
    LOG(("ScriptLoadRequest (%p): Stream complete (url = %s)", mRequest.get(),
         url.get()));
  }

  nsCOMPtr<nsIRequest> channelRequest;
  aLoader->GetRequest(getter_AddRefs(channelRequest));

  auto firstMessage = !mPreloadStartNotified;
  if (!mPreloadStartNotified) {
    mPreloadStartNotified = true;
    mRequest->GetScriptLoadContext()->NotifyStart(channelRequest);
  }

  auto notifyStop = MakeScopeExit([&] {
    mRequest->GetScriptLoadContext()->NotifyStop(channelRequest, rv);
  });

  if (!mRequest->IsCanceled()) {
    if (mRequest->IsUnknownDataType()) {
      rv = EnsureKnownDataType(aLoader);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (mRequest->IsBytecode() && !firstMessage) {
      // if firstMessage, then entire stream is in aData, and PerfStats would
      // measure 0 time
      PerfStats::RecordMeasurementEnd(PerfStats::Metric::JSBC_IO_Read);
    }

    if (mRequest->IsTextSource()) {
      DebugOnly<bool> encoderSet =
          EnsureDecoder(aLoader, aData, aDataLength, /* aEndOfStream = */ true);
      MOZ_ASSERT(encoderSet);
      rv = mDecoder->DecodeRawData(mRequest, aData, aDataLength,
                                   /* aEndOfStream = */ true);
      NS_ENSURE_SUCCESS(rv, rv);

      LOG(("ScriptLoadRequest (%p): Source length in code units = %u",
           mRequest.get(), unsigned(mRequest->ScriptTextLength())));

      // If SRI is required for this load, appending new bytes to the hash.
      if (mSRIDataVerifier && NS_SUCCEEDED(mSRIStatus)) {
        mSRIStatus = mSRIDataVerifier->Update(aDataLength, aData);
      }
    } else {
      MOZ_ASSERT(mRequest->IsBytecode());
      JS::TranscodeBuffer& bytecode = mRequest->SRIAndBytecode();
      if (!bytecode.append(aData, aDataLength)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      LOG(("ScriptLoadRequest (%p): Bytecode length = %u", mRequest.get(),
           unsigned(bytecode.length())));

      // If we abort while decoding the SRI, we fallback on explictly requesting
      // the source. Thus, we should not continue in
      // ScriptLoader::OnStreamComplete, which removes the request from the
      // waiting lists.
      //
      // We calculate the SRI length below.
      uint32_t unused;
      rv = MaybeDecodeSRI(&unused);
      if (NS_FAILED(rv)) {
        return channelRequest->Cancel(mScriptLoader->RestartLoad(mRequest));
      }

      // The bytecode cache always starts with the SRI hash, thus even if there
      // is no SRI data verifier instance, we still want to skip the hash.
      uint32_t sriLength;
      rv = SRICheckDataVerifier::DataSummaryLength(
          bytecode.length(), bytecode.begin(), &sriLength);
      if (NS_FAILED(rv)) {
        return channelRequest->Cancel(mScriptLoader->RestartLoad(mRequest));
      }

      mRequest->SetSRILength(sriLength);

      Vector<uint8_t> compressedBytecode;
      // mRequest has the compressed bytecode, but will be filled with the
      // uncompressed bytecode
      compressedBytecode.swap(bytecode);
      if (!JS::loader::ScriptBytecodeDecompress(
              compressedBytecode, mRequest->GetSRILength(), bytecode)) {
        return NS_ERROR_UNEXPECTED;
      }
    }
  }

  // Everything went well, keep the CacheInfoChannel alive such that we can
  // later save the bytecode on the cache entry.
  if (NS_SUCCEEDED(rv) && mRequest->IsSource() &&
      StaticPrefs::dom_script_loader_bytecode_cache_enabled()) {
    mRequest->mCacheInfo = do_QueryInterface(channelRequest);
    LOG(("ScriptLoadRequest (%p): nsICacheInfoChannel = %p", mRequest.get(),
         mRequest->mCacheInfo.get()));
  }

  // we have to mediate and use mRequest.
  rv = mScriptLoader->OnStreamComplete(aLoader, mRequest, aStatus, mSRIStatus,
                                       mSRIDataVerifier.get());

  // In case of failure, clear the mCacheInfoChannel to avoid keeping it alive.
  if (NS_FAILED(rv)) {
    mRequest->mCacheInfo = nullptr;
  }

  return rv;
}

#undef LOG_ENABLED
#undef LOG

}  // namespace mozilla::dom
