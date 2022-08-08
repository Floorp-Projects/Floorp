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
        const graphics = await getNearestInputFromText(
          aboutProfilingDocument,
          "Graphics"
        );
        const media = await getNearestInputFromText(
          aboutProfilingDocument,
          "Media"
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

        // Select "graphics" using the popup
        ok(!graphics.checked, "The Graphics preset is not checked.");

        presetsInPopup.menupopup.openPopup();
        presetsInPopup.menupopup.activateItem(
          await getElementByLabel(presetsInPopup, "Graphics")
        );

        await TestUtils.waitForCondition(
          () => !webdev.checked,
          "After selecting the preset in the popup, waiting until the Web Developer preset isn't selected anymore in the about:profiling interface."
        );
        await TestUtils.waitForCondition(
          () => graphics.checked,
          "After selecting the preset in the popup, waiting until the Graphics preset is checked in the about:profiling interface."
        );
        await TestUtils.waitForCondition(
          () => presetsInDevtools.value === "graphics",
          "After selecting the preset in the popup, waiting until the preset is changed to Graphics in the devtools panel too."
        );
        await TestUtils.waitForCondition(
          () => presetsInPopup.value === "graphics",
          "After selecting the preset in the popup, waiting until the preset is changed to Graphics in the popup."
        );

        // Select "firefox frontend" using the popup
        ok(!media.checked, "The Media preset is not checked.");

        presetsInPopup.menupopup.openPopup();
        presetsInPopup.menupopup.activateItem(
          await getElementByLabel(presetsInPopup, "Media")
        );

        await TestUtils.waitForCondition(
          () => !graphics.checked,
          "After selecting the preset in the popup, waiting until the Graphics preset is not checked anymore in the about:profiling interface."
        );
        await TestUtils.waitForCondition(
          () => media.checked,
          "After selecting the preset in the popup, waiting until the Media preset is checked in the about:profiling interface."
        );
        await TestUtils.waitForCondition(
          () => presetsInDevtools.value === "media",
          "After selecting the preset in the popup, waiting until the preset is changed to Firefox Front-end in the devtools panel."
        );
        await TestUtils.waitForCondition(
          () => presetsInPopup.value === "media",
          "After selecting the preset in the popup, waiting until the preset is changed to Media in the popup."
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
      const graphics = await getNearestInputFromText(
        aboutProfilingDocument,
        "Graphics"
      );
      const media = await getNearestInputFromText(
        aboutProfilingDocument,
        "Media"
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
      ok(!graphics.checked, "The Graphics preset is not checked.");
      graphics.click();
      ok(
        graphics.checked,
        "After clicking the input, the Graphics preset is now checked in about:profiling."
      );
      await TestUtils.waitForCondition(
        () => presetsInDevtools.value === "graphics",
        "The preset was changed to Graphics in the devtools panel too."
      );

      ok(!media.checked, "The Media preset is not checked.");
      media.click();
      ok(
        media.checked,
        "After clicking the input, the Media preset is now checked in about:profiling."
      );
      await TestUtils.waitForCondition(
        () => presetsInDevtools.value === "media",
        "The preset was changed to Media in the devtools panel too."
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
        Number(intervalInput.value) + 1
      );
      ok(
        custom.checked,
        "After changing the interval, the Custom preset is now checked in about:profiling."
      );
      await TestUtils.waitForCondition(
        () => presetsInDevtools.value === "custom",
        "The preset was changed to Custom in the devtools panel too."
      );

      ok(
        getDevtoolsCustomPresetContent(devtoolsDocument).includes(
          "Interval: 2 ms"
        ),
        "The new interval should be in the custom preset description"
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
      ok(
        getDevtoolsCustomPresetContent(devtoolsDocument).includes(
          "StyleThread"
        ),
        "The StyleThread thread should be listed in the custom preset description"
      );
      styleThreadInput.click();
      await TestUtils.waitForCondition(
        () => !threadsLine.textContent.includes("StyleThread"),
        "Waiting until the StyleThread disappears from the devtools panel."
      );
      ok(
        !getDevtoolsCustomPresetContent(devtoolsDocument).includes(
          "StyleThread"
        ),
        "The StyleThread thread should no longer be listed in the custom preset description"
      );

      info("Change the threads values using the input.");
      const threadInput = await getNearestInputFromText(
        aboutProfilingDocument,
        "Add custom threads by name"
      );

      function setThreadInputValue(newThreadValue) {
        // Actually set the new value.
        setReactFriendlyInputValue(threadInput, newThreadValue);
        // The settings are actually changed on the blur event.
        threadInput.dispatchEvent(new FocusEvent("blur"));
      }

      let newThreadValue = "GeckoMain,Foo";
      setThreadInputValue(newThreadValue);
      await TestUtils.waitForCondition(
        () => threadsLine.textContent.includes("Foo"),
        "Waiting for Foo to be displayed in the devtools panel."
      );

      // The code detecting changes to the thread list has a fast path
      // to detect that the list of threads has changed if the 2 lists
      // have different lengths. Exercise the slower code path by changing
      // the list of threads to a list with the same number of threads.
      info("Change the thread list again to a list of the same length");
      newThreadValue = "GeckoMain,Dummy";
      is(
        threadInput.value.split(",").length,
        newThreadValue.split(",").length,
        "The new value should have the same count of threads as the old value, please double check the test code."
      );
      setThreadInputValue(newThreadValue);
      checkDevtoolsCustomPresetContent(
        devtoolsDocument,
        `
          Interval: 2 ms
          Threads: GeckoMain, Dummy
          JavaScript
          Native Stacks
          CPU Utilization
          Audio Callback Tracing
          IPC Messages
          Process CPU Utilization
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
      const graphics = await getNearestInputFromText(
        aboutProfilingDocument,
        "Graphics"
      );
      const media = await getNearestInputFromText(
        aboutProfilingDocument,
        "Media"
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
        !graphics.checked,
        "The Graphics preset is not checked in about:profiling."
      );

      setReactFriendlyInputValue(presetsInDevtools, "graphics");
      await TestUtils.waitForCondition(
        () => graphics.checked,
        "After changing the preset in the devtools panel, the Graphics preset is now checked in about:profiling."
      );
      await TestUtils.waitForCondition(
        () => presetsInDevtools.value === "graphics",
        "The preset was changed to Graphics in the devtools panel too."
      );

      // Change another preset now
      ok(!media.checked, "The Media preset is not checked.");
      setReactFriendlyInputValue(presetsInDevtools, "media");
      await TestUtils.waitForCondition(
        () => media.checked,
        "After changing the preset in the devtools panel, the Media preset is now checked in about:profiling."
      );
      await TestUtils.waitForCondition(
        () => presetsInDevtools.value === "media",
        "The preset was changed to Media in the devtools panel too."
      );
    }
  );
});
