/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* global __dirname */

"use strict";

module.exports = {
  verbose: true,
  moduleNameMapper: {
    "^Services": `${__dirname}/__mock__/Services`,
    // Map all require("devtools/...") to the real devtools root.
    "^devtools\\/(.*)": `${__dirname}/../../../../../$1`,
  },
  setupFiles: ["<rootDir>/setup.js"],
  snapshotSerializers: ["enzyme-to-json/serializer"],
  testURL: "http://localhost/",
};
