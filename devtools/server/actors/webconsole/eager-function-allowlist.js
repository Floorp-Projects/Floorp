/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const idlPureAllowlist = require("resource://devtools/server/actors/webconsole/webidl-pure-allowlist.js");

// Exclude interfaces only with "instance" property, such as Location,
// which is not available in sandbox.
const props = [];
for (const [iface, ifaceData] of Object.entries(idlPureAllowlist)) {
  if ("static" in ifaceData || "prototype" in ifaceData) {
    props.push(iface);
  }
}

const natives = [];

if (Components.Constructor && Cu) {
  const sandbox = Cu.Sandbox(
    Components.Constructor("@mozilla.org/systemprincipal;1", "nsIPrincipal")(),
    {
      invisibleToDebugger: true,
      wantGlobalProperties: props,
    }
  );

  function maybePush(maybeFunc) {
    if (maybeFunc) {
      natives.push(maybeFunc);
    }
  }

  function collectMethodsAndGetters(obj, methodsAndGetters) {
    if ("methods" in methodsAndGetters) {
      for (const name of methodsAndGetters.methods) {
        maybePush(obj[name]);
      }
    }
    if ("getters" in methodsAndGetters) {
      for (const name of methodsAndGetters.getters) {
        maybePush(Object.getOwnPropertyDescriptor(obj, name)?.get);
      }
    }
  }

  for (const [iface, ifaceData] of Object.entries(idlPureAllowlist)) {
    const ctor = sandbox[iface];
    if (!ctor) {
      continue;
    }

    if ("static" in ifaceData) {
      collectMethodsAndGetters(ctor, ifaceData.static);
    }

    if ("prototype" in ifaceData) {
      const proto = ctor.prototype;
      if (!proto) {
        continue;
      }

      collectMethodsAndGetters(proto, ifaceData.prototype);
    }
  }
}

module.exports = { natives, idlPureAllowlist };
