/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Don't switch tabs via the keyboard while the contextmenu is open.
 */
add_task(async function cant_tabswitch_mid_contextmenu() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/idontexist"
  );
  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.org/idontexist"
  );

  const contextMenu = document.getElementById("contentAreaContextMenu");
  let promisePopupShown = BrowserTestUtils.waitForPopupEvent(
    contextMenu,
    "shown"
  );
  await BrowserTestUtils.synthesizeMouse(
    "body",
    0,
    0,
    {
      type: "contextmenu",
      button: 2,
    },
    tab2.linkedBrowser
  );
  await promisePopupShown;
  EventUtils.synthesizeKey("VK_TAB", { accelKey: true });
  ok(tab2.selected, "tab2 should stay selected");
  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  let promisePopupHidden = BrowserTestUtils.waitForPopupEvent(
    contextMenu,
    "hidden"
  );
  contextMenu.hidePopup();
  await promisePopupHidden;
});
