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
 * The Original Code is Bug 384370 code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dietrich Ayala <dietrich@mozilla.com>
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

const LOAD_IN_SIDEBAR_ANNO = "bookmarkProperties/loadInSidebar";
const DESCRIPTION_ANNO = "bookmarkProperties/description";
const POST_DATA_ANNO = "bookmarkProperties/POSTData";

do_check_eq(typeof PlacesUtils, "object");

// main
function run_test() {
  do_test_pending();
  /*
    HTML+FEATURES SUMMARY:
    - import legacy bookmarks
    - export as json, import, test (tests integrity of html > json)
    - export as html, import, test (tests integrity of json > html)

    BACKUP/RESTORE SUMMARY:
    - create a bookmark in each root
    - tag multiple URIs with multiple tags
    - export as json, import, test
  */

  // import the importer
  Cu.import("resource://gre/modules/BookmarkHTMLUtils.jsm");

  // avoid creating the places smart folder during tests
  Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch).
  setIntPref("browser.places.smartBookmarksVersion", -1);

  // file pointer to legacy bookmarks file
  //var bookmarksFileOld = do_get_file("bookmarks.large.html");
  var bookmarksFileOld = do_get_file("bookmarks.preplaces.html");
  // file pointer to a new places-exported json file
  var jsonFile = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
  jsonFile.append("bookmarks.exported.json");

  // create bookmarks.exported.json
  if (jsonFile.exists())
    jsonFile.remove(false);
  jsonFile.create(Ci.nsILocalFile.NORMAL_FILE_TYPE, 0600);
  if (!jsonFile.exists())
    do_throw("couldn't create file: bookmarks.exported.json");

  // Test importing a pre-Places canonical bookmarks file.
  // 1. import bookmarks.preplaces.html
  // 2. run the test-suite
  // Note: we do not empty the db before this import to catch bugs like 380999
  try {
    BookmarkHTMLUtils.importFromFile(bookmarksFileOld, true, after_import);
  } catch(ex) { do_throw("couldn't import legacy bookmarks file: " + ex); }
}

function after_import(success) {
  if (!success) {
    do_throw("Couldn't import legacy bookmarks file.");
  }
  populate();
  validate();

  waitForAsyncUpdates(function () {
    // Test exporting a Places canonical json file.
    // 1. export to bookmarks.exported.json
    // 2. empty bookmarks db
    // 3. import bookmarks.exported.json
    // 4. run the test-suite
    try {
      var jsonFile = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
      jsonFile.append("bookmarks.exported.json");
      PlacesUtils.backups.saveBookmarksToJSONFile(jsonFile);
    } catch(ex) { do_throw("couldn't export to file: " + ex); }
    LOG("exported json");
    try {
      PlacesUtils.restoreBookmarksFromJSONFile(jsonFile);
    } catch(ex) { do_throw("couldn't import the exported file: " + ex); }
    LOG("imported json");
    validate();
    LOG("validated import");

    waitForAsyncUpdates(do_test_finished);
  });
}

var tagData = [
  { uri: uri("http://slint.us"), tags: ["indie", "kentucky", "music"] },
  { uri: uri("http://en.wikipedia.org/wiki/Diplodocus"), tags: ["dinosaur", "dj", "rad word"] }
];

var bookmarkData = [
  { uri: uri("http://slint.us"), title: "indie, kentucky, music" },
  { uri: uri("http://en.wikipedia.org/wiki/Diplodocus"), title: "dinosaur, dj, rad word" }
];

/*
populate data in each folder
(menu is populated via the html import)
*/
function populate() {
  // add tags
  for each(let {uri: u, tags: t} in tagData)
    PlacesUtils.tagging.tagURI(u, t);
  
  // add unfiled bookmarks
  for each(let {uri: u, title: t} in bookmarkData) {
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.bookmarks.unfiledBookmarksFolder,
                                         u, PlacesUtils.bookmarks.DEFAULT_INDEX, t);
  }

  // add to the toolbar
  for each(let {uri: u, title: t} in bookmarkData) {
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.bookmarks.toolbarFolder,
                                         u, PlacesUtils.bookmarks.DEFAULT_INDEX, t);
  }
}

function validate() {
  testCanonicalBookmarks(PlacesUtils.bookmarks.bookmarksMenuFolder);
  testToolbarFolder();
  testUnfiledBookmarks();
  testTags();
}

// Tests a bookmarks datastore that has a set of bookmarks, etc
// that flex each supported field and feature.
function testCanonicalBookmarks() {
  // query to see if the deleted folder and items have been imported
  var query = PlacesUtils.history.getNewQuery();
  query.setFolders([PlacesUtils.bookmarks.bookmarksMenuFolder], 1);
  var result = PlacesUtils.history.executeQuery(query, PlacesUtils.history.getNewQueryOptions());
  var rootNode = result.root;
  rootNode.containerOpen = true;

  // 6-2: the toolbar contents are imported to the places-toolbar folder,
  // the separator above it is removed.
  do_check_eq(rootNode.childCount, DEFAULT_BOOKMARKS_ON_MENU + 1);

  // get test folder
  var testFolder = rootNode.getChild(DEFAULT_BOOKMARKS_ON_MENU);
  do_check_eq(testFolder.type, testFolder.RESULT_TYPE_FOLDER);
  do_check_eq(testFolder.title, "test");

  /*
  // add date 
  do_check_eq(PlacesUtils.bookmarks.getItemDateAdded(testFolder.itemId)/1000000, 1177541020);
  // last modified
  do_check_eq(PlacesUtils.bookmarks.getItemLastModified(testFolder.itemId)/1000000, 1177541050);
  */

  testFolder = testFolder.QueryInterface(Ci.nsINavHistoryQueryResultNode);
  do_check_eq(testFolder.hasChildren, true);
  // folder description
  do_check_true(PlacesUtils.annotations.itemHasAnnotation(testFolder.itemId,
                                                          DESCRIPTION_ANNO));
  do_check_eq("folder test comment",
              PlacesUtils.annotations.getItemAnnotation(testFolder.itemId, DESCRIPTION_ANNO));
  // open test folder, and test the children
  testFolder.containerOpen = true;
  var cc = testFolder.childCount;
  // XXX Bug 380468
  // do_check_eq(cc, 2);
  do_check_eq(cc, 1);

  // test bookmark 1
  var testBookmark1 = testFolder.getChild(0);
  // url
  do_check_eq("http://test/post", testBookmark1.uri);
  // title
  do_check_eq("test post keyword", testBookmark1.title);
  // keyword
  do_check_eq("test", PlacesUtils.bookmarks.getKeywordForBookmark(testBookmark1.itemId));
  // sidebar
  do_check_true(PlacesUtils.annotations.itemHasAnnotation(testBookmark1.itemId,
                                                          LOAD_IN_SIDEBAR_ANNO));
  /*
  // add date 
  do_check_eq(testBookmark1.dateAdded/1000000, 1177375336);

  // last modified
  do_check_eq(testBookmark1.lastModified/1000000, 1177375423);
  */

  // post data
  do_check_true(PlacesUtils.annotations.itemHasAnnotation(testBookmark1.itemId, POST_DATA_ANNO));
  do_check_eq("hidden1%3Dbar&text1%3D%25s",
              PlacesUtils.annotations.getItemAnnotation(testBookmark1.itemId, POST_DATA_ANNO));

  // last charset
  var testURI = PlacesUtils._uri(testBookmark1.uri);
  do_check_eq("ISO-8859-1", PlacesUtils.history.getCharsetForURI(testURI));

  // description 
  do_check_true(PlacesUtils.annotations.itemHasAnnotation(testBookmark1.itemId,
                                                          DESCRIPTION_ANNO));
  do_check_eq("item description",
              PlacesUtils.annotations.getItemAnnotation(testBookmark1.itemId,
                                                        DESCRIPTION_ANNO));

  // clean up
  testFolder.containerOpen = false;
  rootNode.containerOpen = false;
}

function testToolbarFolder() {
  var query = PlacesUtils.history.getNewQuery();
  query.setFolders([PlacesUtils.bookmarks.toolbarFolder], 1);
  var result = PlacesUtils.history.executeQuery(query, PlacesUtils.history.getNewQueryOptions());

  var toolbar = result.root;
  toolbar.containerOpen = true;

  // child count (add 2 for pre-existing items)
  do_check_eq(toolbar.childCount, bookmarkData.length + 2);
  
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

  // test added bookmark data
  var child = toolbar.getChild(2);
  do_check_eq(child.uri, bookmarkData[0].uri.spec);
  do_check_eq(child.title, bookmarkData[0].title);
  child = toolbar.getChild(3);
  do_check_eq(child.uri, bookmarkData[1].uri.spec);
  do_check_eq(child.title, bookmarkData[1].title);

  toolbar.containerOpen = false;
}

function testUnfiledBookmarks() {
  var query = PlacesUtils.history.getNewQuery();
  query.setFolders([PlacesUtils.bookmarks.unfiledBookmarksFolder], 1);
  var result = PlacesUtils.history.executeQuery(query, PlacesUtils.history.getNewQueryOptions());
  var rootNode = result.root;
  rootNode.containerOpen = true;
  // child count (add 1 for pre-existing item)
  do_check_eq(rootNode.childCount, bookmarkData.length + 1);
  for (var i = 1; i < rootNode.childCount; i++) {
    var child = rootNode.getChild(i);
    dump(bookmarkData[i - 1].uri.spec + " == " + child.uri + "?\n");
    do_check_true(bookmarkData[i - 1].uri.equals(uri(child.uri)));
    do_check_eq(child.title, bookmarkData[i - 1].title);
    /* WTF
    if (child.tags)
      do_check_eq(child.tags, bookmarkData[i].title);
    */
  }
  rootNode.containerOpen = false;
}

function testTags() {
  for each(let {uri: u, tags: t} in tagData) {
    var i = 0;
    dump("test tags for " + u.spec + ": " + t + "\n");
    var tt = PlacesUtils.tagging.getTagsForURI(u);
    dump("true tags for " + u.spec + ": " + tt + "\n");
    do_check_true(t.every(function(el) {
      i++;
      return tt.indexOf(el) > -1;
    }));
    do_check_eq(i, t.length);
  }
}
