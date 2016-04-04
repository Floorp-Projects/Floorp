/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_textencoder_h_
#define mozilla_dom_textencoder_h_

#include "mozilla/dom/NonRefcountedDOMObject.h"
#include "mozilla/dom/TextEncoderBinding.h"
#include "mozilla/dom/TypedArray.h"
#include "nsIUnicodeEncoder.h"

namespace mozilla {
class ErrorResult;

namespace dom {

class TextEncoder final : public NonRefcountedDOMObject
{
public:
  // The WebIDL constructor.

  static TextEncoder*
  Constructor(const GlobalObject& aGlobal,
              ErrorResult& aRv)
  {
    nsAutoPtr<TextEncoder> txtEncoder(new TextEncoder());
    txtEncoder->Init();
    return txtEncoder.forget();
  }

  TextEncoder()
  {
  }

  virtual
  ~TextEncoder()
  {}

  bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto, JS::MutableHandle<JSObject*> aReflector)
  {
    return TextEncoderBinding::Wrap(aCx, this, aGivenProto, aReflector);
  }

protected:

  void Init();

public:
  /**
   * Return the encoding name.
   *
   * @param aEncoding, current encoding.
   */
  void GetEncoding(nsAString& aEncoding);

  /**
   * Encodes incoming utf-16 code units/ DOM string to utf-8.
   *
   * @param aCx        Javascript context.
   * @param aObj       the wrapper of the TextEncoder
   * @param aString    utf-16 code units to be encoded.
   * @return JSObject* The Uint8Array wrapped in a JS object.  Returned via
   *                   the aRetval out param.
   */
  void Encode(JSContext* aCx,
              JS::Handle<JSObject*> aObj,
              const nsAString& aString,
              JS::MutableHandle<JSObject*> aRetval,
              ErrorResult& aRv);
private:
  nsCOMPtr<nsIUnicodeEncoder> mEncoder;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_textencoder_h_
