/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/css/StreamLoader.h"

#include "mozilla/IntegerTypeTraits.h"
#include "mozilla/Encoding.h"
#include "nsIChannel.h"
#include "nsIInputStream.h"

using namespace mozilla;

namespace mozilla {
namespace css {

StreamLoader::StreamLoader(SheetLoadData& aSheetLoadData)
    : mSheetLoadData(&aSheetLoadData), mStatus(NS_OK) {}

StreamLoader::~StreamLoader() {}

NS_IMPL_ISUPPORTS(StreamLoader, nsIStreamListener)

/* nsIRequestObserver implementation */
NS_IMETHODIMP
StreamLoader::OnStartRequest(nsIRequest* aRequest) {
  // It's kinda bad to let Web content send a number that results
  // in a potentially large allocation directly, but efficiency of
  // compression bombs is so great that it doesn't make much sense
  // to require a site to send one before going ahead and allocating.
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  if (channel) {
    int64_t length;
    nsresult rv = channel->GetContentLength(&length);
    if (NS_SUCCEEDED(rv) && length > 0) {
      if (length > MaxValue<nsACString::size_type>::value) {
        return (mStatus = NS_ERROR_OUT_OF_MEMORY);
      }
      if (!mBytes.SetCapacity(length, mozilla::fallible_t())) {
        return (mStatus = NS_ERROR_OUT_OF_MEMORY);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
StreamLoader::OnStopRequest(nsIRequest* aRequest, nsresult aStatus) {
  // Decoded data
  nsCString utf8String;
  {
    // Hold the nsStringBuffer for the bytes from the stack to ensure release
    // no matter which return branch is taken.
    nsCString bytes(mBytes);
    mBytes.Truncate();

    nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);

    if (NS_FAILED(mStatus)) {
      mSheetLoadData->VerifySheetReadyToParse(mStatus, EmptyCString(),
                                              EmptyCString(), channel);
      return mStatus;
    }

    nsresult rv = mSheetLoadData->VerifySheetReadyToParse(aStatus, mBOMBytes,
                                                          bytes, channel);
    if (rv != NS_OK_PARSE_SHEET) {
      return rv;
    }
    rv = NS_OK;

    // BOM detection generally happens during the write callback, but that won't
    // have happened if fewer than three bytes were received.
    if (mEncodingFromBOM.isNothing()) {
      HandleBOM();
      MOZ_ASSERT(mEncodingFromBOM.isSome());
    }

    // The BOM handling has happened, but we still may not have an encoding if
    // there was no BOM. Ensure we have one.
    const Encoding* encoding = mEncodingFromBOM.value();
    if (!encoding) {
      // No BOM
      encoding = mSheetLoadData->DetermineNonBOMEncoding(bytes, channel);
    }
    mSheetLoadData->mEncoding = encoding;

    size_t validated = 0;
    if (encoding == UTF_8_ENCODING) {
      validated = Encoding::UTF8ValidUpTo(bytes);
    }

    if (validated == bytes.Length()) {
      // Either this is UTF-8 and all valid, or it's not UTF-8 but is an
      // empty string. This assumes that an empty string in any encoding
      // decodes to empty string, which seems like a plausible assumption.
      utf8String.Assign(bytes);
    } else {
      rv = encoding->DecodeWithoutBOMHandling(bytes, utf8String, validated);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }  // run destructor for `bytes`

  // For reasons I don't understand, factoring the below lines into
  // a method on SheetLoadData resulted in a linker error. Hence,
  // accessing fields of mSheetLoadData from here.
  mSheetLoadData->mLoader->ParseSheet(utf8String, *mSheetLoadData,
                                      Loader::AllowAsyncParse::Yes);
  return NS_OK;
}

/* nsIStreamListener implementation */
NS_IMETHODIMP
StreamLoader::OnDataAvailable(nsIRequest*, nsIInputStream* aInputStream,
                              uint64_t, uint32_t aCount) {
  if (NS_FAILED(mStatus)) {
    return mStatus;
  }
  uint32_t dummy;
  return aInputStream->ReadSegments(WriteSegmentFun, this, aCount, &dummy);
}

void StreamLoader::HandleBOM() {
  MOZ_ASSERT(mEncodingFromBOM.isNothing());
  MOZ_ASSERT(mBytes.IsEmpty());

  const Encoding* encoding;
  size_t bomLength;
  Tie(encoding, bomLength) = Encoding::ForBOM(mBOMBytes);
  mEncodingFromBOM.emplace(encoding);  // Null means no BOM.

  // BOMs are three bytes at most, but may be fewer. Copy over anything
  // that wasn't part of the BOM to mBytes. Note that we need to track
  // any BOM bytes as well for SRI handling.
  mBytes.Append(Substring(mBOMBytes, bomLength));
  mBOMBytes.Truncate(bomLength);
}

nsresult StreamLoader::WriteSegmentFun(nsIInputStream*, void* aClosure,
                                       const char* aSegment, uint32_t,
                                       uint32_t aCount, uint32_t* aWriteCount) {
  *aWriteCount = 0;
  StreamLoader* self = static_cast<StreamLoader*>(aClosure);
  if (NS_FAILED(self->mStatus)) {
    return self->mStatus;
  }

  // If we haven't done BOM detection yet, divert bytes into the special buffer.
  if (self->mEncodingFromBOM.isNothing()) {
    size_t bytesToCopy = std::min(3 - self->mBOMBytes.Length(), aCount);
    self->mBOMBytes.Append(aSegment, bytesToCopy);
    aSegment += bytesToCopy;
    *aWriteCount += bytesToCopy;
    aCount -= bytesToCopy;

    if (self->mBOMBytes.Length() == 3) {
      self->HandleBOM();
    } else {
      return NS_OK;
    }
  }

  if (!self->mBytes.Append(aSegment, aCount, mozilla::fallible_t())) {
    self->mBytes.Truncate();
    return (self->mStatus = NS_ERROR_OUT_OF_MEMORY);
  }

  *aWriteCount += aCount;
  return NS_OK;
}

}  // namespace css
}  // namespace mozilla
