"use strict";

module.exports = {
  "extends": [
    "plugin:mozilla/xpcshell-test"
  ],

  "rules": {
    "no-unused-vars": ["error", {
      "vars": "all",
      "varsIgnorePattern": "^(Cc|Ci|Cr|Cu|EXPORTED_SYMBOLS)$",
      "args": "none"
    }]
  }
};
