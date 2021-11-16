/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const FRONTEND_BASE_HOST = "http://example.com";
const FRONTEND_BASE_PATH =
  "/browser/devtools/client/performance-new/test/browser/fake-frontend.html";
const FRONTEND_BASE_URL = FRONTEND_BASE_HOST + FRONTEND_BASE_PATH;

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

  // First check for "firefox-platform" preset which will have no "view" query
  // string because this is where our traditional "full" view opens up.
  await openPopupAndAssertUrlForPreset({
    preset: "Firefox Platform",
    expectedUrl: FRONTEND_BASE_URL,
  });

  // Now, let's check for "web-developer" preset. This will open up the frontend
  // with "active-tab" view query string. Frontend will understand and open the active tab view for it.
  await openPopupAndAssertUrlForPreset({
    preset: "Web Developer",
    expectedUrl: FRONTEND_BASE_URL + "?view=active-tab&implementation=js",
  });
});

async function openPopupAndAssertUrlForPreset({ preset, expectedUrl }) {
  // Let's capture a profile and assert newly created tab's url.
  await openPopupAndEnsureCloses(window, async () => {
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
