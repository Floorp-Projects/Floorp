/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_textdecoder_h_
#define mozilla_dom_textdecoder_h_

#include "mozilla/dom/NonRefcountedDOMObject.h"
#include "mozilla/dom/TextDecoderBinding.h"
#include "mozilla/dom/TypedArray.h"
#include "nsIUnicodeDecoder.h"

namespace mozilla {

class ErrorResult;

namespace dom {

class TextDecoder MOZ_FINAL
  : public NonRefcountedDOMObject
{
public:
  // The WebIDL constructor.
  static TextDecoder*
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aEncoding,
              const TextDecoderOptions& aOptions,
              ErrorResult& aRv)
  {
    nsAutoPtr<TextDecoder> txtDecoder(new TextDecoder());
    txtDecoder->Init(aEncoding, aOptions.mFatal, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
    return txtDecoder.forget();
  }

  TextDecoder()
    : mFatal(false)
  {
    MOZ_COUNT_CTOR(TextDecoder);
  }

  ~TextDecoder()
  {
    MOZ_COUNT_DTOR(TextDecoder);
  }

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope,
                       bool* aTookOwnership)
  {
    return TextDecoderBinding::Wrap(aCx, aScope, this, aTookOwnership);
  }

  nsISupports*
  GetParentObject()
  {
    return nullptr;
  }

  /**
   * Validates provided encoding and throws an exception if invalid encoding.
   * If no encoding is provided then mEncoding is default initialised to "utf-8".
   *
   * @param aEncoding    Optional encoding (case insensitive) provided.
   *                     Default value is "utf-8" if no encoding is provided.
   * @param aFatal       aFatal, indicates whether to throw an 'EncodingError'
   *                     exception or not.
   * @return aRv         EncodingError exception else null.
   */
  void Init(const nsAString& aEncoding, const bool aFatal, ErrorResult& aRv);

  /**
   * Return the encoding name.
   *
   * @param aEncoding, current encoding.
   */
  void GetEncoding(nsAString& aEncoding);

  /**
   * Decodes incoming byte stream of characters in charset indicated by
   * encoding.
   *
   * The encoding algorithm state is reset if aOptions.mStream is not set.
   *
   * If the fatal flag is set then a decoding error will throw EncodingError.
   * Else the decoder will return a decoded string with replacement
   * character(s) for unidentified character(s).
   *
   * @param      aView, incoming byte stream of characters to be decoded to
   *                    to UTF-16 code points.
   * @param      aOptions, indicates if streaming or not.
   * @param      aOutDecodedString, decoded string of UTF-16 code points.
   * @param      aRv, error result.
   */
  void Decode(const char* aInput, const int32_t aLength,
              const bool aStream, nsAString& aOutDecodedString,
              ErrorResult& aRv);

  void Decode(nsAString& aOutDecodedString,
              ErrorResult& aRv) {
    Decode(nullptr, 0, false, aOutDecodedString, aRv);
  }

  void Decode(const ArrayBufferView& aView,
              const TextDecodeOptions& aOptions,
              nsAString& aOutDecodedString,
              ErrorResult& aRv) {
    Decode(reinterpret_cast<char*>(aView.Data()), aView.Length(),
           aOptions.mStream, aOutDecodedString, aRv);
  }

private:
  nsCString mEncoding;
  nsCOMPtr<nsIUnicodeDecoder> mDecoder;
  bool mFatal;
};

} // dom
} // mozilla

#endif // mozilla_dom_textdecoder_h_
