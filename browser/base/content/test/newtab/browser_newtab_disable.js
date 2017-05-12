/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * These tests make sure that the 'New Tab Page' feature can be disabled if the
 * decides not to use it.
 */
add_task(async function() {
  // create a new tab page and hide it.
  await setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("");

  let firstTab = await addNewTabPageTab();

  function isGridDisabled(browser = gBrowser.selectedBrowser) {
    return ContentTask.spawn(browser, {}, async function() {
      return content.gGrid.node.hasAttribute("page-disabled");
    });
  }

  let isDisabled = await isGridDisabled();
  ok(!isDisabled, "page is not disabled");

  NewTabUtils.allPages.enabled = false;

  isDisabled = await isGridDisabled();
  ok(isDisabled, "page is disabled");

  // create a second new tab page and make sure it's disabled. enable it
  // again and check if the former page gets enabled as well.
  await addNewTabPageTab();
  isDisabled = await isGridDisabled(firstTab.linkedBrowser);
  ok(isDisabled, "page is disabled");

  // check that no sites have been rendered
  await ContentTask.spawn(gBrowser.selectedBrowser, {}, async function() {
    Assert.equal(content.document.querySelectorAll(".site").length, 0,
      "no sites have been rendered");
  });

  NewTabUtils.allPages.enabled = true;

  isDisabled = await isGridDisabled();
  ok(!isDisabled, "page is not disabled");

  isDisabled = await isGridDisabled(firstTab.linkedBrowser);
  ok(!isDisabled, "old page is not disabled");
});
