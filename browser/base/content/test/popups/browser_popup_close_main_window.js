/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function muffleMainWindowType() {
  let oldWinType = document.documentElement.getAttribute("windowtype");
  // Check if we've already done this to allow calling multiple times:
  if (oldWinType != "navigator:testrunner") {
    // Make the main test window not count as a browser window any longer
    document.documentElement.setAttribute("windowtype", "navigator:testrunner");

    registerCleanupFunction(() => {
      document.documentElement.setAttribute("windowtype", oldWinType);
    });
  }
}

/**
 * Check that if we close the 1 remaining window, we treat it as quitting on
 * non-mac.
 *
 * Sets the window type for the main browser test window to something else to
 * avoid having to actually close the main browser window.
 */
add_task(async function closing_last_window_equals_quitting() {
  if (navigator.platform.startsWith("Mac")) {
    ok(true, "Not testing on mac");
    return;
  }
  muffleMainWindowType();

  let observed = 0;
  function obs() {
    observed++;
  }
  Services.obs.addObserver(obs, "browser-lastwindow-close-requested");
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let closedPromise = BrowserTestUtils.windowClosed(newWin);
  newWin.BrowserCommands.tryToCloseWindow();
  await closedPromise;
  is(observed, 1, "Got a notification for closing the normal window.");
  Services.obs.removeObserver(obs, "browser-lastwindow-close-requested");
});

/**
 * Check that if we close the 1 remaining window and also have a popup open,
 * we don't treat it as quitting.
 *
 * Sets the window type for the main browser test window to something else to
 * avoid having to actually close the main browser window.
 */
add_task(async function closing_last_window_equals_quitting() {
  if (navigator.platform.startsWith("Mac")) {
    ok(true, "Not testing on mac");
    return;
  }
  muffleMainWindowType();
  let observed = 0;
  function obs() {
    observed++;
  }
  Services.obs.addObserver(obs, "browser-lastwindow-close-requested");
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let popupPromise = BrowserTestUtils.waitForNewWindow("https://example.com/");
  SpecialPowers.spawn(newWin.gBrowser.selectedBrowser, [], function () {
    content.open("https://example.com/", "_blank", "height=500");
  });
  let popupWin = await popupPromise;
  let closedPromise = BrowserTestUtils.windowClosed(newWin);
  newWin.BrowserCommands.tryToCloseWindow();
  await closedPromise;
  is(observed, 0, "Got no notification for closing the normal window.");

  closedPromise = BrowserTestUtils.windowClosed(popupWin);
  popupWin.BrowserCommands.tryToCloseWindow();
  await closedPromise;
  is(
    observed,
    0,
    "Got no notification now that we're closing the last window, as it's a popup."
  );
  Services.obs.removeObserver(obs, "browser-lastwindow-close-requested");
});
