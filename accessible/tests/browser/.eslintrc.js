"use strict";

module.exports = {
  "rules": {
    "mozilla/no-aArgs": "error",
    "mozilla/reject-importGlobalProperties": ["error", "everything"],
    "mozilla/var-only-at-top-level": "error",

    "block-scoped-var": "error",
    "camelcase": "error",
    "comma-dangle": ["error", "never"],
    "complexity": ["error", 20],
    "curly": ["error", "multi-line"],
    "dot-location": ["error", "property"],

    "handle-callback-err": ["error", "er"],
    "indent-legacy": ["error", 2, {"SwitchCase": 1}],
    "max-nested-callbacks": ["error", 4],
    "new-cap": ["error", {"capIsNew": false}],
    "new-parens": "error",
    "no-fallthrough": "error",
    "no-multi-str": "error",
    "no-multiple-empty-lines": ["error", {"max": 1}],
    "no-proto": "error",
    "no-return-assign": "error",
    "no-shadow": "error",
    "no-unused-vars": ["error", {"vars": "all", "args": "none"}],
    "one-var": ["error", "never"],
    "radix": "error",
    "semi-spacing": ["error", {"before": false, "after": true}],
    "space-in-parens": ["error", "never"],
    "strict": ["error", "global"],
    "yoda": "error",
    "no-undef-init": "error",
    "operator-linebreak": ["error", "after"]
  }
};
