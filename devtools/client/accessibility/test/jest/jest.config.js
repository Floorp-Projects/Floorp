/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global __dirname */

module.exports = {
  verbose: true,
  moduleNameMapper: {
    // Custom name mappers for modules that require m-c specific API.
    "^../utils/l10n": `${__dirname}/fixtures/l10n`,
    "^devtools/client/shared/link": `${__dirname}/fixtures/stub`,
    "^devtools/shared/flags": `${__dirname}/fixtures/stub`,
    "^devtools/shared/event-emitter": `${__dirname}/node_modules/devtools-modules/src/utils/event-emitter`,
    "^devtools/shared/layout/utils": `${__dirname}/fixtures/stub`,
    "^devtools/shared/plural-form": `${__dirname}/fixtures/plural-form`,
    "^devtools/client/shared/components/tree/TreeView": `${__dirname}/fixtures/stub`,
    "^Services": `${__dirname}/fixtures/Services`,
    // Map all require("devtools/...") to the real devtools root.
    "^devtools\\/(.*)": `${__dirname}/../../../../$1`,
  },
  setupFiles: ["<rootDir>setup.js"],
  snapshotSerializers: ["enzyme-to-json/serializer"],
};
