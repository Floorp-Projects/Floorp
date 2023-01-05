/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { actionTypes: at } = ChromeUtils.importESModule(
  "resource://activity-stream/common/Actions.sys.mjs"
);

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  clearInterval: "resource://gre/modules/Timer.sys.mjs",
  setInterval: "resource://gre/modules/Timer.sys.mjs",
});

// Frequency at which SYSTEM_TICK events are fired
const SYSTEM_TICK_INTERVAL = 5 * 60 * 1000;

class SystemTickFeed {
  init() {
    this.intervalId = lazy.setInterval(
      () => this.store.dispatch({ type: at.SYSTEM_TICK }),
      SYSTEM_TICK_INTERVAL
    );
  }

  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.init();
        break;
      case at.UNINIT:
        lazy.clearInterval(this.intervalId);
        break;
    }
  }
}

const EXPORTED_SYMBOLS = ["SystemTickFeed", "SYSTEM_TICK_INTERVAL"];
