/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_xmlhttprequestupload_h__
#define mozilla_dom_workers_xmlhttprequestupload_h__

#include "mozilla/dom/workers/bindings/XMLHttpRequestEventTarget.h"

BEGIN_WORKERS_NAMESPACE

class XMLHttpRequest;

class XMLHttpRequestUpload : public XMLHttpRequestEventTarget
{
  XMLHttpRequest* mXHR;

protected:
  XMLHttpRequestUpload(JSContext* aCx, XMLHttpRequest* aXHR)
  : XMLHttpRequestEventTarget(aCx), mXHR(aXHR)
  { }

  virtual ~XMLHttpRequestUpload()
  { }

public:
  static XMLHttpRequestUpload*
  Create(JSContext* aCx, XMLHttpRequest* aXHR);

  virtual void
  _Trace(JSTracer* aTrc) MOZ_OVERRIDE;

  virtual void
  _Finalize(JSFreeOp* aFop) MOZ_OVERRIDE;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_xmlhttprequestupload_h__
