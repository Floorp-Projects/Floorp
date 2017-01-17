/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * The eventLoopLag actor emits "event-loop-lag" events when the event
 * loop gets unresponsive. The event comes with a "time" property (the
 * duration of the lag in milliseconds).
 */

const {Ci} = require("chrome");
const Services = require("Services");
const {XPCOMUtils} = require("resource://gre/modules/XPCOMUtils.jsm");
const {Actor, ActorClassWithSpec} = require("devtools/shared/protocol");
const events = require("sdk/event/core");
const {eventLoopLagSpec} = require("devtools/shared/specs/eventlooplag");

exports.EventLoopLagActor = ActorClassWithSpec(eventLoopLagSpec, {
  _observerAdded: false,

  /**
   * Start tracking the event loop lags.
   */
  start: function () {
    if (!this._observerAdded) {
      Services.obs.addObserver(this, "event-loop-lag", false);
      this._observerAdded = true;
    }
    return Services.appShell.startEventLoopLagTracking();
  },

  /**
   * Stop tracking the event loop lags.
   */
  stop: function () {
    if (this._observerAdded) {
      Services.obs.removeObserver(this, "event-loop-lag");
      this._observerAdded = false;
    }
    Services.appShell.stopEventLoopLagTracking();
  },

  destroy: function () {
    this.stop();
    Actor.prototype.destroy.call(this);
  },

  // nsIObserver

  observe: function (subject, topic, data) {
    if (topic == "event-loop-lag") {
      // Forward event loop lag event
      events.emit(this, "event-loop-lag", data);
    }
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),
});
