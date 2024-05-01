/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

/**
 * Tests Places views (toolbar, tree) for icons update.
 * The menu is not tested since it uses the same code as the toolbar.
 */

add_task(async function () {
  const PAGE_URI = NetUtil.newURI("http://places.test/");
  const ICON_URI = NetUtil.newURI(
    "http://mochi.test:8888/browser/browser/components/places/tests/browser/favicon-normal16.png"
  );

  info("Uncollapse the personal toolbar if needed");
  let toolbar = document.getElementById("PersonalToolbar");
  let wasCollapsed = toolbar.collapsed;
  if (wasCollapsed) {
    await promiseSetToolbarVisibility(toolbar, true);
    registerCleanupFunction(async function () {
      await promiseSetToolbarVisibility(toolbar, false);
    });
  }

  info("Open the bookmarks sidebar");
  let sidebar = document.getElementById("sidebar");
  let promiseSidebarLoaded = new Promise(resolve => {
    sidebar.addEventListener("load", resolve, { capture: true, once: true });
  });
  SidebarController.show("viewBookmarksSidebar");
  registerCleanupFunction(() => {
    SidebarController.hide();
  });
  await promiseSidebarLoaded;

  // Add a bookmark to the bookmarks toolbar.
  let bm = await PlacesUtils.bookmarks.insert({
    url: PAGE_URI,
    title: "test icon",
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
  });
  registerCleanupFunction(async function () {
    await PlacesUtils.bookmarks.remove(bm);
  });

  // The icon is read asynchronously from the network, we don't have an easy way
  // to wait for that.
  await new Promise(resolve => {
    setTimeout(resolve, 3000);
  });

  let toolbarElt = getNodeForToolbarItem(bm.guid);
  let toolbarShot1 = TestUtils.screenshotArea(toolbarElt, window);
  let sidebarRect = await getRectForSidebarItem(bm.guid);
  let sidebarShot1 = TestUtils.screenshotArea(sidebarRect, window);

  info("Toolbar: " + toolbarShot1);
  info("Sidebar: " + sidebarShot1);

  let iconURI = await new Promise(resolve => {
    PlacesUtils.favicons.setAndFetchFaviconForPage(
      PAGE_URI,
      ICON_URI,
      true,
      PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
      resolve,
      Services.scriptSecurityManager.getSystemPrincipal()
    );
  });
  Assert.ok(iconURI.equals(ICON_URI), "Succesfully set the icon");

  // The icon is read asynchronously from the network, we don't have an easy way
  // to wait for that, thus we must poll.
  await TestUtils.waitForCondition(() => {
    // Assert.notEqual truncates the strings, so it is unusable here for failure
    // debugging purposes.
    let toolbarShot2 = TestUtils.screenshotArea(toolbarElt, window);
    if (toolbarShot1 != toolbarShot2) {
      info("After toolbar: " + toolbarShot2);
    }
    return toolbarShot1 != toolbarShot2;
  }, "Waiting for the toolbar icon to update");

  await TestUtils.waitForCondition(() => {
    let sidebarShot2 = TestUtils.screenshotArea(sidebarRect, window);
    if (sidebarShot1 != sidebarShot2) {
      info("After sidebar: " + sidebarShot2);
    }
    return sidebarShot1 != sidebarShot2;
  }, "Waiting for the sidebar icon to update");
});

/**
 * Get Element for a bookmark in the bookmarks toolbar.
 *
 * @param {string} guid
 *        GUID of the item to search.
 * @returns {object} DOM Node of the element.
 */
function getNodeForToolbarItem(guid) {
  return Array.from(
    document.getElementById("PlacesToolbarItems").children
  ).find(child => child._placesNode && child._placesNode.bookmarkGuid == guid);
}

/**
 * Get a rect for a bookmark in the bookmarks sidebar
 *
 * @param {string} guid
 *        GUID of the item to search.
 * @returns {object} DOM Node of the element.
 */
async function getRectForSidebarItem(guid) {
  let sidebar = document.getElementById("sidebar");
  let tree = sidebar.contentDocument.getElementById("bookmarks-view");
  tree.selectItems([guid]);
  let treerect = tree.getBoundingClientRect();
  let cellrect = tree.getCoordsForCellItem(
    tree.currentIndex,
    tree.columns[0],
    "cell"
  );

  // Adjust the position for the tree and sidebar.
  return {
    left: treerect.left + cellrect.left + sidebar.getBoundingClientRect().left,
    top: treerect.top + cellrect.top + sidebar.getBoundingClientRect().top,
    width: cellrect.width,
    height: cellrect.height,
  };
}
