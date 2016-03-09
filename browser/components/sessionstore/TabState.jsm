/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["TabState"];

const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);

XPCOMUtils.defineLazyModuleGetter(this, "console",
  "resource://gre/modules/Console.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivacyFilter",
  "resource:///modules/sessionstore/PrivacyFilter.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TabStateCache",
  "resource:///modules/sessionstore/TabStateCache.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TabAttributes",
  "resource:///modules/sessionstore/TabAttributes.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Utils",
  "resource:///modules/sessionstore/Utils.jsm");

/**
 * Module that contains tab state collection methods.
 */
this.TabState = Object.freeze({
  update: function (browser, data) {
    TabStateInternal.update(browser, data);
  },

  collect: function (tab) {
    return TabStateInternal.collect(tab);
  },

  clone: function (tab) {
    return TabStateInternal.clone(tab);
  },

  copyFromCache(browser, tabData, options) {
    TabStateInternal.copyFromCache(browser, tabData, options);
  },
});

var TabStateInternal = {
  /**
   * Processes a data update sent by the content script.
   */
  update: function (browser, {data}) {
    TabStateCache.update(browser, data);
  },

  /**
   * Collect data related to a single tab, synchronously.
   *
   * @param tab
   *        tabbrowser tab
   *
   * @returns {TabData} An object with the data for this tab.  If the
   * tab has not been invalidated since the last call to
   * collect(aTab), the same object is returned.
   */
  collect: function (tab) {
    return this._collectBaseTabData(tab);
  },

  /**
   * Collect data related to a single tab, including private data.
   * Use with caution.
   *
   * @param tab
   *        tabbrowser tab
   *
   * @returns {object} An object with the data for this tab. This data is never
   *                   cached, it will always be read from the tab and thus be
   *                   up-to-date.
   */
  clone: function (tab) {
    return this._collectBaseTabData(tab, {includePrivateData: true});
  },

  /**
   * Collects basic tab data for a given tab.
   *
   * @param tab
   *        tabbrowser tab
   * @param options (object)
   *        {includePrivateData: true} to always include private data
   *
   * @returns {object} An object with the basic data for this tab.
   */
  _collectBaseTabData: function (tab, options) {
    let tabData = { entries: [], lastAccessed: tab.lastAccessed };
    let browser = tab.linkedBrowser;

    if (tab.pinned) {
      tabData.pinned = true;
    }

    tabData.hidden = tab.hidden;

    if (browser.audioMuted) {
      tabData.muted = true;
      tabData.muteReason = tab.muteReason;
    }

    // Save tab attributes.
    tabData.attributes = TabAttributes.get(tab);

    if (tab.__SS_extdata) {
      tabData.extData = tab.__SS_extdata;
    }

    // Copy data from the tab state cache only if the tab has fully finished
    // restoring. We don't want to overwrite data contained in __SS_data.
    this.copyFromCache(browser, tabData, options);

    // After copyFromCache() was called we check for properties that are kept
    // in the cache only while the tab is pending or restoring. Once that
    // happened those properties will be removed from the cache and will
    // be read from the tab/browser every time we collect data.

    // Store the tab icon.
    if (!("image" in tabData)) {
      let tabbrowser = tab.ownerDocument.defaultView.gBrowser;
      tabData.image = tabbrowser.getIcon(tab);
    }

    // If there is a userTypedValue set, then either the user has typed something
    // in the URL bar, or a new tab was opened with a URI to load. userTypedClear
    // is used to indicate whether the tab was in some sort of loading state with
    // userTypedValue.
    if (!("userTypedValue" in tabData) && browser.userTypedValue) {
      tabData.userTypedValue = browser.userTypedValue;
      tabData.userTypedClear = browser.userTypedClear;
    }

    return tabData;
  },

  /**
   * Copy data for the given |browser| from the cache to |tabData|.
   *
   * @param browser (xul:browser)
   *        The browser belonging to the given |tabData| object.
   * @param tabData (object)
   *        The tab data belonging to the given |tab|.
   * @param options (object)
   *        {includePrivateData: true} to always include private data
   */
  copyFromCache(browser, tabData, options = {}) {
    let data = TabStateCache.get(browser);
    if (!data) {
      return;
    }

    // The caller may explicitly request to omit privacy checks.
    let includePrivateData = options && options.includePrivateData;
    let isPinned = tabData.pinned || false;

    for (let key of Object.keys(data)) {
      let value = data[key];

      // Filter sensitive data according to the current privacy level.
      if (!includePrivateData) {
        if (key === "storage") {
          value = PrivacyFilter.filterSessionStorageData(value);
        } else if (key === "formdata") {
          value = PrivacyFilter.filterFormData(value);
        }
      }

      if (key === "history") {
        tabData.entries = value.entries;

        if (value.hasOwnProperty("userContextId")) {
          tabData.userContextId = value.userContextId;
        }

        if (value.hasOwnProperty("index")) {
          tabData.index = value.index;
        }
      } else {
        tabData[key] = value;
      }
    }
  }
};
