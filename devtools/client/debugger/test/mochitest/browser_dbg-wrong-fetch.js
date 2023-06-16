/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that incorrect source contents are not shown if the server refetches
// different HTML when attaching to an open page.

"use strict";

add_task(async function () {
  await addTab(`${EXAMPLE_URL}different_html.sjs`);

  // This goop is here to manually clear the HTTP cache because setting response
  // headers in different_html.sjs to not cache the response doesn't work.
  Services.cache2.clear();

  const toolbox = await openToolboxForTab(gBrowser.selectedTab, "jsdebugger");
  const dbg = createDebuggerContext(toolbox);
  await selectSource(dbg, "different_html.sjs");
  await waitForLoadedSource(dbg, "different_html.sjs");
  const contents = findSourceContent(dbg, "different_html.sjs");

  ok(
    contents.value.includes("Incorrect contents fetched"),
    "Error message is shown"
  );
});
