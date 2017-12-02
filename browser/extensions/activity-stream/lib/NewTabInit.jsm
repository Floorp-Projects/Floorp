/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu} = Components;

const {actionCreators: ac, actionTypes: at} = Cu.import("resource://activity-stream/common/Actions.jsm", {});

/**
 * NewTabInit - A placeholder for now. This will send a copy of the state to all
 *              newly opened tabs.
 */
this.NewTabInit = class NewTabInit {
  constructor() {
    this._repliedEarlyTabs = new Map();
  }
  reply(target) {
    // Skip this reply if we already replied to an early tab
    if (this._repliedEarlyTabs.get(target)) {
      return;
    }

    const action = {type: at.NEW_TAB_INITIAL_STATE, data: this.store.getState()};
    this.store.dispatch(ac.SendToContent(action, target));

    // Remember that this early tab has already gotten a rehydration response in
    // case it thought we lost its initial REQUEST and asked again
    if (this._repliedEarlyTabs.has(target)) {
      this._repliedEarlyTabs.set(target, true);
    }
  }
  onAction(action) {
    switch (action.type) {
      case at.NEW_TAB_STATE_REQUEST:
        this.reply(action.meta.fromTarget);
        break;
      case at.NEW_TAB_INIT:
        // Initialize data for early tabs that might REQUEST twice
        if (action.data.simulated) {
          this._repliedEarlyTabs.set(action.data.portID, false);
        }
        break;
      case at.NEW_TAB_UNLOAD:
        // Clean up for any tab (no-op if not an early tab)
        this._repliedEarlyTabs.delete(action.meta.fromTarget);
        break;
    }
  }
};

this.EXPORTED_SYMBOLS = ["NewTabInit"];
