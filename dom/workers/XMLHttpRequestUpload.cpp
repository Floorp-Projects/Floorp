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
XMLHttpRequestUpload::_trace(JSTracer* aTrc)
{
  if (mXHR) {
    mXHR->TraceJSObject(aTrc, "mXHR");
  }
  XMLHttpRequestEventTarget::_trace(aTrc);
}

void
XMLHttpRequestUpload::_finalize(JSFreeOp* aFop)
{
  XMLHttpRequestEventTarget::_finalize(aFop);
}
