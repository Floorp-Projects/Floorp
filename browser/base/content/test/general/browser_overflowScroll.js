var tabstrip = gBrowser.tabContainer.mTabstrip;
var scrollbox = tabstrip._scrollbox;
var originalSmoothScroll = tabstrip.smoothScroll;
var tabs = gBrowser.tabs;

var rect = ele => ele.getBoundingClientRect();
var width = ele => rect(ele).width;
var height = ele => rect(ele).height;
var left = ele => rect(ele).left;
var right = ele => rect(ele).right;
var isLeft = (ele, msg) => is(left(ele), left(scrollbox), msg);
var isRight = (ele, msg) => is(right(ele), right(scrollbox), msg);
var elementFromPoint = x => tabstrip._elementFromPoint(x);
var nextLeftElement = () => elementFromPoint(left(scrollbox) - 1);
var nextRightElement = () => elementFromPoint(right(scrollbox) + 1);
var firstScrollable = () => tabs[gBrowser._numPinnedTabs];

var clickCenter = (ele, opts) => {
  EventUtils.synthesizeMouse(ele, Math.ceil(width(ele) / 2),
                             Math.ceil(height(ele) / 2), opts);
}

function test() {
  requestLongerTimeout(2);
  waitForExplicitFinish();

  // If the previous (or more) test finished with cleaning up the tabs,
  // there may be some pending animations. That can cause a failure of
  // this tests, so, we should test this in another stack.
  setTimeout(doTest, 0);
}

function doTest() {
  tabstrip.smoothScroll = false;

  var tabMinWidth = parseInt(getComputedStyle(gBrowser.selectedTab, null).minWidth);
  var tabCountForOverflow = Math.ceil(width(tabstrip) / tabMinWidth * 3);
  while (tabs.length < tabCountForOverflow)
    BrowserTestUtils.addTab(gBrowser, "about:blank", {skipAnimation: true});
  gBrowser.pinTab(tabs[0]);

  tabstrip.addEventListener("overflow", runOverflowTests);
}

function runOverflowTests(aEvent) {
  if (aEvent.detail != 1 ||
      aEvent.target != tabstrip)
    return;

  tabstrip.removeEventListener("overflow", runOverflowTests);

  var upButton = tabstrip._scrollButtonUp;
  var downButton = tabstrip._scrollButtonDown;
  var element;

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
  clickCenter(upButton, {});
  isLeft(element, "Scrolled one tab to the left with a single click");

  let elementPoint = left(scrollbox) - width(scrollbox);
  element = elementFromPoint(elementPoint);
  if (elementPoint == right(element)) {
    element = element.nextSibling;
  }
  clickCenter(upButton, {clickCount: 2});
  isLeft(element, "Scrolled one page of tabs with a double click");

  clickCenter(upButton, {clickCount: 3});
  var firstScrollableLeft = left(firstScrollable());
  ok(left(scrollbox) <= firstScrollableLeft, "Scrolled to the start with a triple click " +
     "(" + left(scrollbox) + " <= " + firstScrollableLeft + ")");

  while (tabs.length > 1)
    gBrowser.removeTab(tabs[0]);

  tabstrip.smoothScroll = originalSmoothScroll;
  finish();
}
