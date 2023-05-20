/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

add_task(async function () {
  let initialTabsLength = gBrowser.tabs.length;

  let arrowScrollbox = gBrowser.tabContainer.arrowScrollbox;
  let tabMinWidth = parseInt(
    getComputedStyle(gBrowser.selectedTab, null).minWidth
  );

  let width = ele => ele.getBoundingClientRect().width;

  let tabCountForOverflow = Math.ceil(
    (width(arrowScrollbox) / tabMinWidth) * 1.1
  );

  let newTab1 = (gBrowser.selectedTab = BrowserTestUtils.addTab(
    gBrowser,
    "about:robots",
    { skipAnimation: true }
  ));
  let newTab2 = (gBrowser.selectedTab = BrowserTestUtils.addTab(
    gBrowser,
    "about:about",
    { skipAnimation: true }
  ));
  let newTab3 = (gBrowser.selectedTab = BrowserTestUtils.addTab(
    gBrowser,
    "about:config",
    { skipAnimation: true }
  ));

  await BrowserTestUtils.overflowTabs(registerCleanupFunction, window, {
    overflowAtStart: false,
  });

  registerCleanupFunction(function () {
    while (gBrowser.tabs.length > initialTabsLength) {
      gBrowser.removeTab(
        gBrowser.tabContainer.getItemAtIndex(initialTabsLength)
      );
    }
  });

  let tabs = gBrowser.tabs;
  is(tabs.length, tabCountForOverflow, "new tabs are opened");
  is(tabs[initialTabsLength], newTab1, "newTab1 position is correct");
  is(tabs[initialTabsLength + 1], newTab2, "newTab2 position is correct");
  is(tabs[initialTabsLength + 2], newTab3, "newTab3 position is correct");

  await dragAndDrop(newTab1, newTab2, false);
  tabs = gBrowser.tabs;
  is(tabs.length, tabCountForOverflow, "tabs are still there");
  is(tabs[initialTabsLength], newTab2, "newTab2 and newTab1 are swapped");
  is(tabs[initialTabsLength + 1], newTab1, "newTab1 and newTab2 are swapped");
  is(tabs[initialTabsLength + 2], newTab3, "newTab3 stays same place");
});
