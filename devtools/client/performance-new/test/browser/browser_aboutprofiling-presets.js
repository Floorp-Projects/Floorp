/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function test() {
  info("Test that about:profiling presets configure the profiler");

  if (!Services.profiler.GetFeatures().includes("stackwalk")) {
    ok(true, "This platform does not support stackwalking, skip this test.");
    return;
  }
  // This test assumes that the Web Developer preset is set by default, which is
  // not the case on Nightly and custom builds.
  BackgroundJSM.changePreset(
    "aboutprofiling",
    "web-developer",
    Services.profiler.GetFeatures()
  );

  await withAboutProfiling(async document => {
    const webdev = await getNearestInputFromText(document, "Web Developer");
    ok(webdev.checked, "By default the Web Developer preset is selected.");

    ok(
      !activeConfigurationHasFeature("stackwalk"),
      "Stackwalking is not enabled for the Web Developer workflow"
    );

    const graphics = await getNearestInputFromText(document, "Graphics");

    ok(!graphics.checked, "The Graphics preset is not checked.");
    graphics.click();
    ok(
      graphics.checked,
      "After clicking the input, the Graphics preset is now checked."
    );

    ok(
      activeConfigurationHasFeature("stackwalk"),
      "The graphics preset uses stackwalking."
    );

    const media = await getNearestInputFromText(document, "Media");

    ok(!media.checked, "The media preset is not checked.");
    media.click();
    ok(
      media.checked,
      "After clicking the input, the Media preset is now checked."
    );
  });
});
