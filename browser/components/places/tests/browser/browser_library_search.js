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

var testCases = [
  function allBookmarksScope() {
    let defScope = getDefaultScope(PlacesUIUtils.allBookmarksFolderId);
    search(PlacesUIUtils.allBookmarksFolderId, "dummy", defScope);
  },

  function historyScope() {
    let defScope = getDefaultScope(PlacesUIUtils.leftPaneQueries.History);
    search(PlacesUIUtils.leftPaneQueries.History, "dummy", defScope);
  },

  function downloadsScope() {
    let defScope = getDefaultScope(PlacesUIUtils.leftPaneQueries.Downloads);
    search(PlacesUIUtils.leftPaneQueries.Downloads, "dummy", defScope);
  },
];

/**
 * Returns the default search scope for a given folder.
 *
 * @param  aFolderId
 *         the item ID of a node in the left pane's tree
 * @return the default scope when the folder is newly selected
 */
function getDefaultScope(aFolderId) {
  switch (aFolderId) {
    case PlacesUIUtils.leftPaneQueries.History:
      return "scopeBarHistory"
    case PlacesUIUtils.leftPaneQueries.Downloads:
      return "scopeBarDownloads";
    default:
      return "scopeBarAll";
  }
}

/**
 * Returns the single nsINavHistoryQuery represented by a given place URI.
 *
 * @param  aPlaceURI
 *         a URI that represents a single query
 * @return an nsINavHistoryQuery object
 */
function queryStringToQuery(aPlaceURI) {
  let queries = {};
  PlacesUtils.history.queryStringToQueries(aPlaceURI, queries, {}, {});
  return queries.value[0];
}

/**
 * Resets the search by clearing the search box's text and ensures that the
 * search scope remains as expected.
 *
 * @param  aExpectedScopeButtonId
 *         this button should be selected after the reset
 */
function resetSearch(aExpectedScopeButtonId) {
  search(null, "", aExpectedScopeButtonId);
}

/**
 * Performs a search for a given folder and search string and ensures that the
 * URI of the right pane's content tree is as expected for the folder and search
 * string.  Also ensures that the search scope button is as expected after the
 * search.
 *
 * @param  aFolderId
 *         the item ID of a node in the left pane's tree
 * @param  aSearchStr
 *         the search text; may be empty to reset the search
 * @param  aExpectedScopeButtonId
 *         after searching the selected scope button should be this
 */
function search(aFolderId, aSearchStr, aExpectedScopeButtonId) {
  let doc = gLibrary.document;
  let folderTree = doc.getElementById("placesList");
  let contentTree = doc.getElementById("placeContent");

  // First, ensure that selecting the folder in the left pane updates the
  // content tree properly.
  if (aFolderId) {
    folderTree.selectItems([aFolderId]);
    isnot(folderTree.selectedNode, null,
       "Sanity check: left pane tree should have selection after selecting!");

    // getFolders() on a History query returns an empty array, so no use
    // comparing against aFolderId in that case.
    if (aFolderId !== PlacesUIUtils.leftPaneQueries.History &&
        aFolderId !== PlacesUIUtils.leftPaneQueries.Downloads) {
      // contentTree.place should be equal to contentTree.result.root.uri,
      // but it's not until bug 476952 is fixed.
      let query = queryStringToQuery(contentTree.result.root.uri);
      is(query.getFolders()[0], aFolderId,
         "Content tree's folder should be what was selected in the left pane");
    }
  }

  // Second, ensure that searching updates the content tree and search UI
  // properly.
  let searchBox = doc.getElementById("searchFilter");
  searchBox.value = aSearchStr;
  gLibrary.PlacesSearchBox.search(searchBox.value);
  let query = queryStringToQuery(contentTree.result.root.uri);
  if (aSearchStr) {
    is(query.searchTerms, aSearchStr,
       "Content tree's searchTerms should be text in search box");
  } else {
    is(query.hasSearchTerms, false,
       "Content tree's searchTerms should not exist after search reset");
  }
}

/**
 * test() contains window-launching boilerplate that calls this to really kick
 * things off.  Add functions to the testCases array, and this will call them.
 */
function onLibraryAvailable() {
  testCases.forEach(aTest => aTest());

  gLibrary.close();
  gLibrary = null;

  // Cleanup.
  PlacesUtils.tagging.untagURI(PlacesUtils._uri(TEST_URL), ["dummyTag"]);
  PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.unfiledBookmarksFolderId);
  PlacesTestUtils.clearHistory().then(finish);
}

function test() {
  waitForExplicitFinish();

  // Sanity:
  ok(PlacesUtils, "PlacesUtils in context");

  // Add visits, a bookmark and a tag.
  PlacesTestUtils.addVisits(
    [{ uri: PlacesUtils._uri(TEST_URL), visitDate: Date.now() * 1000,
       transition: PlacesUtils.history.TRANSITION_TYPED },
     { uri: PlacesUtils._uri(TEST_DOWNLOAD_URL), visitDate: Date.now() * 1000,
       transition: PlacesUtils.history.TRANSITION_DOWNLOAD }]
    ).then(() => {
      PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                           PlacesUtils._uri(TEST_URL),
                                           PlacesUtils.bookmarks.DEFAULT_INDEX,
                                           "dummy");
      PlacesUtils.tagging.tagURI(PlacesUtils._uri(TEST_URL), ["dummyTag"]);

      gLibrary = openLibrary(onLibraryAvailable);
    });
}
