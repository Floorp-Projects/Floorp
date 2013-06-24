/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TCPSocketUtils_h
#define TCPSocketUtils_h

#include "js/RootingAPI.h"

namespace TCPSocketUtils {

extern bool
SocketEnabled(JSContext* aCx, JS::Handle<JSObject*> aGlobal);

}

#endif // TCPSocketUtils_h
