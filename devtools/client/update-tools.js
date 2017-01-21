/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global process */

/*
  Update Tools is responsible for synking projects in github and mc

  1. checking if assets have changed
  2. updating assets in m-c

  ### Check for changes

  ```
  node update-tools.js --check
  ```

  ### Update Assets

  ```
  node update-tools.js --update
  ```
*/

const ps = require("child_process");
const fs = require("fs");
const { dirname } = require("path");
var dircompare = require("dir-compare");

/*eslint-disable */
const paths = [
  "./node_modules/debugger.html/assets/build/debugger.js ./debugger/new/debugger.js",
  "./node_modules/debugger.html/assets/build/source-map-worker.js ./debugger/new/source-map-worker.js",
  "./node_modules/debugger.html/assets/build/pretty-print-worker.js ./debugger/new/pretty-print-worker.js",
  "./node_modules/debugger.html/assets/build/debugger.css ./debugger/new/debugger.css",
  "./node_modules/debugger.html/assets/build/debugger.properties ./locales/en-US/debugger.properties",
  "./node_modules/debugger.html/assets/build/codemirror-mozilla.css ./sourceeditor/codemirror/mozilla.css",
];

const dirs = [
  "./node_modules/debugger.html/assets/build/mochitest ./debugger/new/test/mochitest"
];
/*eslint-enable */

function isDirectory(path) {
  return fs.statSync(path).isDirectory();
}

function checkFile(path) {
  const result = ps.spawnSync(`diff`, path.split(" "), {
    encoding: "utf8"
  });

  const stdout = result.output[1].trim();
  return stdout.length > 0;
}

function copyFile(path) {
  const [ghPath, mcFile] = path.split(" ");
  const destPath = isDirectory(mcFile) ? dirname(mcFile) : mcFile;
  ps.execSync(`cp -r ${ghPath} ${destPath}`);
}

function checkAssets() {
  paths.forEach(path => {
    if (checkFile(path)) {
      console.log(`Changed: diff: ${path}`);
    }
  });

  dirs.forEach(dir => {
    const filesChanged = checkDir(dir);
    if (filesChanged) {
      filesChanged.forEach(file => {
        const { name1, path1, name2, path2 } = file;
        console.log(`Changed: diff ${path1}/${name1} ${path2}/${name2}`);
      });
    }
  });
}

function checkDir(path) {
  const [ghDir, mcDir] = path.split(" ");
  const res = dircompare.compareSync(ghDir, mcDir, { compareSize: true });

  return res.diffSet.filter(entry => entry.state != "equal");
}

function copyAssets(files) {
  paths.forEach(copyFile);
  dirs.forEach(copyFile);
}

if (process.argv.includes("--check")) {
  checkAssets();
}

if (process.argv.includes("--update")) {
  copyAssets();
}
