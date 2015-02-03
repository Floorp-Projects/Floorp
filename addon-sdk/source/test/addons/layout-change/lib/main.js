/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { isNative } = require("@loader/options");

if (isNative) {
  module.exports = require("./test-toolkit-loader");
}
else {
  module.exports = require("./test-cuddlefish-loader");
}

require("sdk/test/runner").runTestsFromModule(module);
