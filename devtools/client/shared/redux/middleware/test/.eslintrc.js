"use strict";

module.exports = {
  // Extend from the shared list of defined globals for mochitests.
  "extends": "../../../../../.eslintrc.mochitests.js",
  "globals": {
    "run_test": true,
    "run_next_test": true,
    "equal": true,
    "do_print": true,
    "waitUntilState": true
  },
  "rules": {
    // Stop giving errors for run_test
    "camelcase": "off"
  }
};
