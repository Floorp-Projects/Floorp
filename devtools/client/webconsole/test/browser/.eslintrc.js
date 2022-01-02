"use strict";

module.exports = {
  overrides: [
    {
      files: ["test-dynamic-import.js", "test-error-worklet.js"],
      parserOptions: {
        sourceType: "module",
      },
    },
  ],
};
