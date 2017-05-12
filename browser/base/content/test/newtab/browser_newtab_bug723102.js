/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  // create a new tab page and hide it.
  await setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("");

  await addNewTabPageTab();
  let firstTab = gBrowser.selectedTab;

  await addNewTabPageTab();
  await BrowserTestUtils.removeTab(firstTab);

  ok(NewTabUtils.allPages.enabled, "page is enabled");
  NewTabUtils.allPages.enabled = false;

  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    Assert.ok(content.gGrid.node.hasAttribute("page-disabled"), "page is disabled");
  });

  NewTabUtils.allPages.enabled = true;
});

