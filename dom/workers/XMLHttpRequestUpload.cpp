/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XMLHttpRequestUpload.h"

#include "XMLHttpRequest.h"

#include "DOMBindingInlines.h"

USING_WORKERS_NAMESPACE

// static
XMLHttpRequestUpload*
XMLHttpRequestUpload::Create(JSContext* aCx, XMLHttpRequest* aXHR)
{
  nsRefPtr<XMLHttpRequestUpload> upload = new XMLHttpRequestUpload(aCx, aXHR);
  return Wrap(aCx, NULL, upload) ? upload : NULL;
}

void
XMLHttpRequestUpload::_Trace(JSTracer* aTrc)
{
  if (mXHR) {
    JS_CALL_OBJECT_TRACER(aTrc, mXHR->GetJSObject(), "mXHR");
  }
  XMLHttpRequestEventTarget::_Trace(aTrc);
}

void
XMLHttpRequestUpload::_Finalize(JSFreeOp* aFop)
{
  XMLHttpRequestEventTarget::_Finalize(aFop);
}
