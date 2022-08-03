"use strict";

module.exports = {
  overrides: [
    {
      files: ["file_module_js_cache.js", "file_script_module_*.js"],
      parserOptions: {
        sourceType: "module",
      },
    },
  ],
};
