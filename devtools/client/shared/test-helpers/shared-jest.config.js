/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global __dirname */
const fixturesDir = `${__dirname}/jest-fixtures`;

module.exports = {
  verbose: true,
  moduleNameMapper: {
    // Custom name mappers for modules that require m-c specific API.
    "^devtools/shared/generate-uuid": `${fixturesDir}/generate-uuid`,
    "^devtools/shared/DevToolsUtils": `${fixturesDir}/devtools-utils`,
    // This is needed for the Debugger, for some reason
    "shared/DevToolsUtils$": `${fixturesDir}/devtools-utils`,

    // Mocks only used by node tests.
    "Services-mock": `${fixturesDir}/Services`,
    "ChromeUtils-mock": `${fixturesDir}/ChromeUtils`,

    "^promise": `${fixturesDir}/promise`,
    "^chrome": `${fixturesDir}/Chrome`,
    "^devtools/client/shared/fluent-l10n/fluent-l10n$": `${fixturesDir}/fluent-l10n`,
    "^devtools/client/shared/unicode-url": `${fixturesDir}/unicode-url`,
    // This is needed for the Debugger, for some reason
    "client/shared/unicode-url$": `${fixturesDir}/unicode-url`,
    "^devtools/client/shared/telemetry": `${fixturesDir}/telemetry`,
    // This is needed for the Debugger, for some reason
    "client/shared/telemetry$": `${fixturesDir}/telemetry`,
    "devtools/shared/plural-form$": `${fixturesDir}/plural-form`,
    // Sometimes returning an empty object is enough
    "^devtools/client/shared/link": `${fixturesDir}/empty-module`,
    "^devtools/shared/flags": `${fixturesDir}/empty-module`,
    "^devtools/shared/layout/utils": `${fixturesDir}/empty-module`,
    "^devtools/client/shared/components/tree/TreeView": `${fixturesDir}/empty-module`,
    // Map all require("devtools/...") to the real devtools root.
    "^devtools\\/(.*)": `${__dirname}/../../../$1`,
  },
};
