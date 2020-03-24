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

  await withDevToolsPanel(async document => {
    {
      const presets = await getNearestInputFromText(document, "Settings");

      is(presets.value, "web-developer", "The presets default to webdev mode.");
      ok(
        !(await devToolsActiveConfigurationHasFeature(document, "stackwalk")),
        "Stack walking is not used in Web Developer mode."
      );
    }

    {
      const presets = await getNearestInputFromText(document, "Settings");
      setReactFriendlyInputValue(presets, "firefox-platform");
      is(
        presets.value,
        "firefox-platform",
        "The preset was changed to Firefox Platform"
      );
      ok(
        await devToolsActiveConfigurationHasFeature(document, "stackwalk"),
        "Stack walking is used in Firefox Platform mode."
      );
    }
  });
});
