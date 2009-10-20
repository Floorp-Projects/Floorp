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
 * Tests that nsBrowserGlue does not overwrite bookmarks imported from the
 * migrators.  They usually run before nsBrowserGlue, so if we find any
 * bookmark on init, we should not try to import.
 */

const PREF_SMART_BOOKMARKS_VERSION = "browser.places.smartBookmarksVersion";
const PREF_BMPROCESSED = "distribution.516444.bookmarksProcessed";
const PREF_DISTRIBUTION_ID = "distribution.id";

const TOPIC_FINAL_UI_STARTUP = "final-ui-startup";
const TOPIC_PLACES_INIT_COMPLETE = "places-init-complete";
const TOPIC_CUSTOMIZATION_COMPLETE = "distribution-customization-complete";

let os = Cc["@mozilla.org/observer-service;1"].
         getService(Ci.nsIObserverService);

let observer = {
  observe: function(aSubject, aTopic, aData) {
    if (aTopic == TOPIC_CUSTOMIZATION_COMPLETE) {
      os.removeObserver(this, TOPIC_CUSTOMIZATION_COMPLETE);
      do_timeout(0, "continue_test();");
    }
  }
}
os.addObserver(observer, TOPIC_CUSTOMIZATION_COMPLETE, false);

function run_test() {
  // Copy distribution.ini file to our app dir.
  let distroDir = dirSvc.get("XCurProcD", Ci.nsIFile);
  distroDir.append("distribution");
  let iniFile = distroDir.clone();
  iniFile.append("distribution.ini");
  if (iniFile.exists()) {
    iniFile.remove(false);
    print("distribution.ini already exists, did some test forget to cleanup?");
  }

  let testDistributionFile = gTestDir.clone();
  testDistributionFile.append("distribution.ini");
  testDistributionFile.copyTo(distroDir, "distribution.ini");
  do_check_true(testDistributionFile.exists());

  // Disable Smart Bookmarks creation.
  let ps = Cc["@mozilla.org/preferences-service;1"].
           getService(Ci.nsIPrefBranch);
  ps.setIntPref(PREF_SMART_BOOKMARKS_VERSION, -1);

  // Initialize Places through the History Service, so it won't trigger
  // browserGlue::_initPlaces since browserGlue is not yet in context.
  let hs = Cc["@mozilla.org/browser/nav-history-service;1"].
           getService(Ci.nsINavHistoryService);
  // Check a new database has been created.
  // nsBrowserGlue will use databaseStatus to manage initialization.
  do_check_eq(hs.databaseStatus, hs.DATABASE_STATUS_CREATE);

  // Initialize nsBrowserGlue.
  Cc["@mozilla.org/browser/browserglue;1"].getService(Ci.nsIBrowserGlue);

  // Places initialization has already happened, so we need to simulate a new
  // one.  This will force browserGlue::_initPlaces().
  os.notifyObservers(null, TOPIC_FINAL_UI_STARTUP, null);
  os.notifyObservers(null, TOPIC_PLACES_INIT_COMPLETE, null);

  do_test_pending();
  // Test will continue on customization complete notification.
}

function continue_test() {
  let bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
           getService(Ci.nsINavBookmarksService);

  // Check the custom bookmarks exist on menu.
  let menuItemId = bs.getIdForItemAt(bs.bookmarksMenuFolder, 0);
  do_check_neq(menuItemId, -1);
  do_check_eq(bs.getItemTitle(menuItemId), "Menu Link Before");
  menuItemId = bs.getIdForItemAt(bs.bookmarksMenuFolder, 1 + DEFAULT_BOOKMARKS_ON_MENU);
  do_check_neq(menuItemId, -1);
  do_check_eq(bs.getItemTitle(menuItemId), "Menu Link After");

  // Check the custom bookmarks exist on toolbar.
  let toolbarItemId = bs.getIdForItemAt(bs.toolbarFolder, 0);
  do_check_neq(toolbarItemId, -1);
  do_check_eq(bs.getItemTitle(toolbarItemId), "Toolbar Link Before");
  toolbarItemId = bs.getIdForItemAt(bs.toolbarFolder, 1 + DEFAULT_BOOKMARKS_ON_TOOLBAR);
  do_check_neq(toolbarItemId, -1);
  do_check_eq(bs.getItemTitle(toolbarItemId), "Toolbar Link After");

  // Check the bmprocessed pref has been created.
  let ps = Cc["@mozilla.org/preferences-service;1"].
           getService(Ci.nsIPrefBranch);
  do_check_true(ps.getBoolPref(PREF_BMPROCESSED));

  // Check distribution prefs have been created.
  do_check_eq(ps.getCharPref(PREF_DISTRIBUTION_ID), "516444");

  do_test_finished();
}

do_register_cleanup(function() {
  // Remove the distribution file, even if the test failed, otherwise all
  // next tests will import it.
  let iniFile = dirSvc.get("XCurProcD", Ci.nsIFile);
  iniFile.append("distribution");
  iniFile.append("distribution.ini");
  iniFile.remove(false);
  do_check_false(iniFile.exists());
});
