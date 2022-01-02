"use strict";

const { require } = ChromeUtils.import("resource://devtools/shared/loader/Loader.jsm");

this.validator = require("devtools/shared/storage/vendor/stringvalidator/validator");

function describe(suite, testFunc) {
  info(`\n                            Test suite: ${suite}`.toUpperCase());
  testFunc();
}

function it(description, testFunc) {
  info(`\n                              - ${description}:\n`.toUpperCase());
  testFunc();
}
