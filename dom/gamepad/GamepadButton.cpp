/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/GamepadButton.h"
#include "mozilla/dom/GamepadBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(GamepadButton)
NS_IMPL_CYCLE_COLLECTING_RELEASE(GamepadButton)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(GamepadButton)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(GamepadButton, mParent)

/* virtual */ JSObject*
GamepadButton::WrapObject(JSContext* aCx)
{
  return GamepadButtonBinding::Wrap(aCx, this);
}

} // namespace dom
} // namespace mozilla
