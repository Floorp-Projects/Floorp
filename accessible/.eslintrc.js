"use strict";

module.exports = {
  "rules": {
    // Warn about cyclomatic complexity in functions.
    "complexity": ["error", 42],

    // XXX These are rules that are enabled in the recommended configuration, but
    // disabled here due to failures when initially implemented. They should be
    // removed (and hence enabled) at some stage.
    "brace-style": "off",
    "consistent-return": "off",
    "quotes": "off",
    "object-shorthand": "off",
    "space-before-function-paren": "off",
    "space-infix-ops": "off",
    "no-else-return": "off",
    "no-multi-spaces": "off",
    "no-unexpected-multiline": "off",
    "no-unsafe-finally": "off",
    "no-useless-call": "off",
    "spaced-comment": "off",
  }
};
