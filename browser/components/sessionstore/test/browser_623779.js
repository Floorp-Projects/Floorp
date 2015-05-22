"use strict";

add_task(function* () {
  gBrowser.pinTab(gBrowser.selectedTab);

  let newTab = gBrowser.duplicateTab(gBrowser.selectedTab);
  yield promiseTabRestored(newTab);

  ok(!newTab.pinned, "duplicating a pinned tab creates unpinned tab");
  yield promiseRemoveTab(newTab);

  gBrowser.unpinTab(gBrowser.selectedTab);
});
