/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test() {
  let url = "about:robots";
  let tab0 = gBrowser.tabs[0];
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  gBrowser.discardBrowser(tab0.linkedBrowser);
  ok(!tab0.linkedPanel, "tab0 is suspended");

  await BrowserTestUtils.switchTab(gBrowser, tab0);

  // Test that active tab is not able to be suspended.
  gBrowser.discardBrowser(tab0.linkedBrowser);
  ok(tab0.linkedPanel, "active tab is not able to be suspended");

  // Test that tab that is closing is not able to be suspended.
  gBrowser._beginRemoveTab(tab1);
  gBrowser.discardBrowser(tab1.linkedBrowser);

  ok(tab1.linkedPanel, "cannot suspend a tab that is closing");

  gBrowser._endRemoveTab(tab1);

  // Test that tab with beforeunload handler is not able to be suspended.
  url = "http://example.com/browser/browser/components/sessionstore/test/browser_1284886_suspend_tab.html";

  tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await BrowserTestUtils.switchTab(gBrowser, tab0);

  gBrowser.discardBrowser(tab1.linkedBrowser);
  ok(tab1.linkedPanel, "cannot suspend a tab with beforeunload handler");

  await promiseRemoveTab(tab1);

  // Test that remote tab is not able to be suspended.
  url = "about:robots";
  tab1 = BrowserTestUtils.addTab(gBrowser, url, { forceNotRemote: true });
  await promiseBrowserLoaded(tab1.linkedBrowser, true, url);
  await BrowserTestUtils.switchTab(gBrowser, tab1);
  await BrowserTestUtils.switchTab(gBrowser, tab0);

  gBrowser.discardBrowser(tab1.linkedBrowser);
  ok(tab1.linkedPanel, "cannot suspend a remote tab");

  promiseRemoveTab(tab1);
});

