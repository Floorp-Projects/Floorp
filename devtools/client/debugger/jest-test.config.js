/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

/* global __dirname */

const sharedJestConfig = require(`${__dirname}/../shared/test-helpers/shared-jest.config`);

module.exports = {
  testEnvironment: "jsdom",
  testPathIgnorePatterns: [
    "/node_modules/",
    "/helpers/",
    "/fixtures/",
    "src/test/mochitest/examples/",
    "package.json",
  ],
  modulePathIgnorePatterns: ["test/mochitest"],
  setupFilesAfterEnv: ["<rootDir>/src/test/tests-setup.js"],
  setupFiles: ["<rootDir>/src/test/shim.js", "jest-localstorage-mock"],
  snapshotSerializers: ["enzyme-to-json/serializer"],
  moduleNameMapper: {
    ...sharedJestConfig.moduleNameMapper,
    "react-dom-factories": "<rootDir>/../shared/vendor/react-dom-factories.js",
  },
};
