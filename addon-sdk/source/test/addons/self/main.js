/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const self = require("sdk/self");

exports["test self.data.load"] = assert => {

  assert.equal(self.data.load("data.md").trim(),
               "# hello world",
               "paths work");

  assert.equal(self.data.load("./data.md").trim(),
               "# hello world",
               "relative paths work");
};

exports["test self.id"] = assert => {
  assert.equal(self.id, "test-self@jetpack", "self.id should be correct.");
};

require("sdk/test/runner").runTestsFromModule(module);
