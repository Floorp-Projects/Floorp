/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BodyExtractor_h
#define mozilla_dom_BodyExtractor_h

#include "jsapi.h"
#include "nsString.h"

class nsIInputStream;
class nsIGlobalObject;

namespace mozilla {
namespace dom {

class BodyExtractorBase
{
public:
  virtual nsresult GetAsStream(nsIInputStream** aResult,
                               uint64_t* aContentLength,
                               nsACString& aContentTypeWithCharset,
                               nsACString& aCharset) const = 0;
};

// The implementation versions of this template are:
// ArrayBuffer, ArrayBufferView, Blob, FormData,
// URLSearchParams, nsAString, nsIDocument, nsIInputStream.
template<typename Type>
class BodyExtractor final : public BodyExtractorBase
{
  Type* mBody;
public:
  explicit BodyExtractor(Type* aBody) : mBody(aBody)
  {}

  nsresult GetAsStream(nsIInputStream** aResult,
                       uint64_t* aContentLength,
                       nsACString& aContentTypeWithCharset,
                       nsACString& aCharset) const override;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_BodyExtractor_h
