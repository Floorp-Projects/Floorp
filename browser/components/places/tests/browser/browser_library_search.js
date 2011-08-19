/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Places test code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Drew Willcoxon <adw@mozilla.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
 *   3. ensures that the folder scope button is disabled appropriate to the
 *      folder,
 *   4. if the folder scope button is enabled clicks it,
 *   5. resets the search and ensures that the content tree is correct and that
 *      the search UI is hidden, and
 *   6. if folder scope was clicked, searches again and ensures folder scope
 *      remains selected.
 */

const TEST_URL = "http://dummy.mozilla.org/";
const TEST_DOWNLOAD_URL = "http://dummy.mozilla.org/dummy.pdf";

let gLibrary;

let testCases = [
  function allBookmarksScope() {
    let defScope = getDefaultScope(PlacesUIUtils.allBookmarksFolderId);
    search(PlacesUIUtils.allBookmarksFolderId, "dummy", defScope);
    ok(!selectScope("scopeBarFolder"),
       "Folder scope should be disabled for All Bookmarks");
    ok(selectScope("scopeBarAll"),
       "Bookmarks scope should be enabled for All Bookmarks");
    resetSearch("scopeBarAll");
  },

  function historyScope() {
    let defScope = getDefaultScope(PlacesUIUtils.leftPaneQueries["History"]);
    search(PlacesUIUtils.leftPaneQueries["History"], "dummy", defScope);
    ok(!selectScope("scopeBarFolder"),
       "Folder scope should be disabled for History");
    ok(selectScope("scopeBarAll"),
       "Bookmarks scope should be enabled for History");
    resetSearch("scopeBarAll");
  },

  function downloadsScope() {
    let defScope = getDefaultScope(PlacesUIUtils.leftPaneQueries["Downloads"]);
    search(PlacesUIUtils.leftPaneQueries["Downloads"], "dummy", defScope);
    ok(!selectScope("scopeBarFolder"),
       "Folder scope should be disabled for Downloads");
    ok(!selectScope("scopeBarAll"),
       "Bookmarks scope should be disabled for Downloads");
    resetSearch(defScope);
  },

  function toolbarFolderScope() {
    let defScope = getDefaultScope(PlacesUtils.toolbarFolderId);
    search(PlacesUtils.toolbarFolderId, "dummy", defScope);
    ok(selectScope("scopeBarAll"),
       "Bookmarks scope should be enabled for toolbar folder");
    ok(selectScope("scopeBarFolder"),
       "Folder scope should be enabled for toolbar folder");
    // Ensure that folder scope is still selected after resetting and searching
    // again.
    resetSearch("scopeBarFolder");
    search(PlacesUtils.toolbarFolderId, "dummy", "scopeBarFolder");
  },

  function subFolderScope() {
    let folderId = PlacesUtils.bookmarks.createFolder(PlacesUtils.toolbarFolderId,
                                                      "dummy folder",
                                                      PlacesUtils.bookmarks.DEFAULT_INDEX);
    let defScope = getDefaultScope(folderId);
    search(folderId, "dummy", defScope);
    ok(selectScope("scopeBarAll"),
       "Bookmarks scope should be enabled for regularfolder");
    ok(selectScope("scopeBarFolder"),
       "Folder scope should be enabled for regular subfolder");
    // Ensure that folder scope is still selected after resetting and searching
    // again.
    resetSearch("scopeBarFolder");
    search(folderId, "dummy", "scopeBarFolder");
    PlacesUtils.bookmarks.removeItem(folderId);
  },
];

///////////////////////////////////////////////////////////////////////////////

/**
 * Returns the default search scope for a given folder.
 *
 * @param  aFolderId
 *         the item ID of a node in the left pane's tree
 * @return the default scope when the folder is newly selected
 */
function getDefaultScope(aFolderId) {
  switch (aFolderId) {
    case PlacesUIUtils.leftPaneQueries["History"]:
      return "scopeBarHistory"
    case PlacesUIUtils.leftPaneQueries["Downloads"]:
      return "scopeBarDownloads";
    default:
      return "scopeBarAll";
  }
}

/**
 * Returns the ID of the search scope button that is currently checked.
 *
 * @return the ID of the selected scope folder button
 */
function getSelectedScopeButtonId() {
  let doc = gLibrary.document;
  let scopeButtons = doc.getElementById("organizerScopeBar").childNodes;
  for (let i = 0; i < scopeButtons.length; i++) {
    if (scopeButtons[i].checked)
      return scopeButtons[i].id;
  }
  return null;
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
    if (aFolderId !== PlacesUIUtils.leftPaneQueries["History"] &&
        aFolderId !== PlacesUIUtils.leftPaneQueries["Downloads"]) {
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
    is(doc.getElementById("searchModifiers").hidden, false,
       "Scope bar should not be hidden after searching");

    let scopeButtonId = getSelectedScopeButtonId();
    if (scopeButtonId == "scopeBarDownloads" ||
        scopeButtonId == "scopeBarHistory" ||
        scopeButtonId == "scopeBarAll" ||
        aFolderId == PlacesUtils.unfiledBookmarksFolderId) {
      // Check that the target node exists in the tree's search results.
      let url, count;
      if (scopeButtonId == "scopeBarDownloads") {
        url = TEST_DOWNLOAD_URL;
        count = 1;
      }
      else {
        url = TEST_URL;
        count = scopeButtonId == "scopeBarHistory" ? 2 : 1;
      }
      is(contentTree.view.rowCount, count, "Found correct number of results");

      let node = null;
      for (let i = 0; i < contentTree.view.rowCount; i++) {
        node = contentTree.view.nodeForTreeIndex(i);
        if (node.uri === url)
          break;
      }
      isnot(node, null, "At least the target node should be in the tree");
      is(node.uri, url, "URI of node should match target URL");
    }
  }
  else {
    is(query.hasSearchTerms, false,
       "Content tree's searchTerms should not exist after search reset");
    ok(doc.getElementById("searchModifiers").hidden,
       "Scope bar should be hidden after search reset");
  }
  is(getSelectedScopeButtonId(), aExpectedScopeButtonId,
     "Proper scope button should be selected after searching or resetting");
}

/**
 * Clicks the given search scope button if it is enabled.
 *
 * @param  aScopeButtonId
 *         the button with this ID will be clicked
 * @return true if the button is enabled, false otherwise
 */
function selectScope(aScopeButtonId) {
  let doc = gLibrary.document;
  let button = doc.getElementById(aScopeButtonId);
  isnot(button, null,
     "Sanity check: scope button with ID " + aScopeButtonId + " should exist");
  // Bug 469436 may hide an inappropriate scope button instead of disabling it.
  if (button.disabled || button.hidden)
    return false;
  button.click();
  return true;
}

/**
 * test() contains window-launching boilerplate that calls this to really kick
 * things off.  Add functions to the testCases array, and this will call them.
 */
function onLibraryAvailable() {
  testCases.forEach(function (aTest) aTest());

  gLibrary.close();
  gLibrary = null;

  // Cleanup.
  PlacesUtils.tagging.untagURI(PlacesUtils._uri(TEST_URL), ["dummyTag"]);
  PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.unfiledBookmarksFolderId);
  waitForClearHistory(finish);
}

///////////////////////////////////////////////////////////////////////////////

function test() {
  waitForExplicitFinish();

  // Sanity:
  ok(PlacesUtils, "PlacesUtils in context");

  // Add visits, a bookmark and a tag.
  PlacesUtils.history.addVisit(PlacesUtils._uri(TEST_URL),
                               Date.now() * 1000, null,
                               PlacesUtils.history.TRANSITION_TYPED, false, 0);
  PlacesUtils.history.addVisit(PlacesUtils._uri(TEST_DOWNLOAD_URL),
                               Date.now() * 1000, null,
                               PlacesUtils.history.TRANSITION_DOWNLOAD, false, 0);
  PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                       PlacesUtils._uri(TEST_URL),
                                       PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       "dummy");
  PlacesUtils.tagging.tagURI(PlacesUtils._uri(TEST_URL), ["dummyTag"]);

  gLibrary = openLibrary(onLibraryAvailable);
}
