/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function test() {
  info(
    "Test that DevTools can capture profiles. This function also unit tests the " +
      "internal RecordingState of the client."
  );

  await setProfilerFrontendUrl(
    "http://example.com/browser/devtools/client/performance-new/test/browser/fake-frontend.html"
  );

  await withDevToolsPanel(async document => {
    const getRecordingState = setupGetRecordingState(document);

    // The initial state of the profiler UI is racy, as it calls out to the PerfFront
    // to get the status of the profiler. This can race with the initialization of
    // the test. Most of the the time the result is "not-yet-known", but rarely
    // the PerfFront will win this race. Allow for both outcomes of the race in this
    // test.
    ok(
      getRecordingState() === "not-yet-known" ||
        getRecordingState() === "available-to-record",
      "The component starts out in an unknown state or is already available to record."
    );

    const startRecording = await getActiveButtonFromText(
      document,
      "Start recording"
    );

    is(
      getRecordingState(),
      "available-to-record",
      "After talking to the actor, we're ready to record."
    );

    info("Click the button to start recording");
    startRecording.click();

    is(
      getRecordingState(),
      "request-to-start-recording",
      "Clicking the start recording button sends in a request to start recording."
    );

    const captureRecording = await getActiveButtonFromText(
      document,
      "Capture recording"
    );

    is(
      getRecordingState(),
      "recording",
      "Once the Capture recording button is available, the actor has started " +
        "its recording"
    );

    info("Click the button to capture the recording.");
    captureRecording.click();

    is(
      getRecordingState(),
      "request-to-get-profile-and-stop-profiler",
      "We have requested to stop the profiler."
    );

    await getActiveButtonFromText(document, "Start recording");
    is(
      getRecordingState(),
      "available-to-record",
      "The profiler is available to record again."
    );

    info(
      "If the DevTools successfully injects a profile into the page, then the " +
        "fake frontend will rename the title of the page."
    );

    await checkTabLoadedProfile({
      initialTitle: "Waiting on the profile",
      successTitle: "Profile received",
      errorTitle: "Error",
    });
  });
});
