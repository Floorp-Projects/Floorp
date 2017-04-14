/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  "stability": "stable"
};

const { CC, Cc, Ci } = require("chrome");
const { when: unload } = require("./system/unload");

const { TYPE_ONE_SHOT, TYPE_REPEATING_SLACK } = Ci.nsITimer;
const Timer = CC("@mozilla.org/timer;1", "nsITimer");
const timers = Object.create(null);
const threadManager = Cc["@mozilla.org/thread-manager;1"].
                      getService(Ci.nsIThreadManager);
const prefBranch = Cc["@mozilla.org/preferences-service;1"].
                    getService(Ci.nsIPrefService).
                    QueryInterface(Ci.nsIPrefBranch);

var MIN_DELAY = 4;
// Try to get min timeout delay used by browser.
try { MIN_DELAY = prefBranch.getIntPref("dom.min_timeout_value"); } finally {}


// Last timer id.
var lastID = 0;

// Sets typer either by timeout or by interval
// depending on a given type.
function setTimer(type, callback, delay, ...args) {
  let id = ++ lastID;
  let timer = timers[id] = Timer();
  timer.initWithCallback({
    notify: function notify() {
      try {
        if (type === TYPE_ONE_SHOT)
          delete timers[id];
        callback.apply(null, args);
      }
      catch(error) {
        console.exception(error);
      }
    }
  }, Math.max(delay || MIN_DELAY), type);
  return id;
}

function unsetTimer(id) {
  let timer = timers[id];
  delete timers[id];
  if (timer) timer.cancel();
}

var immediates = new Map();

var dispatcher = _ => {
  // Allow scheduling of a new dispatch loop.
  dispatcher.scheduled = false;
  // Take a snapshot of timer `id`'s that have being present before
  // starting a dispatch loop, in order to ignore timers registered
  // in side effect to dispatch while also skipping immediates that
  // were removed in side effect.
  let ids = [...immediates.keys()];
  for (let id of ids) {
    let immediate = immediates.get(id);
    if (immediate) {
      immediates.delete(id);
      try { immediate(); }
      catch (error) { console.exception(error); }
    }
  }
}

function setImmediate(callback, ...params) {
  let id = ++ lastID;
  // register new immediate timer with curried params.
  immediates.set(id, _ => callback.apply(callback, params));
  // if dispatch loop is not scheduled schedule one. Own scheduler
  if (!dispatcher.scheduled) {
    dispatcher.scheduled = true;
    threadManager.currentThread.dispatch(dispatcher,
                                         Ci.nsIThread.DISPATCH_NORMAL);
  }
  return id;
}

function clearImmediate(id) {
  immediates.delete(id);
}

// Bind timers so that toString-ing them looks same as on native timers.
exports.setImmediate = setImmediate.bind(null);
exports.clearImmediate = clearImmediate.bind(null);
exports.setTimeout = setTimer.bind(null, TYPE_ONE_SHOT);
exports.setInterval = setTimer.bind(null, TYPE_REPEATING_SLACK);
exports.clearTimeout = unsetTimer.bind(null);
exports.clearInterval = unsetTimer.bind(null);

// all timers are cleared out on unload.
unload(function() {
  immediates.clear();
  Object.keys(timers).forEach(unsetTimer)
});
