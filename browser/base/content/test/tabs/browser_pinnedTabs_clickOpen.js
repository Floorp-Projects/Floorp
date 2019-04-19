/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function index(tab) {
  return Array.prototype.indexOf.call(gBrowser.tabs, tab);
}

async function testNewTabPosition(expectedPosition, modifiers = {}) {
  let opening = BrowserTestUtils.waitForNewTab(gBrowser, "http://mochi.test:8888/");
  BrowserTestUtils.synthesizeMouseAtCenter("#link", modifiers, gBrowser.selectedBrowser);
  let newtab = await opening;
  is(index(newtab), expectedPosition, "clicked tab is in correct position");
  return newtab;
}

// Test that a tab opened from a pinned tab is not in the pinned region.
add_task(async function test_pinned_content_click() {
  let testUri = "data:text/html;charset=utf-8,<a href=\"http://mochi.test:8888/\" target=\"_blank\" id=\"link\">link</a>";
  let tabs = [
    gBrowser.selectedTab,
    await BrowserTestUtils.openNewForegroundTab(gBrowser, testUri),
    BrowserTestUtils.addTab(gBrowser),
  ];
  gBrowser.pinTab(tabs[1]);
  gBrowser.pinTab(tabs[2]);

  // First test new active tabs open at the start of non-pinned tabstrip.
  let newtab1 = await testNewTabPosition(2);
  // Switch back to our test tab.
  await BrowserTestUtils.switchTab(gBrowser, tabs[1]);
  let newtab2 = await testNewTabPosition(2);

  gBrowser.removeTab(newtab1);
  gBrowser.removeTab(newtab2);

  // Second test new background tabs open in order.
  let modifiers = AppConstants.platform == "macosx" ? {metaKey: true} : {ctrlKey: true};
  await BrowserTestUtils.switchTab(gBrowser, tabs[1]);

  newtab1 = await testNewTabPosition(2, modifiers);
  newtab2 = await testNewTabPosition(3, modifiers);

  gBrowser.removeTab(tabs[1]);
  gBrowser.removeTab(tabs[2]);
  gBrowser.removeTab(newtab1);
  gBrowser.removeTab(newtab2);
});
