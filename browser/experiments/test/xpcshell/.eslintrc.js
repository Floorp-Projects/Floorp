"use strict";

module.exports = {
  "extends": [
    "../../../../testing/xpcshell/xpcshell.eslintrc.js"
  ],

  "rules": {
    "no-unused-vars": ["error", {
      "vars": "all",
      "varsIgnorePattern": "^(Cc|Ci|Cr|Cu|EXPORTED_SYMBOLS)$",
      "args": "none"
    }]
  }
};
