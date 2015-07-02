/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var _ = require("lodash");
var path = require("path");
var child_process = require("child_process");
var jpm = require.resolve("../../node_modules/jpm/bin/jpm");
var Promise = require("promise");
var chai = require("chai");
var expect = chai.expect;
var assert = chai.assert;
var DEFAULT_PROCESS = process;

var sdk = path.join(__dirname, "..", "..");
var prefsPath = path.join(sdk, "test", "preferences", "test-preferences.js");
var e10sPrefsPath = path.join(sdk, "test", "preferences", "test-e10s-preferences.js");

var OUTPUT_FILTERS = [
  /[^\n\r]+WARNING\: NS_ENSURE_SUCCESS\(rv, rv\) failed[^\n]+\n\r?/
];

var isDebug = (process.env["JPM_FX_DEBUG"] == "1");
exports.isDebug = isDebug;

function spawn (cmd, options) {
  options = options || {};
  var env = _.extend({}, options.env, process.env);

  if (isDebug) {
    env["MOZ_QUIET"] = 1;
  }

  var e10s = options.e10s || false;

  return child_process.spawn("node", [
    jpm, cmd, "-v", "--tbpl",
    "--prefs", e10s ? e10sPrefsPath : prefsPath,
    "-o", sdk,
    "-f", options.filter || ""
  ], {
    cwd: options.cwd || tmpOutputDir,
    env: env
  });
}
exports.spawn = spawn;

function run (cmd, options, p) {
  return new Promise(function(resolve) {
    var output = [];

    var proc = spawn(cmd, options);
    proc.stderr.pipe(process.stderr);
    proc.stdout.on("data", function (data) {
      for (var i = OUTPUT_FILTERS.length - 1; i >= 0; i--) {
        if (OUTPUT_FILTERS[i].test(data)) {
          return null;
        }
      }
      output.push(data);
      return null;
    });

    if (p) {
      proc.stdout.pipe(p.stdout);
    }
    else if (!isDebug) {
      proc.stdout.pipe(DEFAULT_PROCESS.stdout);
    }
    else {
      proc.stdout.on("data", function (data) {
        data = (data || "") + "";
        if (/TEST-/.test(data)) {
          DEFAULT_PROCESS.stdout.write(data.replace(/[\s\n]+$/, "") + "\n");
        }
      });
    }

    proc.on("close", function(code) {
      var out = output.join("");
      var buildDisplayed = /Build \d+/.test(out);
      var noTests = /No tests were run/.test(out);
      var hasSuccess = /All tests passed!/.test(out);
      var hasFailure = /There were test failures\.\.\./.test(out);
      if (noTests || hasFailure || !hasSuccess || code != 0) {
        DEFAULT_PROCESS.stdout.write(out);
      }
      expect(code).to.equal(hasFailure ? 1 : 0);
      expect(buildDisplayed).to.equal(true);
      expect(hasFailure).to.equal(false);
      expect(hasSuccess).to.equal(true);
      expect(noTests).to.equal(false);
      resolve();
    });
  });
}
exports.run = run;

function readParam(name) {
  var index = process.argv.indexOf("--" + name)
  return index >= 0 && process.argv[index + 1]
}
exports.readParam = readParam;
