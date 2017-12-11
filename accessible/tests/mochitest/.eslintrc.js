"use strict";

module.exports = {
  "extends": [
    "plugin:mozilla/mochitest-test"
  ],
  "rules": {
    "mozilla/no-cpows-in-tests": "error",
    "mozilla/reject-importGlobalProperties": "error",

    // XXX These are rules that are enabled in the recommended configuration, but
    // disabled here due to failures when initially implemented. They should be
    // removed (and hence enabled) at some stage.
    "no-nested-ternary": "off",
    "no-new-object": "off",
    "no-redeclare": "off",
    "no-shadow": "off",
    "no-undef": "off",
    "space-unary-ops": "off",
  }
};
