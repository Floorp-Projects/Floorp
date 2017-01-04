"use strict";

module.exports = {
  // When adding items to this file please check for effects on sub-directories.
  "plugins": [
    "mozilla"
  ],
  "rules": {
    "mozilla/import-globals": "warn",
    "mozilla/no-import-into-var-and-global": "error",

    // No (!foo in bar) or (!object instanceof Class)
    "no-unsafe-negation": "error",
  },
  "env": {
    "es6": true
  },
  "parserOptions": {
    "ecmaVersion": 8,
  },
};
