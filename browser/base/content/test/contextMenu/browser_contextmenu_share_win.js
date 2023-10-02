/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
const BASE = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.com"
);
const TEST_URL = BASE + "browser_contextmenu_shareurl.html";

// Setup spies for observing function calls from MacSharingService
let shareUrlSpy = sinon.spy();

let stub = sinon.stub(gBrowser.ownerGlobal, "WindowsUIUtils").get(() => {
  return {
    shareUrl(url, title) {
      shareUrlSpy(url, title);
    },
  };
});

registerCleanupFunction(async function () {
  stub.restore();
});

/**
 * Test the "Share" item in the tab contextmenu on Windows.
 */
add_task(async function test_contextmenu_share_win() {
  await BrowserTestUtils.withNewTab(TEST_URL, async () => {
    await openTabContextMenu(gBrowser.selectedTab);

    let contextMenu = document.getElementById("tabContextMenu");
    let contextMenuClosedPromise = BrowserTestUtils.waitForPopupEvent(
      contextMenu,
      "hidden"
    );
    let itemCreated = contextMenu.querySelector(".share-tab-url-item");

    ok(itemCreated, "Got Share item on Windows 10");

    info("Test the correct URL is shared when Share is selected.");
    EventUtils.synthesizeMouseAtCenter(itemCreated, {});
    await contextMenuClosedPromise;

    ok(shareUrlSpy.calledOnce, "shareUrl called");
    let [url, title] = shareUrlSpy.getCall(0).args;
    is(url, TEST_URL, "Shared correct URL");
    is(title, "Sharing URL", "Shared correct URL");
  });
});

/**
 * Helper for opening the toolbar context menu.
 */
async function openTabContextMenu(tab) {
  info("Opening tab context menu");
  let contextMenu = document.getElementById("tabContextMenu");
  let openTabContextMenuPromise = BrowserTestUtils.waitForPopupEvent(
    contextMenu,
    "shown"
  );

  EventUtils.synthesizeMouseAtCenter(tab, { type: "contextmenu" });
  await openTabContextMenuPromise;
}
