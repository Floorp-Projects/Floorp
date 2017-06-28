/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu} = Components;

const {actionCreators: ac, actionTypes: at} = Cu.import("resource://activity-stream/common/Actions.jsm", {});
const {Prefs} = Cu.import("resource://activity-stream/lib/ActivityStreamPrefs.jsm", {});

this.PrefsFeed = class PrefsFeed {
  constructor(prefMap) {
    this._prefMap = prefMap;
    this._prefs = new Prefs();
  }
  onPrefChanged(name, value) {
    if (this._prefMap.has(name)) {
      this.store.dispatch(ac.BroadcastToContent({type: at.PREF_CHANGED, data: {name, value}}));
    }
  }
  init() {
    this._prefs.observeBranch(this);

    // Get the initial value of each activity stream pref
    const values = {};
    for (const name of this._prefMap.keys()) {
      values[name] = this._prefs.get(name);
    }

    // Set the initial state of all prefs in redux
    this.store.dispatch(ac.BroadcastToContent({type: at.PREFS_INITIAL_VALUES, data: values}));
  }
  removeListeners() {
    this._prefs.ignoreBranch(this);
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
