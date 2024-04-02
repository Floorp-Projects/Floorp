/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FragmentDirective.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/FragmentDirectiveBinding.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FragmentDirective, mDocument)
NS_IMPL_CYCLE_COLLECTING_ADDREF(FragmentDirective)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FragmentDirective)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FragmentDirective)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

FragmentDirective::FragmentDirective(Document* aDocument)
    : mDocument(aDocument) {}

JSObject* FragmentDirective::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return FragmentDirective_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
