/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

module.exports = {
  env: {
    worker: true,
  },

  overrides: [
    {
      files: ["head.js"],
      env: {
        worker: true,
      },
    },
  ],
};
