/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Bug 451151
 * https://bugzilla.mozilla.org/show_bug.cgi?id=451151
 *
 * Summary:
 * Tests frontend Places Library searching -- search, search reset, search scope
 * consistency.
 *
 * Details:
 * Each test below
 *   1. selects a folder in the left pane and ensures that the content tree is
 *      appropriately updated,
 *   2. performs a search and ensures that the content tree is correct for the
 *      folder and search and that the search UI is visible and appropriate to
 *      folder,
 *   5. resets the search and ensures that the content tree is correct and that
 *      the search UI is hidden, and
 *   6. if folder scope was clicked, searches again and ensures folder scope
 *      remains selected.
 */

const TEST_URL = "http://dummy.mozilla.org/";
const TEST_DOWNLOAD_URL = "http://dummy.mozilla.org/dummy.pdf";

var gLibrary;

/**
 * Performs a search for a given folder and search string and ensures that the
 * URI of the right pane's content tree is as expected for the folder and search
 * string.  Also ensures that the search scope button is as expected after the
 * search.
 *
 * @param  aFolderGuid
 *         the item guid of a node in the left pane's tree
 * @param  aSearchStr
 *         the search text; may be empty to reset the search
 */
async function search(aFolderGuid, aSearchStr) {
  let doc = gLibrary.document;
  let folderTree = doc.getElementById("placesList");
  let contentTree = doc.getElementById("placeContent");

  // First, ensure that selecting the folder in the left pane updates the
  // content tree properly.
  if (aFolderGuid) {
    folderTree.selectItems([aFolderGuid]);
    Assert.notEqual(
      folderTree.selectedNode,
      null,
      "Sanity check: left pane tree should have selection after selecting!"
    );

    // The downloads folder never quite matches the url of the contentTree,
    // probably due to the way downloads are loaded.
    if (aFolderGuid !== PlacesUtils.virtualDownloadsGuid) {
      Assert.equal(
        folderTree.selectedNode.uri,
        contentTree.place,
        "Content tree's folder should be what was selected in the left pane"
      );
    }
  }

  // Second, ensure that searching updates the content tree and search UI
  // properly.
  let searchBox = doc.getElementById("searchFilter");
  searchBox.value = aSearchStr;
  gLibrary.PlacesSearchBox.search(searchBox.value);
  let query = {};
  PlacesUtils.history.queryStringToQuery(
    contentTree.result.root.uri,
    query,
    {}
  );
  if (aSearchStr) {
    Assert.equal(
      query.value.searchTerms,
      aSearchStr,
      "Content tree's searchTerms should be text in search box"
    );
  } else {
    Assert.equal(
      query.value.hasSearchTerms,
      false,
      "Content tree's searchTerms should not exist after search reset"
    );
  }
}

add_task(async function test() {
  // Add visits, a bookmark and a tag.
  await PlacesTestUtils.addVisits([
    {
      uri: Services.io.newURI(TEST_URL),
      visitDate: Date.now() * 1000,
      transition: PlacesUtils.history.TRANSITION_TYPED,
    },
    {
      uri: Services.io.newURI(TEST_DOWNLOAD_URL),
      visitDate: Date.now() * 1000,
      transition: PlacesUtils.history.TRANSITION_DOWNLOAD,
    },
  ]);

  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "dummy",
    url: TEST_URL,
  });

  PlacesUtils.tagging.tagURI(Services.io.newURI(TEST_URL), ["dummyTag"]);

  gLibrary = await promiseLibrary();

  const rootsToTest = [
    PlacesUtils.virtualAllBookmarksGuid,
    PlacesUtils.virtualHistoryGuid,
    PlacesUtils.virtualDownloadsGuid,
  ];

  for (let root of rootsToTest) {
    await search(root, "dummy");
  }

  await promiseLibraryClosed(gLibrary);

  // Cleanup.
  PlacesUtils.tagging.untagURI(Services.io.newURI(TEST_URL), ["dummyTag"]);

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
});
