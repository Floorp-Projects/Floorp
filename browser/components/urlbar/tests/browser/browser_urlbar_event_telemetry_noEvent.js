/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const tests = [
  async function (win) {
    info("Type something, click on search settings.");
    await BrowserTestUtils.withNewTab(
      { gBrowser: win.gBrowser, url: "about:blank" },
      async browser => {
        win.gURLBar.select();
        const promise = onSyncPaneLoaded();
        await UrlbarTestUtils.promiseAutocompleteResultPopup({
          window: win,
          value: "x",
          fireInputEvent: true,
        });
        UrlbarTestUtils.getOneOffSearchButtons(win).settingsButton.click();
        await promise;
      }
    );
    return null;
  },

  async function (win) {
    info("Type something, Up, Enter on search settings.");
    await BrowserTestUtils.withNewTab(
      { gBrowser: win.gBrowser, url: "about:blank" },
      async browser => {
        win.gURLBar.select();
        const promise = onSyncPaneLoaded();
        await UrlbarTestUtils.promiseAutocompleteResultPopup({
          window: win,
          value: "x",
          fireInputEvent: true,
        });
        EventUtils.synthesizeKey("KEY_ArrowUp", {}, win);
        Assert.ok(
          UrlbarTestUtils.getOneOffSearchButtons(
            win
          ).selectedButton.classList.contains("search-setting-button"),
          "Should have selected the settings button"
        );
        EventUtils.synthesizeKey("VK_RETURN", {}, win);
        await promise;
      }
    );
    return null;
  },
];

function onSyncPaneLoaded() {
  return new Promise(resolve => {
    Services.obs.addObserver(function panesLoadedObs() {
      Services.obs.removeObserver(panesLoadedObs, "sync-pane-loaded");
      resolve();
    }, "sync-pane-loaded");
  });
}

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.eventTelemetry.enabled", true]],
  });

  const win = await BrowserTestUtils.openNewBrowserWindow();

  // This is not necessary after each loop, because assertEvents does it.
  Services.telemetry.clearEvents();

  for (let i = 0; i < tests.length; i++) {
    info(`Running no event test at index ${i}`);
    await tests[i](win);
    // Always blur to ensure it's not accounted as an additional abandonment.
    win.gURLBar.blur();
    TelemetryTestUtils.assertEvents([], { category: "urlbar" });
  }

  await BrowserTestUtils.closeWindow(win);
});
