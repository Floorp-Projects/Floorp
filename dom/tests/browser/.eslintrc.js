"use strict";

module.exports = {
  overrides: [
    {
      files: [
        "file_module_loaded.js",
      ],
      parserOptions: {
        sourceType: "module",
      },
    },
  ],
  "extends": [
    "plugin:mozilla/browser-test"
  ]
};
