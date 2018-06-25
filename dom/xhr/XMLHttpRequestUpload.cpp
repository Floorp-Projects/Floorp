/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XMLHttpRequestUpload.h"
#include "mozilla/dom/XMLHttpRequestUploadBinding.h"

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN(XMLHttpRequestUpload)
NS_INTERFACE_MAP_END_INHERITING(XMLHttpRequestEventTarget)

NS_IMPL_ADDREF_INHERITED(XMLHttpRequestUpload, XMLHttpRequestEventTarget)
NS_IMPL_RELEASE_INHERITED(XMLHttpRequestUpload, XMLHttpRequestEventTarget)

/* virtual */ JSObject*
XMLHttpRequestUpload::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return XMLHttpRequestUpload_Binding::Wrap(aCx, this, aGivenProto);
}

} // dom namespace
} // mozilla namespace
