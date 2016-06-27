/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XMLHttpRequestUploadWorker.h"

#include "XMLHttpRequestWorker.h"

#include "mozilla/dom/XMLHttpRequestUploadBinding.h"

USING_WORKERS_NAMESPACE

XMLHttpRequestUploadWorker::XMLHttpRequestUploadWorker(XMLHttpRequestWorker* aXHR)
: mXHR(aXHR)
{
}

XMLHttpRequestUploadWorker::~XMLHttpRequestUploadWorker()
{
}

NS_IMPL_ADDREF_INHERITED(XMLHttpRequestUploadWorker, XMLHttpRequestEventTarget)
NS_IMPL_RELEASE_INHERITED(XMLHttpRequestUploadWorker, XMLHttpRequestEventTarget)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(XMLHttpRequestUploadWorker)
NS_INTERFACE_MAP_END_INHERITING(XMLHttpRequestEventTarget)

NS_IMPL_CYCLE_COLLECTION_INHERITED(XMLHttpRequestUploadWorker, XMLHttpRequestEventTarget,
                                   mXHR)

JSObject*
XMLHttpRequestUploadWorker::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return XMLHttpRequestUploadBinding_workers::Wrap(aCx, this, aGivenProto);
}

// static
already_AddRefed<XMLHttpRequestUploadWorker>
XMLHttpRequestUploadWorker::Create(XMLHttpRequestWorker* aXHR)
{
  RefPtr<XMLHttpRequestUploadWorker> upload = new XMLHttpRequestUploadWorker(aXHR);
  return upload.forget();
}
