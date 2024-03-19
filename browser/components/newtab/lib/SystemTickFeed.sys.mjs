/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { actionTypes as at } from "resource://activity-stream/common/Actions.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  clearInterval: "resource://gre/modules/Timer.sys.mjs",
  setInterval: "resource://gre/modules/Timer.sys.mjs",
});

// Frequency at which SYSTEM_TICK events are fired
export const SYSTEM_TICK_INTERVAL = 5 * 60 * 1000;

export class SystemTickFeed {
  init() {
    this._idleService = Cc["@mozilla.org/widget/useridleservice;1"].getService(
      Ci.nsIUserIdleService
    );
    this._hasObserver = false;
    this.setTimer();
  }

  setTimer() {
    this.intervalId = lazy.setInterval(() => {
      if (this._idleService.idleTime > SYSTEM_TICK_INTERVAL) {
        this.cancelTimer();
        Services.obs.addObserver(this, "user-interaction-active");
        this._hasObserver = true;
        return;
      }
      this.dispatchTick();
    }, SYSTEM_TICK_INTERVAL);
  }

  cancelTimer() {
    lazy.clearInterval(this.intervalId);
    this.intervalId = null;
  }

  observe() {
    this.dispatchTick();
    Services.obs.removeObserver(this, "user-interaction-active");
    this._hasObserver = false;
    this.setTimer();
  }

  dispatchTick() {
    ChromeUtils.idleDispatch(() =>
      this.store.dispatch({ type: at.SYSTEM_TICK })
    );
  }

  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.init();
        break;
      case at.UNINIT:
        this.cancelTimer();
        if (this._hasObserver) {
          Services.obs.removeObserver(this, "user-interaction-active");
          this._hasObserver = false;
        }
        break;
    }
  }
}
