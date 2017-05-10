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
    "consistent-return": "off",
    "no-native-reassign": "off",
    "no-shadow": "off",
    "no-unused-vars": "off",
  }
};
