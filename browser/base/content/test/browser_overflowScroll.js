const tabContainer = gBrowser.tabContainer;
const tabStrip = tabContainer.mTabstrip;

function runOverflowTests(aEvent) {

  if (aEvent.detail != 1)
    return;
  tabStrip.removeEventListener("overflow", runOverflowTests, false);
  var firstTab = tabContainer.firstChild;
  var lastTab = tabContainer.lastChild;
  var upButton = tabStrip._scrollButtonUp;
  var upButtonRect = upButton.getBoundingClientRect();
  var downButton = tabStrip._scrollButtonDown;
  var downButtonRect = downButton.getBoundingClientRect();

  tabStrip.ensureElementIsVisible(firstTab);
  var rect = firstTab.getBoundingClientRect();
  is(rect.left, upButtonRect.right, "Tab should scroll into view");
  tabStrip.scrollByIndex(1);
  rect = lastTab.getBoundingClientRect();
  is(rect.right, downButtonRect.left, "Tab should scroll into view");
  var element = tabStrip._elementFromPoint(upButtonRect.right);
  if (element.getBoundingClientRect().left == upButtonRect.right)
    element = element.previousSibling;
  EventUtils.synthesizeMouse(upButton, 0, 0, {clickCount: 1});
  rect = element.getBoundingClientRect();
  is(rect.left, upButtonRect.right, "One tab should have been scrolled");
  EventUtils.synthesizeMouse(upButton, 0, 0, {clickCount: 2});
  EventUtils.synthesizeMouse(upButton, 0, 0, {clickCount: 3, detail: 3, originalTarget: upButton});
  rect = firstTab.getBoundingClientRect();
  is(rect.left, upButtonRect.right, "Tabs should scroll to start");
  element = tabStrip._elementFromPoint(rect.left);
  ok(element == firstTab, "_elementFromPoint should recognize the tabs");

  while (tabContainer.childNodes.length > 1)
    gBrowser.removeTab(tabContainer.lastChild);

  finish();
}

function test() {
  waitForExplicitFinish();

  tabStrip.smoothScroll = false;
  let tabMinWidth = Components.classes['@mozilla.org/preferences-service;1']
                              .getService(Components.interfaces.nsIPrefBranch2)
                              .getIntPref("browser.tabs.tabMinWidth");
  var tabCountForOverflow =
    Math.floor(tabStrip.getBoundingClientRect().width / tabMinWidth);
  while (tabContainer.childNodes.length < tabCountForOverflow)
    gBrowser.addTab();
  gBrowser.addTab();
  gBrowser.selectedTab = tabContainer.lastChild.previousSibling;

  tabStrip.addEventListener("overflow", runOverflowTests, false);
}
