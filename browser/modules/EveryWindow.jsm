/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["EveryWindow"];

/*
 * This module enables consumers to register callbacks on every
 * current and future browser window.
 *
 * Usage: EveryWindow.registerCallback(id, init, uninit);
 *        EveryWindow.unregisterCallback(id);
 *
 * id is expected to be a unique value that identifies the
 * consumer, to be used for unregistration. If the id is already
 * in use, registerCallback returns false without doing anything.
 *
 * Each callback will receive the window for which it is presently
 * being called as the first argument.
 *
 * init is called on every existing window at the time of registration,
 * and on all future windows at browser-delayed-startup-finished.
 *
 * uninit is called on every existing window if requested at the time
 * of unregistration, and at the time of domwindowclosed.
 * If the window is closing, a second argument is passed with value `true`.
 */

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

var initialized = false;
var callbacks = new Map();

function callForEveryWindow(callback) {
  let windowList = Services.wm.getEnumerator("navigator:browser");
  for (let win of windowList) {
    win.delayedStartupPromise.then(() => { callback(win); });
  }
}

this.EveryWindow = {
  /**
   * Registers init and uninit functions to be called on every window.
   *
   * @param {string} id A unique identifier for the consumer, to be
   *   used for unregistration.
   * @param {function} init The function to be called on every currently
   *   existing window and every future window after delayed startup.
   * @param {function} uninit The function to be called on every window
   *   at the time of callback unregistration or after domwindowclosed.
   * @returns {boolean} Returns false if the id was taken, else true.
   */
  registerCallback: function EW_registerCallback(id, init, uninit) {
    if (callbacks.has(id)) {
      return false;
    }

    if (!initialized) {
      let addUnloadListener = (win) => {
        function observer(subject, topic, data) {
          if (topic == "domwindowclosed" && subject === win) {
            Services.ww.unregisterNotification(observer);
            for (let c of callbacks.values()) {
              c.uninit(win, true);
            }
          }
        }
        Services.ww.registerNotification(observer);
      };

      Services.obs.addObserver(win => {
        for (let c of callbacks.values()) {
          c.init(win);
        }
        addUnloadListener(win);
      }, "browser-delayed-startup-finished");

      callForEveryWindow(addUnloadListener);

      initialized = true;
    }

    callForEveryWindow(init);
    callbacks.set(id, {id, init, uninit});

    return true;
  },

  /**
   * Unregisters a previously registered consumer.
   *
   * @param {string} id The id to unregister.
   * @param {boolean} [callUninit=true] Whether to call the registered uninit
   *   function on every window.
   */
  unregisterCallback: function EW_unregisterCallback(id, callUninit = true) {
    if (!callbacks.has(id)) {
      return;
    }

    if (callUninit) {
      callForEveryWindow(callbacks.get(id).uninit);
    }

    callbacks.delete(id);
  },
};
