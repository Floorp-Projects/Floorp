"use strict";

module.exports = {
  overrides: [
    {
      files: [
        "res/components/*.js",
        "res/containers/*.js",
        "res/mixins/*.js",
        "res/paymentRequest.js",
        "res/PaymentsStore.js",
        "test/mochitest/test_*.html",
      ],
      parserOptions: {
        sourceType: "module",
      },
    },
    {
      "files": "test/unit/head.js",
      "rules": {
        "no-unused-vars": ["error", {
          "args": "none",
          "vars": "local",
        }],
      },
    },
  ],
  rules: {
    "mozilla/var-only-at-top-level": "error",

    "block-scoped-var": "error",
    complexity: ["error", {
      max: 20,
    }],
    "max-nested-callbacks": ["error", 4],
    "no-console": ["error", { allow: ["error"] }],
    "no-fallthrough": "error",
    "no-multi-str": "error",
    "no-proto": "error",
    "no-unused-expressions": "error",
    "no-unused-vars": ["error", {
      args: "none",
      vars: "all"
    }],
    "no-use-before-define": ["error", {
      functions: false,
    }],
    radix: "error",
    "valid-jsdoc": ["error", {
      prefer: {
        return: "returns",
      },
      preferType: {
        Boolean: "boolean",
        Number: "number",
        String: "string",
        bool: "boolean",
      },
      requireParamDescription: false,
      requireReturn: false,
      requireReturnDescription: false,
    }],
    yoda: "error",
  },
};
