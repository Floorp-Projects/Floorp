/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let listService;

// Tests for the strip on share functionality of the urlbar.

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.query_stripping.strip_list", "stripParam"],
      ["privacy.query_stripping.enabled", false],
    ],
  });

  // Get the list service so we can wait for it to be fully initialized before running tests.
  listService = Cc["@mozilla.org/query-stripping-list-service;1"].getService(
    Ci.nsIURLQueryStrippingListService
  );

  await listService.testWaitForInit();
});

// Menu item should be visible, the whole url is copied without a selection, url should be stripped.
add_task(async function testQueryParamIsStripped() {
  await testMenuItemEnabled(false);
});

// Menu item should be visible, selecting the whole url, url should be stripped.
add_task(async function testQueryParamIsStrippedSelectURL() {
  await testMenuItemEnabled(true);
});

// We cannot strip anything, menu item should be hidden
add_task(async function testUnknownQueryParam() {
  await testMenuItemDisabled(
    "https://www.example.com/?noStripParam=1234",
    true,
    false
  );
});

// Selection is not a valid URI, menu item should be hidden
add_task(async function testInvalidURI() {
  await testMenuItemDisabled(
    "https://www.example.com/?stripParam=1234",
    true,
    true
  );
});

// Pref is not enabled, menu item should be hidden
add_task(async function testPrefDisabled() {
  await testMenuItemDisabled(
    "https://www.example.com/?stripParam=1234",
    false,
    false
  );
});

/**
 * Opens a new tab, opens the ulr bar context menu and checks that the strip-on-share menu item is not visible
 *
 * @param {string} url - The url to be loaded
 * @param {boolean} prefEnabled - Whether privacy.query_stripping.strip_on_share.enabled should be enabled for the test
 * @param {boolean} selection - True: The whole url will be selected, false: Only part of the url will be selected
 */
async function testMenuItemDisabled(url, prefEnabled, selection) {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.query_stripping.strip_on_share.enabled", prefEnabled]],
  });
  await BrowserTestUtils.withNewTab(url, async function (browser) {
    gURLBar.focus();
    if (selection) {
      //select only part of the url
      gURLBar.selectionStart = url.indexOf("example");
      gURLBar.selectionEnd = url.indexOf("4");
    }
    let menuitem = await promiseContextualMenuitem("strip-on-share");
    Assert.ok(
      !BrowserTestUtils.is_visible(menuitem),
      "Menu item is not visible"
    );
    let hidePromise = BrowserTestUtils.waitForEvent(
      menuitem.parentElement,
      "popuphidden"
    );
    menuitem.parentElement.hidePopup();
    await hidePromise;
  });
}

/**
 * Opens a new tab, opens the url bar context menu and checks that the strip-on-share menu item is visible.
 * Checks that the stripped version of the url is copied to the clipboard.
 *
 * @param {boolean} selectWholeUrl - Whether the whole url should be explicitely selected
 */
async function testMenuItemEnabled(selectWholeUrl) {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.query_stripping.strip_on_share.enabled", true]],
  });
  let validUrl = "https://www.example.com/?stripParam=1234";
  let strippedUrl = "https://www.example.com/";
  await BrowserTestUtils.withNewTab(validUrl, async function (browser) {
    gURLBar.focus();
    if (selectWholeUrl) {
      //select the whole url
      gURLBar.select();
    }
    let menuitem = await promiseContextualMenuitem("strip-on-share");
    Assert.ok(BrowserTestUtils.is_visible(menuitem), "Menu item is visible");
    let hidePromise = BrowserTestUtils.waitForEvent(
      menuitem.parentElement,
      "popuphidden"
    );
    // Make sure the clean copy of the link will be copied to the clipboard
    await SimpleTest.promiseClipboardChange(strippedUrl, () => {
      menuitem.closest("menupopup").activateItem(menuitem);
    });
    await hidePromise;
  });
}
