/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Test whether or not that url field in library window will update properly
 * when changing it.
 */

const TEST_URLS = ["https://example.com/", "https://example.org/"];

add_setup(async function () {
  await PlacesUtils.bookmarks.eraseEverything();

  registerCleanupFunction(async () => {
    await PlacesUtils.bookmarks.eraseEverything();
  });
});

add_task(async function basic() {
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: TEST_URLS[0],
  });
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: TEST_URLS[1],
  });

  info("Open library for bookmarks toolbar");
  const library = await promiseLibrary("BookmarksToolbar");

  info("Check the initial content");
  const tree = library.document.getElementById("placeContent");
  Assert.equal(tree.view.rowCount, 2);
  assertRow(tree, 0, TEST_URLS[0]);
  assertRow(tree, 1, TEST_URLS[1]);

  info("Check the url");
  const newURL = `${TEST_URLS[0]}/?test`;
  await updateURL(newURL, library);

  info("Check the update");
  Assert.equal(tree.view.rowCount, 2);
  assertRow(tree, 0, newURL);
  assertRow(tree, 1, TEST_URLS[1]);

  info("Close library window");
  await promiseLibraryClosed(library);

  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function whileFiltering() {
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: TEST_URLS[0],
  });
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: TEST_URLS[1],
  });

  info("Open library for bookmarks toolbar");
  const library = await promiseLibrary("BookmarksToolbar");

  info("Check the initial content");
  const tree = library.document.getElementById("placeContent");
  Assert.equal(tree.view.rowCount, 2);
  assertRow(tree, 0, TEST_URLS[0]);
  assertRow(tree, 1, TEST_URLS[1]);

  info("Filter by search chars");
  library.PlacesSearchBox.search("org");
  Assert.equal(tree.view.rowCount, 1);
  assertRow(tree, 0, TEST_URLS[1]);

  info("Check the url");
  const newURL = `${TEST_URLS[1]}/?test`;
  await updateURL(newURL, library);

  info("Check the update");
  Assert.equal(tree.view.rowCount, 1);
  assertRow(tree, 0, newURL);

  info("Close library window");
  await promiseLibraryClosed(library);

  await PlacesUtils.bookmarks.eraseEverything();
});

async function updateURL(newURL, library) {
  const promiseUrlChange = PlacesTestUtils.waitForNotification(
    "bookmark-url-changed",
    () => true
  );
  fillBookmarkTextField("editBMPanel_locationField", newURL, library);
  await promiseUrlChange;
}

function assertRow(tree, targeRow, expectedUrl) {
  const url = tree.view.getCellText(targeRow, tree.columns.placesContentUrl);
  Assert.equal(url, expectedUrl, "URL is correct");
}
