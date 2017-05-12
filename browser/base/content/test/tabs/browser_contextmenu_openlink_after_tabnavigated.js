"use strict";

const example_base = "http://example.com/browser/browser/base/content/test/tabs/";

add_task(async function test_contextmenu_openlink_after_tabnavigated() {
  let url = example_base + "test_bug1358314.html";

  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  const contextMenu = document.getElementById("contentAreaContextMenu");
  let awaitPopupShown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  await BrowserTestUtils.synthesizeMouse("a", 0, 0, {
    type: "contextmenu",
    button: 2,
  }, gBrowser.selectedBrowser);
  await awaitPopupShown;
  info("Popup Shown");

  info("Navigate the tab with the opened context menu");
  gBrowser.selectedBrowser.loadURI("about:blank");
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  let awaitNewTabOpen = BrowserTestUtils.waitForNewTab(gBrowser, "http://example.com/");

  info("Click the 'open link in new tab' menu item");
  let openLinkMenuItem = contextMenu.querySelector("#context-openlinkintab");
  openLinkMenuItem.click();

  info("Wait for the new tab to be opened");
  const newTab = await awaitNewTabOpen;

  // Close the contextMenu popup if it has not been closed yet.
  contextMenu.hidePopup();

  await BrowserTestUtils.browserLoaded(newTab.linkedBrowser);
  const newTabURL = await ContentTask.spawn(newTab.linkedBrowser, null, async function() {
    return content.location.href;
  });
  is(newTabURL, "http://example.com/", "Got the expected URL loaded in the new tab");

  await BrowserTestUtils.removeTab(newTab);
  await BrowserTestUtils.removeTab(tab);
});
