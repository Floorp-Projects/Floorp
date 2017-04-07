/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu} = Components;
const {actionTypes: at, actionCreators: ac} = Cu.import("resource://activity-stream/common/Actions.jsm", {});

/**
 * NewTabInit - A placeholder for now. This will send a copy of the state to all
 *              newly opened tabs.
 */
this.NewTabInit = class NewTabInit {
  onAction(action) {
    let newAction;
    switch (action.type) {
      case at.NEW_TAB_LOAD:
        newAction = {type: at.NEW_TAB_INITIAL_STATE, data: this.store.getState()};
        this.store.dispatch(ac.SendToContent(newAction, action.meta.fromTarget));
        break;
    }
  }
};

this.EXPORTED_SYMBOLS = ["NewTabInit"];
