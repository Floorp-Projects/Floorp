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
var packageINI = path.resolve("./test/jetpack-package.ini");
var packageDir = path.resolve("./test/");
var packageIgnorables = [ "addons", "preferences" ];
var packageSupportFiles = [
  "fixtures.js",
  "test-context-menu.html",
  "util.js"
]

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
      // get a list of folders
      var folders = files.filter(function(file) {
        return fs.statSync(path.resolve(addonsDir, file)).isDirectory();
      }).sort();

      // copy any related data from the existing ini
      folders.forEach(function(folder) {
        var oldData = data[folder + ".xpi"];
        result[folder] = oldData ? oldData : {};
      });

      // build a new ini file
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

function makePackageIniContent() {
  return new Promise(function(resolve) {
    var data = parser.parse(fs.readFileSync(packageINI, { encoding: "utf8" }).toString());
    var result = {};

    fs.readdir(packageDir, function(err, files) {
      // get a list of folders
      var folders = files.filter(function(file) {
        var ignore = (packageIgnorables.indexOf(file) >= 0);
        var isDir = fs.statSync(path.resolve(packageDir, file)).isDirectory();
        return (isDir && !ignore);
      }).sort();

      // get a list of "test-"" files
      var files = files.filter(function(file) {
        var ignore = !/^test\-.*\.js$/i.test(file);
        var isDir = fs.statSync(path.resolve(packageDir, file)).isDirectory();
        return (!isDir && !ignore);
      }).sort();

      // get a list of the support files
      var support_files = packageSupportFiles.map(function(file) {
        return "  " + file;
      });
      folders.forEach(function(folder) {
        support_files.push("  " + folder + "/**");
      });
      support_files = support_files.sort();

      // copy any related data from the existing ini
      files.forEach(function(file) {
        var oldData = data[file];
        result[file] = oldData ? oldData : {};
      });

      // build a new ini file
      var contents = [
        "[DEFAULT]",
        "support-files ="
      ];
      support_files.forEach(function(support_file) {
        contents.push(support_file);
      });
      contents.push("");

      Object.keys(result).sort().forEach(function(key) {
        contents.push("[" + key + "]");
        Object.keys(result[key]).forEach(function(dataKey) {
          contents.push(dataKey + " = " + result[key][dataKey]);
        });
      });
      contents = contents.join("\n") + "\n";

      return resolve(contents);
    });
  });
}
exports.makePackageIniContent = makePackageIniContent;

function updatePackageINI() {
  return new Promise(function(resolve) {
    console.log("Start updating " + packageINI);

    makeAddonIniContent().
    then(function(contents) {
      fs.writeFileSync(packageINI, contents, { encoding: "utf8" });
      console.log("Done updating " + packageINI);
      resolve();
    });
  })
}
exports.updatePackageINI = updatePackageINI;
