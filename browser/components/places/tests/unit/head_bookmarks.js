/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Places.
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
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

version(170);

const NS_APP_USER_PROFILE_50_DIR = "ProfD";
const NS_APP_PROFILE_DIR_STARTUP = "ProfDS";
const NS_APP_BOOKMARKS_50_FILE = "BMarks";

var Ci = Components.interfaces;
var Cc = Components.classes;
var Cr = Components.results;

function LOG(aMsg) {
  aMsg = ("*** PLACES TESTS: " + aMsg);
  Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService).
                                      logStringMessage(aMsg);
  print(aMsg);
}

var dirSvc = Cc["@mozilla.org/file/directory_service;1"].
             getService(Ci.nsIProperties);
var gTestRoot = dirSvc.get("CurProcD", Ci.nsILocalFile);
gTestRoot = gTestRoot.parent.parent;
gTestRoot.append("_tests");
gTestRoot.append("xpcshell-simple");
gTestRoot.append("test_browser_places");
gTestRoot.normalize();

// Need to create and register a profile folder.
var gProfD = gTestRoot.clone();
gProfD.append("profile");
if (gProfD.exists())
  gProfD.remove(true);
gProfD.create(Ci.nsIFile.DIRECTORY_TYPE, 0755);

var dirProvider = {
  getFile: function(prop, persistent) {
    persistent.value = true;
    if (prop == NS_APP_USER_PROFILE_50_DIR ||
        prop == NS_APP_PROFILE_DIR_STARTUP)
      return gProfD.clone();
    else if (prop == NS_APP_BOOKMARKS_50_FILE) {
      var bmarks = gProfD.clone();
      bmarks.append("bookmarks.html");
      return bmarks;
    }
    return null;
  },
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIDirectoryServiceProvider) ||
        iid.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};
dirSvc.QueryInterface(Ci.nsIDirectoryService).registerProvider(dirProvider);

var XULAppInfo = {
  vendor: "Mozilla",
  name: "PlacesTest",
  ID: "{230de50e-4cd1-11dc-8314-0800200c9a66}",
  version: "1",
  appBuildID: "2007010101",
  platformVersion: "",
  platformBuildID: "2007010101",
  inSafeMode: false,
  logConsoleErrors: true,
  OS: "XPCShell",
  XPCOMABI: "noarch-spidermonkey",

  QueryInterface: function QueryInterface(iid) {
    if (iid.equals(Ci.nsIXULAppInfo) ||
        iid.equals(Ci.nsIXULRuntime) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

var XULAppInfoFactory = {
  createInstance: function (outer, iid) {
    if (outer != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return XULAppInfo.QueryInterface(iid);
  }
};

var registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
registrar.registerFactory(Components.ID("{fbfae60b-64a4-44ef-a911-08ceb70b9f31}"),
                          "XULAppInfo", "@mozilla.org/xre/app-info;1",
                          XULAppInfoFactory);

var updateSvc = Cc["@mozilla.org/updates/update-service;1"].
                getService(Ci.nsISupports);

var iosvc = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);

function uri(spec) {
  return iosvc.newURI(spec, null, null);
}

/*
 * Removes all bookmarks and checks for correct cleanup
 */
function remove_all_bookmarks() {
  var bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
           getService(Ci.nsINavBookmarksService);
  // Clear all bookmarks
  bs.removeFolderChildren(bs.bookmarksMenuFolder);
  bs.removeFolderChildren(bs.toolbarFolder);
  bs.removeFolderChildren(bs.unfiledBookmarksFolder);
  // Check for correct cleanup
  check_no_bookmarks()
}

/*
 * Checks that we don't have any bookmark
 */
function check_no_bookmarks() {
  var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
           getService(Ci.nsINavHistoryService);
  var bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
           getService(Ci.nsINavBookmarksService);
  var query = hs.getNewQuery();
  query.setFolders([bs.toolbarFolder, bs.bookmarksMenuFolder, bs.unfiledBookmarksFolder], 3);
  var options = hs.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  var result = hs.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 0);
  root.containerOpen = false;
}

let gTestDir = do_get_file("browser/components/places/tests/unit/");
const FILENAME_BOOKMARKS_HTML = "bookmarks.html";
let backup_date = new Date().toLocaleFormat("%Y-%m-%d");
const FILENAME_BOOKMARKS_JSON = "bookmarks-" + backup_date + ".json";
// Number of smart bookmarks we have on the toolbar
const SMART_BOOKMARKS_ON_TOOLBAR = 1;

/**
 * Creates a bookmarks.html file in the profile folder from a given source file.
 *
 * @param aFilename
 *        Name of the file to copy to the profile folder.  This file must
 *        exist in the directory that contains the test files.
 *
 * @return nsIFile object for the file.
 */
function create_bookmarks_html(aFilename) {
  remove_bookmarks_html();
  let bookmarksHTMLFile = gTestDir.clone();
  bookmarksHTMLFile.append(aFilename);
  do_check_true(bookmarksHTMLFile.exists());
  bookmarksHTMLFile.copyTo(gProfD, FILENAME_BOOKMARKS_HTML);
  let profileBookmarksHTMLFile = gProfD.clone();
  profileBookmarksHTMLFile.append(FILENAME_BOOKMARKS_HTML);
  do_check_true(profileBookmarksHTMLFile.exists());
  return profileBookmarksHTMLFile;
}

/**
 * Remove bookmarks.html file from the profile folder.
 */
function remove_bookmarks_html() {
  let profileBookmarksHTMLFile = gProfD.clone();
  profileBookmarksHTMLFile.append(FILENAME_BOOKMARKS_HTML);
  if (profileBookmarksHTMLFile.exists()) {
    profileBookmarksHTMLFile.remove(false);
    do_check_false(profileBookmarksHTMLFile.exists());
  }
}

/**
 * Check bookmarks.html file exists in the profile folder.
 *
 * @return nsIFile object for the file.
 */
function check_bookmarks_html() {
  let profileBookmarksHTMLFile = gProfD.clone();
  profileBookmarksHTMLFile.append(FILENAME_BOOKMARKS_HTML);
  do_check_true(profileBookmarksHTMLFile.exists());
  return profileBookmarksHTMLFile;
}

/**
 * Creates a JSON backup in the profile folder folder from a given source file.
 *
 * @param aFilename
 *        Name of the file to copy to the profile folder.  This file must
 *        exist in the directory that contains the test files.
 *
 * @return nsIFile object for the file.
 */
function create_JSON_backup(aFilename) {
  remove_all_JSON_backups();
  let bookmarksBackupDir = gProfD.clone();
  bookmarksBackupDir.append("bookmarkbackups");
  if (!bookmarksBackupDir.exists()) {
    bookmarksBackupDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0777);
    do_check_true(bookmarksBackupDir.exists());
  }
  let bookmarksJSONFile = gTestDir.clone();
  bookmarksJSONFile.append(aFilename);
  do_check_true(bookmarksJSONFile.exists());
  bookmarksJSONFile.copyTo(bookmarksBackupDir, FILENAME_BOOKMARKS_JSON);
  let profileBookmarksJSONFile = bookmarksBackupDir.clone();
  profileBookmarksJSONFile.append(FILENAME_BOOKMARKS_JSON);
  do_check_true(profileBookmarksJSONFile.exists());
  return profileBookmarksJSONFile;
}

/**
 * Remove bookmarksbackup dir and all backups from the profile folder.
 */
function remove_all_JSON_backups() {
  let bookmarksBackupDir = gProfD.clone();
  bookmarksBackupDir.append("bookmarkbackups");
  if (bookmarksBackupDir.exists()) {
    bookmarksBackupDir.remove(true);
    do_check_false(bookmarksBackupDir.exists());
  }
}

/**
 * Check a JSON backup file for today exists in the profile folder.
 *
 * @return nsIFile object for the file.
 */
function check_JSON_backup() {
  let profileBookmarksJSONFile = gProfD.clone();
  profileBookmarksJSONFile.append("bookmarkbackups");
  profileBookmarksJSONFile.append(FILENAME_BOOKMARKS_JSON);
  do_check_true(profileBookmarksJSONFile.exists());
  return profileBookmarksJSONFile;
}
