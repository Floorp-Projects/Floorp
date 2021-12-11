/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { CC, Cu } = require("chrome");

const idlPureAllowlist = require("devtools/server/actors/webconsole/webidl-pure-allowlist");

// TODO: Bug 1616013 - Move more of these to be part of the pure list.
const customEagerFunctions = {
  Document: [
    ["prototype", "getSelection"],
    ["prototype", "hasStorageAccess"],
  ],
  Range: [
    ["prototype", "isPointInRange"],
    ["prototype", "comparePoint"],
    ["prototype", "intersectsNode"],

    // These two functions aren't pure because they do trigger layout when
    // they are called, but in the context of eager evaluation, that should be
    // a totally fine thing to do.
    ["prototype", "getClientRects"],
    ["prototype", "getBoundingClientRect"],
  ],
  Selection: [
    ["prototype", "getRangeAt"],
    ["prototype", "containsNode"],
  ],
};

const mergedFunctions = {};
for (const [key, values] of Object.entries(idlPureAllowlist)) {
  mergedFunctions[key] = [...values];
}
for (const [key, values] of Object.entries(customEagerFunctions)) {
  if (!mergedFunctions[key]) {
    mergedFunctions[key] = [];
  }
  mergedFunctions[key].push(...values);
}

const natives = [];
if (CC && Cu) {
  const sandbox = Cu.Sandbox(
    CC("@mozilla.org/systemprincipal;1", "nsIPrincipal")(),
    {
      invisibleToDebugger: true,
      wantGlobalProperties: Object.keys(mergedFunctions),
    }
  );

  for (const iface of Object.keys(mergedFunctions)) {
    for (const path of mergedFunctions[iface]) {
      let value = sandbox;
      for (const part of [iface, ...path]) {
        value = value[part];
        if (!value) {
          break;
        }
      }

      if (value) {
        natives.push(value);
      }
    }
  }
}

module.exports = natives;
