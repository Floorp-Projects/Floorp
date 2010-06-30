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
 * The Original Code is Jetpack.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Atul Varma <atul@mozilla.com>
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
