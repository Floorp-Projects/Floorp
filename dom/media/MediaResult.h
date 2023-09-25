/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaResult_h_
#define MediaResult_h_

#include "nsString.h"  // Required before 'mozilla/ErrorNames.h'!?
#include "mozilla/ErrorNames.h"
#include "mozilla/TimeStamp.h"
#include "nsError.h"
#include "nsPrintfCString.h"

// MediaResult can be used interchangeably with nsresult.
// It allows to store extra information such as where the error occurred.
// While nsresult is typically passed by value; due to its potential size, using
// MediaResult const references is recommended.
namespace mozilla {

class CDMProxy;

class MediaResult {
 public:
  MediaResult() : mCode(NS_OK) {}
  MOZ_IMPLICIT MediaResult(nsresult aResult) : mCode(aResult) {}
  MediaResult(nsresult aResult, const nsACString& aMessage)
      : mCode(aResult), mMessage(aMessage) {}
  MediaResult(nsresult aResult, const char* aMessage)
      : mCode(aResult), mMessage(aMessage) {}
  MediaResult(nsresult aResult, CDMProxy* aCDMProxy)
      : mCode(aResult), mCDMProxy(aCDMProxy) {
    MOZ_ASSERT(aResult == NS_ERROR_DOM_MEDIA_CDM_PROXY_NOT_SUPPORTED_ERR);
  }
  MediaResult(const MediaResult& aOther) = default;
  MediaResult(MediaResult&& aOther) = default;
  MediaResult& operator=(const MediaResult& aOther) = default;
  MediaResult& operator=(MediaResult&& aOther) = default;

  nsresult Code() const { return mCode; }
  nsCString ErrorName() const {
    nsCString name;
    GetErrorName(mCode, name);
    return name;
  }

  const nsCString& Message() const { return mMessage; }

  // Interoperations with nsresult.
  bool operator==(nsresult aResult) const { return aResult == mCode; }
  bool operator!=(nsresult aResult) const { return aResult != mCode; }
  operator nsresult() const { return mCode; }

  nsCString Description() const {
    if (NS_SUCCEEDED(mCode)) {
      return nsCString();
    }
    return nsPrintfCString("%s (0x%08" PRIx32 ")%s%s", ErrorName().get(),
                           static_cast<uint32_t>(mCode),
                           mMessage.IsEmpty() ? "" : " - ", mMessage.get());
  }

  CDMProxy* GetCDMProxy() const { return mCDMProxy; }

 private:
  nsresult mCode;
  nsCString mMessage;
  // It's used when the error is NS_ERROR_DOM_MEDIA_CDM_PROXY_NOT_SUPPORTED_ERR.
  CDMProxy* mCDMProxy = nullptr;
};

#ifdef _MSC_VER
#  define RESULT_DETAIL(arg, ...) \
    nsPrintfCString("%s: " arg, __FUNCSIG__, ##__VA_ARGS__)
#else
#  define RESULT_DETAIL(arg, ...) \
    nsPrintfCString("%s: " arg, __PRETTY_FUNCTION__, ##__VA_ARGS__)
#endif

}  // namespace mozilla
#endif  // MediaResult_h_
