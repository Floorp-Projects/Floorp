"use strict";

module.exports = {
  "rules": {
    "no-unused-vars": ["error", {
      "vars": "local",
      "varsIgnorePattern": "^Cc|Ci|Cu|Cr|EXPORTED_SYMBOLS",
      "args": "none",
    }]
  }
};
