/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function dummyPageURL(domain, query = "") {
  return (
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      `https://${domain}`
    ) +
    "dummy_page.html" +
    query
  );
}

async function openContextMenu(browser) {
  let contextMenu = document.getElementById("contentAreaContextMenu");
  let awaitPopupShown = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );
  await BrowserTestUtils.synthesizeMouse(
    "body",
    1,
    1,
    {
      type: "contextmenu",
      button: 2,
    },
    browser
  );
  await awaitPopupShown;
}

async function closeContextMenu() {
  let contextMenu = document.getElementById("contentAreaContextMenu");
  let awaitPopupHidden = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );
  contextMenu.hidePopup();
  await awaitPopupHidden;
}

async function testWithDomain(domain) {
  // Passing a query to make sure the next load is never a same-document
  // navigation.
  let dummy = dummyPageURL("example.org", "?start");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, dummy);
  let browser = tab.linkedBrowser;

  let sel = await SpecialPowers.spawn(browser, [], function () {
    let sel = content.getSelection();
    sel.removeAllRanges();
    sel.selectAllChildren(content.document.body);
    return sel.toString();
  });

  await openContextMenu(browser);

  let copyItem = document.getElementById("context-copy");
  ok(!copyItem.disabled, "Copy item should be enabled if text is selected.");

  await closeContextMenu();

  let loaded = BrowserTestUtils.browserLoaded(browser, false, null, true);
  BrowserTestUtils.startLoadingURIString(browser, dummyPageURL(domain));
  await loaded;
  loaded = BrowserTestUtils.waitForLocationChange(gBrowser, dummy);
  browser.goBack();
  await loaded;

  let sel2 = await SpecialPowers.spawn(browser, [], function () {
    return content.getSelection().toString();
  });
  is(sel, sel2, "Selection should remain when coming out of BFCache.");

  await openContextMenu(browser);

  ok(!copyItem.disabled, "Copy item should be enabled if text is selected.");

  await closeContextMenu();

  await BrowserTestUtils.removeTab(tab);
}

add_task(async function testValidSameOrigin() {
  await testWithDomain("example.org");
});

add_task(async function testValidCrossOrigin() {
  await testWithDomain("example.com");
});

add_task(async function testInvalid() {
  await testWithDomain("this.is.invalid");
});
