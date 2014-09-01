/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_apps_InterAppComm_h
#define mozilla_dom_apps_InterAppComm_h

#include "mozilla/dom/MozInterAppMessageEvent.h"

// Forward declarations.
struct JSContext;
class JSObject;

namespace mozilla {
namespace dom {

class InterAppComm
{
public:
  static bool EnabledForScope(JSContext* /* unused */,
			      JS::Handle<JSObject*> /* unused */);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_apps_InterAppComm_h
