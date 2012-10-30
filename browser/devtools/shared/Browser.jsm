/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Define various constants to match the globals provided by the browser.
 * This module helps cases where code is shared between the web and Firefox.
 * See also Console.jsm for an implementation of the Firefox console that
 * forwards to dump();
 */

const EXPORTED_SYMBOLS = [ "Node", "HTMLElement", "setTimeout", "clearTimeout" ];

/**
 * Expose Node/HTMLElement objects. This allows us to use the Node constants
 * without resorting to hardcoded numbers
 */
const Node = Components.interfaces.nsIDOMNode;
const HTMLElement = Components.interfaces.nsIDOMHTMLElement;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

/**
 * The next value to be returned by setTimeout
 */
let nextID = 1;

/**
 * The map of outstanding timeouts
 */
const timers = {};

/**
 * Object to be passed to Timer.initWithCallback()
 */
function TimerCallback(callback) {
  this._callback = callback;
  const interfaces = [ Components.interfaces.nsITimerCallback ];
  this.QueryInterface = XPCOMUtils.generateQI(interfaces);
}

TimerCallback.prototype.notify = function(timer) {
  try {
    for (let timerID in timers) {
      if (timers[timerID] === timer) {
        delete timers[timerID];
        break;
      }
    }
    this._callback.apply(null, []);
  }
  catch (ex) {
    dump(ex +  '\n');
  }
};

/**
 * Executes a code snippet or a function after specified delay.
 * This is designed to have the same interface contract as the browser
 * function.
 * @param callback is the function you want to execute after the delay.
 * @param delay is the number of milliseconds that the function call should
 * be delayed by. Note that the actual delay may be longer, see Notes below.
 * @return the ID of the timeout, which can be used later with
 * window.clearTimeout.
 */
const setTimeout = function setTimeout(callback, delay) {
  const timer = Components.classes["@mozilla.org/timer;1"]
                        .createInstance(Components.interfaces.nsITimer);

  let timerID = nextID++;
  timers[timerID] = timer;

  timer.initWithCallback(new TimerCallback(callback), delay, timer.TYPE_ONE_SHOT);
  return timerID;
};

/**
 * Clears the delay set by window.setTimeout() and prevents the callback from
 * being executed (if it hasn't been executed already)
 * @param timerID the ID of the timeout you wish to clear, as returned by
 * window.setTimeout().
 */
const clearTimeout = function clearTimeout(timerID) {
  let timer = timers[timerID];
  if (timer) {
    timer.cancel();
    delete timers[timerID];
  }
};
