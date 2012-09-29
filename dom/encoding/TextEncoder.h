/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_textencoder_h_
#define mozilla_dom_textencoder_h_

#include "jsapi.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/TextEncoderBinding.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/ErrorResult.h"
#include "nsIUnicodeEncoder.h"
#include "nsString.h"

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla {
namespace dom {

class TextEncoder : public nsISupports, public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(TextEncoder)

  // The WebIDL constructor.
  static already_AddRefed<TextEncoder>
  Constructor(nsISupports* aGlobal,
              const Optional<nsAString>& aEncoding,
              ErrorResult& aRv)
  {
    nsRefPtr<TextEncoder> txtEncoder = new TextEncoder(aGlobal);
    txtEncoder->Init(aEncoding, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
    return txtEncoder.forget();
  }

  TextEncoder(nsISupports* aGlobal)
    : mGlobal(aGlobal)
  {
    MOZ_ASSERT(aGlobal);
    SetIsDOMBinding();
  }

  virtual
  ~TextEncoder()
  {}

  virtual JSObject*
  WrapObject(JSContext* aCx, JSObject* aScope, bool* aTriedToWrap)
  {
    return TextEncoderBinding::Wrap(aCx, aScope, this, aTriedToWrap);
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
  JSObject* Encode(JSContext* aCx,
                   const nsAString& aString,
                   const TextEncodeOptions& aOptions,
                   ErrorResult& aRv);
private:
  const char* mEncoding;
  nsCOMPtr<nsIUnicodeEncoder> mEncoder;
  nsCOMPtr<nsISupports> mGlobal;

  /**
   * Validates provided encoding and throws an exception if invalid encoding.
   * If no encoding is provided then mEncoding is default initialised to "utf-8".
   *
   * @param aEncoding    Optional encoding (case insensitive) provided.
   *                     (valid values are "utf-8", "utf-16", "utf-16be")
   *                     Default value is "utf-8" if no encoding is provided.
   * @return aRv         EncodingError exception else null.
   */
  void Init(const Optional<nsAString>& aEncoding,
            ErrorResult& aRv);
};

} // dom
} // mozilla

#endif // mozilla_dom_textencoder_h_
