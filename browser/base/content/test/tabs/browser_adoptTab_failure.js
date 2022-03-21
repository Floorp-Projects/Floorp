/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// 'adoptTab' aborts when swapBrowsersAndCloseOther returns false.
// That's usually a bug, but this function forces it to happen in order to check
// that callers will behave as good as possible when it happens accidentally.
function makeAdoptTabFailOnceFor(gBrowser, tab) {
  const original = gBrowser.swapBrowsersAndCloseOther;
  gBrowser.swapBrowsersAndCloseOther = function(aOurTab, aOtherTab) {
    if (tab !== aOtherTab) {
      return original.call(gBrowser, aOurTab, aOtherTab);
    }
    gBrowser.swapBrowsersAndCloseOther = original;
    return false;
  };
}

add_task(async function test_adoptTab() {
  const tab = await addTab();
  const win2 = await BrowserTestUtils.openNewBrowserWindow();
  const gBrowser2 = win2.gBrowser;

  makeAdoptTabFailOnceFor(gBrowser2, tab);
  is(gBrowser2.adoptTab(tab), null, "adoptTab returns null in case of failure");
  ok(gBrowser2.adoptTab(tab), "adoptTab returns new tab in case of success");

  await BrowserTestUtils.closeWindow(win2);
});

add_task(async function test_replaceTabsWithWindow() {
  const nonAdoptableTab = await addTab("data:text/plain,nonAdoptableTab");
  const auxiliaryTab = await addTab("data:text/plain,auxiliaryTab");
  const selectedTab = await addTab("data:text/plain,selectedTab");
  gBrowser.selectedTabs = [selectedTab, nonAdoptableTab, auxiliaryTab];

  const windowOpenedPromise = BrowserTestUtils.waitForNewWindow();
  const win2 = gBrowser.replaceTabsWithWindow(selectedTab);
  await BrowserTestUtils.waitForEvent(win2, "DOMContentLoaded");
  const gBrowser2 = win2.gBrowser;
  makeAdoptTabFailOnceFor(gBrowser2, nonAdoptableTab);
  await windowOpenedPromise;

  // nonAdoptableTab couldn't be adopted, but the new window should have adopted
  // the other 2 tabs, and they should be in the proper order.
  is(gBrowser2.tabs.length, 2);
  is(gBrowser2.tabs[0].label, "data:text/plain,auxiliaryTab");
  is(gBrowser2.tabs[1].label, "data:text/plain,selectedTab");

  gBrowser.removeTab(nonAdoptableTab);
  await BrowserTestUtils.closeWindow(win2);
});
