"use strict";

module.exports = {
  // Extend from the shared list of defined globals for mochitests.
  "extends": "../../../../.eslintrc.mochitests.js",
  "rules": {
    "no-unused-vars": ["error", { "vars": "local", "args": "none" }],
  }
};
