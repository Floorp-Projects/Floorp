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
 *  Test we correctly migrate Library left pane to the latest version.
 *  Note: this test MUST be the first between browser chrome tests, or results
 *        of next tests could be unexpected due to PlacesUIUtils getters.
 */

const TEST_URI = "http://www.mozilla.org/";

var ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
         getService(Ci.nsIWindowWatcher);

var gFakeLeftPanes = [];

var windowObserver = {
  observe: function(aSubject, aTopic, aData) {
    if (aTopic === "domwindowopened") {
      ww.unregisterNotification(this);
      var organizer = aSubject.QueryInterface(Ci.nsIDOMWindow);
      organizer.addEventListener("load", function onLoad(event) {
        organizer.removeEventListener("load", onLoad, false);
        executeSoon(function () {
          // Check left pane.
          ok(PlacesUIUtils.leftPaneFolderId > 0,
             "Left pane folder correctly created");
          var leftPaneItems =
            PlacesUtils.annotations
                       .getItemsWithAnnotation(ORGANIZER_FOLDER_ANNO, {});
          is(leftPaneItems.length, 1,
             "We correctly have only 1 left pane folder");

          // Check that all old left pane items have been removed.
          gFakeLeftPanes.forEach(function(aItemId) {
            try {
              PlacesUtils.bookmarks.getItemTitle(aItemId);
              throw("This folder should have been removed");
            } catch (ex) {}
          });

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

  // We need 2 left pane folders.
  do {
    let leftPaneItems = PlacesUtils.annotations
                                 .getItemsWithAnnotation(ORGANIZER_FOLDER_ANNO, {});
    // Create a fake left pane folder.
    let fakeLeftPaneRoot =
      PlacesUtils.bookmarks.createFolder(PlacesUtils.placesRootId, "",
                                         PlacesUtils.bookmarks.DEFAULT_INDEX);
    PlacesUtils.annotations.setItemAnnotation(fakeLeftPaneRoot,
                                              ORGANIZER_FOLDER_ANNO,
                                              ORGANIZER_LEFTPANE_VERSION,
                                              0,
                                              PlacesUtils.annotations.EXPIRE_NEVER);
    gFakeLeftPanes.push(fakeLeftPaneRoot);
  } while (gFakeLeftPanes.length < 2);

  // Open Library, this will fix the left pane.
  ww.registerNotification(windowObserver);
  ww.openWindow(null,
                "chrome://browser/content/places/places.xul",
                "",
                "chrome,toolbar=yes,dialog=no,resizable",
                null);
}
