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

StreamLoader::StreamLoader(mozilla::css::SheetLoadData* aSheetLoadData)
  : mSheetLoadData(aSheetLoadData)
  , mStatus(NS_OK)
{
  MOZ_ASSERT(!aSheetLoadData->mSheet->IsGecko());
}

StreamLoader::~StreamLoader()
{
}

NS_IMPL_ISUPPORTS(StreamLoader, nsIStreamListener)

/* nsIRequestObserver implementation */
NS_IMETHODIMP
StreamLoader::OnStartRequest(nsIRequest* aRequest, nsISupports*)
{
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
StreamLoader::OnStopRequest(nsIRequest* aRequest,
                            nsISupports* aContext,
                            nsresult aStatus)
{
  // Decoded data
  nsCString utf8String;
  // How many bytes of decoded data to skip (3 when skipping UTF-8 BOM needed,
  // 0 otherwise)
  size_t skip = 0;

  const Encoding* encoding;

  nsresult rv = NS_OK;

  {
    // Hold the nsStringBuffer for the bytes from the stack to ensure release
    // no matter which return branch is taken.
    nsCString bytes(mBytes);
    mBytes.Truncate();

    nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);

    if (NS_FAILED(mStatus)) {
      mSheetLoadData->VerifySheetReadyToParse(mStatus, EmptyCString(), channel);
      return mStatus;
    }

    nsresult rv =
      mSheetLoadData->VerifySheetReadyToParse(aStatus, bytes, channel);
    if (rv != NS_OK_PARSE_SHEET) {
      return rv;
    }

    rv = NS_OK;

    size_t bomLength;
    Tie(encoding, bomLength) = Encoding::ForBOM(bytes);
    if (!encoding) {
      // No BOM
      encoding = mSheetLoadData->DetermineNonBOMEncoding(bytes, channel);

      rv = encoding->DecodeWithoutBOMHandling(bytes, utf8String);
    } else if (encoding == UTF_8_ENCODING) {
      // UTF-8 BOM; handling this manually because mozilla::Encoding
      // can't handle this without copying with C++ types and uses
      // infallible allocation with Rust types (which could avoid
      // the copy).

      // First, chop off the BOM.
      auto tail = Span<const uint8_t>(bytes).From(bomLength);
      size_t upTo = Encoding::UTF8ValidUpTo(tail);
      if (upTo == tail.Length()) {
        // No need to copy
        skip = bomLength;
        utf8String.Assign(bytes);
      } else {
        rv = encoding->DecodeWithoutBOMHandling(tail, utf8String, upTo);
      }
    } else {
      // UTF-16LE or UTF-16BE
      rv = encoding->DecodeWithBOMRemoval(bytes, utf8String);
    }
  } // run destructor for `bytes`

  if (NS_FAILED(rv)) {
    return rv;
  }

  // For reasons I don't understand, factoring the below lines into
  // a method on SheetLoadData resulted in a linker error. Hence,
  // accessing fields of mSheetLoadData from here.
  mSheetLoadData->mEncoding = encoding;
  bool dummy;
  return mSheetLoadData->mLoader->ParseSheet(
    EmptyString(),
    Span<const uint8_t>(utf8String).From(skip),
    mSheetLoadData,
    dummy);
}

/* nsIStreamListener implementation */
NS_IMETHODIMP
StreamLoader::OnDataAvailable(nsIRequest*,
                              nsISupports*,
                              nsIInputStream* aInputStream,
                              uint64_t,
                              uint32_t aCount)
{
  if (NS_FAILED(mStatus)) {
    return mStatus;
  }
  uint32_t dummy;
  return aInputStream->ReadSegments(WriteSegmentFun, this, aCount, &dummy);
}

nsresult
StreamLoader::WriteSegmentFun(nsIInputStream*,
                              void* aClosure,
                              const char* aSegment,
                              uint32_t,
                              uint32_t aCount,
                              uint32_t* aWriteCount)
{
  StreamLoader* self = static_cast<StreamLoader*>(aClosure);
  if (NS_FAILED(self->mStatus)) {
    return self->mStatus;
  }
  if (!self->mBytes.Append(aSegment, aCount, mozilla::fallible_t())) {
    self->mBytes.Truncate();
    return (self->mStatus = NS_ERROR_OUT_OF_MEMORY);
  }
  *aWriteCount = aCount;
  return NS_OK;
}

} // namespace css
} // namespace mozilla
