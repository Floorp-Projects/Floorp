/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function test() {
  info(
    "Test that about:profiling presets override the individual settings when changed."
  );
  const supportedFeatures = Services.profiler.GetFeatures();

  if (!supportedFeatures.includes("stackwalk")) {
    ok(true, "This platform does not support stackwalking, skip this test.");
    return;
  }
  // This test assumes that the Web Developer preset is set by default, which is
  // not the case on Nightly and custom builds.
  BackgroundJSM.changePreset(
    "aboutprofiling",
    "web-developer",
    supportedFeatures
  );

  await withAboutProfiling(async document => {
    const webdevPreset = await getNearestInputFromText(
      document,
      "Web Developer"
    );
    const customPreset = await getNearestInputFromText(document, "Custom");
    const stackwalkFeature = await getNearestInputFromText(
      document,
      "Native Stacks"
    );
    const geckoMainThread = await getNearestInputFromText(
      document,
      "GeckoMain"
    );

    {
      info("Check the defaults on the about:profiling page.");
      ok(
        webdevPreset.checked,
        "By default the Web Developer preset is checked."
      );
      ok(!customPreset.checked, "By default the custom preset is not checked.");
      ok(
        !stackwalkFeature.checked,
        "Stack walking is not enabled for Web Developer."
      );
      ok(
        !activeConfigurationHasFeature("stackwalk"),
        "Stack walking is not in the active configuration."
      );
      ok(
        geckoMainThread.checked,
        "The GeckoMain thread is tracked for the Web Developer preset"
      );
      ok(
        activeConfigurationHasThread("GeckoMain"),
        "The GeckoMain thread is in the active configuration."
      );
    }

    {
      info("Change some settings, which will move the preset over to Custom.");

      info("Click stack walking.");
      stackwalkFeature.click();

      info("Click the GeckoMain thread.");
      geckoMainThread.click();
    }

    {
      info("Check that the various settings were actually updated in the UI.");
      ok(
        !webdevPreset.checked,
        "The Web Developer preset is no longer enabled."
      );
      ok(customPreset.checked, "The Custom preset is now checked.");
      ok(stackwalkFeature.checked, "Stack walking was enabled");
      ok(
        activeConfigurationHasFeature("stackwalk"),
        "Stack walking is in the active configuration."
      );
      ok(
        !geckoMainThread.checked,
        "GeckoMain was removed from tracked threads."
      );
      ok(
        !activeConfigurationHasThread("GeckoMain"),
        "The GeckoMain thread is not in the active configuration."
      );
    }

    {
      info(
        "Click the Web Developer preset, which should revert the other settings."
      );
      webdevPreset.click();
    }

    {
      info(
        "Now verify that everything was reverted back to the original settings."
      );
      ok(webdevPreset.checked, "The Web Developer preset is checked again.");
      ok(!customPreset.checked, "The custom preset is not checked.");
      ok(
        !stackwalkFeature.checked,
        "Stack walking is reverted for the Web Developer preset."
      );
      ok(
        !activeConfigurationHasFeature("stackwalk"),
        "Stack walking is not in the active configuration."
      );
      ok(
        geckoMainThread.checked,
        "GeckoMain was added back to the tracked threads."
      );
      ok(
        activeConfigurationHasThread("GeckoMain"),
        "The GeckoMain thread is in the active configuration."
      );
    }
  });
});
