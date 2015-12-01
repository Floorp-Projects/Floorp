/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["TabGroupsMigrator"];

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services", "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils", "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task", "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AsyncShutdown", "resource://gre/modules/AsyncShutdown.jsm");

XPCOMUtils.defineLazyGetter(this, "gBrowserBundle", function() {
  return Services.strings.createBundle('chrome://browser/locale/browser.properties');
});


this.TabGroupsMigrator = {
  /**
   * If this state contains tab groups, migrate the user's data. This means:
   * - make a backup of the user's data.
   * - create bookmarks of all the user's tab groups in a single folder
   * - append a tab to the active window that lets the user restore background
   *   groups.
   * - remove all the tabs hidden through tab groups from the state data.
   */
  migrate(stateAsSupportsString) {
    stateAsSupportsString.QueryInterface(Ci.nsISupportsString);
    let stateStr = stateAsSupportsString.data;
    let state;
    try {
      state = JSON.parse(stateStr);
    } catch (ex) {
      Cu.reportError("Failed to parse sessionstore state JSON to migrate tab groups: " + ex);
      return; // can't recover from invalid JSON
    }

    let groupData = this._gatherGroupData(state);

    // This strips out the hidden tab groups and puts them in a different object.
    // It also removes all tabview metadata.
    // We always do this, and always reassign the new state back into the
    // nsISupportsString for use by sessionstore, in order to tidy up the state
    // object.
    let hiddenTabState = this._removeHiddenTabGroupsFromState(state, groupData);

    // However, we will only create a backup file, bookmarks and a new tab to
    // restore hidden tabs if tabs were actually removed from |state| by
    // _removeHiddenTabGroupsFromState.
    if (hiddenTabState.windows.length) {
      // We create the backup with the original string, from before all our
      // changes:
      this._createBackup(stateStr);

      this._createBackgroundTabGroupRestorationPage(state, hiddenTabState);

      // Bookmark creation is async. We need to return synchronously,
      // so we purposefully don't wait for this to be finished here. We do
      // store the promise it creates and use that in the session restore page
      // to be able to link to the bookmarks folder...
      let bookmarksFinishedPromise = this._bookmarkAllGroupsFromState(groupData);
      // ... and we make sure we finish before shutting down:
      AsyncShutdown.profileBeforeChange.addBlocker(
        "Tab groups migration bookmarks",
        bookmarksFinishedPromise
      );
    }

    stateAsSupportsString.data = JSON.stringify(state);
  },

  /**
   * Returns a Map from window state objects to per-window group data.
   * Specifically, the values in the Map are themselves Maps from group IDs to
   * JS Objects which have these properties:
   *  - title: the title of the group (or empty string)
   *  - tabs: an array of the tabs objects in this group.
   */
  _gatherGroupData(state) {
    let allGroupData = new Map();
    let globalAnonGroupID = 0;
    for (let win of state.windows) {
      if (win.extData && win.extData["tabview-group"]) {
        let groupInfo = {};
        try {
          groupInfo = JSON.parse(win.extData["tabview-group"]);
        } catch (ex) {
          // This is annoying, but we'll try to deal with this.
        }

        let windowGroupData = new Map();
        for (let tab of win.tabs) {
          let group;
          // Get a string group ID:
          try {
            group = tab.extData && tab.extData["tabview-tab"] &&
                    (JSON.parse(tab.extData["tabview-tab"]).groupID + "");
          } catch (ex) {
            // Ignore tabs with no group info
            continue;
          }
          let groupData = windowGroupData.get(group);
          if (!groupData) {
            let title = (groupInfo[group] && groupInfo[group].title) || "";
            groupData = {
              tabs: [],
              title,
            };
            if (!title) {
              groupData.anonGroupID = ++globalAnonGroupID;
            }
            windowGroupData.set(group, groupData);
          }
          groupData.tabs.push(tab);
        }

        allGroupData.set(win, windowGroupData);
      }
    }
    return allGroupData;
  },

  _createBackup(stateStr) {
    // TODO
  },

  _bookmarkAllGroupsFromState: Task.async(function*(groupData) {
    // TODO
  }),

  _removeHiddenTabGroupsFromState(state, groupData) {
    // TODO
  },

  _createBackgroundTabGroupRestorationPage(state, backgroundData) {
    // TODO
  },
};

