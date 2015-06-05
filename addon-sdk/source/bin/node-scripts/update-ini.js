/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var path = require("path");
var cp = require("child_process");
var fs = require("fs");
var Promise = require("promise");
var parser = require("ini-parser");

var addonINI = path.resolve("./test/addons/jetpack-addon.ini");
var addonsDir = path.resolve("./test/addons/");

function updateAddonINI() {
  return new Promise(function(resolve) {
    console.log("Start updating " + addonINI);

    makeAddonIniContent().
    then(function(contents) {
      fs.writeFileSync(addonINI, contents, { encoding: "utf8" });
      console.log("Done updating " + addonINI);
      resolve();
    });
  })
}
exports.updateAddonINI = updateAddonINI;

function makeAddonIniContent() {
  return new Promise(function(resolve) {
    var data = parser.parse(fs.readFileSync(addonINI, { encoding: "utf8" }).toString());
    var result = {};

    fs.readdir(addonsDir, function(err, files) {
      var folders = files.filter(function(file) {
        return fs.statSync(path.resolve(addonsDir, file)).isDirectory();
      }).sort();

      folders.forEach(function(folder) {
        var oldData = data[folder + ".xpi"];
        result[folder] = oldData ? oldData : {};
      });

      // build ini file
      var contents = [];
      Object.keys(result).sort().forEach(function(key) {
        contents.push("[" + key + ".xpi]");
        Object.keys(result[key]).forEach(function(dataKey) {
          contents.push(dataKey + " = " + result[key][dataKey]);
        });
      });
      contents = contents.join("\n") + "\n";

      return resolve(contents);
    });
  });
}
exports.makeAddonIniContent = makeAddonIniContent;
