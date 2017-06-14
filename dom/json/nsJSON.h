/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsJSON_h__
#define nsJSON_h__

#include "nsIJSON.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIOutputStream.h"
#include "mozilla/Encoding.h"
#include "nsIRequestObserver.h"
#include "nsIStreamListener.h"
#include "nsTArray.h"

class nsIURI;

class MOZ_STACK_CLASS nsJSONWriter
{
public:
  nsJSONWriter();
  explicit nsJSONWriter(nsIOutputStream* aStream);
  virtual ~nsJSONWriter();
  nsCOMPtr<nsIOutputStream> mStream;
  nsresult Write(const char16_t *aBuffer, uint32_t aLength);
  nsString mOutputString;
  bool DidWrite();
  void FlushBuffer();

protected:
  char16_t *mBuffer;
  uint32_t mBufferCount;
  bool mDidWrite;
  nsresult WriteToStream(nsIOutputStream* aStream,
                         mozilla::Encoder* encoder,
                         const char16_t* aBuffer,
                         uint32_t aLength);

  mozilla::UniquePtr<mozilla::Encoder> mEncoder;
};

class nsJSON : public nsIJSON
{
public:
  nsJSON();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIJSON

protected:
  virtual ~nsJSON();

  nsresult EncodeInternal(JSContext* cx,
                          const JS::Value& val,
                          nsJSONWriter* writer);

  nsresult DecodeInternal(JSContext* cx,
                          nsIInputStream* aStream,
                          int32_t aContentLength,
                          bool aNeedsConverter,
                          JS::MutableHandle<JS::Value> aRetVal);
  nsCOMPtr<nsIURI> mURI;
};

nsresult
NS_NewJSON(nsISupports* aOuter, REFNSIID aIID, void** aResult);

class nsJSONListener : public nsIStreamListener
{
public:
  nsJSONListener(JSContext *cx, JS::Value *rootVal, bool needsConverter);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

protected:
  virtual ~nsJSONListener();

  bool mNeedsConverter;
  JSContext *mCx;
  JS::Value *mRootVal;
  mozilla::UniquePtr<mozilla::Decoder> mDecoder;
  nsTArray<char16_t> mBufferedChars;
  nsresult ProcessBytes(const char* aBuffer, uint32_t aByteLength);
  nsresult ConsumeConverted(const char* aBuffer, uint32_t aByteLength);
  nsresult Consume(const char16_t *data, uint32_t len);
};

#endif
