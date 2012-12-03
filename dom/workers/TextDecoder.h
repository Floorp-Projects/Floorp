/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_textdecoder_h_
#define mozilla_dom_workers_textdecoder_h_

#include "mozilla/dom/TextDecoderBase.h"
#include "mozilla/dom/workers/bindings/DOMBindingBase.h"
#include "mozilla/dom/TextDecoderBinding.h"

BEGIN_WORKERS_NAMESPACE

class TextDecoder MOZ_FINAL : public DOMBindingBase,
                              public TextDecoderBase
{
protected:
  TextDecoder(JSContext* aCx)
  : DOMBindingBase(aCx)
  {}

  virtual
  ~TextDecoder()
  {}

public:
  virtual void
  _trace(JSTracer* aTrc) MOZ_OVERRIDE;

  virtual void
  _finalize(JSFreeOp* aFop) MOZ_OVERRIDE;

  static TextDecoder*
  Constructor(const WorkerGlobalObject& aGlobal,
              const nsAString& aEncoding,
              const TextDecoderOptionsWorkers& aOptions,
              ErrorResult& aRv);

  void
  Decode(nsAString& aOutDecodedString,
         ErrorResult& aRv) {
    TextDecoderBase::Decode(nullptr, 0, false,
                            aOutDecodedString, aRv);
  }

  void
  Decode(const ArrayBufferView& aView,
         const TextDecodeOptionsWorkers& aOptions,
         nsAString& aOutDecodedString,
         ErrorResult& aRv) {
    TextDecoderBase::Decode(reinterpret_cast<char*>(aView.Data()),
                            aView.Length(), aOptions.mStream,
                            aOutDecodedString, aRv);
  }
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_textdecoder_h_
