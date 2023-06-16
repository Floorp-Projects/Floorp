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

registerCleanupFunction(async function () {
  stub.restore();
});

/**
 * Test the "Share" item menus in the tab contextmenu on MacOSX.
 */
add_task(async function test_contextmenu_share_macosx() {
  await BrowserTestUtils.withNewTab(TEST_URL, async () => {
    let contextMenu = await openTabContextMenu(gBrowser.selectedTab);
    await BrowserTestUtils.waitForMutationCondition(
      contextMenu,
      { childList: true },
      () => contextMenu.querySelector(".share-tab-url-item")
    );
    ok(true, "Got Share item");

    await openMenuPopup(contextMenu);
    ok(getSharingProvidersSpy.calledOnce, "getSharingProviders called");

    info(
      "Check we have a service and one extra menu item for the More... button"
    );
    let popup = contextMenu.querySelector(".share-tab-url-item").menupopup;
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
    await openMenuPopup(contextMenu);
    // Since the tab context menu was collapsed previously, the popup needs to get the
    // providers again.
    ok(getSharingProvidersSpy.calledTwice, "getSharingProviders called again");
    popup = contextMenu.querySelector(".share-tab-url-item").menupopup;
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

async function openMenuPopup(contextMenu) {
  info("Opening Share menu popup.");
  let shareItem = contextMenu.querySelector(".share-tab-url-item");
  shareItem.openMenu(true);
  await BrowserTestUtils.waitForPopupEvent(shareItem.menupopup, "shown");
}
