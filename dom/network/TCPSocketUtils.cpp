/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TCPSocketUtils.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/Preferences.h"
#include "nsGlobalWindow.h"

namespace TCPSocketUtils {

using mozilla::Preferences;
using mozilla::dom::CheckPermissions;

bool
SocketEnabled(JSContext* aCx, JS::Handle<JSObject*> aGlobal)
{
  if (!Preferences::GetBool("dom.mozTCPSocket.enabled")) {
    return false;
  }

  nsPIDOMWindow* window = xpc::WindowGlobalOrNull(aGlobal);
  if (!window) {
    return true;
  }

  const char* permissions[] = {"tcp-socket", nullptr};
  return CheckPermissions(aCx, aGlobal, permissions);
}

}
