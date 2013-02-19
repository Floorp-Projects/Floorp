/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { packed } = require("sdk/self");
const url = require("sdk/url");

exports["test self.packed"] = function (assert) {
  assert.ok(packed, "require('sdk/self').packed is correct");
}

exports["test url.toFilename"] = function (assert) {
  assert.throws(
      function() { url.toFilename(module.uri); },
      /cannot map to filename: /,
      "url.toFilename() can fail for packed XPIs");
}

require("sdk/test/runner").runTestsFromModule(module);
