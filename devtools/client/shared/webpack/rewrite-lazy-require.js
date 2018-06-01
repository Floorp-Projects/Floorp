/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = function(content) {
  this.cacheable && this.cacheable();

  // taking care of "named" depedencies.
  const res = content.replace(
    /loader.lazyRequireGetter\(this,\s*"([^"]+)",[^"]*"([^"]+)", true\);/g,
    "let { $1 } = require(\"$2\")"
  );
  // And then of direct ones.
  return res.replace(
    /loader.lazyRequireGetter\(this,\s*"([^"]+)",[^"]*"([^"]+)"(, false)?\);/g,
    "let $1 = require(\"$2\")"
  );
};
