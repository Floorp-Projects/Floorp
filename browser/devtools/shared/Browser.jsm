/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is GCLI.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Walker <jwalker@mozilla.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
