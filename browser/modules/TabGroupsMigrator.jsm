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
XPCOMUtils.defineLazyModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AsyncShutdown", "resource://gre/modules/AsyncShutdown.jsm");

XPCOMUtils.defineLazyGetter(this, "gBrowserBundle", function() {
  return Services.strings.createBundle('chrome://browser/locale/browser.properties');
});

const RECOVERY_URL = "chrome://browser/content/aboutTabGroupsMigration.xhtml";


this.TabGroupsMigrator = {
  bookmarkedGroupsPromise: null,

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
    // If this is the very first startup of this profile, this is going to be empty:
    if (!stateStr) {
      return;
    }
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

    // We always write this back to ensure that any spurious tab groups data is
    // removed:
    stateAsSupportsString.data = JSON.stringify(state);
  },

  /**
   * Returns a Map from window state objects to per-window group data.
   * Specifically, the values in the Map are themselves Maps from group IDs to
   * JS Objects which have these properties:
   *  - tabGroupsMigrationTitle: the title of the group (or empty string)
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
        let activeGroupID = null;
        let tabsWithoutGroup = [];
        for (let tab of win.tabs) {
          let group;
          // Get a string group ID:
          try {
            let tabViewData = tab.extData && tab.extData["tabview-tab"] &&
                              JSON.parse(tab.extData["tabview-tab"]);
            if (tabViewData && ("groupID" in tabViewData)) {
              group = tabViewData.groupID + "";
            }
          } catch (ex) {
            // Ignore errors reading group info, treat as active group
          }
          if (!group) {
            // We didn't find group info. If we already have an active group,
            // pretend this is part of that group:
            if (activeGroupID) {
              group = activeGroupID;
            } else {
              if (!tabsWithoutGroup) {
                Cu.reportError("ERROR: the list of tabs without groups was " +
                               "nulled out, but there's no active group ID? " +
                               "This should never happen!");
                tabsWithoutGroup = [];
              }
              // Otherwise, add to the list of tabs with no group and move to
              // the next tab immediately. We'll add all these tabs at the
              // beginning of the active group as soon as we find a tab in it,
              // so as to preserve their order.
              tabsWithoutGroup.push(tab);
              continue;
            }
          }
          let groupData = windowGroupData.get(group);
          if (!groupData) {
            let title = (groupInfo[group] && groupInfo[group].title) || "";
            groupData = {
              tabs: [],
              tabGroupsMigrationTitle: title,
            };
            if (!title) {
              groupData.anonGroupID = ++globalAnonGroupID;
              groupData.tabGroupsMigrationTitle =
                gBrowserBundle.formatStringFromName("tabgroups.migration.anonGroup",
                                                    [groupData.anonGroupID], 1);
            }
            // If this is the active group, set the active group ID and add
            // all the already-known tabs (that didn't list a group ID), if any.
            if (!activeGroupID && !tab.hidden) {
              activeGroupID = group;
              groupData.tabs = tabsWithoutGroup;
              tabsWithoutGroup = null;
            }
            windowGroupData.set(group, groupData);
          }
          groupData.tabs.push(tab);
        }

        // If we found tabs but no active group, assume there's just 1 group:
        if (tabsWithoutGroup && tabsWithoutGroup.length) {
          windowGroupData.set("active group", {
            tabs: tabsWithoutGroup,
            anonGroupID: ++globalAnonGroupID,
          });
        }

        allGroupData.set(win, windowGroupData);
      }
    }
    return allGroupData;
  },

  _createBackup(stateStr) {
    let dest = Services.dirsvc.get("ProfD", Ci.nsIFile);
    dest.append("tabgroups-session-backup.json");
    let promise = OS.File.writeAtomic(dest.path, stateStr, {encoding: "utf-8"});
    AsyncShutdown.webWorkersShutdown.addBlocker("TabGroupsMigrator", promise);
    return promise;
  },

  _groupSorter(a, b) {
    if (!a.anonGroupID) {
      return -1;
    }
    if (!b.anonGroupID) {
      return 1;
    }
    return a.anonGroupID - b.anonGroupID;
  },

  _bookmarkAllGroupsFromState: Task.async(function*(groupData) {
    // First create a folder in which to put all these bookmarks:
    this.bookmarkedGroupsPromise = PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 0,
      title: gBrowserBundle.GetStringFromName("tabgroups.migration.tabGroupBookmarkFolderName"),
    }).catch(Cu.reportError);
    let tabgroupsFolder = yield this.bookmarkedGroupsPromise;

    for (let [, windowGroupMap] of groupData) {
      let windowGroups = [... windowGroupMap.values()].sort(this._groupSorter);
      for (let group of windowGroups) {
        let groupFolder = yield PlacesUtils.bookmarks.insert({
          parentGuid: tabgroupsFolder.guid,
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          title: group.tabGroupsMigrationTitle
        }).catch(Cu.reportError);

        for (let tab of group.tabs) {
          let entry = tab.entries[tab.index - 1];
          yield PlacesUtils.bookmarks.insert({
            parentGuid: groupFolder.guid,
            title: tab.title || entry.title,
            url: entry.url,
          }).catch(Cu.reportError);
        }
      }
    }
  }),

  _removeHiddenTabGroupsFromState(state, groups) {
    let stateToReturn = {windows: []};
    for (let win of state.windows) {
      let groupInfoForWindow = groups.get(win);
      let hiddenGroupIDs = new Set();
      for (let i = win.tabs.length - 1; i >= 0; i--) {
        let tab =  win.tabs[i];
        // Determine whether the tab is grouped:
        let tabGroupInfo = null;
        try {
          tabGroupInfo = tab.extData && tab.extData["tabview-tab"] &&
                         JSON.parse(tab.extData["tabview-tab"]);
        } catch (ex) {}

        // Then remove this data.
        if (tab.extData) {
          delete tab.extData["tabview-tab"];
          if (Object.keys(tab.extData).length == 0) {
            delete tab.extData;
          }
        }

        // If the tab was grouped and hidden, remove it:
        if (tabGroupInfo && tab.hidden) {
          hiddenGroupIDs.add(tabGroupInfo.groupID);
          win.tabs.splice(i, 1);
          // Make sure we unhide it, or it won't get restored correctly
          tab.hidden = false;
        }
      }

      // We then convert any hidden groups into windows for the state object
      // we show in about:tabgroupsdata
      if (groupInfoForWindow) {
        let windowsToReturn = [];
        for (let groupID of hiddenGroupIDs) {
          let group = groupInfoForWindow.get("" + groupID);
          if (group) {
            windowsToReturn.push(group);
          }
        }
        windowsToReturn.sort(this._groupSorter);
        stateToReturn.windows = stateToReturn.windows.concat(windowsToReturn);
      }

      // Finally we remove tab groups data from the window:
      if (win.extData) {
        delete win.extData["tabview-group"];
        delete win.extData["tabview-groups"];
        delete win.extData["tabview-ui"];
        delete win.extData["tabview-visibility"];
        if (Object.keys(win.extData).length == 0) {
          delete win.extData;
        }
      }
    }
    return stateToReturn;
  },

  _createBackgroundTabGroupRestorationPage(state, backgroundData) {
    let win = state.windows[(state.selectedWindow || 1) - 1];
    let formdata = {id: {sessionData: JSON.stringify(backgroundData)}, url: RECOVERY_URL};
    let newTab = { entries: [{url: RECOVERY_URL}], formdata, index: 1 };
    // Add tab and mark it as selected:
    win.selected = win.tabs.push(newTab);
  },
};

