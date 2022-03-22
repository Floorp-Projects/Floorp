/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests that the sidebar recreates the contents of the <tree> element
 * for live app locale switching.
 */

add_task(function cleanup() {
  registerCleanupFunction(() => {
    SidebarUI.hide();
  });
});

/**
 * @param {string} sidebarName
 */
async function testLiveReloading(sidebarName) {
  info("Showing the sidebar " + sidebarName);
  await SidebarUI.show(sidebarName);

  function getTreeChildren() {
    const sidebarDoc = document.querySelector("#sidebar").contentWindow
      .document;
    return sidebarDoc.querySelector(".sidebar-placesTreechildren");
  }

  const childrenBefore = getTreeChildren();
  ok(childrenBefore, "Found the sidebar children");
  is(childrenBefore, getTreeChildren(), "The children start out as equal");

  info("Simulating an app locale change.");
  Services.obs.notifyObservers(null, "intl:app-locales-changed");

  await TestUtils.waitForCondition(
    getTreeChildren,
    "Waiting for a new child tree element."
  );

  isnot(
    childrenBefore,
    getTreeChildren(),
    "The tree's contents are re-computed."
  );

  info("Hiding the sidebar");
  SidebarUI.hide();
}

add_task(async function test_bookmarks_sidebar() {
  await testLiveReloading("viewBookmarksSidebar");
});

add_task(async function test_history_sidebar() {
  await testLiveReloading("viewHistorySidebar");
});
