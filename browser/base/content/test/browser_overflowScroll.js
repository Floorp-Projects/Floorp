var tabContainer = gBrowser.tabContainer;
var tabstrip = tabContainer.mTabstrip;
var scrollbox = tabstrip._scrollbox;
var originalSmoothScroll = tabstrip.smoothScroll;

function rect(ele)           ele.getBoundingClientRect();
function width(ele)          rect(ele).width;
function left(ele)           rect(ele).left;
function right(ele)          rect(ele).right;
function isLeft(ele, msg)    is(left(ele), left(scrollbox), msg);
function isRight(ele, msg)   is(right(ele), right(scrollbox), msg);
function elementFromPoint(x) tabstrip._elementFromPoint(x);
function nextLeftElement()   elementFromPoint(left(scrollbox) - 1);
function nextRightElement()  elementFromPoint(right(scrollbox) + 1);

function test() {
  waitForExplicitFinish();

  tabstrip.smoothScroll = false;

  var tabMinWidth = gPrefService.getIntPref("browser.tabs.tabMinWidth");
  var tabCountForOverflow = Math.ceil(width(tabstrip) / tabMinWidth * 3);
  while (tabContainer.childNodes.length < tabCountForOverflow)
    gBrowser.addTab();

  tabstrip.addEventListener("overflow", runOverflowTests, false);
}

function runOverflowTests(aEvent) {
  if (aEvent.detail != 1)
    return;

  tabstrip.removeEventListener("overflow", runOverflowTests, false);

  var upButton = tabstrip._scrollButtonUp;
  var downButton = tabstrip._scrollButtonDown;
  var element;

  gBrowser.selectedTab = tabContainer.firstChild;
  isLeft(tabContainer.firstChild, "Selecting the first tab scrolls it into view");

  element = nextRightElement();
  EventUtils.synthesizeMouse(downButton, 0, 0, {});
  isRight(element, "Scrolled one tab to the right with a single click");

  gBrowser.selectedTab = tabContainer.lastChild;
  isRight(tabContainer.lastChild, "Selecting the last tab scrolls it into view");

  element = nextLeftElement();
  EventUtils.synthesizeMouse(upButton, 0, 0, {});
  isLeft(element, "Scrolled one tab to the left with a single click");

  element = elementFromPoint(left(scrollbox) - width(scrollbox));
  EventUtils.synthesizeMouse(upButton, 0, 0, {clickCount: 2});
  isLeft(element, "Scrolled one page of tabs with a double click");

  EventUtils.synthesizeMouse(upButton, 0, 0, {clickCount: 3});
  isLeft(tabContainer.firstChild, "Scrolled to the start with a triple click");

  element = nextRightElement();
  EventUtils.synthesizeMouseScroll(scrollbox, 0, 0, {delta: 1});
  isRight(element, "Scrolled one tab to the right with the mouse wheel");

  while (tabContainer.childNodes.length > 1)
    gBrowser.removeTab(tabContainer.lastChild);

  tabstrip.smoothScroll = originalSmoothScroll;
  finish();
}
