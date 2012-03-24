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
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Marco Bonardo <mak77@supereva.it>
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

// Get bookmarks service
try {
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
              getService(Ci.nsINavBookmarksService);
} catch(ex) {
  do_throw("Could not get Bookmarks service\n");
}

// Get annotation service
try {
  var annosvc = Cc["@mozilla.org/browser/annotation-service;1"].
                getService(Ci.nsIAnnotationService);
} catch(ex) {
  do_throw("Could not get Annotation service\n");
}

// Get browser glue
try {
  var gluesvc = Cc["@mozilla.org/browser/browserglue;1"].
                getService(Ci.nsIBrowserGlue);
  // Avoid default bookmarks import.
  gluesvc.QueryInterface(Ci.nsIObserver).observe(null, "initial-migration", null);
} catch(ex) {
  do_throw("Could not get BrowserGlue service\n");
}

// Get pref service
try {
  var pref =  Cc["@mozilla.org/preferences-service;1"].
              getService(Ci.nsIPrefBranch);
} catch(ex) {
  do_throw("Could not get Preferences service\n");
}

const SMART_BOOKMARKS_ANNO = "Places/SmartBookmark";
const SMART_BOOKMARKS_PREF = "browser.places.smartBookmarksVersion";

// main
function run_test() {
  // TEST 1: smart bookmarks disabled
  pref.setIntPref("browser.places.smartBookmarksVersion", -1);
  gluesvc.ensurePlacesDefaultQueriesInitialized();
  var smartBookmarkItemIds = annosvc.getItemsWithAnnotation(SMART_BOOKMARKS_ANNO);
  do_check_eq(smartBookmarkItemIds.length, 0);
  // check that pref has not been bumped up
  do_check_eq(pref.getIntPref("browser.places.smartBookmarksVersion"), -1);

  // TEST 2: create smart bookmarks
  pref.setIntPref("browser.places.smartBookmarksVersion", 0);
  gluesvc.ensurePlacesDefaultQueriesInitialized();
  smartBookmarkItemIds = annosvc.getItemsWithAnnotation(SMART_BOOKMARKS_ANNO);
  do_check_neq(smartBookmarkItemIds.length, 0);
  // check that pref has been bumped up
  do_check_true(pref.getIntPref("browser.places.smartBookmarksVersion") > 0);

  var smartBookmarksCount = smartBookmarkItemIds.length;

  // TEST 3: smart bookmarks restore
  // remove one smart bookmark and restore
  bmsvc.removeItem(smartBookmarkItemIds[0]);
  pref.setIntPref("browser.places.smartBookmarksVersion", 0);
  gluesvc.ensurePlacesDefaultQueriesInitialized();
  smartBookmarkItemIds = annosvc.getItemsWithAnnotation(SMART_BOOKMARKS_ANNO);
  do_check_eq(smartBookmarkItemIds.length, smartBookmarksCount);
  // check that pref has been bumped up
  do_check_true(pref.getIntPref("browser.places.smartBookmarksVersion") > 0);

  // TEST 4: move a smart bookmark, change its title, then restore
  // smart bookmark should be restored in place
  var parent = bmsvc.getFolderIdForItem(smartBookmarkItemIds[0]);
  var oldTitle = bmsvc.getItemTitle(smartBookmarkItemIds[0]);
  // create a subfolder and move inside it
  var newParent = bmsvc.createFolder(parent, "test", bmsvc.DEFAULT_INDEX);
  bmsvc.moveItem(smartBookmarkItemIds[0], newParent, bmsvc.DEFAULT_INDEX);
  // change title
  bmsvc.setItemTitle(smartBookmarkItemIds[0], "new title");
  // restore
  pref.setIntPref("browser.places.smartBookmarksVersion", 0);
  gluesvc.ensurePlacesDefaultQueriesInitialized();
  smartBookmarkItemIds = annosvc.getItemsWithAnnotation(SMART_BOOKMARKS_ANNO);
  do_check_eq(smartBookmarkItemIds.length, smartBookmarksCount);
  do_check_eq(bmsvc.getFolderIdForItem(smartBookmarkItemIds[0]), newParent);
  do_check_eq(bmsvc.getItemTitle(smartBookmarkItemIds[0]), oldTitle);
  // check that pref has been bumped up
  do_check_true(pref.getIntPref("browser.places.smartBookmarksVersion") > 0);
}
