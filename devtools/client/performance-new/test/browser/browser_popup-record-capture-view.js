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
    "Test that the profiler pop-up correctly opens the captured profile on the " +
      "correct frontend view by adding proper view query string"
  );

  // This test assumes that the Web Developer preset is set by default, which is
  // not the case on Nightly and custom builds.
  BackgroundJSM.changePreset(
    "aboutprofiling",
    "web-developer",
    Services.profiler.GetFeatures()
  );

  await setProfilerFrontendUrl(FRONTEND_BASE_HOST, FRONTEND_BASE_PATH);
  await makeSureProfilerPopupIsEnabled();

  // First check for the "Media" preset which will have no "view" query
  // string because it opens our traditional "full" view.
  await openPopupAndAssertUrlForPreset({
    window,
    preset: "Media",
    expectedUrl: FRONTEND_BASE_URL,
  });

  // Now, let's check for "web-developer" preset. This will open up the frontend
  // with "active-tab" view query string. Frontend will understand and open the active tab view for it.
  await openPopupAndAssertUrlForPreset({
    window,
    preset: "Web Developer",
    expectedUrl: FRONTEND_BASE_URL + "?view=active-tab&implementation=js",
  });
});

add_task(async function test_in_private_window() {
  info(
    "Test that the profiler pop-up correctly opens the captured profile on the " +
      "correct frontend view by adding proper view query string. This also tests " +
      "that a tab is opened on the non-private window even when the popup is used " +
      "in the private window."
  );

  // This test assumes that the Web Developer preset is set by default, which is
  // not the case on Nightly and custom builds.
  BackgroundJSM.changePreset(
    "aboutprofiling",
    "web-developer",
    Services.profiler.GetFeatures()
  );

  await setProfilerFrontendUrl(FRONTEND_BASE_HOST, FRONTEND_BASE_PATH);
  await makeSureProfilerPopupIsEnabled();

  info("Open a private window.");
  const privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  // First check for the "Media" preset which will have no "view" query
  // string because it opens our traditional "full" view.
  // Note that this utility will check for a new tab in the main non-private
  // window, which is exactly what we want here.
  await openPopupAndAssertUrlForPreset({
    window: privateWindow,
    preset: "Media",
    expectedUrl: FRONTEND_BASE_URL,
  });

  // Now, let's check for "web-developer" preset. This will open up the frontend
  // with "active-tab" view query string. Frontend will understand and open the active tab view for it.
  await openPopupAndAssertUrlForPreset({
    window: privateWindow,
    preset: "Web Developer",
    expectedUrl: FRONTEND_BASE_URL + "?view=active-tab&implementation=js",
  });

  await BrowserTestUtils.closeWindow(privateWindow);
});

async function openPopupAndAssertUrlForPreset({ window, preset, expectedUrl }) {
  // Let's capture a profile and assert newly created tab's url.
  await openPopupAndEnsureCloses(window, async () => {
    const { document } = window;
    {
      // Select the preset in the popup
      const presetsInPopup = document.getElementById(
        "PanelUI-profiler-presets"
      );
      presetsInPopup.menupopup.openPopup();
      presetsInPopup.menupopup.activateItem(
        await getElementByLabel(presetsInPopup, preset)
      );

      await TestUtils.waitForCondition(
        () => presetsInPopup.label === preset,
        `After selecting the preset in the popup, waiting until the preset is changed to ${preset} in the popup.`
      );
    }

    {
      const button = await getElementByLabel(document, "Start Recording");
      info("Click the button to start recording.");
      button.click();
    }

    {
      const button = await getElementByLabel(document, "Capture");
      info("Click the button to capture the recording.");
      button.click();
    }

    info(
      "If the profiler successfully captures a profile, it will create a new " +
        "tab with the proper view query string depending on the preset."
    );

    await waitForTabUrl({
      initialTitle: "Waiting on the profile",
      successTitle: "Profile received",
      errorTitle: "Error",
      expectedUrl,
    });
  });
}
