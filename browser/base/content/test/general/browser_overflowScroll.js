var tabstrip = gBrowser.tabContainer.mTabstrip;
var scrollbox = tabstrip._scrollbox;
var originalSmoothScroll = tabstrip.smoothScroll;
var tabs = gBrowser.tabs;

function rect(ele)           ele.getBoundingClientRect();
function width(ele)          rect(ele).width;
function left(ele)           rect(ele).left;
function right(ele)          rect(ele).right;
function isLeft(ele, msg)    is(left(ele) + tabstrip._tabMarginLeft, left(scrollbox), msg);
function isRight(ele, msg)   is(right(ele) - tabstrip._tabMarginRight, right(scrollbox), msg);
function elementFromPoint(x) tabstrip._elementFromPoint(x);
function nextLeftElement()   elementFromPoint(left(scrollbox) - 1);
function nextRightElement()  elementFromPoint(right(scrollbox) + 1);
function firstScrollable()   tabs[gBrowser._numPinnedTabs];

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
    gBrowser.addTab("about:blank", {skipAnimation: true});
  gBrowser.pinTab(tabs[0]);

  tabstrip.addEventListener("overflow", runOverflowTests, false);
}

function runOverflowTests(aEvent) {
  if (aEvent.detail != 1)
    return;

  tabstrip.removeEventListener("overflow", runOverflowTests, false);

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
  EventUtils.synthesizeMouse(upButton, 1, 1, {});
  isLeft(element, "Scrolled one tab to the left with a single click");

  let elementPoint = left(scrollbox) - width(scrollbox);
  element = elementFromPoint(elementPoint);
  if (elementPoint == right(element)) {
    element = element.nextSibling;
  }
  EventUtils.synthesizeMouse(upButton, 1, 1, {clickCount: 2});
  isLeft(element, "Scrolled one page of tabs with a double click");

  EventUtils.synthesizeMouse(upButton, 1, 1, {clickCount: 3});
  var firstScrollableLeft = left(firstScrollable());
  ok(left(scrollbox) <= firstScrollableLeft, "Scrolled to the start with a triple click " +
     "(" + left(scrollbox) + " <= " + firstScrollableLeft + ")");

  for (var i = 2; i; i--)
    EventUtils.synthesizeWheel(scrollbox, 1, 1, { deltaX: -1.0, deltaMode: WheelEvent.DOM_DELTA_LINE });
  is(left(firstScrollable()), firstScrollableLeft, "Remained at the start with the mouse wheel");

  element = nextRightElement();
  EventUtils.synthesizeWheel(scrollbox, 1, 1, { deltaX: 1.0, deltaMode: WheelEvent.DOM_DELTA_LINE});
  isRight(element, "Scrolled one tab to the right with the mouse wheel");

  while (tabs.length > 1)
    gBrowser.removeTab(tabs[0]);

  tabstrip.smoothScroll = originalSmoothScroll;
  finish();
}
