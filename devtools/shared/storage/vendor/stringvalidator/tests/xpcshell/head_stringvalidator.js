"use strict";

const { require } = ChromeUtils.import("resource://devtools/shared/loader/Loader.jsm");

this.validator = require("resource://devtools/shared/storage/vendor/stringvalidator/validator.js");

function describe(suite, testFunc) {
  info(`\n                            Test suite: ${suite}`.toUpperCase());
  testFunc();
}

function it(description, testFunc) {
  info(`\n                              - ${description}:\n`.toUpperCase());
  testFunc();
}
