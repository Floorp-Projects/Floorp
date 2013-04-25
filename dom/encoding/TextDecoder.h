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
  : public nsWrapperCache, public TextDecoderBase
{
public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(TextDecoder)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(TextDecoder)

  // The WebIDL constructor.
  static already_AddRefed<TextDecoder>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aEncoding,
              const TextDecoderOptions& aOptions,
              ErrorResult& aRv)
  {
    nsRefPtr<TextDecoder> txtDecoder = new TextDecoder(aGlobal.Get());
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

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE
  {
    return TextDecoderBinding::Wrap(aCx, aScope, this);
  }

  nsISupports*
  GetParentObject()
  {
    return mGlobal;
  }

  void Decode(nsAString& aOutDecodedString,
              ErrorResult& aRv) {
    TextDecoderBase::Decode(nullptr, 0, false,
                            aOutDecodedString, aRv);
  }

  void Decode(const ArrayBufferView& aView,
              const TextDecodeOptions& aOptions,
              nsAString& aOutDecodedString,
              ErrorResult& aRv) {
    TextDecoderBase::Decode(reinterpret_cast<char*>(aView.Data()),
                            aView.Length(), aOptions.mStream,
                            aOutDecodedString, aRv);
  }

private:
  nsCOMPtr<nsISupports> mGlobal;
};

} // dom
} // mozilla

#endif // mozilla_dom_textdecoder_h_
