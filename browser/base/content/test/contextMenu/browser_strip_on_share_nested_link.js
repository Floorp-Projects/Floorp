/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let listService;

const TEST_URL =
  "https://example.com/browser/browser/base/content/test/general/dummy_page.html";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.query_stripping.strip_list", "stripParam"]],
  });

  // Get the list service so we can wait for it to be fully initialized before running tests.
  listService = Cc["@mozilla.org/query-stripping-list-service;1"].getService(
    Ci.nsIURLQueryStrippingListService
  );

  await listService.testWaitForInit();
});

/*
Tests the strip-on-share feature for in-content links with nested urls
*/

// Testing nested stripping with global params
add_task(async function testNestedStrippingGlobalParam() {
  let validUrl =
    "https://www.example.com/?test=https%3A%2F%2Fwww.example.net%2F%3Futm_ad%3D1234";
  let shortenedUrl =
    "https://www.example.com/?test=https%3A%2F%2Fwww.example.net%2F";
  await testStripOnShare({
    originalURI: validUrl,
    strippedURI: shortenedUrl,
  });
});

// Testing nested stripping with site specific params
add_task(async function testNestedStrippingSiteSpecific() {
  let validUrl =
    "https://www.example.com/?test=https%3A%2F%2Fwww.example.net%2F%3Ftest_3%3D1234";
  let shortenedUrl =
    "https://www.example.com/?test=https%3A%2F%2Fwww.example.net%2F";
  await testStripOnShare({
    originalURI: validUrl,
    strippedURI: shortenedUrl,
  });
});

// Testing nested stripping with incorrect site specific params
add_task(async function testNoStrippedNestedParam() {
  let validUrl =
    "https://www.example.com/?test=https%3A%2F%2Fwww.example.com%2F%3Ftest_3%3D1234";
  let shortenedUrl =
    "https://www.example.com/?test=https%3A%2F%2Fwww.example.com%2F%3Ftest_3%3D1234";
  await testStripOnShare({
    originalURI: validUrl,
    strippedURI: shortenedUrl,
  });
});

// Testing order of stripping for nested stripping
add_task(async function testOrderOfStripping() {
  let validUrl =
    "https://www.example.com/?test_1=https%3A%2F%2Fwww.example.net%2F%3Ftest_3%3D1234";
  let shortenedUrl = "https://www.example.com/";
  await testStripOnShare({
    originalURI: validUrl,
    strippedURI: shortenedUrl,
  });
});

// Testing correct scoping of site specific params for nested stripping
add_task(async function testMultipleQueryParamsWithNestedStripping() {
  let validUrl =
    "https://www.example.com/?test_3=1234&test=https%3A%2F%2Fwww.example.net%2F%3Ftest_3%3D1234";
  let shortenedUrl =
    "https://www.example.com/?test_3=1234&test=https%3A%2F%2Fwww.example.net%2F";
  await testStripOnShare({
    originalURI: validUrl,
    strippedURI: shortenedUrl,
  });
});

/**
 * Opens a new tab, opens the context menu and checks that the strip-on-share menu item is visible.
 * Checks that the stripped version of the url is copied to the clipboard.
 *
 * @param {string} originalURI - The orginal url before the stripping occurs
 * @param {string} strippedURI - The expected url after stripping occurs
 */
async function testStripOnShare({ originalURI, strippedURI }) {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.query_stripping.strip_on_share.enabled", true],
      ["privacy.query_stripping.strip_on_share.enableTestMode", true],
    ],
  });

  let testJson = {
    global: {
      queryParams: ["utm_ad"],
      topLevelSites: ["*"],
    },
    example: {
      queryParams: ["test_2", "test_1"],
      topLevelSites: ["www.example.com"],
    },
    exampleNet: {
      queryParams: ["test_3", "test_4"],
      topLevelSites: ["www.example.net"],
    },
  };

  await listService.testSetList(testJson);

  await BrowserTestUtils.withNewTab(TEST_URL, async function (browser) {
    // Prepare a link
    await SpecialPowers.spawn(browser, [originalURI], function (startingURI) {
      let link = content.document.createElement("a");
      link.href = startingURI;
      link.textContent = "link with query param";
      link.id = "link";
      content.document.body.appendChild(link);
    });
    let contextMenu = document.getElementById("contentAreaContextMenu");
    // Open the context menu
    let awaitPopupShown = BrowserTestUtils.waitForEvent(
      contextMenu,
      "popupshown"
    );
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#link",
      { type: "contextmenu", button: 2 },
      browser
    );
    await awaitPopupShown;
    let awaitPopupHidden = BrowserTestUtils.waitForEvent(
      contextMenu,
      "popuphidden"
    );
    let stripOnShare = contextMenu.querySelector("#context-stripOnShareLink");
    Assert.ok(BrowserTestUtils.isVisible(stripOnShare), "Menu item is visible");
    // Make sure the stripped link will be copied to the clipboard
    await SimpleTest.promiseClipboardChange(strippedURI, () => {
      contextMenu.activateItem(stripOnShare);
    });
    await awaitPopupHidden;
  });
}
