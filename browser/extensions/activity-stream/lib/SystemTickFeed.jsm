/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const {actionTypes: at} = Cu.import("resource://activity-stream/common/Actions.jsm", {});

XPCOMUtils.defineLazyModuleGetter(this, "setInterval", "resource://gre/modules/Timer.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "clearInterval", "resource://gre/modules/Timer.jsm");

// Frequency at which SYSTEM_TICK events are fired
const SYSTEM_TICK_INTERVAL = 5 * 60 * 1000;

this.SystemTickFeed = class SystemTickFeed {
  init() {
    this.intervalId = setInterval(() => this.store.dispatch({type: at.SYSTEM_TICK}), SYSTEM_TICK_INTERVAL);
  }

  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.init();
        break;
      case at.UNINIT:
        clearInterval(this.intervalId);
        break;
    }
  }
};

this.SYSTEM_TICK_INTERVAL = SYSTEM_TICK_INTERVAL;
this.EXPORTED_SYMBOLS = ["SystemTickFeed", "SYSTEM_TICK_INTERVAL"];
