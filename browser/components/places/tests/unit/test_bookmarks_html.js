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

// Get livemark service
try {
  var livemarksvc = Cc["@mozilla.org/browser/livemark-service;2"].getService(Ci.nsILivemarkService);
} catch(ex) {
  do_throw("Could not get livemark service\n");
} 

// Get microsummary service
try {
  var mssvc = Cc["@mozilla.org/microsummary/service;1"].getService(Ci.nsIMicrosummaryService);
} catch(ex) {
  do_throw("Could not get microsummary service\n");
}

// Get io service
try {
  var iosvc = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
} catch (ex) {
  do_throw("Could not get io service\n");
}


const DESCRIPTION_ANNO = "bookmarkProperties/description";
const LOAD_IN_SIDEBAR_ANNO = "bookmarkProperties/loadInSidebar";
const POST_DATA_ANNO = "URIProperties/POSTData";
const LAST_CHARSET_ANNO = "URIProperties/characterSet";

// main
function run_test() {
  // get places import/export service
  var importer = Cc["@mozilla.org/browser/places/import-export-service;1"].getService(Ci.nsIPlacesImportExportService);

  // file pointer to legacy bookmarks file
  var bookmarksFileOld = do_get_file("browser/components/places/tests/unit/bookmarks.preplaces.html");
  // file pointer to a new places-exported bookmarks file
  var bookmarksFileNew = dirSvc.get("ProfD", Ci.nsILocalFile);
  bookmarksFileNew.append("bookmarks.exported.html");

  // create bookmarks.exported.html
  if (bookmarksFileNew.exists())
    bookmarksFileNew.remove(false);
  bookmarksFileNew.create(Ci.nsILocalFile.NORMAL_FILE_TYPE, 0600);
  if (!bookmarksFileNew.exists())
    do_throw("couldn't create file: bookmarks.exported.html");

  // Test importing a pre-Places canonical bookmarks file.
  // 1. import bookmarks.preplaces.html
  // 2. run the test-suite
  // Note: we do not empty the db before this import to catch bugs like 380999
  try {
    importer.importHTMLFromFile(bookmarksFileOld, true);
  } catch(ex) { do_throw("couldn't import legacy bookmarks file: " + ex); }
  testCanonicalBookmarks(bmsvc.bookmarksMenuFolder); 

  // Test exporting a Places canonical bookmarks file.
  // 1. export to bookmarks.exported.html
  // 2. empty bookmarks db
  // 3. import bookmarks.exported.html
  // 4. run the test-suite
  try {
    importer.exportHTMLToFile(bookmarksFileNew);
  } catch(ex) { do_throw("couldn't export to file: " + ex); }
  bmsvc.removeFolderChildren(bmsvc.bookmarksMenuFolder);
  try {
    importer.importHTMLFromFile(bookmarksFileNew, true);
  } catch(ex) { do_throw("couldn't import the exported file: " + ex); }
  testCanonicalBookmarks(bmsvc.bookmarksMenuFolder); 
  /*
  // XXX import-to-folder tests disabled due to bug 363634
  // Test importing a pre-Places canonical bookmarks file to a specific folder.
  // 1. create a new folder
  // 2. import bookmarks.preplaces.html to that folder
  // 3. run the test-suite
  var testFolder = bmsvc.createFolder(bmsvc.bookmarksMenuFolder, "test-import", bmsvc.DEFAULT_INDEX);
  try {
    importer.importHTMLFromFileToFolder(bookmarksFileOld, testFolder, false);
  } catch(ex) { do_throw("couldn't import the exported file to folder: " + ex); }
  testCanonicalBookmarks(testFolder);
  bmsvc.removeFolder(testFolder);

  // Test importing a Places canonical bookmarks file to a specific folder.
  // 1. create a new folder
  // 2. import bookmarks.exported.html to that folder
  // 3. run the test-suite
  var testFolder = bmsvc.createFolder(bmsvc.bookmarksMenuFolder, "test-import", bmsvc.DEFAULT_INDEX);
  try {
    importer.importHTMLFromFileToFolder(bookmarksFileNew, testFolder, false);
  } catch(ex) { do_throw("couldn't import the exported file to folder: " + ex); }
  testCanonicalBookmarks(testFolder); 
  bmsvc.removeFolder(testFolder);

  // XXX Disabled due to bug 381129 - separators will be duplicated on re-import
  // Test importing the exported bookmarks.html file *on top of* the existing
  // bookmarks. This tests import of IDs. If we support IDs correctly, there
  // should be no difference after the import.
  // 1. empty bookmarks db
  // 2. import the exported bookmarks file
  // 3. export to file
  // 3. import the exported bookmarks file
  // 4. run the test-suite
  bmsvc.removeFolderChildren(bmsvc.bookmarksMenuFolder);
  try {
    importer.importHTMLFromFile(bookmarksFileNew, true);
  } catch(ex) { do_throw("couldn't import the exported file: " + ex); }
  try {
    importer.exportHTMLToFile(bookmarksFileNew);
  } catch(ex) { do_throw("couldn't export to file: " + ex); }
  try {
    importer.importHTMLFromFile(bookmarksFileNew, true);
  } catch(ex) { do_throw("couldn't import the exported file: " + ex); }
  testCanonicalBookmarks(bmsvc.bookmarksMenuFolder);
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
}

// Tests a bookmarks datastore that has a set of bookmarks, etc
// that flex each supported field and feature.
function testCanonicalBookmarks(aFolder) {
  // query to see if the deleted folder and items have been imported
  var query = histsvc.getNewQuery();
  query.setFolders([aFolder], 1);
  var result = histsvc.executeQuery(query, histsvc.getNewQueryOptions());
  var rootNode = result.root;
  rootNode.containerOpen = true;

  // 6-2: the toolbar contents are imported to the places-toolbar folder,
  // the separator above it is removed.
  do_check_eq(rootNode.childCount, 4);

  // get test folder
  var testFolder = rootNode.getChild(3);
  do_check_eq(testFolder.type, testFolder.RESULT_TYPE_FOLDER);
  do_check_eq(testFolder.title, "test");

  // add date 
  do_check_eq(bmsvc.getItemDateAdded(testFolder.itemId)/1000000, 1177541020);
  // last modified
  do_check_eq(bmsvc.getItemLastModified(testFolder.itemId)/1000000, 1177541050);

  testFolder = testFolder.QueryInterface(Ci.nsINavHistoryQueryResultNode);
  do_check_eq(testFolder.hasChildren, true);
  // folder description
  do_check_true(annosvc.itemHasAnnotation(testFolder.itemId,
                                          DESCRIPTION_ANNO));
  do_check_eq("folder test comment",
              annosvc.getItemAnnotation(testFolder.itemId, DESCRIPTION_ANNO));
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
  do_check_true(annosvc.itemHasAnnotation(testBookmark1.itemId,
                                          LOAD_IN_SIDEBAR_ANNO));
  // add date 
  do_check_eq(testBookmark1.dateAdded/1000000, 1177375336);

  // last modified
  do_check_eq(testBookmark1.lastModified/1000000, 1177375423);

  // post data
  var pageURI = iosvc.newURI(testBookmark1.uri, "", null);
  do_check_true(annosvc.pageHasAnnotation(pageURI, POST_DATA_ANNO));
  do_check_eq("hidden1%3Dbar&text1%3D%25s",
              annosvc.getPageAnnotation(pageURI, POST_DATA_ANNO));
  // last charset 
  do_check_true(annosvc.pageHasAnnotation(pageURI, LAST_CHARSET_ANNO));
  do_check_eq("ISO-8859-1", annosvc.getPageAnnotation(pageURI,
                                                      LAST_CHARSET_ANNO));
  // description 
  do_check_true(annosvc.itemHasAnnotation(testBookmark1.itemId,
                                          DESCRIPTION_ANNO));
  do_check_eq("item description",
              annosvc.getItemAnnotation(testBookmark1.itemId,
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

  query.setFolders([bmsvc.toolbarFolder], 1);
  result = histsvc.executeQuery(query, histsvc.getNewQueryOptions());
  // bookmarks toolbar
  var toolbar = result.root;
  toolbar.containerOpen = true;
  do_check_eq(toolbar.childCount, 2);
  
  // livemark
  var livemark = toolbar.getChild(1);
  // title
  do_check_eq("Latest Headlines", livemark.title);
  // livemark check
  do_check_true(livemarksvc.isLivemark(livemark.itemId));
  // site url
  do_check_eq("http://en-us.fxfeeds.mozilla.com/en-US/firefox/livebookmarks/",
              livemarksvc.getSiteURI(livemark.itemId).spec);
  // feed url
  do_check_eq("http://en-us.fxfeeds.mozilla.com/en-US/firefox/headlines.xml",
              livemarksvc.getFeedURI(livemark.itemId).spec);

  toolbar.containerOpen = false;
}
