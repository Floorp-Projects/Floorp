/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global __dirname */
const sharedJestConfig = require(`${__dirname}/../../../shared/test-helpers/shared-jest.config`);

module.exports = {
  ...sharedJestConfig,
  moduleNameMapper: {
    // This needs to be set before the other rules
    "^resource://devtools/shared/event-emitter": `${__dirname}/node_modules/devtools-modules/src/utils/event-emitter`,
    ...sharedJestConfig.moduleNameMapper,
  },
  setupFiles: ["<rootDir>setup.js"],
  snapshotSerializers: ["enzyme-to-json/serializer"],
};
