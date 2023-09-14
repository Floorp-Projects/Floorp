"use strict";

const example_base =
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.com/browser/browser/base/content/test/tabs/";

add_task(async function test_contextmenu_openlink_after_tabnavigated() {
  let url = example_base + "test_bug1358314.html";

  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  const contextMenu = document.getElementById("contentAreaContextMenu");
  let awaitPopupShown = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );
  await BrowserTestUtils.synthesizeMouse(
    "a",
    0,
    0,
    {
      type: "contextmenu",
      button: 2,
    },
    gBrowser.selectedBrowser
  );
  await awaitPopupShown;
  info("Popup Shown");

  info("Navigate the tab with the opened context menu");
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    "about:blank"
  );
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  let awaitNewTabOpen = BrowserTestUtils.waitForNewTab(
    gBrowser,
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://example.com/",
    true
  );

  info("Click the 'open link in new tab' menu item");
  let openLinkMenuItem = contextMenu.querySelector("#context-openlinkintab");
  contextMenu.activateItem(openLinkMenuItem);

  info("Wait for the new tab to be opened");
  const newTab = await awaitNewTabOpen;

  // Close the contextMenu popup if it has not been closed yet.
  contextMenu.hidePopup();

  is(
    newTab.linkedBrowser.currentURI.spec,
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://example.com/",
    "Got the expected URL loaded in the new tab"
  );

  BrowserTestUtils.removeTab(newTab);
  BrowserTestUtils.removeTab(tab);
});
