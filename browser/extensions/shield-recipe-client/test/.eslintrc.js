"use strict";

module.exports = {
  globals: {
    Assert: false,
    BrowserTestUtils: false,
    add_task: false,
    is: false,
    isnot: false,
    ok: false,
    SpecialPowers: false,
    SimpleTest: false,
    registerCleanupFunction: false,
  },
  rules: {
    "spaced-comment": 2,
    "space-before-function-paren": 2,
    "require-yield": 0
  }
};
