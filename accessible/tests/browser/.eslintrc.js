"use strict";

module.exports = {
  rules: {
    "mozilla/no-aArgs": "error",
    "mozilla/reject-importGlobalProperties": ["error", "everything"],
    "mozilla/var-only-at-top-level": "error",

    "block-scoped-var": "error",
    camelcase: ["error", { properties: "never" }],
    complexity: ["error", 20],

    "handle-callback-err": ["error", "er"],
    "max-nested-callbacks": ["error", 4],
    "new-cap": ["error", { capIsNew: false }],
    "no-fallthrough": "error",
    "no-multi-str": "error",
    "no-proto": "error",
    "no-return-assign": "error",
    "no-shadow": "error",
    "no-unused-vars": ["error", { vars: "all", argsIgnorePattern: "^_" }],
    "one-var": ["error", "never"],
    radix: "error",
    strict: ["error", "global"],
    yoda: "error",
    "no-undef-init": "error",
  },
};
