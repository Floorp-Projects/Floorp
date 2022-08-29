/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check we can open the tab manager using the keyboard.
 * Note that navigation to buttons in the toolbar is covered
 * by other tests.
 */
add_task(async function test_open_tabmanager_keyboard() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.tabmanager.enabled", true]],
  });
  let newWindow = await BrowserTestUtils.openNewWindowWithFlushedCacheForMozSupports();
  let elem = newWindow.document.getElementById("alltabs-button");

  // Borrowed from forceFocus() in the keyboard directory head.js
  elem.setAttribute("tabindex", "-1");
  elem.focus();
  elem.removeAttribute("tabindex");

  let focused = BrowserTestUtils.waitForEvent(newWindow, "focus", true);
  EventUtils.synthesizeKey(" ", {}, newWindow);
  let event = await focused;
  ok(
    event.originalTarget.closest("#allTabsMenu-allTabsView"),
    "Focus inside all tabs menu after toolbar button pressed"
  );
  let hidden = BrowserTestUtils.waitForEvent(
    event.target.closest("panel"),
    "popuphidden"
  );
  EventUtils.synthesizeKey("KEY_Escape", { shiftKey: false }, newWindow);
  await hidden;
  await BrowserTestUtils.closeWindow(newWindow);
});
