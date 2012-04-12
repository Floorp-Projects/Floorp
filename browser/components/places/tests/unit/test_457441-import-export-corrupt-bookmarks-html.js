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
 * The Original Code is Bug 457441 code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Marco Bonardo <mak77@bonardo.net> (Original Author)
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

/*
 * This test ensures that importing/exporting to HTML does not stop
 * if a malformed uri is found.
 */

// Get Services
var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);
var dbConn = hs.QueryInterface(Ci.nsPIPlacesDatabase).DBConnection;
var bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
         getService(Ci.nsINavBookmarksService);
var as = Cc["@mozilla.org/browser/annotation-service;1"].
         getService(Ci.nsIAnnotationService);
var icos = Cc["@mozilla.org/browser/favicon-service;1"].
           getService(Ci.nsIFaviconService);
var ps = Cc["@mozilla.org/preferences-service;1"].
         getService(Ci.nsIPrefBranch);
var ies = Cc["@mozilla.org/browser/places/import-export-service;1"].
          getService(Ci.nsIPlacesImportExportService);
Cu.import("resource://gre/modules/BookmarkHTMLUtils.jsm");

const DESCRIPTION_ANNO = "bookmarkProperties/description";
const LOAD_IN_SIDEBAR_ANNO = "bookmarkProperties/loadInSidebar";
const POST_DATA_ANNO = "bookmarkProperties/POSTData";

const TEST_FAVICON_PAGE_URL = "http://en-US.www.mozilla.com/en-US/firefox/central/";
const TEST_FAVICON_DATA_SIZE = 580;

function run_test() {
  do_test_pending();

  // avoid creating the places smart folder during tests
  ps.setIntPref("browser.places.smartBookmarksVersion", -1);

  // import bookmarks from corrupt file
  var corruptBookmarksFile = do_get_file("bookmarks.corrupt.html");
  try {
    BookmarkHTMLUtils.importFromFile(corruptBookmarksFile, true, after_import);
  } catch(ex) { do_throw("couldn't import corrupt bookmarks file: " + ex); }
}

function after_import(success) {
  if (!success) {
    do_throw("Couldn't import corrupt bookmarks file.");
  }

  // Check that every bookmark is correct
  // Corrupt bookmarks should not have been imported
  database_check(function () {
    // Create corruption in database
    var corruptItemId = bs.insertBookmark(bs.toolbarFolder,
                                          uri("http://test.mozilla.org"),
                                          bs.DEFAULT_INDEX, "We love belugas");
    var stmt = dbConn.createStatement("UPDATE moz_bookmarks SET fk = NULL WHERE id = :itemId");
    stmt.params.itemId = corruptItemId;
    stmt.execute();
    stmt.finalize();

    // Export bookmarks
    var bookmarksFile = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
    bookmarksFile.append("bookmarks.exported.html");
    if (bookmarksFile.exists())
      bookmarksFile.remove(false);
    bookmarksFile.create(Ci.nsILocalFile.NORMAL_FILE_TYPE, 0600);
    if (!bookmarksFile.exists())
      do_throw("couldn't create file: bookmarks.exported.html");
    try {
      ies.exportHTMLToFile(bookmarksFile);
    } catch(ex) { do_throw("couldn't export to bookmarks.exported.html: " + ex); }

    // Clear all bookmarks
    remove_all_bookmarks();

    // Import bookmarks
    try {
    BookmarkHTMLUtils.importFromFile(bookmarksFile, true, before_database_check);
    } catch(ex) { do_throw("couldn't import the exported file: " + ex); }
  });
}

function before_database_check(success) {
  // Check that every bookmark is correct
  database_check(do_test_finished);
}

/*
 * Check for imported bookmarks correctness
 *
 * @param aCallback
 *        Called when the checks are finished.
 */
function database_check(aCallback) {
  // BOOKMARKS MENU
  var query = hs.getNewQuery();
  query.setFolders([bs.bookmarksMenuFolder], 1);
  var options = hs.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  var result = hs.executeQuery(query, options);
  var rootNode = result.root;
  rootNode.containerOpen = true;
  do_check_eq(rootNode.childCount, DEFAULT_BOOKMARKS_ON_MENU + 1);

  // get test folder
  var testFolder = rootNode.getChild(DEFAULT_BOOKMARKS_ON_MENU);
  do_check_eq(testFolder.type, testFolder.RESULT_TYPE_FOLDER);
  do_check_eq(testFolder.title, "test");
  // add date
  do_check_eq(bs.getItemDateAdded(testFolder.itemId)/1000000, 1177541020);
  // last modified
  do_check_eq(bs.getItemLastModified(testFolder.itemId)/1000000, 1177541050);
  testFolder = testFolder.QueryInterface(Ci.nsINavHistoryQueryResultNode);
  do_check_eq(testFolder.hasChildren, true);
  // folder description
  do_check_true(as.itemHasAnnotation(testFolder.itemId,
                                          DESCRIPTION_ANNO));
  do_check_eq("folder test comment",
              as.getItemAnnotation(testFolder.itemId, DESCRIPTION_ANNO));
  // open test folder, and test the children
  testFolder.containerOpen = true;
  var cc = testFolder.childCount;
  do_check_eq(cc, 1);

  // test bookmark 1
  var testBookmark1 = testFolder.getChild(0);
  // url
  do_check_eq("http://test/post", testBookmark1.uri);
  // title
  do_check_eq("test post keyword", testBookmark1.title);
  // keyword
  do_check_eq("test", bs.getKeywordForBookmark(testBookmark1.itemId));
  // sidebar
  do_check_true(as.itemHasAnnotation(testBookmark1.itemId,
                                          LOAD_IN_SIDEBAR_ANNO));
  // add date
  do_check_eq(testBookmark1.dateAdded/1000000, 1177375336);
  // last modified
  do_check_eq(testBookmark1.lastModified/1000000, 1177375423);
  // post data
  do_check_true(as.itemHasAnnotation(testBookmark1.itemId,
                                          POST_DATA_ANNO));
  do_check_eq("hidden1%3Dbar&text1%3D%25s",
              as.getItemAnnotation(testBookmark1.itemId, POST_DATA_ANNO));
  // last charset
  var testURI = uri(testBookmark1.uri);
  do_check_eq("ISO-8859-1", hs.getCharsetForURI(testURI));
  // description
  do_check_true(as.itemHasAnnotation(testBookmark1.itemId,
                                          DESCRIPTION_ANNO));
  do_check_eq("item description",
              as.getItemAnnotation(testBookmark1.itemId,
                                        DESCRIPTION_ANNO));

  // clean up
  testFolder.containerOpen = false;
  rootNode.containerOpen = false;

  // BOOKMARKS TOOLBAR
  query.setFolders([bs.toolbarFolder], 1);
  result = hs.executeQuery(query, hs.getNewQueryOptions());
  var toolbar = result.root;
  toolbar.containerOpen = true;
  do_check_eq(toolbar.childCount, 3);
  
  // livemark
  var livemark = toolbar.getChild(1);
  // title
  do_check_eq("Latest Headlines", livemark.title);
  PlacesUtils.livemarks.getLivemark(
    { id: livemark.itemId },
    function (aStatus, aLivemark) {
      do_check_true(Components.isSuccessCode(aStatus));
      do_check_eq("http://en-us.fxfeeds.mozilla.com/en-US/firefox/livebookmarks/",
                  aLivemark.siteURI.spec);
      do_check_eq("http://en-us.fxfeeds.mozilla.com/en-US/firefox/headlines.xml",
                  aLivemark.feedURI.spec);
    }
  );

  // cleanup
  toolbar.containerOpen = false;

  // UNFILED BOOKMARKS
  query.setFolders([bs.unfiledBookmarksFolder], 1);
  result = hs.executeQuery(query, hs.getNewQueryOptions());
  var unfiledBookmarks = result.root;
  unfiledBookmarks.containerOpen = true;
  do_check_eq(unfiledBookmarks.childCount, 1);
  unfiledBookmarks.containerOpen = false;

  // favicons
  icos.getFaviconDataForPage(uri(TEST_FAVICON_PAGE_URL),
    function DC_onComplete(aURI, aDataLen, aData, aMimeType) {
      // aURI should never be null when aDataLen > 0.
      do_check_neq(aURI, null);
      // Favicon data is stored in the bookmarks file as a "data:" URI.  For
      // simplicity, instead of converting the data we receive to a "data:" URI
      // and comparing it, we just check the data size.
      do_check_eq(TEST_FAVICON_DATA_SIZE, aDataLen);
      aCallback();
    });
}
