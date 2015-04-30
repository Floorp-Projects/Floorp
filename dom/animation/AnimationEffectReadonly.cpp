/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AnimationEffectReadonly.h"
#include "mozilla/dom/AnimationEffectReadonlyBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(AnimationEffectReadonly, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(AnimationEffectReadonly)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AnimationEffectReadonly)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AnimationEffectReadonly)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

} // namespace dom
} // namespace mozilla
