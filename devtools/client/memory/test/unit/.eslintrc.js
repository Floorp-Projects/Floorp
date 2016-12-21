"use strict";

module.exports = {
  // Extend from the shared list of defined globals for mochitests.
  "extends": "../../../../.eslintrc.xpcshell.js",
  "rules": {
    "no-unused-vars": ["error", {
      "vars": "local",
      "varsIgnorePattern": "^run_test$"
    }]
  }
};
