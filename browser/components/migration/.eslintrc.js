"use strict";

module.exports = {
  "rules": {
    "block-scoped-var": "error",
    "complexity": ["error", {"max": 22}],
    "max-nested-callbacks": ["error", 3],
    "no-extend-native": "error",
    "no-fallthrough": ["error", { "commentPattern": ".*[Ii]ntentional(?:ly)?\\s+fall(?:ing)?[\\s-]*through.*" }],
    "no-multi-str": "error",
    "no-return-assign": "error",
    "no-shadow": "error",
    "no-unused-vars": ["error", { "args": "after-used", "vars": "all" }],
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
