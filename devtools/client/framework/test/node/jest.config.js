/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global __dirname */

module.exports = {
  verbose: true,
  moduleNameMapper: {
    // Custom name mappers for modules that require m-c specific API.
    "^chrome": `${__dirname}/fixtures/Chrome`,
    "^Services": `${__dirname}/fixtures/Services`,
    "^devtools/shared/DevToolsUtils": `${__dirname}/fixtures/devtools-utils`,
    "^devtools/shared/generate-uuid": `${__dirname}/fixtures/generate-uuid`,
    // Map all require("devtools/...") to the real devtools root.
    "^devtools\\/(.*)": `${__dirname}/../../../../$1`,
  },
  setupFiles: ["<rootDir>setup.js"],
};
