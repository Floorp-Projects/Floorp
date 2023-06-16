/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that URL-less sources have tabs added to the UI, are named correctly
// and do not persist upon reload

"use strict";

add_task(async function () {
  const dbg = await initDebugger(
    "doc-scripts.html",
    "simple1.js",
    "simple2.js"
  );

  await selectSource(dbg, "simple1.js");
  is(countTabs(dbg), 1, "Only the `simple.js` source tab exists");

  invokeInTab("doEval");
  await waitForPaused(dbg);

  is(countTabs(dbg), 2, "The new eval tab is now added");

  info("Assert that the eval source the tab source name correctly");
  ok(
    /source\d+/g.test(getTabContent(dbg, 0)),
    "The tab name pattern is correct"
  );

  await resume(dbg);

  // Test reloading the debugger
  await reload(dbg, "simple1.js", "simple2.js");
  is(countTabs(dbg), 1, "The eval source tab is no longer available");
});

/*
 * Get the tab content for the specific tab
 *
 * @param {Number} index - index of the tab to get the source content
 */
function getTabContent(dbg, index) {
  const tabs = findElement(dbg, "sourceTabs").children;
  return tabs[index]?.innerText || "";
}
