/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let listService;

let url =
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
  Tests the strip-on-share feature for in-content links
 */

// Tests that the link url is properly stripped
add_task(async function testStrip() {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.query_stripping.strip_on_share.enabled", true]],
  });
  let strippedURI = "https://www.example.com/?otherParam=1234";
  await BrowserTestUtils.withNewTab(url, async function (browser) {
    // Prepare a link
    await SpecialPowers.spawn(browser, [], async function () {
      let link = content.document.createElement("a");
      link.href = "https://www.example.com/?stripParam=1234&otherParam=1234";
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
    Assert.ok(
      BrowserTestUtils.is_visible(stripOnShare),
      "Menu item is visible"
    );

    // Make sure the stripped link will be copied to the clipboard
    await SimpleTest.promiseClipboardChange(strippedURI, () => {
      contextMenu.activateItem(stripOnShare);
    });
    await awaitPopupHidden;
  });
});

// Tests that the menu item does not show if the pref is disabled
add_task(async function testPrefDisabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.query_stripping.strip_on_share.enabled", false]],
  });
  await BrowserTestUtils.withNewTab(url, async function (browser) {
    // Prepare a link
    await SpecialPowers.spawn(browser, [], async function () {
      let link = content.document.createElement("a");
      link.href = "https://www.example.com/?stripParam=1234&otherParam=1234";
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
    Assert.ok(
      !BrowserTestUtils.is_visible(stripOnShare),
      "Menu item is not visible"
    );
    contextMenu.hidePopup();
    await awaitPopupHidden;
  });
});

// Tests that the menu item does not show if there is nothing to strip
add_task(async function testUnknownQueryParam() {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.query_stripping.strip_on_share.enabled", true]],
  });
  await BrowserTestUtils.withNewTab(url, async function (browser) {
    // Prepare a link
    await SpecialPowers.spawn(browser, [], async function () {
      let link = content.document.createElement("a");
      link.href = "https://www.example.com/?otherParam=1234";
      link.textContent = "link with unknown query param";
      link.id = "link";
      content.document.body.appendChild(link);
    });
    let contextMenu = document.getElementById("contentAreaContextMenu");
    // open the context menu
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
    Assert.ok(
      !BrowserTestUtils.is_visible(stripOnShare),
      "Menu item is not visible"
    );
    contextMenu.hidePopup();
    await awaitPopupHidden;
  });
});
