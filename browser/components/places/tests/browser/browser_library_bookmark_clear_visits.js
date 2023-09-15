/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 * Test whether or not that Most Recent Visit of bookmark in library window will
 * update properly when removing the history.
 */

const TEST_URLS = ["https://example.com/", "https://example.org/"];

add_setup(async function () {
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: TEST_URLS[0],
  });
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: TEST_URLS[1],
  });
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: TEST_URLS[1],
  });
  PlacesUtils.tagging.tagURI(makeURI(TEST_URLS[0]), ["test"]);
  PlacesUtils.tagging.tagURI(makeURI(TEST_URLS[1]), ["test"]);

  registerCleanupFunction(async () => {
    PlacesUtils.tagging.untagURI(makeURI(TEST_URLS[0]), ["test"]);
    PlacesUtils.tagging.untagURI(makeURI(TEST_URLS[1]), ["test"]);
    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesUtils.history.clear();
  });
});

add_task(async function folder() {
  info("Open bookmarked urls to update most recent visit time");
  await updateMostRecentVisitTime();

  info("Open library for bookmarks toolbar");
  const library = await promiseLibrary("BookmarksToolbar");

  info("Add Most Recent Visit column");
  await showMostRecentColumn(library);

  info("Check the initial content");
  const tree = library.document.getElementById("placeContent");
  Assert.equal(tree.view.rowCount, 3, "All bookmarks are shown");
  assertRow(tree, 0, TEST_URLS[0], true);
  assertRow(tree, 1, TEST_URLS[1], true);
  assertRow(tree, 2, TEST_URLS[1], true);

  info("Clear all visits data");
  await PlacesUtils.history.remove(TEST_URLS);

  info("Check whether or not the content are updated");
  assertRow(tree, 0, TEST_URLS[0], false);
  assertRow(tree, 1, TEST_URLS[1], false);
  assertRow(tree, 2, TEST_URLS[1], false);

  info("Close library window");
  await promiseLibraryClosed(library);
});

add_task(async function tags() {
  info("Open bookmarked urls to update most recent visit time");
  await updateMostRecentVisitTime();

  info("Open library for bookmarks toolbar");
  const library = await promiseLibrary("BookmarksToolbar");

  info("Add Most Recent Visit column");
  await showMostRecentColumn(library);

  info("Open test tag");
  const PO = library.PlacesOrganizer;
  PO.selectLeftPaneBuiltIn("Tags");
  const tagsNode = PO._places.selectedNode;
  PlacesUtils.asContainer(tagsNode).containerOpen = true;
  const tag = tagsNode.getChild(0);
  PO._places.selectNode(tag);

  info("Check the initial content");
  const tree = library.ContentTree.view;
  Assert.equal(tree.view.rowCount, 3, "All bookmarks are shown");
  assertRow(tree, 0, TEST_URLS[0], true);
  assertRow(tree, 1, TEST_URLS[1], true);
  assertRow(tree, 2, TEST_URLS[1], true);

  info("Clear all visits data");
  await PlacesUtils.history.remove(TEST_URLS);

  info("Check whether or not the content are updated");
  assertRow(tree, 0, TEST_URLS[0], false);
  assertRow(tree, 1, TEST_URLS[1], false);
  assertRow(tree, 2, TEST_URLS[1], false);

  info("Close library window");
  await promiseLibraryClosed(library);
});

async function updateMostRecentVisitTime() {
  for (const url of TEST_URLS) {
    const onLoaded = BrowserTestUtils.browserLoaded(gBrowser, false, url);
    BrowserTestUtils.startLoadingURIString(gBrowser, url);
    await onLoaded;
  }
}

async function showMostRecentColumn(library) {
  const viewMenu = library.document.getElementById("viewMenu");
  const viewMenuPopup = library.document.getElementById("viewMenuPopup");
  const onViewMenuPopup = new Promise(resolve => {
    viewMenuPopup.addEventListener("popupshown", () => resolve());
  });
  EventUtils.synthesizeMouseAtCenter(viewMenu, {}, library);
  await onViewMenuPopup;

  const viewColumns = library.document.getElementById("viewColumns");
  const viewColumnsPopup = viewColumns.querySelector("menupopup");
  const onViewColumnsPopup = new Promise(resolve => {
    viewColumnsPopup.addEventListener("popupshown", () => resolve());
  });
  EventUtils.synthesizeMouseAtCenter(viewColumns, {}, library);
  await onViewColumnsPopup;

  const mostRecentVisitColumnMenu = library.document.getElementById(
    "menucol_placesContentDate"
  );
  EventUtils.synthesizeMouseAtCenter(mostRecentVisitColumnMenu, {}, library);
}

function assertRow(tree, targeRow, expectedUrl, expectMostRecentVisitHasValue) {
  const url = tree.view.getCellText(targeRow, tree.columns.placesContentUrl);
  Assert.equal(url, expectedUrl, "URL is correct");
  const mostRecentVisit = tree.view.getCellText(
    targeRow,
    tree.columns.placesContentDate
  );
  Assert.equal(
    !!mostRecentVisit,
    expectMostRecentVisitHasValue,
    "Most Recent Visit data is in the cell correctly"
  );
}
