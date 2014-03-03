/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "unstable"
};

let { emit } = require("./core");

// Simple utility function takes event target, event type and optional
// `options.capture` and returns node style event stream that emits "data"
// events every time event of that type occurs on the given `target`.
function open(target, type, options) {
  let output = {};
  let capture = options && options.capture ? true : false;

  target.addEventListener(type, function(event) {
    emit(output, "data", event);
  }, capture);

  return output;
}
exports.open = open;
