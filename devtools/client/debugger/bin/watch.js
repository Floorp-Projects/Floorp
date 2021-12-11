/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

var chokidar = require("chokidar");
var shell = require("shelljs");
var path = require("path");

const geckoPath = path.join(__dirname, "../../../..");
const srcPath = path.join(__dirname, "../src");
function watch() {
  console.log("Watching for src changes");
  const watcher = chokidar.watch(srcPath);
  let working = false;
  watcher.on("change", filePath => {
    const relPath = path.relative(srcPath, filePath);
    console.log(`Updating ${relPath}`);
    if (working) {
      return;
    }
    working = true;
    const start = new Date();

    shell.exec(`cd ${geckoPath}; ./mach build faster; cd -;`, { silent: true });
    working = false;
    const end = Math.round((new Date() - start) / 1000);
    console.log(`  Built in ${end}s`);
  });
}

watch();
