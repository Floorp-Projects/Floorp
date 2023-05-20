/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that permission popups asking for user approval still appear in RDM
const DUMMY_URL = "http://example.com/";
const TEST_URL = `${URL_ROOT}geolocation.html`;
const TEST_SURL = TEST_URL.replace("http://example.com", "https://example.com");

function waitForGeolocationPrompt(win, browser) {
  return new Promise(resolve => {
    win.PopupNotifications.panel.addEventListener(
      "popupshown",
      function popupShown() {
        const notification = win.PopupNotifications.getNotification(
          "geolocation",
          browser
        );
        if (notification) {
          win.PopupNotifications.panel.removeEventListener(
            "popupshown",
            popupShown
          );
          resolve();
        }
      }
    );
  });
}

addRDMTask(
  null,
  async function () {
    // we want to explicitly tests http and https, hence
    // disabling https-first mode for this test.
    await pushPref("dom.security.https_first", false);

    const tab = await addTab(DUMMY_URL);
    const browser = tab.linkedBrowser;
    const win = browser.ownerGlobal;

    let waitPromptPromise = waitForGeolocationPrompt(win, browser);

    // Checks if a geolocation permission doorhanger appears when openning a page
    // requesting geolocation
    await navigateTo(TEST_SURL);
    await waitPromptPromise;

    ok(true, "Permission doorhanger appeared without RDM enabled");

    // Lets switch back to the dummy website and enable RDM
    await navigateTo(DUMMY_URL);
    const { ui } = await openRDM(tab);
    await waitForDeviceAndViewportState(ui);

    const newBrowser = ui.getViewportBrowser();
    waitPromptPromise = waitForGeolocationPrompt(win, newBrowser);

    // Checks if the doorhanger appeared again when reloading the geolocation
    // page inside RDM
    await navigateTo(TEST_SURL);

    await waitPromptPromise;

    ok(true, "Permission doorhanger appeared inside RDM");

    await closeRDM(tab);
    await removeTab(tab);
  },
  { onlyPrefAndTask: true }
);
