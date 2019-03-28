"use strict";

module.exports = {
  "rules": {
    // Rules from the mozilla plugin
    "mozilla/balanced-listeners": "error",
    "mozilla/no-aArgs": "error",
    "mozilla/var-only-at-top-level": "error",

    "valid-jsdoc": ["error", {
      "prefer": {
        "return": "returns",
      },
      "preferType": {
        "Boolean": "boolean",
        "Number": "number",
        "String": "string",
        "bool": "boolean",
      },
      "requireParamDescription": false,
      "requireReturn": false,
      "requireReturnDescription": false,
    }],

    // Forbid spaces inside the square brackets of array literals.
    "array-bracket-spacing": ["error", "never"],

    // Forbid spaces inside the curly brackets of object literals.
    "object-curly-spacing": ["error", "never"],

    // No space padding in parentheses
    "space-in-parens": ["error", "never"],

    // Require braces around blocks that start a new line
    "curly": ["error", "all"],

    // Two space indent
    "indent-legacy": ["error", 2, {"SwitchCase": 1}],

    // Always require parenthesis for new calls
    "new-parens": "error",

    // No expressions where a statement is expected
    "no-unused-expressions": "error",

    // No declaring variables that are never used
    "no-unused-vars": ["error", {
      "args": "none", "vars": "all"
    }],

    // No using variables before defined
    "no-use-before-define": "error",

    // Disallow using variables outside the blocks they are defined (especially
    // since only let and const are used, see "no-var").
    "block-scoped-var": "error",

    // Warn about cyclomatic complexity in functions.
    "complexity": ["error", {"max": 26}],

    // Enforce dots on the next line with property name.
    "dot-location": ["error", "property"],

    // Maximum length of a line.
    // This should be 100 but too many lines were longer than that so set a
    // conservative upper bound for now.
    "max-len": ["error", 140],

    // Maximum depth callbacks can be nested.
    "max-nested-callbacks": ["error", 4],

    // Disallow using the console API.
    "no-console": "error",

    // Disallow fallthrough of case statements, except if there is a comment.
    "no-fallthrough": "error",

    // Disallow use of multiline strings (use template strings instead).
    "no-multi-str": "error",

    // Disallow multiple empty lines.
    "no-multiple-empty-lines": ["error", {"max": 2}],

    // Disallow usage of __proto__ property.
    "no-proto": "error",

    // Disallow use of assignment in return statement. It is preferable for a
    // single line of code to have only one easily predictable effect.
    "no-return-assign": "error",

    // Require use of the second argument for parseInt().
    "radix": "error",

    // Enforce spacing after semicolons.
    "semi-spacing": ["error", {"before": false, "after": true}],

    // Require "use strict" to be defined globally in the script.
    "strict": ["error", "global"],

    // Disallow Yoda conditions (where literal value comes first).
    "yoda": "error",

    // Disallow function or variable declarations in nested blocks
    "no-inner-declarations": "error",
  },

  "overrides": [{
    "files": "test/unit/head.js",
    "rules": {
      "no-unused-vars": ["error", {
        "args": "none",
        "vars": "local",
      }],
    },
  }],
};
