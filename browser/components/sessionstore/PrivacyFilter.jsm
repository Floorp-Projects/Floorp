/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["PrivacyFilter"];

const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);

XPCOMUtils.defineLazyModuleGetter(this, "PrivacyLevel",
  "resource:///modules/sessionstore/PrivacyLevel.jsm");

/**
 * Returns whether the current privacy level allows saving data for the given
 * |url|.
 *
 * @param url The URL we want to save data for.
 * @param isPinned Whether the given |url| is contained in a pinned tab.
 * @return bool
 */
function checkPrivacyLevel(url, isPinned) {
  let isHttps = url.startsWith("https:");
  return PrivacyLevel.canSave({isHttps: isHttps, isPinned: isPinned});
}

/**
 * A module that provides methods to filter various kinds of data collected
 * from a tab by the current privacy level as set by the user.
 */
this.PrivacyFilter = Object.freeze({
  /**
   * Filters the given (serialized) session storage |data| according to the
   * current privacy level and returns a new object containing only data that
   * we're allowed to store.
   *
   * @param data The session storage data as collected from a tab.
   * @param isPinned Whether the tab we collected from is pinned.
   * @return object
   */
  filterSessionStorageData: function (data, isPinned) {
    let retval = {};

    for (let host of Object.keys(data)) {
      if (checkPrivacyLevel(host, isPinned)) {
        retval[host] = data[host];
      }
    }

    return Object.keys(retval).length ? retval : null;
  },

  /**
   * Filters the given (serialized) form |data| according to the current
   * privacy level and returns a new object containing only data that we're
   * allowed to store.
   *
   * @param data The form data as collected from a tab.
   * @param isPinned Whether the tab we collected from is pinned.
   * @return object
   */
  filterFormData: function (data, isPinned) {
    // If the given form data object has an associated URL that we are not
    // allowed to store data for, bail out. We explicitly discard data for any
    // children as well even if storing data for those frames would be allowed.
    if (data.url && !checkPrivacyLevel(data.url, isPinned)) {
      return;
    }

    let retval = {};

    for (let key of Object.keys(data)) {
      if (key === "children") {
        let recurse = child => this.filterFormData(child, isPinned);
        let children = data.children.map(recurse).filter(child => child);

        if (children.length) {
          retval.children = children;
        }
      // Only copy keys other than "children" if we have a valid URL in
      // data.url and we thus passed the privacy level check.
      } else if (data.url) {
        retval[key] = data[key];
      }
    }

    return Object.keys(retval).length ? retval : null;
  },

  /**
   * Removes any private windows and tabs from a given browser state object.
   *
   * @param browserState (object)
   *        The browser state for which we remove any private windows and tabs.
   *        The given object will be modified.
   */
  filterPrivateWindowsAndTabs: function (browserState) {
    // Remove private opened windows.
    for (let i = browserState.windows.length - 1; i >= 0; i--) {
      let win = browserState.windows[i];

      if (win.isPrivate) {
        browserState.windows.splice(i, 1);

        if (browserState.selectedWindow >= i) {
          browserState.selectedWindow--;
        }
      } else {
        // Remove private tabs from all open non-private windows.
        this.filterPrivateTabs(win);
      }
    }

    // Remove private closed windows.
    browserState._closedWindows =
      browserState._closedWindows.filter(win => !win.isPrivate);

    // Remove private tabs from all remaining closed windows.
    browserState._closedWindows.forEach(win => this.filterPrivateTabs(win));
  },

  /**
   * Removes open private tabs from a given window state object.
   *
   * @param winState (object)
   *        The window state for which we remove any private tabs.
   *        The given object will be modified.
   */
  filterPrivateTabs: function (winState) {
    // Remove open private tabs.
    for (let i = winState.tabs.length - 1; i >= 0 ; i--) {
      let tab = winState.tabs[i];

      if (tab.isPrivate) {
        winState.tabs.splice(i, 1);

        if (winState.selected >= i) {
          winState.selected--;
        }
      }
    }

    // Note that closed private tabs are only stored for private windows.
    // There is no need to call this function for private windows as the
    // whole window state should just be discarded so we explicitly don't
    // try to remove closed private tabs as an optimization.
  }
});
