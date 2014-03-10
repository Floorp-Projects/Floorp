/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ResourceStatsConotrl_h
#define mozilla_dom_ResourceStatsConotrl_h

// Forward declarations.
struct JSContext;
class JSObject;

namespace mozilla {
namespace dom {

class ResourceStatsControl
{
public:
  static bool HasResourceStatsSupport(JSContext* /* unused */,
                                      JSObject* aGlobal);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ResourceStatsConotrl_h
