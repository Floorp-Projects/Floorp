/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Sayre <sayrer@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsJSON_h__
#define nsJSON_h__

#include "jsprvtd.h"
#include "nsIJSON.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIOutputStream.h"
#include "nsIUnicodeEncoder.h"
#include "nsIUnicodeDecoder.h"
#include "nsIRequestObserver.h"
#include "nsIStreamListener.h"
#include "nsTArray.h"

#define JSON_MAX_DEPTH  2048
#define JSON_PARSER_BUFSIZE 1024
class nsJSONWriter
{
public:
  nsJSONWriter();
  nsJSONWriter(nsIOutputStream *aStream);
  virtual ~nsJSONWriter();
  nsresult SetCharset(const char *aCharset);
  nsString mBuffer;
  nsCOMPtr<nsIOutputStream> mStream;
  nsresult WriteString(const PRUnichar* aBuffer, PRUint32);
  nsresult Write(const PRUnichar *aBuffer, PRUint32 aLength);

protected:
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
  JSBool   ToJSON(JSContext *cx, jsval *vp);
  nsresult EncodeObject(JSContext *cx, jsval *vp, nsJSONWriter *writer,
                        JSObject *whitelist, PRUint32 depth);
  nsresult EncodeInternal(nsJSONWriter *writer);
  nsresult DecodeInternal(nsIInputStream *aStream,
                          PRInt32 aContentLength,
                          PRBool aNeedsConverter);
};

NS_IMETHODIMP
NS_NewJSON(nsISupports* aOuter, REFNSIID aIID, void** aResult);

enum JSONParserState {
    JSON_PARSE_STATE_INIT,
    JSON_PARSE_STATE_VALUE,
    JSON_PARSE_STATE_OBJECT,
    JSON_PARSE_STATE_OBJECT_PAIR,
    JSON_PARSE_STATE_OBJECT_IN_PAIR,
    JSON_PARSE_STATE_ARRAY,
    JSON_PARSE_STATE_STRING,
    JSON_PARSE_STATE_STRING_ESCAPE,
    JSON_PARSE_STATE_STRING_HEX,
    JSON_PARSE_STATE_NUMBER,
    JSON_PARSE_STATE_KEYWORD,
    JSON_PARSE_STATE_FINISHED
};

class nsJSONObjectStack : public nsTArray<JSObject *>,
                          public JSTempValueRooter
{
};

class nsJSONListener : public nsIStreamListener
{
public:
  nsJSONListener(JSContext *cx, jsval *rootVal, PRBool needsConverter);
  virtual ~nsJSONListener();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

protected:
  PRUint32 mLineNum;
  PRUint32 mColumn;

  /* Used while handling \uNNNN in strings */
  PRUnichar mHexChar;
  PRUint8 mNumHex;

  JSContext *mCx;
  jsval *mRootVal;
  PRBool mNeedsConverter;
  nsCOMPtr<nsIUnicodeDecoder> mDecoder;
  JSONParserState *mStatep;
  JSONParserState mStateStack[JSON_MAX_DEPTH];
  nsString mStringBuffer;
  nsCString mSniffBuffer;

  nsresult PushState(JSONParserState state);
  nsresult PopState();
  nsresult ProcessBytes(const char* aBuffer, PRUint32 aByteLength);
  nsresult ConsumeConverted(const char* aBuffer, PRUint32 aByteLength);
  nsresult Consume(const PRUnichar *data, PRUint32 len);

  // These handle parsed tokens. Could be split to separate interface.
  nsJSONObjectStack mObjectStack;

  nsresult PushValue(JSObject *aParent, jsval aValue);
  nsresult PushObject(JSObject *aObj);
  nsresult OpenObject();
  nsresult CloseObject();
  nsresult OpenArray();
  nsresult CloseArray();
  nsresult HandleString();
  nsresult HandleNumber();
  nsresult HandleKeyword();
  nsString mObjectKey;
};

#endif
