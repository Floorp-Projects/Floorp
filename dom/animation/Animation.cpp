/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Animation.h"
#include "mozilla/dom/AnimationBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Animation, mDocument)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(Animation, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(Animation, Release)

JSObject*
Animation::WrapObject(JSContext* aCx)
{
  return AnimationBinding::Wrap(aCx, this);
}

void
Animation::SetParentTime(Nullable<TimeDuration> aParentTime)
{
  mParentTime = aParentTime;
}

} // namespace dom
} // namespace mozilla
