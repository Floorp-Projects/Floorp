/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XMLHttpRequestUpload.h"

#include "XMLHttpRequest.h"

#include "mozilla/dom/XMLHttpRequestUploadBinding.h"

USING_WORKERS_NAMESPACE

XMLHttpRequestUpload::XMLHttpRequestUpload(XMLHttpRequest* aXHR)
: mXHR(aXHR)
{
  SetIsDOMBinding();
}

XMLHttpRequestUpload::~XMLHttpRequestUpload()
{
}

NS_IMPL_ADDREF_INHERITED(XMLHttpRequestUpload, nsXHREventTarget)
NS_IMPL_RELEASE_INHERITED(XMLHttpRequestUpload, nsXHREventTarget)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(XMLHttpRequestUpload)
NS_INTERFACE_MAP_END_INHERITING(nsXHREventTarget)

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(XMLHttpRequestUpload, nsXHREventTarget,
                                     mXHR)

JSObject*
XMLHttpRequestUpload::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return XMLHttpRequestUploadBinding_workers::Wrap(aCx, aScope, this);
}

// static
already_AddRefed<XMLHttpRequestUpload>
XMLHttpRequestUpload::Create(XMLHttpRequest* aXHR)
{
  nsRefPtr<XMLHttpRequestUpload> upload = new XMLHttpRequestUpload(aXHR);
  return upload.forget();
}
