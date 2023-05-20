/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var tabs = [];

function addTab(aURL) {
  tabs.push(
    BrowserTestUtils.addTab(gBrowser, aURL, {
      skipAnimation: true,
    })
  );
}

function switchTab(index) {
  return BrowserTestUtils.switchTab(gBrowser, gBrowser.tabs[index]);
}

function testAttrib(tabIndex, attrib, expected) {
  is(
    gBrowser.tabs[tabIndex].hasAttribute(attrib),
    expected,
    `tab #${tabIndex} should${
      expected ? "" : "n't"
    } have the ${attrib} attribute`
  );
}

add_setup(async function () {
  is(gBrowser.tabs.length, 1, "one tab is open initially");

  addTab("http://mochi.test:8888/#0");
  addTab("http://mochi.test:8888/#1");
  addTab("http://mochi.test:8888/#2");
  addTab("http://mochi.test:8888/#3");

  is(gBrowser.tabs.length, 5, "five tabs are open after setup");
});

// Add several new tabs in sequence, hiding some, to ensure that the
// correct attributes get set
add_task(async function test() {
  testAttrib(0, "visuallyselected", true);

  await switchTab(2);

  testAttrib(2, "visuallyselected", true);
});

add_task(async function test_pinning() {
  await switchTab(3);
  testAttrib(3, "visuallyselected", true);
  // Causes gBrowser.tabs to change indices
  gBrowser.pinTab(gBrowser.tabs[3]);
  testAttrib(0, "visuallyselected", true);
});

add_task(function cleanup() {
  tabs.forEach(gBrowser.removeTab, gBrowser);
});
