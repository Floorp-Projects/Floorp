/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// 'adoptTab' aborts when swapBrowsersAndCloseOther returns false.
// That's usually a bug, but this function forces it to happen in order to check
// that callers will behave as good as possible when it happens accidentally.
function makeAdoptTabFailOnceFor(gBrowser, tab) {
  const original = gBrowser.swapBrowsersAndCloseOther;
  gBrowser.swapBrowsersAndCloseOther = function (aOurTab, aOtherTab) {
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

add_task(async function test_on_drop() {
  const nonAdoptableTab = await addTab("data:text/html,<title>nonAdoptableTab");
  const auxiliaryTab = await addTab("data:text/html,<title>auxiliaryTab");
  const selectedTab = await addTab("data:text/html,<title>selectedTab");
  gBrowser.selectedTabs = [selectedTab, nonAdoptableTab, auxiliaryTab];

  const win2 = await BrowserTestUtils.openNewBrowserWindow();
  const gBrowser2 = win2.gBrowser;
  makeAdoptTabFailOnceFor(gBrowser2, nonAdoptableTab);
  const initialTab = gBrowser2.tabs[0];
  await dragAndDrop(selectedTab, initialTab, false, win2, false);

  // nonAdoptableTab couldn't be adopted, but the new window should have adopted
  // the other 2 tabs, and they should be in the right position.
  is(gBrowser2.tabs.length, 3, "There are 3 tabs");
  is(gBrowser2.tabs[0].label, "auxiliaryTab", "auxiliaryTab became tab 0");
  is(gBrowser2.tabs[1].label, "selectedTab", "selectedTab became tab 1");
  is(gBrowser2.tabs[2], initialTab, "initialTab became tab 2");
  is(gBrowser2.selectedTab, gBrowser2.tabs[1], "Tab 1 is selected");
  is(gBrowser2.multiSelectedTabsCount, 2, "Three multiselected tabs");
  ok(gBrowser2.tabs[0].multiselected, "Tab 0 is multiselected");
  ok(gBrowser2.tabs[1].multiselected, "Tab 1 is multiselected");
  ok(!gBrowser2.tabs[2].multiselected, "Tab 2 is not multiselected");

  gBrowser.removeTab(nonAdoptableTab);
  await BrowserTestUtils.closeWindow(win2);
});

add_task(async function test_switchToTabHavingURI() {
  const nonAdoptableTab = await addTab("data:text/plain,nonAdoptableTab");
  const uri = nonAdoptableTab.linkedBrowser.currentURI;
  const win2 = await BrowserTestUtils.openNewBrowserWindow();
  const gBrowser2 = win2.gBrowser;

  is(nonAdoptableTab.closing, false);
  is(nonAdoptableTab.selected, false);
  is(gBrowser2.tabs.length, 1);

  makeAdoptTabFailOnceFor(gBrowser2, nonAdoptableTab);
  win2.switchToTabHavingURI(uri, false, { adoptIntoActiveWindow: true });

  is(nonAdoptableTab.closing, false);
  is(nonAdoptableTab.selected, true);
  is(gBrowser2.tabs.length, 1);

  win2.switchToTabHavingURI(uri, false, { adoptIntoActiveWindow: true });

  is(nonAdoptableTab.closing, true);
  is(nonAdoptableTab.selected, false);
  is(gBrowser2.tabs.length, 2);

  await BrowserTestUtils.closeWindow(win2);
});
