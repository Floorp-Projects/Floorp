/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FluentBundle.h"

using namespace mozilla::dom;

namespace mozilla {
namespace intl {
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FluentPattern, mParent)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(FluentPattern, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(FluentPattern, Release)

FluentPattern::FluentPattern(nsISupports* aParent, const nsACString& aId)
    : mId(aId), mParent(aParent) {
  MOZ_COUNT_CTOR(FluentPattern);
}
FluentPattern::FluentPattern(nsISupports* aParent, const nsACString& aId,
                             const nsACString& aAttrName)
    : mId(aId), mAttrName(aAttrName), mParent(aParent) {
  MOZ_COUNT_CTOR(FluentPattern);
}

JSObject* FluentPattern::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return FluentPattern_Binding::Wrap(aCx, this, aGivenProto);
}

FluentPattern::~FluentPattern() { MOZ_COUNT_DTOR(FluentPattern); };

}  // namespace intl
}  // namespace mozilla
