/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_textdecoder_h_
#define mozilla_dom_textdecoder_h_

#include "jsapi.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/TextDecoderBinding.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/ErrorResult.h"
#include "nsIUnicodeDecoder.h"
#include "nsString.h"

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla {
namespace dom {

class TextDecoder : public nsISupports, public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(TextDecoder)

  // The WebIDL constructor.
  static already_AddRefed<TextDecoder>
  Constructor(nsISupports* aGlobal,
              const nsAString& aEncoding,
              const TextDecoderOptions& aFatal,
              ErrorResult& aRv)
  {
    nsRefPtr<TextDecoder> txtDecoder = new TextDecoder(aGlobal);
    txtDecoder->Init(aEncoding, aFatal, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
    return txtDecoder.forget();
  }

  TextDecoder(nsISupports* aGlobal)
    : mGlobal(aGlobal)
    , mFatal(false), mUseBOM(false), mOffset(0), mIsUTF16Family(false)
  {
    MOZ_ASSERT(aGlobal);
    SetIsDOMBinding();
  }

  virtual
  ~TextDecoder()
  {}

  virtual JSObject*
  WrapObject(JSContext* aCx, JSObject* aScope, bool* aTriedToWrap)
  {
    return TextDecoderBinding::Wrap(aCx, aScope, this, aTriedToWrap);
  }

  nsISupports*
  GetParentObject()
  {
    return mGlobal;
  }

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
   * The encoding algorithm state is reset if aOptions.stream is not set.
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
  void Decode(const ArrayBufferView* aView,
              const TextDecodeOptions& aOptions,
              nsAString& aOutDecodedString,
              ErrorResult& aRv);

private:
  const char* mEncoding;
  nsCOMPtr<nsIUnicodeDecoder> mDecoder;
  nsCOMPtr<nsISupports> mGlobal;
  bool mFatal;
  bool mUseBOM;
  uint8_t mOffset;
  char mInitialBytes[3];
  bool mIsUTF16Family;

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
  void Init(const nsAString& aEncoding,
            const TextDecoderOptions& aFatal,
            ErrorResult& aRv);

  // Internal helper functions.
  void CreateDecoder(ErrorResult& aRv);
  void ResetDecoder(bool aResetOffset = true);
  void HandleBOM(const char*& aData, uint32_t& aLength,
                 const TextDecodeOptions& aOptions,
                 nsAString& aOutString, ErrorResult& aRv);
  void FeedBytes(const char* aBytes, nsAString* aOutString = nullptr);
};

} // dom
} // mozilla

#endif // mozilla_dom_textdecoder_h_
