/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");
const BASE = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);
const TEST_URL = BASE + "browser_contextmenu_shareurl.html";

// Setup.
add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.proton.enabled", true]],
  });
});

// Setup spies for observing function calls from MacSharingService
let shareUrlSpy = sinon.spy();

let stub = sinon.stub(gBrowser.ownerGlobal, "WindowsUIUtils").get(() => {
  return {
    shareUrl(url, title) {
      shareUrlSpy(url, title);
    },
  };
});

registerCleanupFunction(async function() {
  stub.restore();
});

/**
 * Test the "Share" item in the tab contextmenu on Windows.
 */
add_task(async function test_contextmenu_share_win() {
  if (!AppConstants.isPlatformAndVersionAtLeast("win", "6.4")) {
    Assert.ok(true, "We only expose share on windows 10 and above");
    return;
  }

  await BrowserTestUtils.withNewTab(TEST_URL, async () => {
    await openTabContextMenu(gBrowser.selectedTab);

    await TestUtils.waitForCondition(() => {
      let itemCreated = document.querySelector("#context_shareTabURL");
      return !!itemCreated;
    }, "Failed to create the Share item.");
    ok(true, "Got Share item");

    info("Test the correct URL is shared when Share is selected.");
    let shareItem = document.querySelector("#context_shareTabURL");
    let popup = document.getElementById("tabContextMenu");
    let contextMenuClosedPromise = BrowserTestUtils.waitForPopupEvent(
      popup,
      "hidden"
    );

    EventUtils.synthesizeMouseAtCenter(shareItem, {});
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
