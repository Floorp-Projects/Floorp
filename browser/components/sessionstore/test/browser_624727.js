/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let TEST_STATE = { windows: [{ tabs: [{ url: "about:blank" }] }] };

add_task(function* () {
  function assertNumberOfTabs(num, msg) {
    is(gBrowser.tabs.length, num, msg);
  }

  function assertNumberOfPinnedTabs(num, msg) {
    is(gBrowser._numPinnedTabs, num, msg);
  }

  // check prerequisites
  assertNumberOfTabs(1, "we start off with one tab");
  assertNumberOfPinnedTabs(0, "no pinned tabs so far");

  // setup
  gBrowser.addTab("about:blank");
  assertNumberOfTabs(2, "there are two tabs, now");

  let [tab1, tab2] = gBrowser.tabs;
  let linkedBrowser = tab1.linkedBrowser;
  gBrowser.pinTab(tab1);
  gBrowser.pinTab(tab2);
  assertNumberOfPinnedTabs(2, "both tabs are now pinned");

  // run the test
  yield promiseBrowserState(TEST_STATE);

  assertNumberOfTabs(1, "one tab left after setBrowserState()");
  assertNumberOfPinnedTabs(0, "there are no pinned tabs");
  is(gBrowser.tabs[0].linkedBrowser, linkedBrowser, "first tab's browser got re-used");
});
