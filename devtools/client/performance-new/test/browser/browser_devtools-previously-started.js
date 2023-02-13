/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function test() {
  info(
    "Test what happens if the profiler was previously started by another tool."
  );

  const { startProfiler } = ChromeUtils.import(
    "resource://devtools/client/performance-new/shared/background.jsm.js"
  );

  info("Start the profiler before DevTools is loaded.");
  startProfiler("aboutprofiling");

  await withDevToolsPanel(async document => {
    const getRecordingState = setupGetRecordingState(document);

    // The initial state of the profiler UI is racy, as it calls out to the PerfFront
    // to get the status of the profiler. This can race with the initialization of
    // the test. Most of the the time the result is "not-yet-known", but rarely
    // the PerfFront will win this race. Allow for both outcomes of the race in this
    // test.
    ok(
      getRecordingState() === "not-yet-known" ||
        getRecordingState() === "recording",
      "The component starts out in an unknown state or in a recording state."
    );

    const cancelRecording = await getActiveButtonFromText(
      document,
      "Cancel recording"
    );

    is(
      getRecordingState(),
      "recording",
      "The profiler is reflecting the recording state."
    );

    info("Click the button to cancel the recording");
    cancelRecording.click();

    is(
      getRecordingState(),
      "request-to-stop-profiler",
      "We can request to stop the profiler."
    );

    await getActiveButtonFromText(document, "Start recording");

    is(
      getRecordingState(),
      "available-to-record",
      "The profiler is now available to record."
    );
  });
});
