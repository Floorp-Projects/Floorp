/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var fs = require("fs");
var Promise = require("promise");
var chai = require("chai");
var expect = chai.expect;
var normalizeBinary = require("fx-runner/lib/utils").normalizeBinary;

//var firefox_binary = process.env["JPM_FIREFOX_BINARY"] || normalizeBinary("nightly");

describe("Checking Firefox binary", function () {

  it("using matching fx-runner version with jpm", function () {
    var sdkPackageJSON = require("../../package.json");
    var jpmPackageINI = require("jpm/package.json");
    expect(sdkPackageJSON.devDependencies["fx-runner"]).to.be.equal(jpmPackageINI.dependencies["fx-runner"]);
  });

  it("exists", function (done) {
    var useEnvVar = new Promise(function(resolve) {
      resolve(process.env["JPM_FIREFOX_BINARY"]);
    });

    var firefox_binary = process.env["JPM_FIREFOX_BINARY"] ? useEnvVar : normalizeBinary("nightly");
    firefox_binary.then(function(path) {
      expect(path).to.be.ok;
      fs.exists(path, function (exists) {
        expect(exists).to.be.ok;
        done();
      });
    })
  });

});
