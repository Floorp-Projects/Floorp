"use strict";

module.exports = {
  "extends": [
    "plugin:mozilla/mochitest-test",
  ],
  "plugins": [
    "mozilla"
  ],
  "rules": {
    "brace-style": "off",
    "no-shadow": "off",
  },
  "globals": {
    "AbortController": true,
    "AbortSignal": true
  }
};
