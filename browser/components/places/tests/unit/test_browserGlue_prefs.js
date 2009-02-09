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
 * Tests that nsBrowserGlue is correctly interpreting the preferences settable
 * by the user or by other components.
 */

// Initialize browserGlue.
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

const PREF_IMPORT_BOOKMARKS_HTML = "browser.places.importBookmarksHTML";
const PREF_RESTORE_DEFAULT_BOOKMARKS = "browser.bookmarks.restore_default_bookmarks";
const PREF_SMART_BOOKMARKS_VERSION = "browser.places.smartBookmarksVersion";
const PREF_AUTO_EXPORT_HTML = "browser.bookmarks.autoExportHTML";

const TOPIC_PLACES_INIT_COMPLETE = "places-init-complete";

let tests = [];

//------------------------------------------------------------------------------

tests.push({
  description: "Import from bookmarks.html if importBookmarksHTML is true.",
  exec: function() {
    // Sanity check: we should not have any bookmark on the toolbar.
    do_check_eq(bs.getIdForItemAt(bs.toolbarFolder, 0), -1);
    // Set preferences.
    ps.setBoolPref(PREF_IMPORT_BOOKMARKS_HTML, true);
    // Force nsBrowserGlue::_initPlaces().
    os.notifyObservers(null, TOPIC_PLACES_INIT_COMPLETE, null);

    // Check bookmarks.html has been imported, and a smart bookmark has been
    // created.
    let itemId = bs.getIdForItemAt(bs.toolbarFolder,
                                   SMART_BOOKMARKS_ON_TOOLBAR);
    do_check_eq(bs.getItemTitle(itemId), "example");
    // Check preferences have been reverted.
    do_check_false(ps.getBoolPref(PREF_IMPORT_BOOKMARKS_HTML));

    next_test();
  }
});

//------------------------------------------------------------------------------

tests.push({
  description: "import from bookmarks.html, but don't create smart bookmarks if they are disabled",
  exec: function() {
    // Sanity check: we should not have any bookmark on the toolbar.
    do_check_eq(bs.getIdForItemAt(bs.toolbarFolder, 0), -1);
    // Set preferences.
    ps.setIntPref(PREF_SMART_BOOKMARKS_VERSION, -1);
    ps.setBoolPref(PREF_IMPORT_BOOKMARKS_HTML, true);
    // Force nsBrowserGlue::_initPlaces().
    os.notifyObservers(null, TOPIC_PLACES_INIT_COMPLETE, null);

    // Check bookmarks.html has been imported, but smart bookmarks have not
    // been created.
    let itemId = bs.getIdForItemAt(bs.toolbarFolder, 0);
    do_check_eq(bs.getItemTitle(itemId), "example");
    // Check preferences have been reverted.
    do_check_false(ps.getBoolPref(PREF_IMPORT_BOOKMARKS_HTML));

    next_test();
  }
});

//------------------------------------------------------------------------------

tests.push({
  description: "Import from bookmarks.html, but don't create smart bookmarks if autoExportHTML is true and they are at latest version",
  exec: function() {
    // Sanity check: we should not have any bookmark on the toolbar.
    do_check_eq(bs.getIdForItemAt(bs.toolbarFolder, 0), -1);
    // Set preferences.
    ps.setIntPref(PREF_SMART_BOOKMARKS_VERSION, 999);
    ps.setBoolPref(PREF_AUTO_EXPORT_HTML, true);
    ps.setBoolPref(PREF_IMPORT_BOOKMARKS_HTML, true);
    // Force nsBrowserGlue::_initPlaces()
    os.notifyObservers(null, TOPIC_PLACES_INIT_COMPLETE, null);

    // Check bookmarks.html has been imported, but smart bookmarks have not
    // been created.
    let itemId = bs.getIdForItemAt(bs.toolbarFolder, 0);
    do_check_eq(bs.getItemTitle(itemId), "example");
    do_check_false(ps.getBoolPref(PREF_IMPORT_BOOKMARKS_HTML));
    // Check preferences have been reverted.
    ps.setBoolPref(PREF_AUTO_EXPORT_HTML, false);

    next_test();
  }
});

//------------------------------------------------------------------------------

tests.push({
  description: "Import from bookmarks.html, and create smart bookmarks if autoExportHTML is true and they are not at latest version.",
  exec: function() {
    // Sanity check: we should not have any bookmark on the toolbar.
    do_check_eq(bs.getIdForItemAt(bs.toolbarFolder, 0), -1);
    // Set preferences.
    ps.setIntPref(PREF_SMART_BOOKMARKS_VERSION, 0);
    ps.setBoolPref(PREF_AUTO_EXPORT_HTML, true);
    ps.setBoolPref(PREF_IMPORT_BOOKMARKS_HTML, true);
    // Force nsBrowserGlue::_initPlaces()
    os.notifyObservers(null, TOPIC_PLACES_INIT_COMPLETE, null);

    // Check bookmarks.html has been imported, but smart bookmarks have not
    // been created.
    let itemId = bs.getIdForItemAt(bs.toolbarFolder, SMART_BOOKMARKS_ON_TOOLBAR);
    do_check_eq(bs.getItemTitle(itemId), "example");
    do_check_false(ps.getBoolPref(PREF_IMPORT_BOOKMARKS_HTML));
    // Check preferences have been reverted.
    ps.setBoolPref(PREF_AUTO_EXPORT_HTML, false);

    next_test();
  }
});

//------------------------------------------------------------------------------
tests.push({
  description: "restore from default bookmarks.html if restore_default_bookmarks is true.",
  exec: function() {
    // Sanity check: we should not have any bookmark on the toolbar.
    do_check_eq(bs.getIdForItemAt(bs.toolbarFolder, 0), -1);
    // Set preferences.
    ps.setBoolPref(PREF_RESTORE_DEFAULT_BOOKMARKS, true);
    // Force nsBrowserGlue::_initPlaces()
    os.notifyObservers(null, TOPIC_PLACES_INIT_COMPLETE, null);

    // Check bookmarks.html has been restored.
    let itemId = bs.getIdForItemAt(bs.toolbarFolder, SMART_BOOKMARKS_ON_TOOLBAR + 1);
    do_check_true(itemId > 0);
    // Check preferences have been reverted.
    do_check_false(ps.getBoolPref(PREF_RESTORE_DEFAULT_BOOKMARKS));

    next_test();
  }
});

//------------------------------------------------------------------------------

tests.push({
  description: "setting both importBookmarksHTML and restore_default_bookmarks should restore defaults.",
  exec: function() {
    // Sanity check: we should not have any bookmark on the toolbar.
    do_check_eq(bs.getIdForItemAt(bs.toolbarFolder, 0), -1);
    // Set preferences.
    ps.setBoolPref(PREF_IMPORT_BOOKMARKS_HTML, true);
    ps.setBoolPref(PREF_RESTORE_DEFAULT_BOOKMARKS, true);
    // Force nsBrowserGlue::_initPlaces()
    os.notifyObservers(null, TOPIC_PLACES_INIT_COMPLETE, null);

    // Check bookmarks.html has been restored.
    let itemId = bs.getIdForItemAt(bs.toolbarFolder, SMART_BOOKMARKS_ON_TOOLBAR + 1);
    do_check_true(itemId > 0);
    // Check preferences have been reverted.
    do_check_false(ps.getBoolPref(PREF_RESTORE_DEFAULT_BOOKMARKS));
    do_check_false(ps.getBoolPref(PREF_IMPORT_BOOKMARKS_HTML));

    finish_test();
  }
});

//------------------------------------------------------------------------------

function finish_test() {
  do_test_finished();
}

var testIndex = 0;
function next_test() {
  // Clean up database from all bookmarks.
  remove_all_bookmarks();

  // nsBrowserGlue stops observing topics after first notification,
  // so we add back the observer to test additional runs.
  os.addObserver(bg, TOPIC_PLACES_INIT_COMPLETE, false);

  // Execute next test.
  let test = tests.shift();
  dump("\nTEST " + (++testIndex) + ": " + test.description);
  test.exec();
}

function run_test() {
  // Create our bookmarks.html from bookmarks.glue.html.
  create_bookmarks_html("bookmarks.glue.html");

  // Create our JSON backup from bookmarks.glue.json.
  create_JSON_backup("bookmarks.glue.json");

  // Kick-off tests.
  do_test_pending();
  next_test();
}
