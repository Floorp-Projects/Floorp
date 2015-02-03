/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { name } = require("sdk/self");

exports["test self.name"] = (assert) => {
  assert.equal(name, "0-0", "using '0-0' works.");
}

require("sdk/test/runner").runTestsFromModule(module);
