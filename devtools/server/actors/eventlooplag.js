/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The eventLoopLag actor emits "event-loop-lag" events when the event
 * loop gets unresponsive. The event comes with a "time" property (the
 * duration of the lag in milliseconds).
 */

const {Ci, Cu} = require("chrome");
const Services = require("Services");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
const protocol = require("devtools/shared/protocol");
const {method, Arg, RetVal} = protocol;
const events = require("sdk/event/core");

var EventLoopLagActor = exports.EventLoopLagActor = protocol.ActorClass({

  typeName: "eventLoopLag",

  _observerAdded: false,

  events: {
    "event-loop-lag" : {
      type: "event-loop-lag",
      time: Arg(0, "number") // duration of the lag in milliseconds.
    }
  },

  /**
   * Start tracking the event loop lags.
   */
  start: method(function () {
    if (!this._observerAdded) {
      Services.obs.addObserver(this, "event-loop-lag", false);
      this._observerAdded = true;
    }
    return Services.appShell.startEventLoopLagTracking();
  }, {
    request: {},
    response: {success: RetVal("number")}
  }),

  /**
   * Stop tracking the event loop lags.
   */
  stop: method(function () {
    if (this._observerAdded) {
      Services.obs.removeObserver(this, "event-loop-lag");
      this._observerAdded = false;
    }
    Services.appShell.stopEventLoopLagTracking();
  }, {request: {}, response: {}}),

  destroy: function () {
    this.stop();
    protocol.Actor.prototype.destroy.call(this);
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

exports.EventLoopLagFront = protocol.FrontClass(EventLoopLagActor, {
  initialize: function (client, form) {
    protocol.Front.prototype.initialize.call(this, client);
    this.actorID = form.eventLoopLagActor;
    this.manage(this);
  },
});
