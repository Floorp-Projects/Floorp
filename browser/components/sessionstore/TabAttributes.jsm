/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["TabAttributes"];

// A set of tab attributes to persist. We will read a given list of tab
// attributes when collecting tab data and will re-set those attributes when
// the given tab data is restored to a new tab.
this.TabAttributes = Object.freeze({
  persist: function (name) {
    return TabAttributesInternal.persist(name);
  },

  get: function (tab) {
    return TabAttributesInternal.get(tab);
  },

  set: function (tab, data = {}) {
    TabAttributesInternal.set(tab, data);
  }
});

let TabAttributesInternal = {
  _attrs: new Set(),

  // We never want to directly read or write those attributes.
  // 'image' should not be accessed directly but handled by using the
  //         gBrowser.getIcon()/setIcon() methods.
  // 'pending' is used internal by sessionstore and managed accordingly.
  _skipAttrs: new Set(["image", "pending"]),

  persist: function (name) {
    if (this._attrs.has(name) || this._skipAttrs.has(name)) {
      return false;
    }

    this._attrs.add(name);
    return true;
  },

  get: function (tab) {
    let data = {};

    for (let name of this._attrs) {
      if (tab.hasAttribute(name)) {
        data[name] = tab.getAttribute(name);
      }
    }

    return data;
  },

  set: function (tab, data = {}) {
    // Clear attributes.
    for (let name of this._attrs) {
      tab.removeAttribute(name);
    }

    // Set attributes.
    for (let name in data) {
      tab.setAttribute(name, data[name]);
    }
  }
};

