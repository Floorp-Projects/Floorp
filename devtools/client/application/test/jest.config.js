/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global __dirname */

module.exports = {
  verbose: true,
  moduleNameMapper: {
    // Custom name mappers for modules that require m-c specific API.
    "^../utils/l10n": `${__dirname}/fixtures/l10n`,
    "^devtools/client/shared/link": `${__dirname}/fixtures/stub`,
    "^devtools/shared/plural-form": `${__dirname}/fixtures/plural-form`,
    "^chrome": `${__dirname}/fixtures/Chrome`,
    "^Services": `${__dirname}/fixtures/Services`,
    // Map all require("devtools/...") to the real devtools root.
    "^devtools\\/(.*)": `${__dirname}/../../../$1`,
  },
  setupFiles: ["<rootDir>setup.js"],
  snapshotSerializers: ["enzyme-to-json/serializer"],
};
