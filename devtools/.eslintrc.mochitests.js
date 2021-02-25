/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Parent config file for all devtools browser mochitest files.
module.exports = {
  extends: ["plugin:mozilla/browser-test"],
  // All globals made available in the test environment.
  globals: {
    DevToolsUtils: true,
    gDevTools: true,
    once: true,
    synthesizeKeyFromKeyTag: true,
    TargetFactory: true,
    waitForTick: true,
    waitUntilState: true,
  },

  parserOptions: {
    ecmaFeatures: {
      jsx: true,
    },
  },

  rules: {
    // Allow non-camelcase so that run_test doesn't produce a warning.
    camelcase: "off",
    // Tests don't have to cleanup observers
    "mozilla/balanced-observers": 0,
    // Tests can always import anything.
    "mozilla/reject-some-requires": 0,
  },
};
