/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 "use strict";

var xulApp = require("sdk/system/xul-app");

exports["test xulapp"] = function (assert) {
  assert.equal(typeof(xulApp.ID), "string",
                   "ID is a string");
  assert.equal(typeof(xulApp.name), "string",
                   "name is a string");
  assert.equal(typeof(xulApp.version), "string",
                   "version is a string");
  assert.equal(typeof(xulApp.platformVersion), "string",
                   "platformVersion is a string");

  assert.throws(() => xulApp.is("blargy"),
      /Unkown Mozilla Application: blargy/,
      "is() throws error on bad app name");

  assert.throws(() => xulApp.isOneOf(["blargy"]),
      /Unkown Mozilla Application: blargy/,
      "isOneOf() throws error on bad app name");

  function testSupport(name) {
    var item = xulApp.is(name);
    assert.ok(item === true || item === false,
                  "is('" + name + "') is true or false.");
  }

  var apps = ["Firefox", "Mozilla", "SeaMonkey", "Fennec", "Thunderbird"];

  apps.forEach(testSupport);

  assert.ok(xulApp.isOneOf(apps) == true || xulApp.isOneOf(apps) == false,
                "isOneOf() returns true or false.");

  assert.equal(xulApp.versionInRange(xulApp.platformVersion, "1.9", "*"),
                   true, "platformVersion in range [1.9, *)");
  assert.equal(xulApp.versionInRange("3.6.4", "3.6.4", "3.6.*"),
                   true, "3.6.4 in [3.6.4, 3.6.*)");
  assert.equal(xulApp.versionInRange("1.9.3", "1.9.2", "1.9.3"),
                   false, "1.9.3 not in [1.9.2, 1.9.3)");
};

exports["test satisfies version range"] = function (assert) {
  [ ["1.0.0 - 2.0.0", "1.2.3"],
    ["1.0.0", "1.0.0"],
    [">=*", "0.2.4"],
    ["", "1.0.0"],
    ["*", "1.2.3"],
    [">=1.0.0", "1.0.0"],
    [">=1.0.0", "1.0.1"],
    [">=1.0.0", "1.1.0"],
    [">1.0.0", "1.0.1"],
    [">1.0.0", "1.1.0"],
    ["<=2.0.0", "2.0.0"],
    ["<=2.0.0", "1.9999.9999"],
    ["<=2.0.0", "0.2.9"],
    ["<2.0.0", "1.9999.9999"],
    ["<2.0.0", "0.2.9"],
    [">= 1.0.0", "1.0.0"],
    [">=  1.0.0", "1.0.1"],
    [">=   1.0.0", "1.1.0"],
    ["> 1.0.0", "1.0.1"],
    [">  1.0.0", "1.1.0"],
    ["<1", "1.0.0beta"],
    ["< 1", "1.0.0beta"],
    ["<=   2.0.0", "2.0.0"],
    ["<= 2.0.0", "1.9999.9999"],
    ["<=  2.0.0", "0.2.9"],
    ["<    2.0.0", "1.9999.9999"],
    ["<\t2.0.0", "0.2.9"],
    [">=0.1.97", "0.1.97"],
    ["0.1.20 || 1.2.4", "1.2.4"],
    [">=0.2.3 || <0.0.1", "0.0.0"],
    [">=0.2.3 || <0.0.1", "0.2.3"],
    [">=0.2.3 || <0.0.1", "0.2.4"],
    ["||", "1.3.4"],
    ["2.x.x", "2.1.3"],
    ["1.2.x", "1.2.3"],
    ["1.2.x || 2.x", "2.1.3"],
    ["1.2.x || 2.x", "1.2.3"],
    ["x", "1.2.3"],
    ["2.*.*", "2.1.3"],
    ["1.2.*", "1.2.3"],
    ["1.2.* || 2.*", "2.1.3"],
    ["1.2.* || 2.*", "1.2.3"],
    ["*", "1.2.3"],
    ["2.*", "2.1.2"],
    [">=1", "1.0.0"],
    [">= 1", "1.0.0"],
    ["<1.2", "1.1.1"],
    ["< 1.2", "1.1.1"],
    ["=0.7.x", "0.7.2"],
    [">=0.7.x", "0.7.2"],
    ["<=0.7.x", "0.6.2"],
    ["<=0.7.x", "0.7.2"]
  ].forEach(function (v) {
    assert.ok(xulApp.satisfiesVersion(v[1], v[0]), v[0] + " satisfied by " + v[1]);
  });
}
exports["test not satisfies version range"] = function (assert) {
  [ ["1.0.0 - 2.0.0", "2.2.3"],
    ["1.0.0", "1.0.1"],
    [">=1.0.0", "0.0.0"],
    [">=1.0.0", "0.0.1"],
    [">=1.0.0", "0.1.0"],
    [">1.0.0", "0.0.1"],
    [">1.0.0", "0.1.0"],
    ["<=2.0.0", "3.0.0"],
    ["<=2.0.0", "2.9999.9999"],
    ["<=2.0.0", "2.2.9"],
    ["<2.0.0", "2.9999.9999"],
    ["<2.0.0", "2.2.9"],
    [">=0.1.97", "v0.1.93"],
    [">=0.1.97", "0.1.93"],
    ["0.1.20 || 1.2.4", "1.2.3"],
    [">=0.2.3 || <0.0.1", "0.0.3"],
    [">=0.2.3 || <0.0.1", "0.2.2"],
    ["2.x.x", "1.1.3"],
    ["2.x.x", "3.1.3"],
    ["1.2.x", "1.3.3"],
    ["1.2.x || 2.x", "3.1.3"],
    ["1.2.x || 2.x", "1.1.3"],
    ["2.*.*", "1.1.3"],
    ["2.*.*", "3.1.3"],
    ["1.2.*", "1.3.3"],
    ["1.2.* || 2.*", "3.1.3"],
    ["1.2.* || 2.*", "1.1.3"],
    ["2", "1.1.2"],
    ["2.3", "2.3.1"],
    ["2.3", "2.4.1"],
    ["<1", "1.0.0"],
    [">=1.2", "1.1.1"],
    ["1", "2.0.0beta"],
    ["=0.7.x", "0.8.2"],
    [">=0.7.x", "0.6.2"],
  ].forEach(function (v) {
    assert.ok(!xulApp.satisfiesVersion(v[1], v[0]), v[0] + " not satisfied by " + v[1]);
  });
}

require("sdk/test").run(exports);
