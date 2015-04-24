/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var Promise = require("promise");
var Mocha = require("mocha");
var mocha = new Mocha({
  ui: "bdd",
  reporter: "spec",
  timeout: 900000
});

var isDebug = require("./node-scripts/utils").isDebug;

exports.run = function(type) {
  return new Promise(function(resolve) {
    type = type || "";
    [
      (!isDebug && /^(modules)?$/.test(type)) && require.resolve("../bin/node-scripts/test.modules"),
      (!isDebug && /^(addons)?$/.test(type)) && require.resolve("../bin/node-scripts/test.addons"),
      (/^(examples)?$/.test(type)) && require.resolve("../bin/node-scripts/test.examples"),
      (!isDebug && /^(docs)?$/.test(type)) && require.resolve("../bin/node-scripts/test.docs"),
      (!isDebug && /^(ini)?$/.test(type)) && require.resolve("../bin/node-scripts/test.ini"),
    ].sort().forEach(function(filepath) {
      filepath && mocha.addFile(filepath);
    })

    mocha.run(function(failures) {
      resolve(failures);
    });
  });
}
