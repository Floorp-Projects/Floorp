/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Run through a series of basic recording actions for the perf actor.
 */
add_task(async function () {
  const { front, client } = await initPerfFront();

  // Assert the initial state.
  is(
    await front.isSupportedPlatform(),
    true,
    "This test only runs on supported platforms."
  );
  is(await front.isActive(), false, "The profiler is not active yet.");

  // Getting the active Browser ID to assert in the "profiler-started" event.
  const win = Services.wm.getMostRecentWindow("navigator:browser");
  const activeTabID = win.gBrowser.selectedBrowser.browsingContext.browserId;

  front.once(
    "profiler-started",
    (entries, interval, features, duration, activeTID) => {
      is(entries, 1024, "Should apply entries by startProfiler");
      is(interval, 0.1, "Should apply interval by startProfiler");
      is(typeof features, "number", "Should apply features by startProfiler");
      is(duration, 2, "Should apply duration by startProfiler");
      is(
        activeTID,
        activeTabID,
        "Should apply active browser ID by startProfiler"
      );
    }
  );

  // Start the profiler.
  await front.startProfiler({
    entries: 1000,
    duration: 2,
    interval: 0.1,
    features: ["js", "stackwalk"],
  });

  is(await front.isActive(), true, "The profiler is active.");

  // clean up
  await front.stopProfilerAndDiscardProfile();
  await front.destroy();
  await client.close();
});
