"use strict";

module.exports = {
  // Extend from the devtools eslintrc.
  "extends": "../../../../.eslintrc.js",

  "rules": {
    // All code in this directory must be content-clean.
    "mozilla/reject-some-requires": ["error", "^(chrome|chrome:.*|resource:.*|devtools/server/.*|.*\\.jsm|devtools/shared/platform/(chome|content)/.*)$"],
  },
};
