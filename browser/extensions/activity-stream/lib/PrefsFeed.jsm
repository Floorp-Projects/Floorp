/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu} = Components;
const {actionTypes: at, actionCreators: ac} = Cu.import("resource://activity-stream/common/Actions.jsm", {});
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Prefs",
  "resource://activity-stream/lib/ActivityStreamPrefs.jsm");

this.PrefsFeed = class PrefsFeed {
  constructor(prefNames) {
    this._prefNames = prefNames;
    this._prefs = new Prefs();
    this._observers = new Map();
  }
  onPrefChanged(name, value) {
    this.store.dispatch(ac.BroadcastToContent({type: at.PREF_CHANGED, data: {name, value}}));
  }
  init() {
    const values = {};

    // Set up listeners for each activity stream pref
    for (const name of this._prefNames) {
      const handler = value => {
        this.onPrefChanged(name, value);
      };
      this._observers.set(name, handler, this);
      this._prefs.observe(name, handler);
      values[name] = this._prefs.get(name);
    }

    // Set the initial state of all prefs in redux
    this.store.dispatch(ac.BroadcastToContent({type: at.PREFS_INITIAL_VALUES, data: values}));
  }
  removeListeners() {
    for (const name of this._prefNames) {
      this._prefs.ignore(name, this._observers.get(name));
    }
    this._observers.clear();
  }
  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.init();
        break;
      case at.UNINIT:
        this.removeListeners();
        break;
      case at.SET_PREF:
        this._prefs.set(action.data.name, action.data.value);
        break;
    }
  }
};

this.EXPORTED_SYMBOLS = ["PrefsFeed"];
