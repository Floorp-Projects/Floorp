"use strict";

module.exports = {
  overrides: [
    {
      files: ["file_module_loaded.js", "file_module_loaded2.js"],
      parserOptions: {
        sourceType: "module",
      },
    },
  ],
};
