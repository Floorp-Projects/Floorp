/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// demonstrates that requiring a test file from a test addon works

exports["test part 1"] = function(assert) {
  let test = require("./addons/addon-manager/lib/test-main.js");
  assert.equal(Object.keys(test).length, 1, "there is only one test");
  assert.ok("test getAddonByID" in test, "the test is corret");
};

exports["test getAddonByID"] = require("./addons/addon-manager/lib/test-main.js")["test getAddonByID"];

require("sdk/test").run(exports);
