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
 * The Original Code is Places test code.
 *
 * The Initial Developer of the Original Code is Mozilla Corp.
 * Portions created by the Initial Developer are Copyright (C) 2009
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
 * decision by devaring the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not devare
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 *  Test we correctly fix broken Library left pane queries names.
 */

var ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
         getService(Ci.nsIWindowWatcher);

// Array of left pane queries objects, each one has the following properties:
// name: query's identifier got from annotations,
// itemId: query's itemId,
// correctTitle: original and correct query's title.
var leftPaneQueries = [];

var windowObserver = {
  observe: function(aSubject, aTopic, aData) {
    if (aTopic === "domwindowopened") {
      ww.unregisterNotification(this);
      var organizer = aSubject.QueryInterface(Ci.nsIDOMWindow);
      organizer.addEventListener("load", function onLoad(event) {
        organizer.removeEventListener("load", onLoad, false);
        executeSoon(function () {
          // Check titles have been fixed.
          for (var i = 0; i < leftPaneQueries.length; i++) {
            var query = leftPaneQueries[i];
            is(PlacesUtils.bookmarks.getItemTitle(query.itemId),
               query.correctTitle, "Title is correct for query " + query.name);
          }

          // Close Library window.
          organizer.close();
          // No need to cleanup anything, we have a correct left pane now.
          finish();
        });
      }, false);
    }
  }
};

function test() {
  waitForExplicitFinish();
  // Sanity checks.
  ok(PlacesUtils, "PlacesUtils is running in chrome context");
  ok(PlacesUIUtils, "PlacesUIUtils is running in chrome context");
  ok(ORGANIZER_LEFTPANE_VERSION > 0,
     "Left pane version in chrome context, current version is: " + ORGANIZER_LEFTPANE_VERSION );

  // Ensure left pane is initialized.
  ok(PlacesUIUtils.leftPaneFolderId > 0, "left pane folder is initialized");

  // Get the left pane folder.
  var leftPaneItems = PlacesUtils.annotations
                                 .getItemsWithAnnotation(ORGANIZER_FOLDER_ANNO);

  is(leftPaneItems.length, 1, "We correctly have only 1 left pane folder");
  // Check version.
  var version = PlacesUtils.annotations
                           .getItemAnnotation(leftPaneItems[0],
                                              ORGANIZER_FOLDER_ANNO);
  is(version, ORGANIZER_LEFTPANE_VERSION, "Left pane version is actual");

  // Get all left pane queries.
  var items = PlacesUtils.annotations
                         .getItemsWithAnnotation(ORGANIZER_QUERY_ANNO);
  // Get current queries names.
  for (var i = 0; i < items.length; i++) {
    var itemId = items[i];
    var queryName = PlacesUtils.annotations
                               .getItemAnnotation(items[i],
                                                  ORGANIZER_QUERY_ANNO);
    leftPaneQueries.push({ name: queryName,
                           itemId: itemId,
                           correctTitle: PlacesUtils.bookmarks
                                                    .getItemTitle(itemId) });
    // Rename to a bad title.
    PlacesUtils.bookmarks.setItemTitle(itemId, "badName");
  }

  // Open Library, this will kick-off left pane code.
  ww.registerNotification(windowObserver);
  ww.openWindow(null,
                "chrome://browser/content/places/places.xul",
                "",
                "chrome,toolbar=yes,dialog=no,resizable",
                null);
}
