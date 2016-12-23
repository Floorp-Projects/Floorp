"use strict";

module.exports = {
  // Extend from the shared list of defined globals for mochitests.
  "extends": "../../../../.eslintrc.mochitests.js",
  "globals": {
    "addTab": true,
    "censusState": true,
    "refreshTab": true,
    "removeTab": true,
    "waitForTime": true,
    "waitUntilState": true
  },
  "rules": {
    "no-unused-vars": ["error", { "vars": "local", "args": "none" }],
  }
};
