"use strict";

module.exports = {
  "extends": [
    "plugin:mozilla/browser-test"
  ],

  "rules": {
    "no-unused-vars": ["error", {
      "vars": "all",
      "varsIgnorePattern": "^(Cc|Ci|Cr|Cu|EXPORTED_SYMBOLS)$",
      "args": "none"
    }]
  }
};
