/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const app = require("sdk/system/xul-app");
const packaging = require("@loader/options");

if (app.is("Firefox")) {
  module.exports = require("./windows/test-firefox-windows");
}
else if (app.is("Fennec")) {
  module.exports = require("./windows/test-fennec-windows");
}
else {
  module.exports = {
    "test Unsupported Application": (assert) => {
      assert.pass(
        "The windows module currently supports only Firefox and Fennec." +
        "In the future we would like it to support other applications, however.");
    }
  };
  require("sdk/test").run(exports);
}
