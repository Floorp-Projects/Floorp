/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_xmlhttprequestupload_h__
#define mozilla_dom_workers_xmlhttprequestupload_h__

#include "nsXMLHttpRequest.h"

BEGIN_WORKERS_NAMESPACE

class XMLHttpRequest;

class XMLHttpRequestUpload MOZ_FINAL : public nsXHREventTarget
{
  nsRefPtr<XMLHttpRequest> mXHR;

  XMLHttpRequestUpload(XMLHttpRequest* aXHR);

  ~XMLHttpRequestUpload();

public:
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  static already_AddRefed<XMLHttpRequestUpload>
  Create(XMLHttpRequest* aXHR);

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XMLHttpRequestUpload, nsXHREventTarget)

  NS_DECL_ISUPPORTS_INHERITED

  nsISupports*
  GetParentObject() const
  {
    // There's only one global on a worker, so we don't need to specify.
    return nullptr;
  }

  bool
  HasListeners()
  {
    return mListenerManager && mListenerManager->HasListeners();
  }
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_xmlhttprequestupload_h__
