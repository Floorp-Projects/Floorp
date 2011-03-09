/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource:///modules/tabview/AllTabs.jsm");

function test() {
  waitForExplicitFinish();

  let newTab = gBrowser.addTab();

  // TabPinned
  let pinned = function(tab) {
    is(tab, newTab, "The tabs are the same after the tab is pinned");
    ok(tab.pinned, "The tab gets pinned");

    gBrowser.unpinTab(tab);
  };

  // TabUnpinned
  let unpinned = function(tab) {
    AllTabs.unregister("pinned", pinned);
    AllTabs.unregister("unpinned", unpinned);

    is(tab, newTab, "The tabs are the same after the tab is unpinned");
    ok(!tab.pinned, "The tab gets unpinned");

    // clean up and finish
    gBrowser.removeTab(tab);
    finish();
  };

  AllTabs.register("pinned", pinned);
  AllTabs.register("unpinned", unpinned);

  ok(!newTab.pinned, "The tab is not pinned");
  gBrowser.pinTab(newTab);
}

