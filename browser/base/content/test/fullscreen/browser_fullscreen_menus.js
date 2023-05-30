/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_shortcut_key_label_in_fullscreen_menu_item() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["full-screen-api.transition-duration.enter", "0 0"],
      ["full-screen-api.transition-duration.leave", "0 0"],
    ],
  });

  const isMac = AppConstants.platform == "macosx";
  const shortCutKeyLabel = isMac ? "\u2303\u2318F" : "F11";
  const enterMenuItemId = isMac ? "enterFullScreenItem" : "fullScreenItem";
  const exitMenuItemId = isMac ? "exitFullScreenItem" : "fullScreenItem";
  const accelKeyLabelSelector = ".menu-accel-container > label";

  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.org/browser/browser/base/content/test/fullscreen/fullscreen.html"
  );

  await SimpleTest.promiseFocus(tab.linkedBrowser);

  document.getElementById(enterMenuItemId).render();
  Assert.equal(
    document
      .getElementById(enterMenuItemId)
      .querySelector(accelKeyLabelSelector)
      ?.getAttribute("value"),
    shortCutKeyLabel,
    `The menu item to enter into the fullscreen mode should show a shortcut key`
  );

  const fullScreenEntered = BrowserTestUtils.waitForEvent(window, "fullscreen");

  EventUtils.synthesizeKey("KEY_F11", {});

  info(`Waiting for entering the fullscreen mode...`);
  await fullScreenEntered;

  document.getElementById(exitMenuItemId).render();
  Assert.equal(
    document
      .getElementById(exitMenuItemId)
      .querySelector(accelKeyLabelSelector)
      ?.getAttribute("value"),
    shortCutKeyLabel,
    `The menu item to exiting from the fullscreen mode should show a shortcut key`
  );

  const fullScreenExited = BrowserTestUtils.waitForEvent(window, "fullscreen");

  EventUtils.synthesizeKey("KEY_F11", {});

  info(`Waiting for exiting from the fullscreen mode...`);
  await fullScreenExited;

  document.getElementById(enterMenuItemId).render();
  Assert.equal(
    document
      .getElementById(enterMenuItemId)
      .querySelector(accelKeyLabelSelector)
      ?.getAttribute("value"),
    shortCutKeyLabel,
    `After exiting from the fullscreen mode, the menu item to enter the fullscreen mode should show a shortcut key`
  );

  BrowserTestUtils.removeTab(tab);
});
