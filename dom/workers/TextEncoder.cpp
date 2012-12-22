/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextEncoder.h"
#include "DOMBindingInlines.h"

USING_WORKERS_NAMESPACE
using mozilla::ErrorResult;

void
TextEncoder::_trace(JSTracer* aTrc)
{
  DOMBindingBase::_trace(aTrc);
}

void
TextEncoder::_finalize(JSFreeOp* aFop)
{
  DOMBindingBase::_finalize(aFop);
}

// static
TextEncoder*
TextEncoder::Constructor(JSContext* aCx, JSObject* aObj,
                         const nsAString& aEncoding,
                         ErrorResult& aRv)
{
  nsRefPtr<TextEncoder> txtEncoder = new TextEncoder(aCx);
  txtEncoder->Init(aEncoding, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (!Wrap(aCx, aObj, txtEncoder)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return txtEncoder;
}
