/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_LINK = "https://example.com/";
const RESOURCE_LINK =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "test_contextmenu_iframe.html";

/* This test checks that a context menu can open up
 * a frame into it's own tab. */

add_task(async function test_open_iframe() {
  let testTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    RESOURCE_LINK
  );
  const selector = "#iframe";
  const openPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    TEST_LINK,
    false
  );
  const contextMenu = document.getElementById("contentAreaContextMenu");
  is(contextMenu.state, "closed", "checking if popup is closed");
  let awaitPopupShown = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    selector,
    {
      type: "contextmenu",
      button: 2,
      centered: true,
    },
    gBrowser.selectedBrowser
  );
  await awaitPopupShown;
  info("Popup Shown");
  const awaitPopupHidden = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );

  // Open frame submenu
  const frameItem = contextMenu.querySelector("#frame");
  const menuPopup = frameItem.menupopup;
  const menuPopupPromise = BrowserTestUtils.waitForEvent(
    menuPopup,
    "popupshown"
  );
  frameItem.openMenu(true);
  await menuPopupPromise;

  let domItem = contextMenu.querySelector("#context-openframeintab");
  info("Going to click item " + domItem.id);
  ok(
    BrowserTestUtils.is_visible(domItem),
    "DOM context menu item tab should be visible"
  );
  ok(!domItem.disabled, "DOM context menu item tab shouldn't be disabled");
  domItem.click();

  let openedTab = await openPromise;
  contextMenu.hidePopup();
  await awaitPopupHidden;
  await BrowserTestUtils.removeTab(openedTab);

  BrowserTestUtils.removeTab(testTab);
});
