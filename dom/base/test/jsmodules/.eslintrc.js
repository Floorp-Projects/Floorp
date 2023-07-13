"use strict";

module.exports = {
  overrides: [
    {
      // eslint-plugin-html doesn't automatically detect module sections in
      // html files. Enable these as a module here. JavaScript files can use
      // the mjs extension.
      files: ["*.html"],
      parserOptions: {
        sourceType: "module",
      },
    },
  ],
};
