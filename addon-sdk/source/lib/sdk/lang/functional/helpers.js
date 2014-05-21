/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Disclaimer: Some of the functions in this module implement APIs from
// Jeremy Ashkenas's http://underscorejs.org/ library and all credits for
// those goes to him.

"use strict";

module.metadata = {
  "stability": "unstable"
}

const arity = f => f.arity || f.length;
exports.arity = arity;

const name = f => f.displayName || f.name;
exports.name = name;

const derive = (f, source) => {
  f.displayName = name(source);
  f.arity = arity(source);
  return f;
};
exports.derive = derive;

const invoke = (callee, params, self) => callee.apply(self, params);
exports.invoke = invoke;
