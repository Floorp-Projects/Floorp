/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * This runs all integration tests against a test using sources maps
 * whose generated files (bundles) are compressed.
 * i.e. bundles are made of a unique line with all sources compressed into one line.
 */

"use strict";

requestLongerTimeout(10);

add_task(async function () {
  await pushPref("devtools.debugger.map-scopes-enabled", true);
  const testFolder = "sourcemaps-reload-compressed";
  const isCompressed = true;

  await runAllIntegrationTests(testFolder, {
    isCompressed,
  });
});
