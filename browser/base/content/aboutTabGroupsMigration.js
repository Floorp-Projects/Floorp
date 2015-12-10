/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";


Cu.import("resource:///modules/TabGroupsMigrator.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");

const SUPPORT_URL = "https://support.mozilla.org/kb/tab-groups-removal";

function createLink() {
  let link = document.getElementById("sumolink");
  link.href = SUPPORT_URL;
  link.target = "_blank";
}

let loadPromise = new Promise(resolve => {
  let loadHandler = e => {
    window.removeEventListener("DOMContentLoaded", loadHandler);
    createLink();
    resolve();
  };
  window.addEventListener("DOMContentLoaded", loadHandler, false);
});

let tabGroupsBookmarkItemId;
// If the session wasn't restored this run/session, this might be null.
// Then we shouldn't show the button:
if (TabGroupsMigrator.bookmarkedGroupsPromise) {
  let bookmarkPromise = TabGroupsMigrator.bookmarkedGroupsPromise.then(bm => {
    return PlacesUtils.promiseItemId(bm.guid);
  }).then(itemId => { tabGroupsBookmarkItemId = itemId });

  Promise.all([bookmarkPromise, loadPromise]).then(function() {
    document.getElementById("show-migrated-bookmarks-button").style.removeProperty("display");
  });
}

function showMigratedGroups() {
  let browserWin = getBrowserWindow();
  browserWin.PlacesCommandHook.showPlacesOrganizer(["BookmarksMenu", tabGroupsBookmarkItemId]);
}

