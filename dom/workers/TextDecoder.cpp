/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextDecoder.h"
#include "DOMBindingInlines.h"

USING_WORKERS_NAMESPACE
using mozilla::ErrorResult;
using mozilla::dom::TextDecoderOptionsWorkers;
using mozilla::dom::WorkerGlobalObject;

void
TextDecoder::_trace(JSTracer* aTrc)
{
  DOMBindingBase::_trace(aTrc);
}

void
TextDecoder::_finalize(JSFreeOp* aFop)
{
  DOMBindingBase::_finalize(aFop);
}

// static
TextDecoder*
TextDecoder::Constructor(const WorkerGlobalObject& aGlobal,
                         const nsAString& aEncoding,
                         const TextDecoderOptionsWorkers& aOptions,
                         ErrorResult& aRv)
{
  nsRefPtr<TextDecoder> txtDecoder = new TextDecoder(aGlobal.GetContext());
  txtDecoder->Init(aEncoding, aOptions.mFatal, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (!Wrap(aGlobal.GetContext(), aGlobal.Get(), txtDecoder)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return txtDecoder;
}
