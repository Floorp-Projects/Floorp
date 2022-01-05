/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from ../../../../components/downloads/test/browser/head.js */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/downloads/test/browser/head.js",
  this
);

add_task(
  async function test_opening_panel_and_closing_should_not_leave_vsync() {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.download.autohideButton", false]],
    });
    await promiseButtonShown("downloads-button");

    const downloadsButton = document.getElementById("downloads-button");
    const shownPromise = promisePanelOpened();
    EventUtils.synthesizeNativeMouseEvent({
      type: "click",
      target: downloadsButton,
      atCenter: true,
    });
    await shownPromise;

    is(DownloadsPanel.panel.state, "open", "Check that panel state is 'open'");

    await TestUtils.waitForCondition(
      () => !ChromeUtils.vsyncEnabled(),
      "Make sure vsync disabled"
    );
    // Should not already be using vsync
    ok(!ChromeUtils.vsyncEnabled(), "vsync should be off initially");

    EventUtils.synthesizeNativeMouseEvent({
      type: "click",
      target: downloadsButton,
      atCenter: true,
    });

    await TestUtils.waitForCondition(() => !ChromeUtils.vsyncEnabled());
    ok(!ChromeUtils.vsyncEnabled(), "vsync should still be off");
    is(
      DownloadsPanel.panel.state,
      "closed",
      "Check that panel state is 'closed'"
    );
  }
);
