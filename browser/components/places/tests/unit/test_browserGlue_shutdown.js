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
 * The Initial Developer of the Original Code is Mozilla Corp.
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
 * Tests that nsBrowserGlue is correctly exporting based on preferences values,
 * and creating bookmarks backup if one does not exist for today.
 */

// Initialize nsBrowserGlue.
let bg = Cc["@mozilla.org/browser/browserglue;1"].
         getService(Ci.nsIBrowserGlue);

// Initialize Places through Bookmarks Service.
let bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
         getService(Ci.nsINavBookmarksService);

// Get other services.
let ps = Cc["@mozilla.org/preferences-service;1"].
         getService(Ci.nsIPrefBranch);
let os = Cc["@mozilla.org/observer-service;1"].
         getService(Ci.nsIObserverService);

const PREF_AUTO_EXPORT_HTML = "browser.bookmarks.autoExportHTML";

const TOPIC_QUIT_APPLICATION_GRANTED = "quit-application-granted";

let tests = [];

//------------------------------------------------------------------------------

tests.push({
  description: "Export to bookmarks.html if autoExportHTML is true.",
  exec: function() {
    // Sanity check: we should have bookmarks on the toolbar.
    do_check_true(bs.getIdForItemAt(bs.toolbarFolder, 0) > 0);
    // Set preferences.
    ps.setBoolPref(PREF_AUTO_EXPORT_HTML, true);
    // Force nsBrowserGlue::_shutdownPlaces().
    os.notifyObservers(null, TOPIC_QUIT_APPLICATION_GRANTED, null);

    // Check bookmarks.html has been created.
    check_bookmarks_html();
    // Check JSON backup has been created.
    check_JSON_backup();

    // Check preferences have not been reverted.
    do_check_true(ps.getBoolPref(PREF_AUTO_EXPORT_HTML));
    // Reset preferences.
    ps.setBoolPref(PREF_AUTO_EXPORT_HTML, false);

    next_test();
  }
});

//------------------------------------------------------------------------------

tests.push({
  description: "Export to bookmarks.html if autoExportHTML is true and a bookmarks.html exists.",
  exec: function() {
    // Sanity check: we should have bookmarks on the toolbar.
    do_check_true(bs.getIdForItemAt(bs.toolbarFolder, 0) > 0);
    // Setpreferences.
    ps.setBoolPref(PREF_AUTO_EXPORT_HTML, true);
    // Create a bookmarks.html in the profile.
    let profileBookmarksHTMLFile = create_bookmarks_html("bookmarks.glue.html");
    // Get file lastModified and size.
    let lastMod = profileBookmarksHTMLFile.lastModifiedTime;
    let fileSize = profileBookmarksHTMLFile.fileSize;
    // Force nsBrowserGlue::_shutdownPlaces().
    os.notifyObservers(null, TOPIC_QUIT_APPLICATION_GRANTED, null);

    // Check a new bookmarks.html has been created.
    let profileBookmarksHTMLFile = check_bookmarks_html();
    do_check_true(profileBookmarksHTMLFile.lastModifiedTime > lastMod);
    do_check_neq(profileBookmarksHTMLFile.fileSize > fileSize);

    // Check preferences have not been reverted.
    do_check_true(ps.getBoolPref(PREF_AUTO_EXPORT_HTML));
    // Reset preferences.
    ps.setBoolPref(PREF_AUTO_EXPORT_HTML, false);

    next_test();
  }
});

//------------------------------------------------------------------------------

tests.push({
  description: "Backup to JSON should be a no-op if a backup for today already exists.",
  exec: function() {
    // Sanity check: we should have bookmarks on the toolbar.
    do_check_true(bs.getIdForItemAt(bs.toolbarFolder, 0) > 0);
    // Create a JSON backup in the profile.
    let profileBookmarksJSONFile = create_JSON_backup("bookmarks.glue.json");
    // Get file lastModified and size.
    let lastMod = profileBookmarksJSONFile.lastModifiedTime;
    let fileSize = profileBookmarksJSONFile.fileSize;
    // Force nsBrowserGlue::_shutdownPlaces().
    os.notifyObservers(null, TOPIC_QUIT_APPLICATION_GRANTED, null);

    // Check a new JSON backup has not been created.
    do_check_true(profileBookmarksJSONFile.exists());
    do_check_eq(profileBookmarksJSONFile.lastModifiedTime, lastMod);
    do_check_eq(profileBookmarksJSONFile.fileSize, fileSize);

    finish_test();
  }
});

//------------------------------------------------------------------------------

function finish_test() {
  do_test_finished();
}

var testIndex = 0;
function next_test() {
  // Remove bookmarks.html from profile.
  remove_bookmarks_html();
  // Remove JSON backups from profile.
  remove_all_JSON_backups();

  // Execute next test.
  let test = tests.shift();
  dump("\nTEST " + (++testIndex) + ": " + test.description);
  test.exec();
}

function run_test() {
  // Clean up bookmarks.
  remove_all_bookmarks();

  // Create some bookmarks.
  bs.insertBookmark(bs.bookmarksMenuFolder, uri("http://mozilla.org/"),
                    bs.DEFAULT_INDEX, "bookmark-on-menu");
  bs.insertBookmark(bs.toolbarFolder, uri("http://mozilla.org/"),
                    bs.DEFAULT_INDEX, "bookmark-on-toolbar");

  // Kick-off tests.
  do_test_pending();
  next_test();
}
