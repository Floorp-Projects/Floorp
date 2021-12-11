/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable max-nested-callbacks */
"use strict";

add_task(async function test_change_in_popup() {
  // This test assumes that the Web Developer preset is set by default, which is
  // not the case on Nightly and custom builds.
  BackgroundJSM.changePreset(
    "aboutprofiling",
    "web-developer",
    Services.profiler.GetFeatures()
  );

  info(
    "Test that changing settings in the popup changes settings in the devtools panel and about:profiling too."
  );

  const browserWindow = window;
  const browserDocument = document;

  await makeSureProfilerPopupIsEnabled();
  await withDevToolsPanel(
    "about:profiling",
    async (devtoolsDocument, aboutProfilingDocument) => {
      await withPopupOpen(browserWindow, async () => {
        const presetsInPopup = browserDocument.getElementById(
          "PanelUI-profiler-presets"
        );

        const presetsInDevtools = await getNearestInputFromText(
          devtoolsDocument,
          "Settings"
        );

        const webdev = await getNearestInputFromText(
          aboutProfilingDocument,
          "Web Developer"
        );
        const platform = await getNearestInputFromText(
          aboutProfilingDocument,
          "Firefox Platform"
        );
        const frontEnd = await getNearestInputFromText(
          aboutProfilingDocument,
          "Firefox Front-End"
        );

        // Default situation
        ok(
          webdev.checked,
          "By default the Web Developer preset is selected in the about:profiling interface."
        );
        is(
          presetsInDevtools.value,
          "web-developer",
          "The presets default to webdev mode in the devtools panel."
        );
        is(
          presetsInPopup.value,
          "web-developer",
          "The presets default to webdev mode in the popup."
        );

        // Select "firefox platform" using the popup
        ok(!platform.checked, "The Firefox Platform preset is not checked.");

        presetsInPopup.menupopup.openPopup();
        presetsInPopup.menupopup.activateItem(
          await getElementByLabel(presetsInPopup, "Firefox Platform")
        );

        await TestUtils.waitForCondition(
          () => !webdev.checked,
          "After selecting the preset in the popup, waiting until the Web Developer preset isn't selected anymore in the about:profiling interface."
        );
        await TestUtils.waitForCondition(
          () => platform.checked,
          "After selecting the preset in the popup, waiting until the Firefox Platform preset is checked in the about:profiling interface."
        );
        await TestUtils.waitForCondition(
          () => presetsInDevtools.value === "firefox-platform",
          "After selecting the preset in the popup, waiting until the preset is changed to Firefox Platform in the devtools panel too."
        );
        await TestUtils.waitForCondition(
          () => presetsInPopup.value === "firefox-platform",
          "After selecting the preset in the popup, waiting until the preset is changed to Firefox Platform in the popup."
        );

        // Select "firefox frontend" using the popup
        ok(!frontEnd.checked, "The Firefox front-end preset is not checked.");

        presetsInPopup.menupopup.openPopup();
        presetsInPopup.menupopup.activateItem(
          await getElementByLabel(presetsInPopup, "Firefox Front-End")
        );

        await TestUtils.waitForCondition(
          () => !platform.checked,
          "After selecting the preset in the popup, waiting until the Firefox Platform preset is not checked anymore in the about:profiling interface."
        );
        await TestUtils.waitForCondition(
          () => frontEnd.checked,
          "After selecting the preset in the popup, waiting until the Firefox Front-End preset is checked in the about:profiling interface."
        );
        await TestUtils.waitForCondition(
          () => presetsInDevtools.value === "firefox-front-end",
          "After selecting the preset in the popup, waiting until the preset is changed to Firefox Front-end in the devtools panel."
        );
        await TestUtils.waitForCondition(
          () => presetsInPopup.value === "firefox-front-end",
          "After selecting the preset in the popup, waiting until the preset is changed to Firefox Front-End in the popup."
        );
      });
    }
  );
});

// In the following tests we don't look at changes in the popup. Indeed because
// the popup rerenders each time it's open, we don't need to mirror it.
add_task(async function test_change_in_about_profiling() {
  // This test assumes that the Web Developer preset is set by default, which is
  // not the case on Nightly and custom builds, or after previous tests.
  BackgroundJSM.changePreset(
    "aboutprofiling",
    "web-developer",
    Services.profiler.GetFeatures()
  );

  info(
    "Test that changing settings in about:profiling changes settings in the devtools panel too."
  );

  await withDevToolsPanel(
    "about:profiling",
    async (devtoolsDocument, aboutProfilingDocument) => {
      const presetsInDevtools = await getNearestInputFromText(
        devtoolsDocument,
        "Settings"
      );

      const webdev = await getNearestInputFromText(
        aboutProfilingDocument,
        "Web Developer"
      );
      const platform = await getNearestInputFromText(
        aboutProfilingDocument,
        "Firefox Platform"
      );
      const frontEnd = await getNearestInputFromText(
        aboutProfilingDocument,
        "Firefox Front-End"
      );
      const custom = await getNearestInputFromText(
        aboutProfilingDocument,
        "Custom"
      );

      // Default values
      ok(
        webdev.checked,
        "By default the Web Developer preset is selected in the about:profiling interface."
      );
      is(
        presetsInDevtools.value,
        "web-developer",
        "The presets default to webdev mode in the devtools panel."
      );

      // Change the preset in about:profiling, check it changes also in the
      // devtools panel.
      ok(!platform.checked, "The Firefox Platform preset is not checked.");
      platform.click();
      ok(
        platform.checked,
        "After clicking the input, the Firefox Platform preset is now checked in about:profiling."
      );
      await TestUtils.waitForCondition(
        () => presetsInDevtools.value === "firefox-platform",
        "The preset was changed to Firefox Platform in the devtools panel too."
      );

      ok(!frontEnd.checked, "The Firefox front-end preset is not checked.");
      frontEnd.click();
      ok(
        frontEnd.checked,
        "After clicking the input, the Firefox front-end preset is now checked in about:profiling."
      );
      await TestUtils.waitForCondition(
        () => presetsInDevtools.value === "firefox-front-end",
        "The preset was changed to Firefox Front-End in the devtools panel too."
      );

      // Now let's try to change some configuration!
      info(
        "Increase the interval by an arbitrary amount. The input range will " +
          "scale that to the final value presented to the profiler."
      );
      const intervalInput = await getNearestInputFromText(
        aboutProfilingDocument,
        "Sampling interval:"
      );
      setReactFriendlyInputValue(
        intervalInput,
        Number(intervalInput.value) + 8
      );
      ok(
        custom.checked,
        "After changing the interval, the Custom preset is now checked in about:profiling."
      );
      await TestUtils.waitForCondition(
        () => presetsInDevtools.value === "custom",
        "The preset was changed to Custom in the devtools panel too."
      );

      checkDevtoolsCustomPresetContent(
        devtoolsDocument,
        `
          Interval: 2 ms
          Threads: GeckoMain, Compositor, Renderer, DOM Worker
          Screenshots
          JavaScript
          Native Leaf Stack
          Native Stacks
          CPU Utilization
        `
      );

      // Let's check some thread as well
      info("Change the threads values using the checkboxes");

      const styleThreadInput = await getNearestInputFromText(
        aboutProfilingDocument,
        "StyleThread"
      );
      ok(
        !styleThreadInput.checked,
        "The StyleThread thread isn't checked by default."
      );

      info("Click the StyleThread checkbox.");
      styleThreadInput.click();

      // For some reason, it's not possible to directly match "StyleThread".
      const threadsLine = (
        await getElementFromDocumentByText(devtoolsDocument, "Threads")
      ).parentElement;
      await TestUtils.waitForCondition(
        () => threadsLine.textContent.includes("StyleThread"),
        "Waiting that StyleThread is displayed in the devtools panel."
      );
      checkDevtoolsCustomPresetContent(
        devtoolsDocument,
        `
          Interval: 2 ms
          Threads: GeckoMain, Compositor, Renderer, DOM Worker, StyleThread
          Screenshots
          JavaScript
          Native Leaf Stack
          Native Stacks
          CPU Utilization
        `
      );
      styleThreadInput.click();
      await TestUtils.waitForCondition(
        () => !threadsLine.textContent.includes("StyleThread"),
        "Waiting until the StyleThread disappears from the devtools panel."
      );
      checkDevtoolsCustomPresetContent(
        devtoolsDocument,
        `
          Interval: 2 ms
          Threads: GeckoMain, Compositor, Renderer, DOM Worker
          Screenshots
          JavaScript
          Native Leaf Stack
          Native Stacks
          CPU Utilization
        `
      );

      info("Change the threads values using the input.");
      const threadInput = await getNearestInputFromText(
        aboutProfilingDocument,
        "Add custom threads by name"
      );

      // Take care to use the same number of threads as we have already, so
      // that we fully test the equality function.
      const oldThreadValue = threadInput.value;
      const newThreadValue = "GeckoMain,Compositor,StyleThread,DOM Worker";
      is(
        oldThreadValue.split(",").length,
        newThreadValue.split(",").length,
        "The new value should have the same count of threads as the old value, please double check the test code."
      );

      // Actually set the new value.
      setReactFriendlyInputValue(threadInput, newThreadValue);
      // The settings are actually changed on the blur event.
      threadInput.dispatchEvent(new FocusEvent("blur"));

      await TestUtils.waitForCondition(
        () => threadsLine.textContent.includes("StyleThread"),
        "Waiting that StyleThread is displayed in the devtools panel."
      );
      checkDevtoolsCustomPresetContent(
        devtoolsDocument,
        `
          Interval: 2 ms
          Threads: GeckoMain, Compositor, StyleThread, DOM Worker
          Screenshots
          JavaScript
          Native Leaf Stack
          Native Stacks
          CPU Utilization
        `
      );
    }
  );
});

add_task(async function test_change_in_devtools_panel() {
  // This test assumes that the Web Developer preset is set by default, which is
  // not the case on Nightly and custom builds, or after previous tests.
  BackgroundJSM.changePreset(
    "aboutprofiling",
    "web-developer",
    Services.profiler.GetFeatures()
  );

  info(
    "Test that changing settings in the devtools panel changes settings in about:profiling too."
  );

  await withDevToolsPanel(
    "about:profiling",
    async (devtoolsDocument, aboutProfilingDocument) => {
      const presetsInDevtools = await getNearestInputFromText(
        devtoolsDocument,
        "Settings"
      );

      const webdev = await getNearestInputFromText(
        aboutProfilingDocument,
        "Web Developer"
      );
      const platform = await getNearestInputFromText(
        aboutProfilingDocument,
        "Firefox Platform"
      );
      const frontEnd = await getNearestInputFromText(
        aboutProfilingDocument,
        "Firefox Front-End"
      );

      // Default values
      ok(
        webdev.checked,
        "By default the Web Developer preset is selected in the about:profiling interface."
      );
      is(
        presetsInDevtools.value,
        "web-developer",
        "The presets default to webdev mode in the devtools panel."
      );

      // Change the preset in devtools panel, check it changes also in
      // about:profiling.
      ok(
        !platform.checked,
        "The Firefox Platform preset is not checked in about:profiling."
      );

      setReactFriendlyInputValue(presetsInDevtools, "firefox-platform");
      await TestUtils.waitForCondition(
        () => platform.checked,
        "After changing the preset in the devtools panel, the Firefox Platform preset is now checked in about:profiling."
      );
      await TestUtils.waitForCondition(
        () => presetsInDevtools.value === "firefox-platform",
        "The preset was changed to Firefox Platform in the devtools panel too."
      );

      // Change another preset now
      ok(!frontEnd.checked, "The Firefox front-end preset is not checked.");
      setReactFriendlyInputValue(presetsInDevtools, "firefox-front-end");
      await TestUtils.waitForCondition(
        () => frontEnd.checked,
        "After changing the preset in the devtools panel, the Firefox front-end preset is now checked in about:profiling."
      );
      await TestUtils.waitForCondition(
        () => presetsInDevtools.value === "firefox-front-end",
        "The preset was changed to Firefox Front-End in the devtools panel too."
      );
    }
  );
});
