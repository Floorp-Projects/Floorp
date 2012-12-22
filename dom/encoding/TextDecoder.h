/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_textdecoder_h_
#define mozilla_dom_textdecoder_h_

#include "mozilla/dom/TextDecoderBase.h"
#include "mozilla/dom/TextDecoderBinding.h"

namespace mozilla {
namespace dom {

class TextDecoder MOZ_FINAL
  : public nsISupports, public nsWrapperCache, public TextDecoderBase
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(TextDecoder)

  // The WebIDL constructor.
  static already_AddRefed<TextDecoder>
  Constructor(nsISupports* aGlobal,
              const nsAString& aEncoding,
              const TextDecoderOptions& aOptions,
              ErrorResult& aRv)
  {
    nsRefPtr<TextDecoder> txtDecoder = new TextDecoder(aGlobal);
    txtDecoder->Init(aEncoding, aOptions.mFatal, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
    return txtDecoder.forget();
  }

  TextDecoder(nsISupports* aGlobal)
    : mGlobal(aGlobal)
  {
    MOZ_ASSERT(aGlobal);
    SetIsDOMBinding();
  }

  virtual
  ~TextDecoder()
  {}

  virtual JSObject*
  WrapObject(JSContext* aCx, JSObject* aScope, bool* aTriedToWrap) MOZ_OVERRIDE
  {
    return TextDecoderBinding::Wrap(aCx, aScope, this, aTriedToWrap);
  }

  nsISupports*
  GetParentObject()
  {
    return mGlobal;
  }

  void Decode(const ArrayBufferView* aView,
              const TextDecodeOptions& aOptions,
              nsAString& aOutDecodedString,
              ErrorResult& aRv) {
    return TextDecoderBase::Decode(aView, aOptions.mStream,
                                   aOutDecodedString, aRv);
  }

private:
  nsCOMPtr<nsISupports> mGlobal;
};

} // dom
} // mozilla

#endif // mozilla_dom_textdecoder_h_
