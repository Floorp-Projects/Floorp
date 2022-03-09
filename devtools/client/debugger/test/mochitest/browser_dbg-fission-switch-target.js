/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test switching for the top-level target.

"use strict";

const PARENT_PROCESS_URI = "about:robots";

add_task(async function() {
  // Start the debugger on a parent process URL
  const dbg = await initDebuggerWithAbsoluteURL(
    PARENT_PROCESS_URI,
    "aboutRobots.js"
  );

  // Navigate to a content process URL and check that the sources tree updates
  await navigate(dbg, "doc-scripts.html", "simple1.js");
  info("Wait for all sources to be in the store");
  await waitFor(() => dbg.selectors.getSourceCount() == 5);
  is(dbg.selectors.getSourceCount(), 5, "5 sources are loaded.");

  // Check that you can still break after target switching.
  await selectSource(dbg, "simple1.js");
  await addBreakpoint(dbg, "simple1.js", 4);
  invokeInTab("main");
  await waitForPaused(dbg);
  await waitForLoadedSource(dbg, "simple1.js");

  await dbg.toolbox.closeToolbox();
});
