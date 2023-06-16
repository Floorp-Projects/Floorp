/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const idlPureAllowlist = require("resource://devtools/server/actors/webconsole/webidl-pure-allowlist.js");

const natives = [];
if (Components.Constructor && Cu) {
  const sandbox = Cu.Sandbox(
    Components.Constructor("@mozilla.org/systemprincipal;1", "nsIPrincipal")(),
    {
      invisibleToDebugger: true,
      wantGlobalProperties: Object.keys(idlPureAllowlist),
    }
  );

  function maybePush(maybeFunc) {
    if (maybeFunc) {
      natives.push(maybeFunc);
    }
  }

  function collectMethods(obj, methods) {
    for (const name of methods) {
      maybePush(obj[name]);
    }
  }

  for (const [iface, ifaceData] of Object.entries(idlPureAllowlist)) {
    const ctor = sandbox[iface];
    if (!ctor) {
      continue;
    }

    if ("static" in ifaceData) {
      collectMethods(ctor, ifaceData.static);
    }

    if ("prototype" in ifaceData) {
      const proto = ctor.prototype;
      if (!proto) {
        continue;
      }

      collectMethods(proto, ifaceData.prototype);
    }
  }
}

module.exports = { natives };
