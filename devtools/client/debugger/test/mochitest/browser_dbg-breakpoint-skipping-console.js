/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/*
 * Test that debugger statements are skipped when
 * skip pausing is enabled.
 */

"use strict";

add_task(async function () {
  const toolbox = await initPane("doc-scripts.html", "webconsole", [
    ["devtools.debugger.skip-pausing", true],
  ]);
  await navigateTo(`${EXAMPLE_URL}doc-debugger-statements.html`);

  await hasConsoleMessage({ toolbox }, "done!");
  ok(true, "We reached the end");
});
