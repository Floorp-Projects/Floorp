"use strict";

module.exports = {
  "rules": {
    "block-scoped-var": "error",
    "complexity": ["error", {"max": 22}],
    "indent-legacy": ["error", 2, {"SwitchCase": 1, "ArrayExpression": "first", "ObjectExpression": "first"}],
    "max-nested-callbacks": ["error", 3],
    "new-parens": "error",
    "no-extend-native": "error",
    "no-fallthrough": ["error", { "commentPattern": ".*[Ii]ntentional(?:ly)?\\s+fall(?:ing)?[\\s-]*through.*" }],
    "no-multi-str": "error",
    "no-return-assign": "error",
    "no-shadow": "error",
    "no-unused-vars": ["error", { "args": "after-used", "vars": "all" }],
    "semi-spacing": ["error", {"before": false, "after": true}],
    "space-in-parens": ["error", "never"],
    "strict": ["error", "global"],
    "yoda": "error"
  },

  "overrides": [{
    "files": "tests/unit/head*.js",
    "rules": {
      "no-unused-vars": ["error", {
        "args": "none",
        "vars": "local",
      }],
    },
  }],
};
