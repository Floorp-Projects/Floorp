/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_textencoder_h_
#define mozilla_dom_workers_textencoder_h_

#include "mozilla/dom/TextEncoderBase.h"
#include "mozilla/dom/workers/bindings/DOMBindingBase.h"
#include "mozilla/dom/TextEncoderBinding.h"

BEGIN_WORKERS_NAMESPACE

class TextEncoder MOZ_FINAL : public DOMBindingBase,
                              public TextEncoderBase
{
protected:
  TextEncoder(JSContext* aCx)
  : DOMBindingBase(aCx)
  {}

  virtual
  ~TextEncoder()
  {}

  virtual JSObject*
  CreateUint8Array(JSContext* aCx, char* aBuf, uint32_t aLen) MOZ_OVERRIDE
  {
    return Uint8Array::Create(aCx, this, aLen,
                              reinterpret_cast<uint8_t*>(aBuf));
  }

public:
  virtual void
  _trace(JSTracer* aTrc) MOZ_OVERRIDE;

  virtual void
  _finalize(JSFreeOp* aFop) MOZ_OVERRIDE;

  static TextEncoder*
  Constructor(JSContext* aCx, JSObject* aObj,
              const nsAString& aEncoding,
              ErrorResult& aRv);

  JSObject*
  Encode(JSContext* aCx,
         const nsAString& aString,
         const TextEncodeOptionsWorkers& aOptions,
         ErrorResult& aRv) {
    return TextEncoderBase::Encode(aCx, aString, aOptions.mStream, aRv);
  }
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_textencoder_h_
