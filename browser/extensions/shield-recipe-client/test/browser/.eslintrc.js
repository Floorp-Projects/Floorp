"use strict";

module.exports = {
  extends: [
    "plugin:mozilla/browser-test"
  ],

  plugins: [
    "mozilla"
  ],

  globals: {
    // Bug 1366720 - SimpleTest isn't being exported correctly, so list
    // it here for now.
    "SimpleTest": false
  }
};
