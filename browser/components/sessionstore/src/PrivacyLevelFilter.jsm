/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["PrivacyLevelFilter"];

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
this.PrivacyLevelFilter = Object.freeze({
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
  }
});
