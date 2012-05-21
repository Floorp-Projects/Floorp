/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsJSON_h__
#define nsJSON_h__

#include "jsapi.h"
#include "json.h"
#include "nsIJSON.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIOutputStream.h"
#include "nsIUnicodeEncoder.h"
#include "nsIUnicodeDecoder.h"
#include "nsIRequestObserver.h"
#include "nsIStreamListener.h"
#include "nsTArray.h"

class nsIURI;

class NS_STACK_CLASS nsJSONWriter
{
public:
  nsJSONWriter();
  nsJSONWriter(nsIOutputStream *aStream);
  virtual ~nsJSONWriter();
  nsresult SetCharset(const char *aCharset);
  nsCOMPtr<nsIOutputStream> mStream;
  nsresult Write(const PRUnichar *aBuffer, PRUint32 aLength);
  nsString mOutputString;
  bool DidWrite();
  void FlushBuffer();

protected:
  PRUnichar *mBuffer;
  PRUint32 mBufferCount;
  bool mDidWrite;
  nsresult WriteToStream(nsIOutputStream *aStream, nsIUnicodeEncoder *encoder,
                         const PRUnichar *aBuffer, PRUint32 aLength);

  nsCOMPtr<nsIUnicodeEncoder> mEncoder;
};

class nsJSON : public nsIJSON
{
public:
  nsJSON();
  virtual ~nsJSON();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIJSON

protected:
  nsresult EncodeInternal(JSContext* cx,
                          const JS::Value& val,
                          nsJSONWriter* writer);

  nsresult DecodeInternal(JSContext* cx,
                          nsIInputStream* aStream,
                          PRInt32 aContentLength,
                          bool aNeedsConverter,
                          JS::Value* aRetVal,
                          DecodingMode mode = STRICT);
  nsCOMPtr<nsIURI> mURI;
};

nsresult
NS_NewJSON(nsISupports* aOuter, REFNSIID aIID, void** aResult);

class nsJSONListener : public nsIStreamListener
{
public:
  nsJSONListener(JSContext *cx, jsval *rootVal, bool needsConverter,
                 DecodingMode mode);
  virtual ~nsJSONListener();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

protected:
  bool mNeedsConverter;
  JSContext *mCx;
  jsval *mRootVal;
  nsCOMPtr<nsIUnicodeDecoder> mDecoder;
  nsCString mSniffBuffer;
  nsTArray<PRUnichar> mBufferedChars;
  DecodingMode mDecodingMode;
  nsresult ProcessBytes(const char* aBuffer, PRUint32 aByteLength);
  nsresult ConsumeConverted(const char* aBuffer, PRUint32 aByteLength);
  nsresult Consume(const PRUnichar *data, PRUint32 len);
};

#endif
