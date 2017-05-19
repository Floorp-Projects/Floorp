"use strict";

module.exports = {
  "extends": [
    "plugin:mozilla/recommended",
    "plugin:mozilla/browser-test",
    "plugin:mozilla/chrome-test",
    "plugin:mozilla/mochitest-test",
  ],
  "plugins": [
    "mozilla"
  ],
  "rules": {
    "brace-style": "off",
    "no-shadow": "off",
  }
};
