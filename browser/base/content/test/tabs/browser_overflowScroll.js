"use strict";

requestLongerTimeout(2);

/**
 * Tests that scrolling the tab strip via the scroll buttons scrolls the right
 * amount in non-smoothscroll mode.
 */
add_task(async function () {
  let arrowScrollbox = gBrowser.tabContainer.arrowScrollbox;
  let scrollbox = arrowScrollbox.scrollbox;

  let rect = ele => ele.getBoundingClientRect();
  let width = ele => rect(ele).width;

  let left = ele => rect(ele).left;
  let right = ele => rect(ele).right;
  let isLeft = (ele, msg) => is(left(ele), left(scrollbox), msg);
  let isRight = (ele, msg) => is(right(ele), right(scrollbox), msg);
  let elementFromPoint = x => arrowScrollbox._elementFromPoint(x);
  let nextLeftElement = () => elementFromPoint(left(scrollbox) - 1);
  let nextRightElement = () => elementFromPoint(right(scrollbox) + 1);
  let firstScrollable = () => gBrowser.tabs[gBrowser._numPinnedTabs];
  let waitForNextFrame = async function () {
    await new Promise(requestAnimationFrame);
    await new Promise(resolve => Services.tm.dispatchToMainThread(resolve));
  };

  await BrowserTestUtils.overflowTabs(registerCleanupFunction, window, {
    overflowAtStart: false,
    overflowTabFactor: 3,
  });

  gBrowser.pinTab(gBrowser.tabs[0]);

  await BrowserTestUtils.waitForCondition(() => {
    return Array.from(gBrowser.tabs).every(tab => tab._fullyOpen);
  });

  ok(
    arrowScrollbox.hasAttribute("overflowing"),
    "Tab strip should be overflowing"
  );

  let upButton = arrowScrollbox._scrollButtonUp;
  let downButton = arrowScrollbox._scrollButtonDown;
  let element;

  gBrowser.selectedTab = firstScrollable();
  await TestUtils.waitForTick();

  Assert.lessOrEqual(
    left(scrollbox),
    left(firstScrollable()),
    "Selecting the first tab scrolls it into view " +
      "(" +
      left(scrollbox) +
      " <= " +
      left(firstScrollable()) +
      ")"
  );

  element = nextRightElement();
  EventUtils.synthesizeMouseAtCenter(downButton, {});
  await waitForNextFrame();
  isRight(element, "Scrolled one tab to the right with a single click");

  gBrowser.selectedTab = gBrowser.tabs[gBrowser.tabs.length - 1];
  await waitForNextFrame();
  Assert.lessOrEqual(
    right(gBrowser.selectedTab),
    right(scrollbox),
    "Selecting the last tab scrolls it into view " +
      "(" +
      right(gBrowser.selectedTab) +
      " <= " +
      right(scrollbox) +
      ")"
  );

  element = nextLeftElement();
  EventUtils.synthesizeMouseAtCenter(upButton, {});
  await waitForNextFrame();
  isLeft(element, "Scrolled one tab to the left with a single click");

  let elementPoint = left(scrollbox) - width(scrollbox);
  element = elementFromPoint(elementPoint);
  element = element.nextElementSibling;

  EventUtils.synthesizeMouseAtCenter(upButton, { clickCount: 2 });
  await waitForNextFrame();
  await BrowserTestUtils.waitForCondition(
    () => !gBrowser.tabContainer.arrowScrollbox._isScrolling
  );
  isLeft(element, "Scrolled one page of tabs with a double click");

  EventUtils.synthesizeMouseAtCenter(upButton, { clickCount: 3 });
  await waitForNextFrame();
  var firstScrollableLeft = left(firstScrollable());
  Assert.lessOrEqual(
    left(scrollbox),
    firstScrollableLeft,
    "Scrolled to the start with a triple click " +
      "(" +
      left(scrollbox) +
      " <= " +
      firstScrollableLeft +
      ")"
  );

  while (gBrowser.tabs.length > 1) {
    BrowserTestUtils.removeTab(gBrowser.tabs[0]);
  }
});
