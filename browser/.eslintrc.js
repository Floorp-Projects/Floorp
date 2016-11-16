"use strict";

module.exports = {
  "extends": [
    "../toolkit/.eslintrc.js"
  ],

  "rules": {
    "no-unused-vars": ["error", {
      "vars": "local",
      "varsIgnorePattern": "^Cc|Ci|Cu|Cr|EXPORTED_SYMBOLS",
      "args": "none",
    }],
    "no-shadow": "error"
  }
};
