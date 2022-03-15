"use strict";

module.exports = {
  extends: [
    "plugin:mozilla/browser-test",
    "plugin:mozilla/chrome-test",
    "plugin:mozilla/mochitest-test",
  ],
  overrides: [
    {
      files: ["file_module_js_cache.js", "file_script_module_*.js"],
      parserOptions: {
        sourceType: "module",
      },
    },
  ],
};
