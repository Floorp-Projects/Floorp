/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var jsm = {}; Cu.import("resource://gre/modules/XPCOMUtils.jsm", jsm);
var XPCOMUtils = jsm.XPCOMUtils;

var timerClass = Cc["@mozilla.org/timer;1"];
var nextID = 1;
var timers = {};

function TimerCallback(callback) {
  this._callback = callback;
  this.QueryInterface = XPCOMUtils.generateQI([Ci.nsITimerCallback]);
};

TimerCallback.prototype = {
  notify : function notify(timer) {
    try {
      for (timerID in timers)
        if (timers[timerID] === timer) {
          delete timers[timerID];
          break;
        }
      this._callback.apply(null, []);
    } catch (e) {
      console.exception(e);
    }
  }
};

var setTimeout = exports.setTimeout = function setTimeout(callback, delay) {
  var timer = timerClass.createInstance(Ci.nsITimer);

  var timerID = nextID++;
  timers[timerID] = timer;

  timer.initWithCallback(new TimerCallback(callback),
                         delay,
                         timer.TYPE_ONE_SHOT);
  return timerID;
};

var clearTimeout = exports.clearTimeout = function clearTimeout(timerID) {
  var timer = timers[timerID];
  if (timer) {
    timer.cancel();
    delete timers[timerID];
  }
};

require("unload").when(
  function cancelAllPendingTimers() {
    var timerIDs = [timerID for (timerID in timers)];
    timerIDs.forEach(function(timerID) { clearTimeout(timerID); });
  });
