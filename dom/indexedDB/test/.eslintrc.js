"use strict";

module.exports = {
  "extends": [
    "plugin:mozilla/browser-test",
    "plugin:mozilla/chrome-test",
    "plugin:mozilla/mochitest-test",
  ],
  "plugins": [
    "mozilla"
  ],
  "rules": {
    "no-shadow": "off",
  }
};
