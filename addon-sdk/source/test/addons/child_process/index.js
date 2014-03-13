/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Ensures using child_process and underlying subprocess.jsm
 * works within an addon
 */

const { exec } = require("sdk/system/child_process");
const { platform, pathFor } = require("sdk/system");
const PROFILE_DIR = pathFor("ProfD");
const isWindows = platform.toLowerCase().indexOf("win") === 0;
const app = require("sdk/system/xul-app");

// Once Bug 903018 is resolved, just move the application testing to
// module.metadata.engines
if (app.is("Firefox")) {
  exports["test child_process in an addon"] = (assert, done) => {
    exec(isWindows ? "DIR /A-D" : "ls -al", {
      cwd: PROFILE_DIR
    }, (err, stdout, stderr) => {
      assert.ok(!err, "no errors");
      assert.equal(stderr, "", "stderr is empty");
      assert.ok(/extensions\.ini/.test(stdout), "stdout output of `ls -al` finds files");

      if (isWindows)
        assert.ok(!/<DIR>/.test(stdout), "passing args works");
      else
        assert.ok(/d(r[-|w][-|x]){3}/.test(stdout), "passing args works");
      done();
    });
  };
} else {
  exports["test unsupported"] = (assert) => assert.pass("This application is unsupported.");
}
require("sdk/test/runner").runTestsFromModule(module);
