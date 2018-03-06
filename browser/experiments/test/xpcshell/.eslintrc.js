"use strict";

module.exports = {
  "extends": [
    "plugin:mozilla/xpcshell-test"
  ],

  "rules": {
    "no-unused-vars": ["error", {
      "vars": "all",
      "args": "none"
    }]
  }
};
