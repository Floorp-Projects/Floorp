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
 * The Original Code is mozilla.com code.
 *
 * The Initial Developer of the Original Code is Mozilla Corp.
 * Portions created by the Initial Developer are Copyright (C) 2007
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

// get history service
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].getService(Ci.nsINavHistoryService);
} catch(ex) {
  do_throw("Could not get nav-history-service\n");
}

// Get bookmark service
try {
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].getService(Ci.nsINavBookmarksService);
} catch(ex) {
  do_throw("Could not get nav-bookmarks-service\n");
}

// Get annotation service
try {
  var annosvc = Cc["@mozilla.org/browser/annotation-service;1"].getService(Ci.nsIAnnotationService);
} catch(ex) {
  do_throw("Could not get annotation service\n");
} 

// Get microsummary service
try {
  var mssvc = Cc["@mozilla.org/microsummary/service;1"].getService(Ci.nsIMicrosummaryService);
} catch(ex) {
  do_throw("Could not get microsummary service\n");
}

const DESCRIPTION_ANNO = "bookmarkProperties/description";

// main
function run_test() {
  // get places import/export service
  var importer = Cc["@mozilla.org/browser/places/import-export-service;1"].getService(Ci.nsIPlacesImportExportService);

  // get file pointers
  var bookmarksFileOld = do_get_file("browser/components/places/tests/unit/bookmarks.preplaces.html");
  var bookmarksFileNew = dirSvc.get("ProfD", Ci.nsILocalFile);
  bookmarksFileNew.append("bookmarks.exported.html");

  // create bookmarks.exported.html if necessary
  if (!bookmarksFileNew.exists())
    bookmarksFileNew.create(Ci.nsILocalFile.NORMAL_FILE_TYPE, 0600);
  if (!bookmarksFileNew.exists())
    do_throw("couldn't create file: bookmarks.exported.html");

  // Test importing a non-Places canonical bookmarks file.
  // 1. import bookmarks.preplaces.html
  // 2. run the test-suite
  bmsvc.removeFolderChildren(bmsvc.bookmarksRoot);
  importer.importHTMLFromFile(bookmarksFileOld);
  testCanonicalBookmarks(bmsvc.bookmarksRoot); 

  // Test exporting a Places canonical bookmarks file.
  // 1. export to bookmarks.exported.html
  // 2. import bookmarks.exported.html
  // 3. run the test-suite
  /*
  importer.exportHTMLToFile(bookmarksFileNew);
  bmsvc.removeFolderChildren(bmsvc.bookmarksRoot);
  importer.importHTMLFromFile(bookmarksFileNew);
  testCanonicalBookmarks(bmsvc.bookmarksRoot); 
  */

  // Test importing a Places canonical bookmarks file to a specific folder.
  // 1. create a new folder
  // 2. import bookmarks.exported.html to that folder
  // 3. run the test-suite
  /*
  var testFolder = bmsvc.createFolder(bmsvc.bookmarksRoot, "test", bmsvc.DEFAULT_INDEX);
  importer.importHTMLFromFileToFolder(bookmarksFileNew, testFolder);
  testCanonicalBookmarks(testFolder); 
  */

  /*
  XXX if there are new fields we add to the bookmarks HTML format
  then test them here
  Test importing a Places canonical bookmarks file
  1. empty the bookmarks datastore
  2. import bookmarks.places.html
  3. run the test-suite
  4. run the additional-test-suite
  */

  /*
  XXX could write another more generic testsuite:
  - a function that fetches all descendant nodes of a folder with all non-static
    node properties removed, and serializes w/ toSource()
  - import a file, get serialization
  - export it, re-import, get serialization
  - do_check_eq(str1, str2)
  */
}

// Tests a bookmarks datastore that has a set of bookmarks, etc
// that flex each supported field and feature.
function testCanonicalBookmarks(aFolder) {
  // query to see if the deleted folder and items have been imported
  try {
    var query = histsvc.getNewQuery();
    query.setFolders([bmsvc.bookmarksRoot], 1);
    var result = histsvc.executeQuery(query, histsvc.getNewQueryOptions());
    var rootNode = result.root;
    rootNode.containerOpen = true;
    // get test folder
    var testFolder = rootNode.getChild(3);
    do_check_eq(testFolder.type, testFolder.RESULT_TYPE_FOLDER);
    do_check_eq(testFolder.title, "test");
    testFolder = testFolder.QueryInterface(Ci.nsINavHistoryQueryResultNode);
    do_check_eq(testFolder.hasChildren, true);
    // folder description
    do_check_true(annosvc.itemHasAnnotation(testFolder.itemId,
                                            DESCRIPTION_ANNO));
    do_check_eq("folder test comment",
                annosvc.getItemAnnotationString(testFolder.itemId,
                                                DESCRIPTION_ANNO));
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
    do_check_eq("test", bmsvc.getKeywordForBookmark(testBookmark1.itemId));
    // sidebar
    // add date 
    // last modified
    // post data
    // last charset 
    // description 
    do_check_true(annosvc.itemHasAnnotation(testBookmark1.itemId,
                                            DESCRIPTION_ANNO));
    do_check_eq("item description",
                annosvc.getItemAnnotationString(testBookmark1.itemId,
                                                DESCRIPTION_ANNO));
    /*
    // XXX Bug 380468
    // test bookmark 2
    var testBookmark2 = testFolder.getChild(1);
    // url
    do_check_eq("http://test/micsum", testBookmark2.uri);
    // title
    do_check_eq("test microsummary", testBookmark2.title);
    // check that it's a microsummary
    var micsum = mssvc.getMicrosummary(testBookmark2.itemId);
    if (!micsum)
      do_throw("Could not import microsummary");
    // check generator uri
    var generator = micsum.generator;
    do_check_eq("urn:source:http://dietrich.ganx4.com/mozilla/test-microsummary.xml", generator.uri.spec);
    // expiration and generated title can change, so don't test them
    */

    // clean up
    testFolder.containerOpen = false;
    rootNode.containerOpen = false;
  }
  catch(ex) {
    do_throw("bookmarks query tests failed: " + ex);
  }
}
