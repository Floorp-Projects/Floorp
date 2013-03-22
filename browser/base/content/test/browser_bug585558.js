/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

let tabs = [];

function addTab(aURL) {
  tabs.push(gBrowser.addTab(aURL, {skipAnimation: true}));
}

function testAttrib(elem, attrib, attribValue, msg) {
  is(elem.hasAttribute(attrib), attribValue, msg);
}

function test() {
  waitForExplicitFinish();
  is(gBrowser.tabs.length, 1, "one tab is open initially");

  // Add several new tabs in sequence, hiding some, to ensure that the
  // correct attributes get set

  addTab("http://mochi.test:8888/#0");
  addTab("http://mochi.test:8888/#1");
  addTab("http://mochi.test:8888/#2");
  addTab("http://mochi.test:8888/#3");

  gBrowser.selectedTab = gBrowser.tabs[0];
  testAttrib(gBrowser.tabs[0], "first-visible-tab", true,
             "First tab marked first-visible-tab!");
  testAttrib(gBrowser.tabs[4], "last-visible-tab", true,
             "Fifth tab marked last-visible-tab!");
  testAttrib(gBrowser.tabs[0], "selected", true, "First tab marked selected!");
  testAttrib(gBrowser.tabs[0], "afterselected-visible", false,
             "First tab not marked afterselected-visible!");
  testAttrib(gBrowser.tabs[1], "afterselected-visible", true,
             "Second tab marked afterselected-visible!");
  gBrowser.hideTab(gBrowser.tabs[1]);
  executeSoon(test_hideSecond);
}

function test_hideSecond() {
  testAttrib(gBrowser.tabs[2], "afterselected-visible", true,
             "Third tab marked afterselected-visible!");
  gBrowser.showTab(gBrowser.tabs[1])
  executeSoon(test_showSecond);
}

function test_showSecond() {
  testAttrib(gBrowser.tabs[1], "afterselected-visible", true,
             "Second tab marked afterselected-visible!");
  testAttrib(gBrowser.tabs[2], "afterselected-visible", false,
             "Third tab not marked as afterselected-visible!");
  gBrowser.selectedTab = gBrowser.tabs[1];
  gBrowser.hideTab(gBrowser.tabs[0]);
  executeSoon(test_hideFirst);
}

function test_hideFirst() {
  testAttrib(gBrowser.tabs[0], "first-visible-tab", false,
              "Hidden first tab not marked first-visible-tab!");
  testAttrib(gBrowser.tabs[1], "first-visible-tab", true,
              "Second tab marked first-visible-tab!");
  gBrowser.showTab(gBrowser.tabs[0]);
  executeSoon(test_showFirst);
}

function test_showFirst() {
  testAttrib(gBrowser.tabs[0], "first-visible-tab", true,
             "First tab marked first-visible-tab!");
  gBrowser.selectedTab = gBrowser.tabs[2];
  testAttrib(gBrowser.tabs[3], "afterselected-visible", true,
             "Fourth tab marked afterselected-visible!");

  gBrowser.moveTabTo(gBrowser.selectedTab, 1);
  executeSoon(test_movedLower);
}

function test_movedLower() {
  testAttrib(gBrowser.tabs[2], "afterselected-visible", true,
             "Third tab marked afterselected-visible!");
  test_hoverOne();
}

function test_hoverOne() {
  EventUtils.synthesizeMouseAtCenter(gBrowser.tabs[4], { type: "mousemove" });
  testAttrib(gBrowser.tabs[3], "beforehovered", true,
             "Fourth tab marked beforehovered");
  EventUtils.synthesizeMouseAtCenter(gBrowser.tabs[3], { type: "mousemove" });
  testAttrib(gBrowser.tabs[2], "beforehovered", true,
             "Third tab marked beforehovered!");
  testAttrib(gBrowser.tabs[4], "afterhovered", true,
             "Fifth tab marked afterhovered!");
  gBrowser.removeTab(tabs.pop());
  test_pinning();
}

function test_pinning() {
  gBrowser.selectedTab = gBrowser.tabs[3];
  testAttrib(gBrowser.tabs[3], "last-visible-tab", true,
             "Fourth tab marked last-visible-tab!");
  testAttrib(gBrowser.tabs[3], "selected", true, "Fourth tab marked selected!");
  testAttrib(gBrowser.tabs[3], "afterselected-visible", false,
             "Fourth tab not marked afterselected-visible!");
  // Causes gBrowser.tabs to change indices
  gBrowser.pinTab(gBrowser.tabs[3]);
  testAttrib(gBrowser.tabs[3], "last-visible-tab", true,
             "Fourth tab marked last-visible-tab!");
  testAttrib(gBrowser.tabs[1], "afterselected-visible", true,
             "Second tab marked afterselected-visible!");
  testAttrib(gBrowser.tabs[0], "first-visible-tab", true,
             "First tab marked first-visible-tab!");
  testAttrib(gBrowser.tabs[0], "selected", true, "First tab marked selected!");
  gBrowser.selectedTab = gBrowser.tabs[1];
  testAttrib(gBrowser.tabs[2], "afterselected-visible", true,
             "Third tab marked afterselected-visible!");
  test_cleanUp();
}

function test_cleanUp() {
  tabs.forEach(gBrowser.removeTab, gBrowser);
  finish();
}
