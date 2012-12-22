/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_textencoderbase_h_
#define mozilla_dom_textencoderbase_h_

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/TypedArray.h"
#include "nsIUnicodeEncoder.h"

namespace mozilla {
class ErrorResult;

namespace dom {

class TextEncoderBase
{
protected:
  TextEncoderBase()
  {}

  virtual
  ~TextEncoderBase()
  {}

  /**
   * Validates provided encoding and throws an exception if invalid encoding.
   * If no encoding is provided then mEncoding is default initialised to "utf-8".
   *
   * @param aEncoding    Optional encoding (case insensitive) provided.
   *                     (valid values are "utf-8", "utf-16", "utf-16be")
   *                     Default value is "utf-8" if no encoding is provided.
   * @return aRv         EncodingError exception else null.
   */
  void Init(const nsAString& aEncoding, ErrorResult& aRv);

public:
  /**
   * Return the encoding name.
   *
   * @param aEncoding, current encoding.
   */
  void GetEncoding(nsAString& aEncoding);

  /**
   * Encodes incoming utf-16 code units/ DOM string to the requested encoding.
   *
   * @param aCx        Javascript context.
   * @param aString    utf-16 code units to be encoded.
   * @param aOptions   Streaming option. Initialised by default to false.
   *                   If the streaming option is false, then the encoding
   *                   algorithm state will get reset. If set to true then
   *                   the previous encoding is reused/continued.
   * @return JSObject* The Uint8Array wrapped in a JS object.
   */
  JSObject* Encode(JSContext* aCx, const nsAString& aString,
                   const bool aStream, ErrorResult& aRv);

protected:
  virtual JSObject*
  CreateUint8Array(JSContext* aCx, char* aBuf, uint32_t aLen) = 0;

private:
  nsCString mEncoding;
  nsCOMPtr<nsIUnicodeEncoder> mEncoder;
};

} // dom
} // mozilla

#endif // mozilla_dom_textencoderbase_h_
