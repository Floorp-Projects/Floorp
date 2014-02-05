/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that we can link with libxul using js-ctypes

const {Cu} = require("chrome");
const {ctypes} = Cu.import("resource://gre/modules/ctypes.jsm", {});
const {OS} = Cu.import("resource://gre/modules/osfile.jsm", {});

exports.test = function(assert) {
  let path = OS.Constants.Path.libxul;
  assert.pass("libxul is at " + path);
  let lib = ctypes.open(path);
  assert.ok(lib != null, "linked to libxul successfully");
};

require('test').run(exports);
