/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that URL-less sources have tabs and selecting that location does not
// create a new tab for the same URL-less source

"use strict";

add_task(async function() {
  const dbg = await initDebugger(
    "doc-scripts.html",
    "simple1.js",
    "simple2.js"
  );

  // Create a URL-less source
  invokeInTab("doEval");
  await waitForPaused(dbg);

  // Click a frame which shouldn't open a new source tab
  await clickElement(dbg, "frame", 2);

  // Click the frame to select the same location and ensure there's only 1 tab
  is(countTabs(dbg), 1);

  await resume(dbg);
});
