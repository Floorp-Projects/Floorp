/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "AudioParamMap.h"
#include "mozilla/dom/AudioParamMapBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(AudioParamMap, mParent)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(AudioParamMap, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(AudioParamMap, Release)

AudioParamMap::AudioParamMap(nsPIDOMWindowInner* aParent) :
  mParent(aParent)
{
}

JSObject*
AudioParamMap::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return AudioParamMapBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
