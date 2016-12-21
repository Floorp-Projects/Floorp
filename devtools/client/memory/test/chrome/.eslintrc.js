"use strict";

module.exports = {
  // Extend from the shared list of defined globals for mochitests.
  "extends": "../../../../.eslintrc.mochitests.js",
  "globals": {
    "SimpleTest": true,
    "ok": true,
    "requestAnimationFrame": true
  },
  "rules": {
    "no-unused-vars": ["error", { "vars": "local", "args": "none" }],
  }
};
