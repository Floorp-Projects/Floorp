/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var utils = require("./utils");
var readParam = utils.readParam;
var path = require("path");
var fs = require("fs");
var jpm = utils.run;
var sdk = path.join(__dirname, "..", "..");
var binary = process.env.JPM_FIREFOX_BINARY || "nightly";

var filterPattern = readParam("filter");

describe("jpm test sdk modules", function () {
  it("SDK Modules", function (done) {
    process.chdir(sdk);

    var options = { cwd: sdk, env: { JPM_FIREFOX_BINARY: binary } };
    if (process.env.DISPLAY) {
      options.env.DISPLAY = process.env.DISPLAY;
    }
    options.filter = filterPattern;

    jpm("test", options, process).then(done);
  });
});
