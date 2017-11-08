/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Test what happens when other tools control the profiler.
 */
add_task(async function () {
  const {front, client} = await initPerfFront();

  // Simulate other tools by getting an independent handle on the Gecko Profiler.
  const geckoProfiler = Cc["@mozilla.org/tools/profiler;1"].getService(Ci.nsIProfiler);

  is(await front.isActive(), false, "The profiler hasn't been started yet.");

  // Start the profiler.
  await front.startProfiler();
  is(await front.isActive(), true, "The profiler was started.");

  // Stop the profiler manually through the Gecko Profiler interface.
  const profilerStopped = once(front, "profiler-stopped");
  geckoProfiler.StopProfiler();
  await profilerStopped;
  is(await front.isActive(), false, "The profiler was stopped by another tool.");

  // Clean up.
  await front.destroy();
  await client.close();
});
