/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
/* globals process, __filename, __dirname */

"use strict";

const Babel = require("./babel");
const fs = require("fs");
const _path = require("path");

const defaultPlugins = [
  "transform-flow-strip-types",
  "proposal-class-properties",
  "transform-react-jsx",
];

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

const deps = [__filename, _path.resolve(__dirname, "babel.js")];

for (let i = 2; i < process.argv.length; i++) {
  const srcPath = process.argv[i];
  const code = transform(srcPath);
  const filePath = _path.basename(srcPath);
  fs.writeFileSync(filePath, code);
  deps.push(srcPath);
}

// Print all dependencies prefixed with 'dep:' in order to help node.py, the script that
// calls this module, to report back the precise list of all dependencies.
console.log(deps.map(file => "dep:" + file).join("\n"));
