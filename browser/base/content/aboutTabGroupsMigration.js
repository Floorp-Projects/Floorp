/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";


Cu.import("resource:///modules/TabGroupsMigrator.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");

const SUPPORT_URL = "https://support.mozilla.org/kb/tab-groups-removal";

function fillInDescription() {
  let descElement = document.getElementById("mainDescription");
  let text = descElement.getAttribute("_description_label");
  let learnMoreText = descElement.getAttribute("_learnmore_label");
  let link = document.createElement("a");
  link.href = SUPPORT_URL;
  link.target = "_blank";
  link.textContent = learnMoreText;
  // We don't want to assign the label (plaintext, because we took it out of
  // getAttribute, so html entities have been resolved) directly to innerHTML.
  // Using a separate element and then using innerHTML on that would mean we
  // depend on the HTML encoding (or lack thereof) of %S, which l10n tools
  // might change.
  // So instead, split the plaintext we have and create textNodes:
  let nodes = text.split(/%S/).map(document.createTextNode.bind(document));
  // then insert the link:
  nodes.splice(1, 0, link);
  // then append all of these in turn:
  for (let node of nodes) {
    descElement.appendChild(node);
  }
}

let loadPromise = new Promise(resolve => {
  let loadHandler = e => {
    window.removeEventListener("DOMContentLoaded", loadHandler);
    fillInDescription();
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

