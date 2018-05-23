/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Run through a series of basic recording actions for the perf actor.
 */
add_task(async function() {
  const {front, client} = await initPerfFront();

  // Assert the initial state.
  is(await front.isSupportedPlatform(), true,
    "This test only runs on supported platforms.");
  is(await front.isLockedForPrivateBrowsing(), false,
    "The browser is not in private browsing mode.");
  is(await front.isActive(), false,
    "The profiler is not active yet.");

  front.once("profiler-started", (entries, interval, features) => {
    is(entries, 1000, "Should apply entries by startProfiler");
    is(interval, 0.1, "Should apply interval by startProfiler");
    is(features, 0x202, "Should apply features by startProfiler");
  });

  // Start the profiler.
  await front.startProfiler({ entries: 1000, interval: 0.1,
                              features: ["js", "stackwalk"] });

  is(await front.isActive(), true, "The profiler is active.");

  // clean up
  await front.stopProfilerAndDiscardProfile();
  await front.destroy();
  await client.close();
});
