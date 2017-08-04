"use strict";

requestLongerTimeout(2);

/**
 * Tests that scrolling the tab strip via the scroll buttons scrolls the right
 * amount in non-smoothscroll mode.
 */
add_task(async function() {
  let tabstrip = gBrowser.tabContainer.mTabstrip;
  let scrollbox = tabstrip._scrollbox;
  let originalSmoothScroll = tabstrip.smoothScroll;
  let tabs = gBrowser.tabs;
  let tabMinWidth = parseInt(getComputedStyle(gBrowser.selectedTab, null).minWidth);

  let rect = ele => ele.getBoundingClientRect();
  let width = ele => rect(ele).width;

  let tabCountForOverflow = Math.ceil(width(tabstrip) / tabMinWidth * 3);

  let left = ele => rect(ele).left;
  let right = ele => rect(ele).right;
  let isLeft = (ele, msg) => is(left(ele), left(scrollbox), msg);
  let isRight = (ele, msg) => is(right(ele), right(scrollbox), msg);
  let elementFromPoint = x => tabstrip._elementFromPoint(x);
  let nextLeftElement = () => elementFromPoint(left(scrollbox) - 1);
  let nextRightElement = () => elementFromPoint(right(scrollbox) + 1);
  let firstScrollable = () => tabs[gBrowser._numPinnedTabs];

  tabstrip.smoothScroll = false;
  registerCleanupFunction(() => {
    tabstrip.smoothScroll = originalSmoothScroll;
  });

  while (tabs.length < tabCountForOverflow) {
    BrowserTestUtils.addTab(gBrowser, "about:blank", { skipAnimation: true });
  }

  gBrowser.pinTab(tabs[0]);

  await BrowserTestUtils.waitForCondition(() => {
    return Array.from(gBrowser.tabs).every(tab => tab._fullyOpen);
  });

  ok(!scrollbox.hasAttribute("notoverflowing"),
     "Tab strip should be overflowing");

  let upButton = tabstrip._scrollButtonUp;
  let downButton = tabstrip._scrollButtonDown;
  let element;

  gBrowser.selectedTab = firstScrollable();
  ok(left(scrollbox) <= left(firstScrollable()), "Selecting the first tab scrolls it into view " +
     "(" + left(scrollbox) + " <= " + left(firstScrollable()) + ")");

  element = nextRightElement();
  EventUtils.synthesizeMouseAtCenter(downButton, {});
  isRight(element, "Scrolled one tab to the right with a single click");

  gBrowser.selectedTab = tabs[tabs.length - 1];
  ok(right(gBrowser.selectedTab) <= right(scrollbox), "Selecting the last tab scrolls it into view " +
     "(" + right(gBrowser.selectedTab) + " <= " + right(scrollbox) + ")");

  element = nextLeftElement();
  EventUtils.synthesizeMouseAtCenter(upButton, {});
  isLeft(element, "Scrolled one tab to the left with a single click");

  let elementPoint = left(scrollbox) - width(scrollbox);
  element = elementFromPoint(elementPoint);
  if (elementPoint == right(element)) {
    element = element.nextSibling;
  }
  EventUtils.synthesizeMouseAtCenter(upButton, {clickCount: 2});
  isLeft(element, "Scrolled one page of tabs with a double click");

  EventUtils.synthesizeMouseAtCenter(upButton, {clickCount: 3});
  var firstScrollableLeft = left(firstScrollable());
  ok(left(scrollbox) <= firstScrollableLeft, "Scrolled to the start with a triple click " +
     "(" + left(scrollbox) + " <= " + firstScrollableLeft + ")");

  while (tabs.length > 1) {
    await BrowserTestUtils.removeTab(gBrowser.tabs[0]);
  }
});
