/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global __dirname */
const testHelpersDir = `${__dirname}/../../../shared/test-helpers`;
const sharedJestConfig = require(`${testHelpersDir}/shared-jest.config`);

module.exports = {
  ...sharedJestConfig,
  moduleNameMapper: {
    ...sharedJestConfig.moduleNameMapper,
    "^chrome://mochitests/content/browser/devtools/client/webconsole/test/browser/stub-generator-helpers.js": `${__dirname}/../../test/browser/stub-generator-helpers.js`,
    "\\.css$": `${testHelpersDir}/jest-fixtures/empty-module`,
    "\\.svg$": `${testHelpersDir}/jest-fixtures/svgMock.js`,
  },
  setupFiles: ["<rootDir>jest-setup.js"],
  transform: {
    "\\.[jt]sx?$": "babel-jest",
  },
};
