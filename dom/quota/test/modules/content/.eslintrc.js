/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

module.exports = {
  overrides: [
    {
      files: [
        "Assert.js",
        "ModuleLoader.js",
        "StorageUtils.js",
        "WorkerDriver.js",
      ],
      parserOptions: {
        sourceType: "module",
      },
    },
  ],
};
