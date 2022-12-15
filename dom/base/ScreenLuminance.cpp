/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScreenLuminance.h"
#include "nsScreen.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ScreenLuminance, mScreen)

JSObject* ScreenLuminance::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return ScreenLuminance_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
