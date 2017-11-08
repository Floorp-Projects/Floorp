/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Run through a series of basic recording actions for the perf actor.
 */
add_task(async function () {
  const {front, client} = await initPerfFront();

  // Assert the initial state.
  is(await front.isSupportedPlatform(), true,
    "This test only runs on supported platforms.");
  is(await front.isLockedForPrivateBrowsing(), false,
    "The browser is not in private browsing mode.");
  is(await front.isActive(), false,
    "The profiler is not active yet.");

  // Start the profiler.
  const profilerStarted = once(front, "profiler-started");
  await front.startProfiler();
  await profilerStarted;
  is(await front.isActive(), true, "The profiler was started.");

  // Stop the profiler and assert the results.
  const profilerStopped1 = once(front, "profiler-stopped");
  const profile = await front.getProfileAndStopProfiler();
  await profilerStopped1;
  is(await front.isActive(), false, "The profiler was stopped.");
  ok("threads" in profile, "The actor was used to record a profile.");

  // Restart the profiler.
  await front.startProfiler();
  is(await front.isActive(), true, "The profiler was re-started.");

  // Stop and discard.
  const profilerStopped2 = once(front, "profiler-stopped");
  await front.stopProfilerAndDiscardProfile();
  await profilerStopped2;
  is(await front.isActive(), false,
    "The profiler was stopped and the profile discarded.");

  // Clean up.
  await front.destroy();
  await client.close();
});
