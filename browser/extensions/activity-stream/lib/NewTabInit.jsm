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
    this._queue = new Set();
  }
  reply(target) {
    const action = {type: at.NEW_TAB_INITIAL_STATE, data: this.store.getState()};
    this.store.dispatch(ac.SendToContent(action, target));
  }
  onAction(action) {
    switch (action.type) {
      case at.NEW_TAB_STATE_REQUEST:
        // If localization hasn't been loaded yet, we should wait for it.
        if (!this.store.getState().App.strings) {
          this._queue.add(action.meta.fromTarget);
          return;
        }
        this.reply(action.meta.fromTarget);
        break;
      case at.LOCALE_UPDATED:
        // If the queue is full because we were waiting for strings,
        // dispatch them now.
        if (this._queue.size > 0 && this.store.getState().App.strings) {
          this._queue.forEach(target => this.reply(target));
          this._queue.clear();
        }
        break;
      case at.NEW_TAB_INIT:
        if (action.data.url === "about:home") {
          const prefs = this.store.getState().Prefs.values;
          if (prefs["aboutHome.autoFocus"] && prefs.showSearch) {
            action.data.browser.focus();
          }
        }
        break;
    }
  }
};

this.EXPORTED_SYMBOLS = ["NewTabInit"];
