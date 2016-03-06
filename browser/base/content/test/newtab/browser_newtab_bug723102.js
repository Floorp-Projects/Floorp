/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function* () {
  // create a new tab page and hide it.
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("");

  yield* addNewTabPageTab();
  let firstTab = gBrowser.selectedTab;

  yield* addNewTabPageTab();
  yield BrowserTestUtils.removeTab(firstTab);

  ok(NewTabUtils.allPages.enabled, "page is enabled");
  NewTabUtils.allPages.enabled = false;

  yield ContentTask.spawn(gBrowser.selectedBrowser, null, function* () {
    Assert.ok(content.gGrid.node.hasAttribute("page-disabled"), "page is disabled");
  });

  NewTabUtils.allPages.enabled = true;
});

