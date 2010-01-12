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
 * The Original Code is Places Unit Test code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Marco Bonardo <mak77@bonardo.net>
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
 * Tests that nsBrowserGlue is correctly interpreting the preferences settable
 * by the user or by other components.
 */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

// Initialize Places.
var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);
var bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
         getService(Ci.nsINavBookmarksService);
// Get other services.
var ps = Cc["@mozilla.org/preferences-service;1"].
         getService(Ci.nsIPrefBranch);
var os = Cc["@mozilla.org/observer-service;1"].
         getService(Ci.nsIObserverService);
var as = Cc["@mozilla.org/browser/annotation-service;1"].
         getService(Ci.nsIAnnotationService);

const PREF_SMART_BOOKMARKS_VERSION = "browser.places.smartBookmarksVersion";
const PREF_AUTO_EXPORT_HTML = "browser.bookmarks.autoExportHTML";
const PREF_IMPORT_BOOKMARKS_HTML = "browser.places.importBookmarksHTML";
const PREF_RESTORE_DEFAULT_BOOKMARKS = "browser.bookmarks.restore_default_bookmarks";

const SMART_BOOKMARKS_ANNO = "Places/SmartBookmark";

/**
 * Rebuilds smart bookmarks listening to console output to report any message or
 * exception generated when calling ensurePlacesDefaultQueriesInitialized().
 */
function rebuildSmartBookmarks() {
  var consoleListener = {
    observe: function(aMsg) {
      print("Got console message: " + aMsg.message);
    },

    QueryInterface: XPCOMUtils.generateQI([
      Ci.nsIConsoleListener
    ]),
  };
  var console = Cc["@mozilla.org/consoleservice;1"].
                getService(Ci.nsIConsoleService);
  console.reset();
  console.registerListener(consoleListener);
  var bg = Cc["@mozilla.org/browser/browserglue;1"].
           getService(Ci.nsIBrowserGlue);
  bg.ensurePlacesDefaultQueriesInitialized();
  console.unregisterListener(consoleListener);
}


var tests = [];
//------------------------------------------------------------------------------

tests.push({
  description: "All smart bookmarks are created if smart bookmarks version is 0.",
  exec: function() {
    // Sanity check: we should have default bookmark.
    do_check_neq(bs.getIdForItemAt(bs.toolbarFolder, 0), -1);
    do_check_neq(bs.getIdForItemAt(bs.bookmarksMenuFolder, 0), -1);

    // Set preferences.
    ps.setIntPref(PREF_SMART_BOOKMARKS_VERSION, 0);

    rebuildSmartBookmarks();

    // Count items.
    do_check_eq(countFolderChildren(bs.toolbarFolder),
                SMART_BOOKMARKS_ON_TOOLBAR + DEFAULT_BOOKMARKS_ON_TOOLBAR);
    do_check_eq(countFolderChildren(bs.bookmarksMenuFolder),
                SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

    // Check version has been updated.
    do_check_eq(ps.getIntPref(PREF_SMART_BOOKMARKS_VERSION),
                SMART_BOOKMARKS_VERSION);

    next_test();
  }
});

//------------------------------------------------------------------------------

tests.push({
  description: "An existing smart bookmark is replaced when version changes.",
  exec: function() {
    // Sanity check: we have a smart bookmark on the toolbar.
    var itemId = bs.getIdForItemAt(bs.toolbarFolder, 0);
    do_check_neq(itemId, -1);
    do_check_true(as.itemHasAnnotation(itemId, SMART_BOOKMARKS_ANNO));
    // Change its title.
    bs.setItemTitle(itemId, "new title");
    do_check_eq(bs.getItemTitle(itemId), "new title");

    // Sanity check items.
    do_check_eq(countFolderChildren(bs.toolbarFolder),
                SMART_BOOKMARKS_ON_TOOLBAR + DEFAULT_BOOKMARKS_ON_TOOLBAR);
    do_check_eq(countFolderChildren(bs.bookmarksMenuFolder),
                SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

    // Set preferences.
    ps.setIntPref(PREF_SMART_BOOKMARKS_VERSION, 1);

    rebuildSmartBookmarks();

    // Count items.
    do_check_eq(countFolderChildren(bs.toolbarFolder),
                SMART_BOOKMARKS_ON_TOOLBAR + DEFAULT_BOOKMARKS_ON_TOOLBAR);
    do_check_eq(countFolderChildren(bs.bookmarksMenuFolder),
                SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

    // Check smart bookmark has been replaced, itemId has changed.
    itemId = bs.getIdForItemAt(bs.toolbarFolder, 0);
    do_check_neq(itemId, -1);
    do_check_neq(bs.getItemTitle(itemId), "new title");
    do_check_true(as.itemHasAnnotation(itemId, SMART_BOOKMARKS_ANNO));

    // Check version has been updated.
    do_check_eq(ps.getIntPref(PREF_SMART_BOOKMARKS_VERSION),
                SMART_BOOKMARKS_VERSION);

    next_test();
  }
});

//------------------------------------------------------------------------------

tests.push({
  description: "An explicitly removed smart bookmark should not be recreated.",
  exec: function() {   
    // Remove toolbar's smart bookmarks
    bs.removeItem(bs.getIdForItemAt(bs.toolbarFolder, 0));

    // Sanity check items.
    do_check_eq(countFolderChildren(bs.toolbarFolder),
                DEFAULT_BOOKMARKS_ON_TOOLBAR);
    do_check_eq(countFolderChildren(bs.bookmarksMenuFolder),
                SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

    // Set preferences.
    ps.setIntPref(PREF_SMART_BOOKMARKS_VERSION, 1);

    rebuildSmartBookmarks();

    // Count items.
    // We should not have recreated the smart bookmark on toolbar.
    do_check_eq(countFolderChildren(bs.toolbarFolder),
                DEFAULT_BOOKMARKS_ON_TOOLBAR);
    do_check_eq(countFolderChildren(bs.bookmarksMenuFolder),
                SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

    // Check version has been updated.
    do_check_eq(ps.getIntPref(PREF_SMART_BOOKMARKS_VERSION),
                SMART_BOOKMARKS_VERSION);

    next_test();
  }
});

//------------------------------------------------------------------------------

tests.push({
  description: "Even if a smart bookmark has been removed recreate it if version is 0.",
  exec: function() {
    // Sanity check items.
    do_check_eq(countFolderChildren(bs.toolbarFolder),
                DEFAULT_BOOKMARKS_ON_TOOLBAR);
    do_check_eq(countFolderChildren(bs.bookmarksMenuFolder),
                SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

    // Set preferences.
    ps.setIntPref(PREF_SMART_BOOKMARKS_VERSION, 0);

    rebuildSmartBookmarks();

    // Count items.
    // We should not have recreated the smart bookmark on toolbar.
    do_check_eq(countFolderChildren(bs.toolbarFolder),
                SMART_BOOKMARKS_ON_TOOLBAR + DEFAULT_BOOKMARKS_ON_TOOLBAR);
    do_check_eq(countFolderChildren(bs.bookmarksMenuFolder),
                SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

    // Check version has been updated.
    do_check_eq(ps.getIntPref(PREF_SMART_BOOKMARKS_VERSION),
                SMART_BOOKMARKS_VERSION);

    next_test();
  }
});
//------------------------------------------------------------------------------

function countFolderChildren(aFolderItemId) {
  var query = hs.getNewQuery();
  query.setFolders([aFolderItemId], 1);
  var options = hs.getNewQueryOptions();
  var rootNode = hs.executeQuery(query, options).root;
  rootNode.containerOpen = true;
  var cc = rootNode.childCount;
  // Dump contents.
  for (var i = 0; i < cc ; i++) {
    var node = rootNode.getChild(i);
    print("Found child at " + i + ": " + node.title);
  }
  rootNode.containerOpen = false;
  return cc;
}

var testIndex = 0;
function next_test() {
  if (tests.length) {
    // Execute next test.
    let test = tests.shift();
    print("\nTEST " + (++testIndex) + ": " + test.description);
    test.exec();
  }
  else {
    // Clean up database from all bookmarks.
    remove_all_bookmarks();
    do_test_finished();
  }
}

function run_test() {
  // Bug 510219.
  // Disabled on Windows due to almost permanent failure.
  if ("@mozilla.org/windows-registry-key;1" in Components.classes)
    return;

  do_test_pending();

  remove_bookmarks_html();
  remove_all_JSON_backups();

  // Initialize browserGlue.
  var bg = Cc["@mozilla.org/browser/browserglue;1"].
           getService(Ci.nsIBrowserGlue);
  bg.QueryInterface(Ci.nsIObserver).observe(null, "places-init-complete", null);

  // Ensure preferences status.
  do_check_false(ps.getBoolPref(PREF_AUTO_EXPORT_HTML));
  try {
    do_check_false(ps.getBoolPref(PREF_IMPORT_BOOKMARKS_HTML));
    do_throw("importBookmarksHTML pref should not exist");
  }
  catch(ex) {}
  do_check_false(ps.getBoolPref(PREF_RESTORE_DEFAULT_BOOKMARKS));

  // Kick-off tests.
  next_test();
}
