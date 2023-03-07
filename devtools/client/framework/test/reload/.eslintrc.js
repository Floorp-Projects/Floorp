/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

module.exports = {
  globals: {
    __dirname: true,
  },
  overrides: [
    {
      files: ["**/*.js"],
      rules: {
        "no-unused-vars": "off",
      },
    },
  ],
};
