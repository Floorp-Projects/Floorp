/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["TabAttributes"];

// We never want to directly read or write these attributes.
// 'image' should not be accessed directly but handled by using the
//         gBrowser.getIcon()/setIcon() methods.
// 'muted' should not be accessed directly but handled by using the
//         tab.linkedBrowser.audioMuted/toggleMuteAudio methods.
// 'pending' is used internal by sessionstore and managed accordingly.
const ATTRIBUTES_TO_SKIP = new Set([
  "image",
  "muted",
  "pending",
  "skipbackgroundnotify",
]);

// A set of tab attributes to persist. We will read a given list of tab
// attributes when collecting tab data and will re-set those attributes when
// the given tab data is restored to a new tab.
var TabAttributes = Object.freeze({
  persist(name) {
    return TabAttributesInternal.persist(name);
  },

  get(tab) {
    return TabAttributesInternal.get(tab);
  },

  set(tab, data = {}) {
    TabAttributesInternal.set(tab, data);
  },
});

var TabAttributesInternal = {
  _attrs: new Set(),

  persist(name) {
    if (this._attrs.has(name) || ATTRIBUTES_TO_SKIP.has(name)) {
      return false;
    }

    this._attrs.add(name);
    return true;
  },

  get(tab) {
    let data = {};

    for (let name of this._attrs) {
      if (tab.hasAttribute(name)) {
        data[name] = tab.getAttribute(name);
      }
    }

    return data;
  },

  set(tab, data = {}) {
    // Clear attributes.
    for (let name of this._attrs) {
      tab.removeAttribute(name);
    }

    // Set attributes.
    for (let [name, value] of Object.entries(data)) {
      if (!ATTRIBUTES_TO_SKIP.has(name)) {
        tab.setAttribute(name, value);
      }
    }
  },
};
