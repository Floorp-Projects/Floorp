/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var tabs = [];

function addTab(aURL) {
  tabs.push(gBrowser.addTab(aURL, {skipAnimation: true}));
}

function switchTab(index) {
  return BrowserTestUtils.switchTab(gBrowser, gBrowser.tabs[index]);
}

function testAttrib(tabIndex, attrib, expected) {
  is(gBrowser.tabs[tabIndex].hasAttribute(attrib), expected,
     `tab #${tabIndex} should${expected ? "" : "n't"} have the ${attrib} attribute`);
}

add_task(async function setup() {
  is(gBrowser.tabs.length, 1, "one tab is open initially");

  addTab("http://mochi.test:8888/#0");
  addTab("http://mochi.test:8888/#1");
  addTab("http://mochi.test:8888/#2");
  addTab("http://mochi.test:8888/#3");
});

// Add several new tabs in sequence, hiding some, to ensure that the
// correct attributes get set
add_task(async function test() {
  await switchTab(0);

  testAttrib(0, "first-visible-tab", true);
  testAttrib(4, "last-visible-tab", true);
  testAttrib(0, "visuallyselected", true);
  testAttrib(0, "beforeselected-visible", false);

  await switchTab(2);

  testAttrib(2, "visuallyselected", true);
  testAttrib(1, "beforeselected-visible", true);

  gBrowser.hideTab(gBrowser.tabs[1]);

  testAttrib(0, "beforeselected-visible", true);

  gBrowser.showTab(gBrowser.tabs[1]);

  testAttrib(1, "beforeselected-visible", true);
  testAttrib(0, "beforeselected-visible", false);

  await switchTab(1);

  testAttrib(0, "beforeselected-visible", true);

  gBrowser.hideTab(gBrowser.tabs[0]);

  testAttrib(0, "first-visible-tab", false);
  testAttrib(1, "first-visible-tab", true);
  testAttrib(0, "beforeselected-visible", false);

  gBrowser.showTab(gBrowser.tabs[0]);

  testAttrib(0, "first-visible-tab", true);
  testAttrib(0, "beforeselected-visible", true);

  gBrowser.moveTabTo(gBrowser.selectedTab, 3);

  testAttrib(2, "beforeselected-visible", true);
});

add_task(async function test_hoverOne() {
  await switchTab(0);
  EventUtils.synthesizeMouseAtCenter(gBrowser.tabs[4], { type: "mousemove" });
  testAttrib(3, "beforehovered", true);
  EventUtils.synthesizeMouseAtCenter(gBrowser.tabs[3], { type: "mousemove" });
  testAttrib(2, "beforehovered", true);
  testAttrib(2, "afterhovered", false);
  testAttrib(4, "afterhovered", true);
  testAttrib(4, "beforehovered", false);
  testAttrib(0, "beforehovered", false);
  testAttrib(0, "afterhovered", false);
  testAttrib(1, "beforehovered", false);
  testAttrib(1, "afterhovered", false);
  testAttrib(3, "beforehovered", false);
  testAttrib(3, "afterhovered", false);
});

// Test that the afterhovered and beforehovered attributes are still there when
// a tab is selected and then unselected again. See bug 856107.
add_task(async function test_hoverStatePersistence() {
  gBrowser.removeTab(tabs.pop());

  function assertState() {
    testAttrib(0, "beforehovered", true);
    testAttrib(0, "afterhovered", false);
    testAttrib(2, "afterhovered", true);
    testAttrib(2, "beforehovered", false);
    testAttrib(1, "beforehovered", false);
    testAttrib(1, "afterhovered", false);
    testAttrib(3, "beforehovered", false);
    testAttrib(3, "afterhovered", false);
  }

  await switchTab(3);
  EventUtils.synthesizeMouseAtCenter(gBrowser.tabs[1], { type: "mousemove" });
  assertState();
  await switchTab(1);
  assertState();
  await switchTab(3);
  assertState();
});

add_task(async function test_pinning() {
  testAttrib(3, "last-visible-tab", true);
  testAttrib(3, "visuallyselected", true);
  testAttrib(2, "beforeselected-visible", true);
  // Causes gBrowser.tabs to change indices
  gBrowser.pinTab(gBrowser.tabs[3]);
  testAttrib(3, "last-visible-tab", true);
  testAttrib(0, "first-visible-tab", true);
  testAttrib(2, "beforeselected-visible", false);
  testAttrib(0, "visuallyselected", true);
  await switchTab(1);
  testAttrib(0, "beforeselected-visible", true);
});

add_task(function cleanup() {
  tabs.forEach(gBrowser.removeTab, gBrowser);
});
