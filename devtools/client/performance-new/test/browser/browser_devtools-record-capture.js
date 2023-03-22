/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const FRONTEND_BASE_HOST = "http://example.com";
const FRONTEND_BASE_PATH =
  "/browser/devtools/client/performance-new/test/browser/fake-frontend.html";
const FRONTEND_BASE_URL = FRONTEND_BASE_HOST + FRONTEND_BASE_PATH;

add_setup(async function setup() {
  // The active tab view isn't enabled in all configurations. Let's make sure
  // it's enabled in these tests.
  SpecialPowers.pushPrefEnv({
    set: [["devtools.performance.recording.active-tab-view.enabled", true]],
  });
});

add_task(async function test() {
  info(
    "Test that DevTools can capture profiles. This function also unit tests the " +
      "internal RecordingState of the client."
  );

  // This test assumes that the Web Developer preset is set by default, which is
  // not the case on Nightly and custom builds.
  BackgroundJSM.changePreset(
    "aboutprofiling",
    "web-developer",
    Services.profiler.GetFeatures()
  );

  await setProfilerFrontendUrl(FRONTEND_BASE_HOST, FRONTEND_BASE_PATH);

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

    // First check for "firefox-platform" preset which will have no "view" query
    // string because this is where our traditional "full" view opens up.
    await setPresetCaptureAndAssertUrl({
      document,
      preset: "firefox-platform",
      expectedUrl: FRONTEND_BASE_URL,
      getRecordingState,
    });

    // Now, let's check for "web-developer" preset. This will open up the frontend
    // with "active-tab" view query string. Frontend will understand and open the active tab view for it.
    await setPresetCaptureAndAssertUrl({
      document,
      preset: "web-developer",
      expectedUrl: FRONTEND_BASE_URL + "?view=active-tab&implementation=js",
      getRecordingState,
    });
  });
});

add_task(async function test_in_private_window() {
  info("Test that DevTools can capture profiles in a private window.");

  // This test assumes that the Web Developer preset is set by default, which is
  // not the case on Nightly and custom builds.
  BackgroundJSM.changePreset(
    "aboutprofiling",
    "web-developer",
    Services.profiler.GetFeatures()
  );

  await setProfilerFrontendUrl(FRONTEND_BASE_HOST, FRONTEND_BASE_PATH);

  info("Open a private window.");
  const privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

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

    // First check for "firefox-platform" preset which will have no "view" query
    // string because this is where our traditional "full" view opens up.
    // Note that this utility will check for a new tab in the main non-private
    // window, which is exactly what we want here.
    await setPresetCaptureAndAssertUrl({
      document,
      preset: "firefox-platform",
      expectedUrl: FRONTEND_BASE_URL,
      getRecordingState,
    });

    // Now, let's check for "web-developer" preset. This will open up the frontend
    // with "active-tab" view query string. Frontend will understand and open the active tab view for it.
    await setPresetCaptureAndAssertUrl({
      document,
      preset: "web-developer",
      expectedUrl: FRONTEND_BASE_URL + "?view=active-tab&implementation=js",
      getRecordingState,
    });
  }, privateWindow);

  await BrowserTestUtils.closeWindow(privateWindow);
});

async function setPresetCaptureAndAssertUrl({
  document,
  preset,
  expectedUrl,
  getRecordingState,
}) {
  const presetsInDevtools = await getNearestInputFromText(document, "Settings");
  setReactFriendlyInputValue(presetsInDevtools, preset);

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

  is(
    document.defaultView.gToolbox.isHighlighted("performance"),
    false,
    "The Performance panel in not highlighted yet."
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

  is(
    document.defaultView.gToolbox.isHighlighted("performance"),
    true,
    "The Performance Panel in the Devtools Tab is highlighted when the profiler " +
      "is recording"
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

  is(
    document.defaultView.gToolbox.isHighlighted("performance"),
    false,
    "The Performance panel in not highlighted anymore when the profiler is stopped"
  );

  info(
    "If the DevTools successfully injects a profile into the page, then the " +
      "fake frontend will rename the title of the page."
  );

  await waitForTabUrl({
    initialTitle: "Waiting on the profile",
    successTitle: "Profile received",
    errorTitle: "Error",
    expectedUrl,
  });
}
