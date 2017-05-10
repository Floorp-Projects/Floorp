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
    "key-spacing": "off",
    "keyword-spacing": "off",
    "no-extra-semi": "off",
    "no-native-reassign": "off",
    "no-shadow": "off",
    "no-trailing-spaces": "off",
    "no-unused-vars": "off",
    "object-shorthand": "off",
    "quotes": "off",
    "spaced-comment": "off",
    "space-before-blocks": "off",
    "space-before-function-paren": "off",
  }
};
