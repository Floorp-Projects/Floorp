/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

module.exports = {
  env: {
    jest: true,
  },
  overrides: [
    {
      files: [
        // Exempt all test files that explicitly want to test http urls from 'no-insecure-url' rule.
        // Gradually change test cases such that this list gets smaller and more precisely. Bug 1758951
        "components/debug-target-info.test.js",
      ],
      rules: {
        "@microsoft/sdl/no-insecure-url": "off",
      },
    },
  ],
};
