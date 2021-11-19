/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function test() {
  info(
    "Test the WebChannel mechanism that it changes the preset to firefox-platform"
  );
  await makeSureProfilerPopupIsDisabled();
  const supportedFeatures = Services.profiler.GetFeatures();

  // This test assumes that the Web Developer preset is set by default, which is
  // not the case on Nightly and custom builds.
  BackgroundJSM.changePreset(
    "aboutprofiling",
    "web-developer",
    supportedFeatures
  );

  await withAboutProfiling(async document => {
    const webdevPreset = document.querySelector("input[value=web-developer]");
    const firefoxPreset = await document.querySelector(
      "input[value=firefox-platform]"
    );

    // Check the presets now to make sure web-developer is selected right now.
    ok(webdevPreset.checked, "By default the Web Developer preset is checked.");
    ok(
      !firefoxPreset.checked,
      "By default the Firefox Platform preset is not checked."
    );

    // Enable the profiler menu button with web channel.
    await withWebChannelTestDocument(async browser => {
      await waitForTabTitle("WebChannel Page Ready");
      await waitForProfilerMenuButton();
      ok(true, "The profiler menu button was enabled by the WebChannel.");
    });

    // firefox-platform preset should be selected now.
    ok(
      !webdevPreset.checked,
      "Web Developer preset should not be checked anymore."
    );
    ok(
      firefoxPreset.checked,
      "Firefox Platform preset should now be checked after enabling the popup with web channel."
    );
  });
});
