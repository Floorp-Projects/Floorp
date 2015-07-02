/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var fs = require("fs");
var path = require("path");
var Promise = require("promise");
var chai = require("chai");
var expect = chai.expect;
var ini = require("./update-ini");

var addonINI = path.resolve("./test/addons/jetpack-addon.ini");
var packageINI = path.resolve("./test/jetpack-package.ini");

describe("Checking ini files", function () {

  it("Check test/addons/jetpack-addon.ini", function (done) {

    fs.readFile(addonINI, function (err, data) {
      if (err) {
        throw err;
      }
      // filter comments
      var text = data.toString().split("\n").filter(function(line) {
        return !/^\s*#/.test(line);
      }).join("\n");
      var expected = "";

      ini.makeAddonIniContent()
      .then(function(contents) {
        expected = contents;

        setTimeout(function end() {
          expect(text.trim()).to.be.equal(expected.trim());
          done();
        });
      });
    });

  });

  it("Check test/jetpack-package.ini", function (done) {

    fs.readFile(packageINI, function (err, data) {
      if (err) {
        throw err;
      }
      // filter comments
      var text = data.toString().split("\n").filter(function(line) {
        return !/^\s*#/.test(line);
      }).join("\n");
      var expected = "";

      ini.makePackageIniContent()
      .then(function(contents) {
        expected = contents;

        setTimeout(function end() {
          expect(text.trim()).to.be.equal(expected.trim());
          done();
        });
      });
    });

  });

});
