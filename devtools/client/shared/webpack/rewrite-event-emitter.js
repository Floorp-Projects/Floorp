/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Remove the header code from event-emitter.js.  This code confuses
// webpack.

"use strict";

module.exports = function(content) {
  this.cacheable && this.cacheable();

  const lines = content.split("\n");
  let ignoring = false;
  const newLines = [];
  for (const line of lines) {
    if (/function \(factory\)/.test(line)) {
      ignoring = true;
    } else if (/call\(this, function /.test(line)) {
      ignoring = false;
    } else if (!ignoring && line !== "});") {
      newLines.push(line);
    }
  }
  return newLines.join("\n");
};
