/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const runtime = require("sdk/system/runtime");
const system = require("sdk/system");

exports["test system architecture and compiler"] = function(assert) {

  if (system.architecture !== null) {
    assert.equal(
      runtime.XPCOMABI.indexOf(system.architecture), 0,
      "system.architecture is starting substring of runtime.XPCOMABI"
    );
  }

  if (system.compiler !== null) {
    assert.equal(
      runtime.XPCOMABI.indexOf(system.compiler),
      runtime.XPCOMABI.length - system.compiler.length,
      "system.compiler is trailing substring of runtime.XPCOMABI"
    );
  }

  assert.ok(
    system.architecture === null || typeof(system.architecture) === "string",
    "system.architecture is string or null if not supported by platform"
  );

  assert.ok(
    system.compiler === null || typeof(system.compiler) === "string",
    "system.compiler is string or null if not supported by platform"
  );
};

require("sdk/test").run(exports);
