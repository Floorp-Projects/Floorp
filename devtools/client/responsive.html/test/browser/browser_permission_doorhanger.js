/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that permission popups asking for user approval still appear in RDM
const DUMMY_URL = "http://example.com/";
const TEST_URL = `${URL_ROOT}geolocation.html`;
const TEST_SURL = TEST_URL.replace("http://example.com", "https://example.com");

function waitForGeolocationPrompt(win, browser) {
  return new Promise(resolve => {
    win.PopupNotifications.panel.addEventListener("popupshown", function popupShown() {
      let notification = win.PopupNotifications.getNotification("geolocation", browser);
      if (notification) {
        win.PopupNotifications.panel.removeEventListener("popupshown", popupShown);
        resolve();
      }
    });
  });
}

add_task(async function () {
  let tab = await addTab(DUMMY_URL);
  let browser = tab.linkedBrowser;
  let win = browser.ownerGlobal;

  let waitPromptPromise = waitForGeolocationPrompt(win, browser);

  // Checks if a geolocation permission doorhanger appears when openning a page
  // requesting geolocation
  await load(browser, TEST_SURL);
  await waitPromptPromise;

  ok(true, "Permission doorhanger appeared without RDM enabled");

  // Lets switch back to the dummy website and enable RDM
  await load(browser, DUMMY_URL);
  let { ui } = await openRDM(tab);
  let newBrowser = ui.getViewportBrowser();

  waitPromptPromise = waitForGeolocationPrompt(win, newBrowser);

  // Checks if the doorhanger appeared again when reloading the geolocation
  // page inside RDM
  await load(browser, TEST_SURL);
  await waitPromptPromise;

  ok(true, "Permission doorhanger appeared inside RDM");

  await closeRDM(tab);
  await removeTab(tab);
});
