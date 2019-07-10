/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const path = require("path");
const config = require("./webpack.system-addon.config.js");
const absolute = relPath => path.join(__dirname, relPath);
module.exports = Object.assign({}, config(), {
  entry: absolute("content-src/aboutlibrary/aboutlibrary.jsx"),
  output: {
    path: absolute("aboutlibrary/content"),
    filename: "aboutlibrary.bundle.js",
  },
});
