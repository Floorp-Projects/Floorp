/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Bug 587922: tabs don't get removed if they're hidden

function test() {
  waitForExplicitFinish();

  // Ensure TabView has been initialized already. Otherwise it could
  // activate at an unexpected time and show/hide tabs.
  TabView._initFrame(runTest);
}

function runTest() {
  // Add a tab that will get removed and hidden
  let testTab = gBrowser.addTab("about:blank", {skipAnimation: true});
  is(gBrowser.visibleTabs.length, 2, "just added a tab, so 2 tabs");
  gBrowser.selectedTab = testTab;

  let numVisBeforeHide, numVisAfterHide;
  gBrowser.tabContainer.addEventListener("TabSelect", function() {
    gBrowser.tabContainer.removeEventListener("TabSelect", arguments.callee, false);

    // While the next tab is being selected, hide the removing tab
    numVisBeforeHide = gBrowser.visibleTabs.length;
    gBrowser.hideTab(testTab);
    numVisAfterHide = gBrowser.visibleTabs.length;
  }, false);
  gBrowser.removeTab(testTab, {animate: true});

  // Make sure the tab gets removed at the end of the animation by polling
  (function checkRemoved() {
    return setTimeout(function() {
      if (gBrowser.tabs.length != 1) {
        checkRemoved();
        return;
      }

      is(numVisBeforeHide, 1, "animated remove has in 1 tab left");
      is(numVisAfterHide, 1, "hiding a removing tab is also has 1 tab");
      finish();
    }, 50);
  })();
}
