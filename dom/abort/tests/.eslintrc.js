"use strict";

module.exports = {
  "extends": [
    "plugin:mozilla/mochitest-test",
  ],
  "plugins": [
    "mozilla"
  ],
  "rules": {
    "no-shadow": "off",
  },
  "globals": {
    "AbortController": true,
    "AbortSignal": true
  }
};
