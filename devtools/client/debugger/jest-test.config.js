/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { resolve } = require("path");
const rootDir = resolve(__dirname);
module.exports = {
  rootDir,
  displayName: "test",
  testURL: "http://localhost/",
  testPathIgnorePatterns: [
    "/node_modules/",
    "/helpers/",
    "/fixtures/",
    "src/test/mochitest/examples/",
    "<rootDir>/firefox",
    "package.json",
    "<rootDir>/packages",
  ],
  modulePathIgnorePatterns: ["test/mochitest", "firefox"],
  collectCoverageFrom: [
    "src/**/*.js",
    "!src/**/fixtures/*.js",
    "!src/test/**/*.js",
    "!src/components/stories/**/*.js",
    "!**/*.mock.js",
    "!**/*.spec.js",
  ],
  transformIgnorePatterns: ["node_modules/(?!(devtools-|react-aria-))"],
  setupTestFrameworkScriptFile: "<rootDir>/src/test/tests-setup.js",
  setupFiles: ["<rootDir>/src/test/shim.js", "jest-localstorage-mock"],
  snapshotSerializers: [
    "jest-serializer-babel-ast",
    "enzyme-to-json/serializer",
  ],
  moduleNameMapper: {
    "\\.css$": "<rootDir>/src/test/__mocks__/styleMock.js",
    "\\.svg$": "<rootDir>/src/test/__mocks__/svgMock.js",
  },
};
