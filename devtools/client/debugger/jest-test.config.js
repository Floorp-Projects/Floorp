/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const sharedJestConfig = require(`${__dirname}/../shared/test-helpers/shared-jest.config`);

const { resolve } = require("path");
const rootDir = resolve(__dirname);
module.exports = {
  rootDir,
  displayName: "test",
  testURL: "http://localhost/",
  testEnvironment: "jsdom",
  testPathIgnorePatterns: [
    "/node_modules/",
    "/helpers/",
    "/fixtures/",
    "src/test/mochitest/examples/",
    "<rootDir>/firefox",
    "package.json",
    "<rootDir>/packages",
  ],
  modulePathIgnorePatterns: ["test/mochitest"],
  collectCoverageFrom: [
    "src/**/*.js",
    "!src/**/fixtures/*.js",
    "!src/test/**/*.js",
    "!src/components/stories/**/*.js",
    "!**/*.mock.js",
    "!**/*.spec.js",
  ],
  transform: {
    "\\.[jt]sx?$": "babel-jest",
  },
  transformIgnorePatterns: ["node_modules/(?!(devtools-|react-aria-))"],
  setupFilesAfterEnv: ["<rootDir>/src/test/tests-setup.js"],
  setupFiles: ["<rootDir>/src/test/shim.js", "jest-localstorage-mock"],
  snapshotSerializers: [
    "jest-serializer-babel-ast",
    "enzyme-to-json/serializer",
  ],
  moduleNameMapper: {
    ...sharedJestConfig.moduleNameMapper,
    "\\.css$": "<rootDir>/src/test/__mocks__/styleMock.js",
    "\\.svg$": "<rootDir>/src/test/__mocks__/svgMock.js",
  },
};
