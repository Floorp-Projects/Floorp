/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

let os = require("node/os");
let system = require("sdk/system");

exports["test os"] = function (assert) {
  assert.equal(os.tmpdir(), system.pathFor("TmpD"), "os.tmpdir() matches temp dir");
  assert.ok(os.endianness() === "BE" || os.endianness() === "LE", "os.endianness is BE or LE");

  assert.ok(os.arch().length > 0, "os.arch() returns a value");
  assert.equal(typeof os.arch(), "string", "os.arch() returns a string");
  assert.ok(os.type().length > 0, "os.type() returns a value");
  assert.equal(typeof os.type(), "string", "os.type() returns a string");
  assert.ok(os.platform().length > 0, "os.platform() returns a value");
  assert.equal(typeof os.platform(), "string", "os.platform() returns a string");

  assert.ok(os.release().length > 0, "os.release() returns a value");
  assert.equal(typeof os.release(), "string", "os.release() returns a string");
  assert.ok(os.hostname().length > 0, "os.hostname() returns a value");
  assert.equal(typeof os.hostname(), "string", "os.hostname() returns a string");
  assert.ok(os.EOL === "\n" || os.EOL === "\r\n", "os.EOL returns a correct EOL char");

  assert.deepEqual(os.loadavg(), [0, 0, 0], "os.loadavg() returns an array of 0s");

  ["uptime", "totalmem", "freemem", "cpus"].forEach(method => {
    assert.throws(() => os[method](), "os." + method + " throws");
  });
};

require("test").run(exports);
