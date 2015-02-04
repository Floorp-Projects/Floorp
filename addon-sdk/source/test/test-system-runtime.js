/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const runtime = require("sdk/system/runtime");

exports["test system runtime"] = function(assert) {
  assert.equal(typeof(runtime.inSafeMode), "boolean",
               "inSafeMode is boolean");
  assert.equal(typeof(runtime.OS), "string",
               "runtime.OS is string");
  assert.equal(typeof(runtime.processType), "number",
               "runtime.processType is a number");
  assert.equal(typeof(runtime.widgetToolkit), "string",
               "runtime.widgetToolkit is string");
  const XPCOMABI = runtime.XPCOMABI;
  assert.ok(XPCOMABI === null || typeof(XPCOMABI) === "string",
            "runtime.XPCOMABI is string or null if not supported by platform");
};


require("sdk/test").run(exports);
