"use strict";

module.exports = {
  "globals": {
    "addMessageListener": false,
    "removeMessageListener": false,
    "sendAsyncMessage": false,
    "TextDecoder": false,
    "TextEncoder": false,
  },

  "rules": {
    // Rules from the mozilla plugin
    "mozilla/balanced-listeners": "error",
    "mozilla/no-aArgs": "error",
    "mozilla/no-cpows-in-tests": "error",
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

    // Commas at the end of the line not the start
    "comma-style": "error",

    // Require braces around blocks that start a new line
    "curly": ["error", "all"],

    // Require function* name()
    "generator-star-spacing": ["error", {"before": false, "after": true}],

    // Two space indent
    "indent": ["error", 2, {"SwitchCase": 1}],

    // Always require parenthesis for new calls
    "new-parens": "error",

    // No expressions where a statement is expected
    "no-unused-expressions": "error",

    // No declaring variables that are never used
    "no-unused-vars": ["error", {"args": "none", "varsIgnorePattern": "^(Cc|Ci|Cr|Cu|EXPORTED_SYMBOLS)$"}],

    // No using variables before defined
    "no-use-before-define": "error",

    // Always require semicolon at end of statement
    "semi": ["error", "always"],

    // Disallow using variables outside the blocks they are defined (especially
    // since only let and const are used, see "no-var").
    "block-scoped-var": "error",

    // Allow trailing commas for easy list extension.  Having them does not
    // impair readability, but also not required either.
    "comma-dangle": ["error", "always-multiline"],

    // Warn about cyclomatic complexity in functions.
    "complexity": ["error", {"max": 20}],

    // Enforce dots on the next line with property name.
    "dot-location": ["error", "property"],

    // Encourage the use of dot notation whenever possible.
    "dot-notation": "error",

    // Maximum length of a line.
    // This should be 100 but too many lines were longer than that so set a
    // conservative upper bound for now.
    "max-len": ["error", 140],

    // Maximum depth callbacks can be nested.
    "max-nested-callbacks": ["error", 4],

    // Disallow use of arguments.caller or arguments.callee.
    "no-caller": "error",

    // Disallow using the console API.
    "no-console": "error",

    // Disallow control characters in regular expressions.
    "no-control-regex": "error",

    // Disallow fallthrough of case statements, except if there is a comment.
    "no-fallthrough": "error",

    // Disallow use of multiline strings (use template strings instead).
    "no-multi-str": "warn",

    // Disallow multiple empty lines.
    "no-multiple-empty-lines": ["warn", {"max": 2}],

    // Disallow usage of __proto__ property.
    "no-proto": "error",

    // Disallow use of assignment in return statement. It is preferable for a
    // single line of code to have only one easily predictable effect.
    "no-return-assign": "error",

    // Disallow throwing literals (eg. throw "error" instead of
    // throw new Error("error")).
    "no-throw-literal": "error",

    // Disallow the use of Boolean literals in conditional expressions.
    "no-unneeded-ternary": "error",

    // Disallow padding within blocks.
    "padded-blocks": ["warn", "never"],

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

    // Disallow creating new instances of String, Number, and Boolean
    "no-new-wrappers": "error",
  },
};
