"use strict";

module.exports = {
  "extends": [
    "../.eslintrc.js"
  ],
  "globals": {
    "Cc": true,
    "Ci": true,
    "Components": true,
    "console": true,
    "Cu": true,
    "dump": true,
    "Services": true,
    "XPCOMUtils": true
  },
  "rules": {
    // Warn about cyclomatic complexity in functions.
    "complexity": ["error", 42],

    // Maximum depth callbacks can be nested.
    "max-nested-callbacks": ["error", 10],
  }
};
