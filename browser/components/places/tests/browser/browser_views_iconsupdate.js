/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

/**
 * Tests Places views (toolbar, tree) for icons update.
 * The menu is not tested since it uses the same code as the toolbar.
 */

add_task(async function() {
  const PAGE_URI = NetUtil.newURI("http://places.test/");
  const ICON_URI = NetUtil.newURI("http://mochi.test:8888/browser/browser/components/places/tests/browser/favicon-normal16.png");

  info("Uncollapse the personal toolbar if needed");
  let toolbar = document.getElementById("PersonalToolbar");
  let wasCollapsed = toolbar.collapsed;
  if (wasCollapsed) {
    await promiseSetToolbarVisibility(toolbar, true);
    registerCleanupFunction(async function() {
      await promiseSetToolbarVisibility(toolbar, false);
    });
  }

  info("Open the bookmarks sidebar");
  let sidebar = document.getElementById("sidebar");
  let promiseSidebarLoaded = new Promise(resolve => {
    sidebar.addEventListener("load", resolve, {capture: true, once: true});
  });
  SidebarUI.show("viewBookmarksSidebar");
  registerCleanupFunction(() => {
    SidebarUI.hide();
  });
  await promiseSidebarLoaded;

  // Add a bookmark to the bookmarks toolbar.
  let bm = await PlacesUtils.bookmarks.insert({
    url: PAGE_URI,
    title: "test icon",
    parentGuid: PlacesUtils.bookmarks.toolbarGuid
  });
  registerCleanupFunction(async function() {
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

  await new Promise(resolve => {
    PlacesUtils.favicons.setAndFetchFaviconForPage(
      PAGE_URI, ICON_URI, true,
      PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
      resolve,
      Services.scriptSecurityManager.getSystemPrincipal()
    );
  });

  // The icon is read asynchronously from the network, we don't have an easy way
  // to wait for that.
  await new Promise(resolve => {
    setTimeout(resolve, 3000);
  });

  // Assert.notEqual truncates the strings, so it is unusable here for failure
  // debugging purposes.
  let toolbarShot2 = TestUtils.screenshotArea(toolbarElt, window);
  if (toolbarShot1 != toolbarShot2) {
    info("Before toolbar: " + toolbarShot1);
    info("After toolbar: " + toolbarShot2);
  }
  Assert.notEqual(toolbarShot1, toolbarShot2, "The UI should have updated");

  let sidebarShot2 = TestUtils.screenshotArea(sidebarRect, window);
  if (sidebarShot1 != sidebarShot2) {
    info("Before sidebar: " + sidebarShot1);
    info("After sidebar: " + sidebarShot2);
  }
  Assert.notEqual(sidebarShot1, sidebarShot2, "The UI should have updated");
});

/**
 * Get Element for a bookmark in the bookmarks toolbar.
 *
 * @param guid
 *        GUID of the item to search.
 * @returns DOM Node of the element.
 */
function getNodeForToolbarItem(guid) {
  return Array.from(document.getElementById("PlacesToolbarItems").childNodes)
              .find(child => child._placesNode && child._placesNode.bookmarkGuid == guid);
}

/**
 * Get a rect for a bookmark in the bookmarks sidebar
 *
 * @param guid
 *        GUID of the item to search.
 * @returns DOM Node of the element.
 */
async function getRectForSidebarItem(guid) {
  let sidebar = document.getElementById("sidebar");
  let tree = sidebar.contentDocument.getElementById("bookmarks-view");
  tree.selectItems([guid]);
  let rect = {};
  [rect.left, rect.top, rect.width, rect.height] = tree.treeBoxObject
                                                       .selectionRegion
                                                       .getRects();
  // Adjust the position for the sidebar.
  rect.left += sidebar.getBoundingClientRect().left;
  rect.top += sidebar.getBoundingClientRect().top;
  return rect;
}
