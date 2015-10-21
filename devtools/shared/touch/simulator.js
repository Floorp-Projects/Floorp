/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var { Ci } = require("chrome");
var promise = require("promise");

const FRAME_SCRIPT =
  "resource://devtools/shared/touch/simulator-content.js";

var trackedBrowsers = new WeakMap();
var savedTouchEventsEnabled =
  Services.prefs.getIntPref("dom.w3c_touch_events.enabled");

/**
 * Simulate touch events for platforms where they aren't generally available.
 * Defers to the `simulator-content.js` frame script to perform the real work.
 */
function TouchEventSimulator(browser) {
  // Returns an already instantiated simulator for this browser
  let simulator = trackedBrowsers.get(browser);
  if (simulator) {
    return simulator;
  }

  let mm = browser.messageManager;
  if (!mm) {
    // Maybe browser is an iframe
    mm = browser.QueryInterface(Ci.nsIFrameLoaderOwner)
                .frameLoader.messageManager;
  }
  mm.loadFrameScript(FRAME_SCRIPT, true);

  simulator = {
    enabled: false,

    start() {
      if (this.enabled) {
        return promise.resolve({ isReloadNeeded: false });
      }
      this.enabled = true;

      let deferred = promise.defer();
      let isReloadNeeded =
        Services.prefs.getIntPref("dom.w3c_touch_events.enabled") != 1;
      Services.prefs.setIntPref("dom.w3c_touch_events.enabled", 1);
      let onStarted = () => {
        mm.removeMessageListener("TouchEventSimulator:Started", onStarted);
        deferred.resolve({ isReloadNeeded });
      };
      mm.addMessageListener("TouchEventSimulator:Started", onStarted);
      mm.sendAsyncMessage("TouchEventSimulator:Start");
      return deferred.promise;
    },

    stop() {
      if (!this.enabled) {
        return promise.resolve();
      }
      this.enabled = false;

      let deferred = promise.defer();
      Services.prefs.setIntPref("dom.w3c_touch_events.enabled",
                                savedTouchEventsEnabled);
      let onStopped = () => {
        mm.removeMessageListener("TouchEventSimulator:Stopped", onStopped);
        deferred.resolve();
      };
      mm.addMessageListener("TouchEventSimulator:Stopped", onStopped);
      mm.sendAsyncMessage("TouchEventSimulator:Stop");
      return deferred.promise;
    }
  };

  trackedBrowsers.set(browser, simulator);

  return simulator;
}

exports.TouchEventSimulator = TouchEventSimulator;
