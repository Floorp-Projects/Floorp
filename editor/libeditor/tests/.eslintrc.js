"use strict";

module.exports = {
  "extends": [
    "plugin:mozilla/browser-test",
    "plugin:mozilla/mochitest-test",
  ],

  "plugins": [
    "no-unsanitized",
  ],

  "rules": {
    "no-unsanitized/property": "off",
  },
};
