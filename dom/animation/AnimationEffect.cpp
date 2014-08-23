/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AnimationEffect.h"
#include "mozilla/dom/AnimationEffectBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(AnimationEffect, mAnimation)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(AnimationEffect, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(AnimationEffect, Release)

JSObject*
AnimationEffect::WrapObject(JSContext* aCx)
{
  return AnimationEffectBinding::Wrap(aCx, this);
}

} // namespace dom
} // namespace mozilla
