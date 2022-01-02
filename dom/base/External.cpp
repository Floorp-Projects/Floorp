/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/External.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(External, mParent)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(External, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(External, Release)

JSObject* External::WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) {
  return External_Binding::Wrap(aCx, this, aGivenProto);
};
