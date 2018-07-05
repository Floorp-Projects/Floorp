/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PREF = "privacy.trackingprotection.enabled";
const TRACKING_PAGE = "http://tracking.example.org/browser/browser/base/content/test/trackingUI/trackingPage.html";

// TODO: replace this once bug 1428847 is done.
function hidden(el) {
  let win = el.ownerGlobal;
  let display = win.getComputedStyle(el).getPropertyValue("display", null);
  let opacity = win.getComputedStyle(el).getPropertyValue("opacity", null);
  return display === "none" || opacity === "0";
}

add_task(async function setup() {
  await UrlClassifierTestUtils.addTestTrackers();
});

// Tests that we show the reload hint if the user enables TP on
// a site that has already loaded trackers before and that pressing
// the reload button reloads the page.
add_task(async function testReloadHint() {
  Services.prefs.setBoolPref(PREF, false);

  await BrowserTestUtils.withNewTab(TRACKING_PAGE, async function() {
    let promisePanelOpen = BrowserTestUtils.waitForEvent(window.gIdentityHandler._identityPopup, "popupshown");
    window.gIdentityHandler._identityBox.click();
    await promisePanelOpen;

    let blockButton = document.getElementById("tracking-action-block");
    let reloadButton = document.getElementById("tracking-action-reload");
    let trackingLoaded = document.getElementById("tracking-loaded");
    let reloadHint = document.getElementById("tracking-reload-required");
    ok(!hidden(trackingLoaded), "The tracking loaded info is shown.");
    ok(hidden(blockButton), "The enable tracking protection button is not shown.");
    ok(hidden(reloadButton), "The reload button is not shown.");
    ok(hidden(reloadHint), "The reload hint is not shown.");

    Services.prefs.setBoolPref(PREF, true);

    ok(hidden(blockButton), "The enable tracking protection button is not shown.");
    ok(hidden(trackingLoaded), "The tracking loaded info is not shown.");
    ok(!hidden(reloadButton), "The reload button is shown.");
    ok(!hidden(reloadHint), "The reload hint is shown.");

    let reloaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, TRACKING_PAGE);
    reloadButton.click();
    await reloaded;
  });

  Services.prefs.clearUserPref(PREF);
});

// Tests that the reload hint does not appear on a non-tracking page.
add_task(async function testReloadHint() {
  Services.prefs.setBoolPref(PREF, false);

  await BrowserTestUtils.withNewTab("https://example.com", async function() {
    let promisePanelOpen = BrowserTestUtils.waitForEvent(window.gIdentityHandler._identityPopup, "popupshown");
    window.gIdentityHandler._identityBox.click();
    await promisePanelOpen;

    let trackingNotDetected = document.getElementById("tracking-not-detected");
    let reloadButton = document.getElementById("tracking-action-reload");
    let reloadHint = document.getElementById("tracking-reload-required");
    ok(!hidden(trackingNotDetected), "The tracking not detected info is shown.");
    ok(hidden(reloadButton), "The reload button is not shown.");
    ok(hidden(reloadHint), "The reload hint is not shown.");

    Services.prefs.setBoolPref(PREF, true);

    ok(!hidden(trackingNotDetected), "The tracking not detected info is shown.");
    ok(hidden(reloadButton), "The reload button is not shown.");
    ok(hidden(reloadHint), "The reload hint is not shown.");
  });

  Services.prefs.clearUserPref(PREF);
});

add_task(async function cleanup() {
  UrlClassifierTestUtils.cleanupTestTrackers();
});
