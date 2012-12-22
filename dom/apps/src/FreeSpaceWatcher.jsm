/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

this.EXPORTED_SYMBOLS = ["FreeSpaceWatcher"];

function debug(aMsg) {
  //dump("-*-*- FreeSpaceWatcher.jsm : " + aMsg + "\n");
}

// Polling delay for free space, in ms.
const DEFAULT_WATCHER_DELAY = 1000;

this.FreeSpaceWatcher = {
  timers: {},
  id: 0,

  /**
   * This function  will call aOnStatusChange
   * each time the free space for apps crosses aThreshold, checking
   * every aDelay milliseconds, or every second by default.
   * aOnStatusChange is called with either "free" or "full" and will
   * always be called at least one to get the initial status.
   * @param aThreshold      The amount of space in bytes to watch for.
   * @param aOnStatusChange The function called when the state changes.
   * @param aDelay          How often (in ms) we check free space. Defaults
   *                        to DEFAULT_WATCHER_DELAY.
   * @return                An opaque value to use with stop().
   */
  create: function spaceWatcher_create(aThreshold, aOnStatusChange, aDelay) {
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    debug("Creating new FreeSpaceWatcher");
    let callback = {
      currentStatus: null,
      notify: function(aTimer) {
        try {
          let deviceStorage = Services.wm.getMostRecentWindow("navigator:browser")
                                         .navigator.getDeviceStorage("apps");
          let req = deviceStorage.stat();
          req.onsuccess = req.onerror = function statResult(e) {
            if (!e.target.result) {
              return;
            }

            let freeBytes = e.target.result.freeBytes;
            debug("Free bytes: " + freeBytes);
            let newStatus = freeBytes > aThreshold;
            if (newStatus != callback.currentStatus) {
              debug("New status: " + (newStatus ? "free" : "full"));
              aOnStatusChange(newStatus ? "free" : "full");
              callback.currentStatus = newStatus;
            }
          }
        } catch(e) { debug(e); }
      }
    }

    timer.initWithCallback(callback, aDelay || DEFAULT_WATCHER_DELAY,
                           Ci.nsITimer.TYPE_REPEATING_SLACK);
    let id = "timer-" + this.id++;
    this.timers[id] = timer;
    return id;
  },

  /**
   * This function stops a running watcher.
   * @param aId The opaque timer id returned by create().
   */
  stop: function spaceWatcher_stop(aId) {
    if (this.timers[aId]) {
      this.timers[aId].cancel();
      delete this.timers[aId];
    }
  }
}
