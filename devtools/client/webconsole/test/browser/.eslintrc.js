"use strict";

module.exports = {
  // Extend from the shared list of defined globals for mochitests.
  "extends": "../../../../.eslintrc.mochitests.js",
  "overrides": [{
    "files": ["test-dynamic-import.js"],
    "parserOptions": {
      "sourceType": "module",
    },
  }]
};
