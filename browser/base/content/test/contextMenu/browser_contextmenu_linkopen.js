/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_LINK = "https://example.com/";
const RESOURCE_LINK =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "test_contextmenu_links.html";

async function activateContextAndWaitFor(selector, where) {
  info("Starting test for " + where);
  let contextMenuItem = "openlink";
  let openPromise;
  let closeMethod;
  switch (where) {
    case "tab":
      contextMenuItem += "intab";
      openPromise = BrowserTestUtils.waitForNewTab(gBrowser, TEST_LINK, false);
      closeMethod = async tab => BrowserTestUtils.removeTab(tab);
      break;
    case "privatewindow":
      contextMenuItem += "private";
      openPromise = BrowserTestUtils.waitForNewWindow({ url: TEST_LINK }).then(
        win => {
          ok(
            PrivateBrowsingUtils.isWindowPrivate(win),
            "Should have opened a private window."
          );
          return win;
        }
      );
      closeMethod = async win => BrowserTestUtils.closeWindow(win);
      break;
    case "window":
      // No contextMenuItem suffix for normal new windows;
      openPromise = BrowserTestUtils.waitForNewWindow({ url: TEST_LINK }).then(
        win => {
          ok(
            !PrivateBrowsingUtils.isWindowPrivate(win),
            "Should have opened a normal window."
          );
          return win;
        }
      );
      closeMethod = async win => BrowserTestUtils.closeWindow(win);
      break;
  }
  let contextMenu = document.getElementById("contentAreaContextMenu");
  is(contextMenu.state, "closed", "checking if popup is closed");
  let awaitPopupShown = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );
  await BrowserTestUtils.synthesizeMouse(
    selector,
    0,
    0,
    {
      type: "contextmenu",
      button: 2,
      centered: true,
    },
    gBrowser.selectedBrowser
  );
  await awaitPopupShown;
  info("Popup Shown");
  let awaitPopupHidden = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );
  let domItem = contextMenu.querySelector("#context-" + contextMenuItem);
  info("Going to click item " + domItem.id);
  ok(
    BrowserTestUtils.is_visible(domItem),
    "DOM context menu item " + where + " should be visible"
  );
  ok(
    !domItem.disabled,
    "DOM context menu item " + where + " shouldn't be disabled"
  );
  domItem.click();
  contextMenu.hidePopup();
  await awaitPopupHidden;

  info("Waiting for the link to open");
  let openedThing = await openPromise;
  info("Waiting for the opened window/tab to close");
  await closeMethod(openedThing);
}

add_task(async function test_select_text_link() {
  let testTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    RESOURCE_LINK
  );
  for (let elementID of [
    "test-link",
    "test-image-link",
    "svg-with-link",
    "svg-with-relative-link",
  ]) {
    for (let where of ["tab", "window", "privatewindow"]) {
      await activateContextAndWaitFor("#" + elementID, where);
    }
  }
  BrowserTestUtils.removeTab(testTab);
});
