/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cu } = require("chrome");
const {
  Front,
  FrontClassWithSpec,
  custom
} = require("devtools/shared/protocol");
const { profilerSpec } = require("devtools/shared/specs/profiler");

loader.lazyRequireGetter(this, "events", "sdk/event/core");
loader.lazyRequireGetter(this, "extend", "sdk/util/object", true);

/**
 * This can be used on older Profiler implementations, but the methods cannot
 * be changed -- you must introduce a new method, and detect the server.
 */
exports.ProfilerFront = FrontClassWithSpec(profilerSpec, {
  initialize: function (client, form) {
    Front.prototype.initialize.call(this, client, form);
    this.actorID = form.profilerActor;
    this.manage(this);

    this._onProfilerEvent = this._onProfilerEvent.bind(this);
    events.on(this, "*", this._onProfilerEvent);
  },

  destroy: function () {
    events.off(this, "*", this._onProfilerEvent);
    Front.prototype.destroy.call(this);
  },

  /**
   * If using the protocol.js Fronts, then make stringify default,
   * since the read/write mechanisms will expose it as an object anyway, but
   * this lets other consumers who connect directly (xpcshell tests, Gecko Profiler) to
   * have unchanged behaviour.
   */
  getProfile: custom(function (options) {
    return this._getProfile(extend({ stringify: true }, options));
  }, {
    impl: "_getProfile"
  }),

  /**
   * Also emit an old `eventNotification` for older consumers of the profiler.
   */
  _onProfilerEvent: function (eventName, data) {
    // If this event already passed through once, don't repropagate
    if (data.relayed) {
      return;
    }
    data.relayed = true;

    if (eventName === "eventNotification") {
      // If this is `eventNotification`, this is coming from an older Gecko (<Fx42)
      // that doesn't use protocol.js style events. Massage it to emit a protocol.js
      // style event as well.
      events.emit(this, data.topic, data);
    } else {
      // Otherwise if a modern protocol.js event, emit it also as `eventNotification`
      // for compatibility reasons on the client (like for any add-ons/Gecko Profiler
      // using this event) and log a deprecation message if there is a listener.
      this.conn.emit("eventNotification", {
        subject: data.subject,
        topic: data.topic,
        data: data.data,
        details: data.details
      });
      if (this.conn._getListeners("eventNotification").length) {
        Cu.reportError(`
          ProfilerActor's "eventNotification" on the DebuggerClient has been deprecated.
          Use the ProfilerFront found in "devtools/server/actors/profiler".`);
      }
    }
  },
});
