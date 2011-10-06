/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  // clean up after ourselves
  registerCleanupFunction(function () {
    while (gBrowser.tabs.length > 1)
      gBrowser.removeTab(gBrowser.tabs[1]);

    hideTabView();
  });

  // select the new tab
  gBrowser.selectedTab = gBrowser.addTab();

  showTabView(function () {
    // enter private browsing mode
    togglePrivateBrowsing(function () {
      ok(!TabView.isVisible(), "tabview is hidden");

      showTabView(function () {
        // leave private browsing mode
        togglePrivateBrowsing(function () {
          ok(TabView.isVisible(), "tabview is visible");
          hideTabView(finish);
        });
      });
    });
  });
}
