/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/ErrorNames.h"
#include "nsCOMPtr.h"
#include "nsConverterInputStream.h"
#include "nsIInputStream.h"
#include "nsISupports.h"
#include "nsStringStream.h"

namespace {

class ShortReadWrapper final : public nsIInputStream {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM

  template <size_t N>
  ShortReadWrapper(const uint32_t (&aShortReads)[N],
                   nsIInputStream* aBaseStream)
      : mShortReadIter(std::begin(aShortReads)),
        mShortReadEnd(std::end(aShortReads)),
        mBaseStream(aBaseStream) {}

  ShortReadWrapper(const ShortReadWrapper&) = delete;
  ShortReadWrapper& operator=(const ShortReadWrapper&) = delete;

 private:
  ~ShortReadWrapper() = default;

  const uint32_t* mShortReadIter;
  const uint32_t* mShortReadEnd;
  nsCOMPtr<nsIInputStream> mBaseStream;
};

NS_IMPL_ISUPPORTS(ShortReadWrapper, nsIInputStream)

NS_IMETHODIMP
ShortReadWrapper::Close() { return mBaseStream->Close(); }

NS_IMETHODIMP
ShortReadWrapper::Available(uint64_t* aAvailable) {
  nsresult rv = mBaseStream->Available(aAvailable);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mShortReadIter != mShortReadEnd) {
    *aAvailable = std::min(uint64_t(*mShortReadIter), *aAvailable);
  }
  return NS_OK;
}

NS_IMETHODIMP
ShortReadWrapper::Read(char* aBuf, uint32_t aCount, uint32_t* _retval) {
  if (mShortReadIter != mShortReadEnd) {
    aCount = std::min(*mShortReadIter, aCount);
  }

  nsresult rv = mBaseStream->Read(aBuf, aCount, _retval);
  if (NS_SUCCEEDED(rv) && mShortReadIter != mShortReadEnd) {
    ++mShortReadIter;
  }
  return rv;
}

NS_IMETHODIMP
ShortReadWrapper::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                               uint32_t aCount, uint32_t* _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ShortReadWrapper::IsNonBlocking(bool* _retval) {
  return mBaseStream->IsNonBlocking(_retval);
}

}  // namespace

TEST(ConverterStreamShortRead, ShortRead)
{
  uint8_t bytes[] = {0xd8, 0x35, 0xdc, 0x20};
  nsCOMPtr<nsIInputStream> baseStream;
  ASSERT_TRUE(NS_SUCCEEDED(NS_NewByteInputStream(getter_AddRefs(baseStream),
                                                 AsChars(mozilla::Span(bytes)),
                                                 NS_ASSIGNMENT_COPY)));

  static const uint32_t kShortReads[] = {1, 2, 1};
  nsCOMPtr<nsIInputStream> shortStream =
      new ShortReadWrapper(kShortReads, baseStream);

  RefPtr<nsConverterInputStream> unicharStream = new nsConverterInputStream();
  ASSERT_TRUE(NS_SUCCEEDED(
      unicharStream->Init(shortStream, "UTF-16BE", 4096,
                          nsIConverterInputStream::ERRORS_ARE_FATAL)));

  uint32_t read;
  nsAutoString result;
  ASSERT_TRUE(
      NS_SUCCEEDED(unicharStream->ReadString(UINT32_MAX, result, &read)));

  ASSERT_EQ(read, 2u);
  ASSERT_TRUE(result == u"\U0001d420");
}
