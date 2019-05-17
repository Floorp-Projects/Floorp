/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { resolve } = require("path");
const rootDir = resolve(__dirname, "src");
module.exports = {
  rootDir,
  displayName: "devtools-components test",
  setupFiles: [
    "<rootDir>/../../../src/test/__mocks__/request-animation-frame.js",
    "<rootDir>/tests/setup.js",
  ],
  testMatch: ["**/tests/**/*.js"],
  testPathIgnorePatterns: [
    "/node_modules/",
    "<rootDir>/tests/__mocks__/",
    "<rootDir>/tests/setup.js",
  ],
  testURL: "http://localhost/",
  moduleNameMapper: {
    "\\.css$": "<rootDir>/../../../src/test/__mocks__/styleMock.js",
  },
};
