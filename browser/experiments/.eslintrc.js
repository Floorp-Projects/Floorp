"use strict";

module.exports = {
  "rules": {
    "no-unused-vars": ["error", {
      "vars": "all",
      "varsIgnorePattern": "^(Cc|Ci|Cr|Cu|EXPORTED_SYMBOLS)$",
      "args": "none"
    }]
  }
};
