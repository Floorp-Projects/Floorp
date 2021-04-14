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

let mockShareData = [
  {
    name: "Test",
    menuItemTitle: "Sharing Service Test",
    image:
      "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKE" +
      "lEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==",
  },
];

// Setup spies for observing function calls from MacSharingService
let shareUrlSpy = sinon.spy();
let openSharingPreferencesSpy = sinon.spy();
let getSharingProvidersSpy = sinon.spy();

let stub = sinon.stub(gBrowser, "MacSharingService").get(() => {
  return {
    getSharingProviders(url) {
      getSharingProvidersSpy(url);
      return mockShareData;
    },
    shareUrl(name, url, title) {
      shareUrlSpy(name, url, title);
    },
    openSharingPreferences() {
      openSharingPreferencesSpy();
    },
  };
});

registerCleanupFunction(async function() {
  stub.restore();
});

/**
 * Test the "Share" item menus in the tab contextmenu on MacOSX.
 */
add_task(async function test_contextmenu_share_macosx() {
  await BrowserTestUtils.withNewTab(TEST_URL, async () => {
    let contextMenu = await openTabContextMenu(gBrowser.selectedTab);
    await TestUtils.waitForCondition(() => {
      let itemCreated = document.getElementById("context_shareTabURL");
      return !!itemCreated;
    }, "Failed to create the Share item.");
    ok(true, "Got Share item");

    await openMenuPopup();
    ok(getSharingProvidersSpy.calledOnce, "getSharingProviders called");

    info(
      "Check we have a service and one extra menu item for the More... button"
    );
    let popup = document.getElementById("context_shareTabURL_popup");
    let items = popup.querySelectorAll("menuitem");
    is(items.length, 2, "There should be 2 sharing services.");

    info("Click on the sharing service");
    let menuPopupClosedPromised = BrowserTestUtils.waitForPopupEvent(
      contextMenu,
      "hidden"
    );
    let shareButton = items[0];
    is(
      shareButton.label,
      mockShareData[0].menuItemTitle,
      "Share button's label should match the service's menu item title. "
    );
    is(
      shareButton.getAttribute("share-name"),
      mockShareData[0].name,
      "Share button's share-name value should match the service's name. "
    );

    popup.activateItem(shareButton);
    await menuPopupClosedPromised;

    ok(shareUrlSpy.calledOnce, "shareUrl called");

    info("Check the correct data was shared.");
    let [name, url, title] = shareUrlSpy.getCall(0).args;
    is(name, mockShareData[0].name, "Shared correct service name");
    is(url, TEST_URL, "Shared correct URL");
    is(title, "Sharing URL", "Shared the correct title.");

    info("Test the More... button");
    contextMenu = await openTabContextMenu(gBrowser.selectedTab);
    await openMenuPopup();
    // Since the tab context menu was collapsed previously, the popup needs to get the
    // providers again.
    ok(getSharingProvidersSpy.calledTwice, "getSharingProviders called again");
    popup = document.getElementById("context_shareTabURL_popup");
    items = popup.querySelectorAll("menuitem");
    is(items.length, 2, "There should be 2 sharing services.");

    info("Click on the More Button");
    let moreButton = items[1];
    menuPopupClosedPromised = BrowserTestUtils.waitForPopupEvent(
      contextMenu,
      "hidden"
    );
    popup.activateItem(moreButton);
    await menuPopupClosedPromised;
    ok(openSharingPreferencesSpy.calledOnce, "openSharingPreferences called");
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
  return contextMenu;
}

async function openMenuPopup() {
  info("Opening Share menu popup.");
  let popup = document.getElementById("context_shareTabURL_popup");
  let shareItem = document.getElementById("context_shareTabURL");
  shareItem.openMenu(true);
  await BrowserTestUtils.waitForPopupEvent(popup, "shown");
}
