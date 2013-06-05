/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function runTests() {
  // create a new tab page and hide it.
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("");

  yield addNewTabPageTab();
  let firstTab = gBrowser.selectedTab;

  yield addNewTabPageTab();
  gBrowser.removeTab(firstTab);

  ok(NewTabUtils.allPages.enabled, "page is enabled");
  NewTabUtils.allPages.enabled = false;
  ok(getGrid().node.hasAttribute("page-disabled"), "page is disabled");
  NewTabUtils.allPages.enabled = true;
}
