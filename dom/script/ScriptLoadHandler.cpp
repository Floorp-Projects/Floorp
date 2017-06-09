/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScriptLoadHandler.h"
#include "ScriptLoader.h"
#include "ScriptTrace.h"

#include "nsContentUtils.h"

#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/Telemetry.h"

namespace mozilla {
namespace dom {

#undef LOG
#define LOG(args) \
  MOZ_LOG(ScriptLoader::gScriptLoaderLog, mozilla::LogLevel::Debug, args)

#define LOG_ENABLED() \
  MOZ_LOG_TEST(ScriptLoader::gScriptLoaderLog, mozilla::LogLevel::Debug)

ScriptLoadHandler::ScriptLoadHandler(ScriptLoader* aScriptLoader,
                                     ScriptLoadRequest* aRequest,
                                     SRICheckDataVerifier* aSRIDataVerifier)
  : mScriptLoader(aScriptLoader),
    mRequest(aRequest),
    mSRIDataVerifier(aSRIDataVerifier),
    mSRIStatus(NS_OK),
    mDecoder()
{
  MOZ_ASSERT(mRequest->IsUnknownDataType());
  MOZ_ASSERT(mRequest->IsLoading());
}

ScriptLoadHandler::~ScriptLoadHandler()
{}

NS_IMPL_ISUPPORTS(ScriptLoadHandler, nsIIncrementalStreamLoaderObserver)

NS_IMETHODIMP
ScriptLoadHandler::OnIncrementalData(nsIIncrementalStreamLoader* aLoader,
                                     nsISupports* aContext,
                                     uint32_t aDataLength,
                                     const uint8_t* aData,
                                     uint32_t* aConsumedLength)
{
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

  if (mRequest->IsSource()) {
    if (!EnsureDecoder(aLoader, aData, aDataLength,
                       /* aEndOfStream = */ false)) {
      return NS_OK;
    }

    // Below we will/shall consume entire data chunk.
    *aConsumedLength = aDataLength;

    // Decoder has already been initialized. -- trying to decode all loaded bytes.
    rv = DecodeRawData(aData, aDataLength, /* aEndOfStream = */ false);
    NS_ENSURE_SUCCESS(rv, rv);

    // If SRI is required for this load, appending new bytes to the hash.
    if (mSRIDataVerifier && NS_SUCCEEDED(mSRIStatus)) {
      mSRIStatus = mSRIDataVerifier->Update(aDataLength, aData);
    }
  } else {
    MOZ_ASSERT(mRequest->IsBytecode());
    if (!mRequest->mScriptBytecode.append(aData, aDataLength)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    *aConsumedLength = aDataLength;
    rv = MaybeDecodeSRI();
    if (NS_FAILED(rv)) {
      nsCOMPtr<nsIRequest> channelRequest;
      aLoader->GetRequest(getter_AddRefs(channelRequest));
      return channelRequest->Cancel(mScriptLoader->RestartLoad(mRequest));
    }
  }

  return rv;
}

nsresult
ScriptLoadHandler::DecodeRawData(const uint8_t* aData,
                                 uint32_t aDataLength,
                                 bool aEndOfStream)
{
  int32_t srcLen = aDataLength;
  const char* src = reinterpret_cast<const char*>(aData);
  int32_t dstLen;
  nsresult rv =
    mDecoder->GetMaxLength(src, srcLen, &dstLen);

  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t haveRead = mRequest->mScriptText.length();

  CheckedInt<uint32_t> capacity = haveRead;
  capacity += dstLen;

  if (!capacity.isValid() || !mRequest->mScriptText.reserve(capacity.value())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = mDecoder->Convert(src,
                         &srcLen,
                         mRequest->mScriptText.begin() + haveRead,
                         &dstLen);

  NS_ENSURE_SUCCESS(rv, rv);

  haveRead += dstLen;
  MOZ_ASSERT(haveRead <= capacity.value(), "mDecoder produced more data than expected");
  MOZ_ALWAYS_TRUE(mRequest->mScriptText.resizeUninitialized(haveRead));

  return NS_OK;
}

bool
ScriptLoadHandler::EnsureDecoder(nsIIncrementalStreamLoader* aLoader,
                                 const uint8_t* aData,
                                 uint32_t aDataLength,
                                 bool aEndOfStream)
{
  // Check if decoder has already been created.
  if (mDecoder) {
    return true;
  }

  nsAutoCString charset;
  if (!EnsureDecoder(aLoader, aData, aDataLength, aEndOfStream, charset)) {
    return false;
  }
  if (charset.Length() == 0) {
    charset = "?";
  }
  mozilla::Telemetry::Accumulate(mozilla::Telemetry::DOM_SCRIPT_SRC_ENCODING,
    charset);
  return true;
}

bool
ScriptLoadHandler::EnsureDecoder(nsIIncrementalStreamLoader* aLoader,
                                 const uint8_t* aData,
                                 uint32_t aDataLength,
                                 bool aEndOfStream,
                                 nsCString& oCharset)
{
  // JavaScript modules are always UTF-8.
  if (mRequest->IsModuleRequest()) {
    oCharset = "UTF-8";
    mDecoder = EncodingUtils::DecoderForEncoding(oCharset);
    return true;
  }

  // Determine if BOM check should be done.  This occurs either
  // if end-of-stream has been reached, or at least 3 bytes have
  // been read from input.
  if (!aEndOfStream && (aDataLength < 3)) {
    return false;
  }

  // Do BOM detection.
  if (nsContentUtils::CheckForBOM(aData, aDataLength, oCharset)) {
    mDecoder = EncodingUtils::DecoderForEncoding(oCharset);
    return true;
  }

  // BOM detection failed, check content stream for charset.
  nsCOMPtr<nsIRequest> req;
  nsresult rv = aLoader->GetRequest(getter_AddRefs(req));
  NS_ASSERTION(req, "StreamLoader's request went away prematurely");
  NS_ENSURE_SUCCESS(rv, false);

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(req);

  if (channel &&
      NS_SUCCEEDED(channel->GetContentCharset(oCharset)) &&
      EncodingUtils::FindEncodingForLabel(oCharset, oCharset)) {
    mDecoder = EncodingUtils::DecoderForEncoding(oCharset);
    return true;
  }

  // Check the hint charset from the script element or preload
  // request.
  nsAutoString hintCharset;
  if (!mRequest->IsPreload()) {
    mRequest->mElement->GetScriptCharset(hintCharset);
  } else {
    nsTArray<ScriptLoader::PreloadInfo>::index_type i =
      mScriptLoader->mPreloads.IndexOf(mRequest, 0,
            ScriptLoader::PreloadRequestComparator());

    NS_ASSERTION(i != mScriptLoader->mPreloads.NoIndex,
                 "Incorrect preload bookkeeping");
    hintCharset = mScriptLoader->mPreloads[i].mCharset;
  }

  if (EncodingUtils::FindEncodingForLabel(hintCharset, oCharset)) {
    mDecoder = EncodingUtils::DecoderForEncoding(oCharset);
    return true;
  }

  // Get the charset from the charset of the document.
  if (mScriptLoader->mDocument) {
    oCharset = mScriptLoader->mDocument->GetDocumentCharacterSet();
    mDecoder = EncodingUtils::DecoderForEncoding(oCharset);
    return true;
  }

  // Curiously, there are various callers that don't pass aDocument. The
  // fallback in the old code was ISO-8859-1, which behaved like
  // windows-1252. Saying windows-1252 for clarity and for compliance
  // with the Encoding Standard.
  oCharset = "windows-1252";
  mDecoder = EncodingUtils::DecoderForEncoding(oCharset);

  return true;
}

nsresult
ScriptLoadHandler::MaybeDecodeSRI()
{
  if (!mSRIDataVerifier || mSRIDataVerifier->IsComplete() || NS_FAILED(mSRIStatus)) {
    return NS_OK;
  }

  // Skip until the content is large enough to be decoded.
  if (mRequest->mScriptBytecode.length() <= mSRIDataVerifier->DataSummaryLength()) {
    return NS_OK;
  }

  mSRIStatus = mSRIDataVerifier->ImportDataSummary(
    mRequest->mScriptBytecode.length(), mRequest->mScriptBytecode.begin());

  if (NS_FAILED(mSRIStatus)) {
    // We are unable to decode the hash contained in the alternate data which
    // contains the bytecode, or it does not use the same algorithm.
    LOG(("ScriptLoadHandler::MaybeDecodeSRI, failed to decode SRI, restart request"));
    return mSRIStatus;
  }

  mRequest->mBytecodeOffset = mSRIDataVerifier->DataSummaryLength();
  return NS_OK;
}

nsresult
ScriptLoadHandler::EnsureKnownDataType(nsIIncrementalStreamLoader* aLoader)
{
  MOZ_ASSERT(mRequest->IsUnknownDataType());
  MOZ_ASSERT(mRequest->IsLoading());
  if (mRequest->IsLoadingSource()) {
    mRequest->mDataType = ScriptLoadRequest::DataType::Source;
    TRACE_FOR_TEST(mRequest->mElement, "scriptloader_load_source");
    return NS_OK;
  }

  nsCOMPtr<nsIRequest> req;
  nsresult rv = aLoader->GetRequest(getter_AddRefs(req));
  MOZ_ASSERT(req, "StreamLoader's request went away prematurely");
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsICacheInfoChannel> cic(do_QueryInterface(req));
  if (cic) {
    nsAutoCString altDataType;
    cic->GetAlternativeDataType(altDataType);
    if (altDataType.Equals(nsContentUtils::JSBytecodeMimeType())) {
      mRequest->mDataType = ScriptLoadRequest::DataType::Bytecode;
      TRACE_FOR_TEST(mRequest->mElement, "scriptloader_load_bytecode");
    } else {
      MOZ_ASSERT(altDataType.IsEmpty());
      mRequest->mDataType = ScriptLoadRequest::DataType::Source;
      TRACE_FOR_TEST(mRequest->mElement, "scriptloader_load_source");
    }
  } else {
    mRequest->mDataType = ScriptLoadRequest::DataType::Source;
    TRACE_FOR_TEST(mRequest->mElement, "scriptloader_load_source");
  }
  MOZ_ASSERT(!mRequest->IsUnknownDataType());
  MOZ_ASSERT(mRequest->IsLoading());
  return NS_OK;
}

NS_IMETHODIMP
ScriptLoadHandler::OnStreamComplete(nsIIncrementalStreamLoader* aLoader,
                                    nsISupports* aContext,
                                    nsresult aStatus,
                                    uint32_t aDataLength,
                                    const uint8_t* aData)
{
  nsresult rv = NS_OK;
  if (LOG_ENABLED()) {
    nsAutoCString url;
    mRequest->mURI->GetAsciiSpec(url);
    LOG(("ScriptLoadRequest (%p): Stream complete (url = %s)",
         mRequest.get(), url.get()));
  }

  nsCOMPtr<nsIRequest> channelRequest;
  aLoader->GetRequest(getter_AddRefs(channelRequest));

  if (!mRequest->IsCanceled()) {
    if (mRequest->IsUnknownDataType()) {
      rv = EnsureKnownDataType(aLoader);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (mRequest->IsSource()) {
      DebugOnly<bool> encoderSet =
        EnsureDecoder(aLoader, aData, aDataLength, /* aEndOfStream = */ true);
      MOZ_ASSERT(encoderSet);
      rv = DecodeRawData(aData, aDataLength, /* aEndOfStream = */ true);
      NS_ENSURE_SUCCESS(rv, rv);

      LOG(("ScriptLoadRequest (%p): Source length = %u",
           mRequest.get(), unsigned(mRequest->mScriptText.length())));

      // If SRI is required for this load, appending new bytes to the hash.
      if (mSRIDataVerifier && NS_SUCCEEDED(mSRIStatus)) {
        mSRIStatus = mSRIDataVerifier->Update(aDataLength, aData);
      }
    } else {
      MOZ_ASSERT(mRequest->IsBytecode());
      if (!mRequest->mScriptBytecode.append(aData, aDataLength)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      LOG(("ScriptLoadRequest (%p): Bytecode length = %u",
           mRequest.get(), unsigned(mRequest->mScriptBytecode.length())));

      // If we abort while decoding the SRI, we fallback on explictly requesting
      // the source. Thus, we should not continue in
      // ScriptLoader::OnStreamComplete, which removes the request from the
      // waiting lists.
      rv = MaybeDecodeSRI();
      if (NS_FAILED(rv)) {
        return channelRequest->Cancel(mScriptLoader->RestartLoad(mRequest));
      }

      // The bytecode cache always starts with the SRI hash, thus even if there
      // is no SRI data verifier instance, we still want to skip the hash.
      rv = SRICheckDataVerifier::DataSummaryLength(mRequest->mScriptBytecode.length(),
                                                   mRequest->mScriptBytecode.begin(),
                                                   &mRequest->mBytecodeOffset);
      if (NS_FAILED(rv)) {
        return channelRequest->Cancel(mScriptLoader->RestartLoad(mRequest));
      }
    }
  }

  // Everything went well, keep the CacheInfoChannel alive such that we can
  // later save the bytecode on the cache entry.
  if (NS_SUCCEEDED(rv) && mRequest->IsSource() &&
      nsContentUtils::IsBytecodeCacheEnabled()) {
    mRequest->mCacheInfo = do_QueryInterface(channelRequest);
    LOG(("ScriptLoadRequest (%p): nsICacheInfoChannel = %p",
         mRequest.get(), mRequest->mCacheInfo.get()));
  }

  // we have to mediate and use mRequest.
  rv = mScriptLoader->OnStreamComplete(aLoader, mRequest, aStatus, mSRIStatus,
                                       mSRIDataVerifier);

  // In case of failure, clear the mCacheInfoChannel to avoid keeping it alive.
  if (NS_FAILED(rv)) {
    mRequest->mCacheInfo = nullptr;
  }

  return rv;
}

#undef LOG_ENABLED
#undef LOG

} // dom namespace
} // mozilla namespace
