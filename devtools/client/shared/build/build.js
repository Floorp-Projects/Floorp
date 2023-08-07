/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
/* globals process, __filename, __dirname */

/* Usage:  node build.js [LIST_OF_SOURCE_FILES...] OUTPUT_DIR
 *    Compiles all source files and places the results of the compilation in
 * OUTPUT_DIR.
 */

"use strict";

const Babel = require("./babel");
const fs = require("fs");
const _path = require("path");

const defaultPlugins = ["proposal-class-properties"];

function transform(filePath) {
  // Use the extra plugins only for the debugger
  const plugins = filePath.includes("devtools/client/debugger")
    ? require("./build-debugger")(filePath)
    : defaultPlugins;

  const doc = fs.readFileSync(filePath, "utf8");

  let out;
  try {
    out = Babel.transform(doc, { plugins });
  } catch (err) {
    throw new Error(`
========================
NODE COMPILATION ERROR!

File:   ${filePath}
Stack:

${err.stack}

========================
`);
  }

  return out.code;
}

// fs.mkdirSync's "recursive" option appears not to work, so I'm writing a
// simple version of the function myself.
function mkdirs(filePath) {
  if (fs.existsSync(filePath)) {
    return;
  }
  mkdirs(_path.dirname(filePath));
  try {
    fs.mkdirSync(filePath);
  } catch (err) {
    // Ignore any errors resulting from the directory already existing.
    if (err.code != "EEXIST") {
      throw err;
    }
  }
}

const deps = [__filename, _path.resolve(__dirname, "babel.js")];
const outputDir = process.argv[process.argv.length - 1];
mkdirs(outputDir);

for (let i = 2; i < process.argv.length - 1; i++) {
  const srcPath = process.argv[i];
  const code = transform(srcPath);
  const fullPath = _path.join(outputDir, _path.basename(srcPath));
  fs.writeFileSync(fullPath, code);
  deps.push(srcPath);
}

// Print all dependencies prefixed with 'dep:' in order to help node.py, the script that
// calls this module, to report back the precise list of all dependencies.
console.log(deps.map(file => "dep:" + file).join("\n"));
