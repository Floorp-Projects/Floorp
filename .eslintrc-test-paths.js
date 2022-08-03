/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint-env node */

// The expressions defined below for test paths are the main path formats we
// prefer to support for tests as they are commonly used across the tree.
// See https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint.html#i-m-adding-tests-how-do-i-set-up-the-right-configuration
// for more information.

const browserTestPaths = ["**/test*/**/browser*/"];

const chromeTestPaths = ["**/test*/chrome/"];

const mochitestTestPaths = [
  // Note: we do not want to match testing/mochitest as that would apply
  // too many globals for that directory.
  "**/test/mochitest/",
  "**/tests/mochitest/",
  "**/test/mochitests/",
  "testing/mochitest/tests/SimpleTest/",
  "testing/mochitest/tests/Harness_sanity/",
];

const xpcshellTestPaths = [
  "**/test*/unit*/**/",
  "**/test*/*/unit*/",
  "**/test*/xpcshell/**/",
];

module.exports = {
  testPaths: {
    browser: [...browserTestPaths],
    chrome: [...chromeTestPaths],
    mochitest: [...mochitestTestPaths],
    xpcshell: [...xpcshellTestPaths],
  },
};
