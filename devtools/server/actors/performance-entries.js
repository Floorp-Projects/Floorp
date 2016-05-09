/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The performanceEntries actor emits events corresponding to performance
 * entries. It receives `performanceentry` events containing the performance
 * entry details and emits an event containing the name, type, origin, and
 * epoch of the performance entry.
 */

const {
  method, Arg, Option, RetVal, Front, FrontClass, Actor, ActorClass
} = require("devtools/shared/protocol");
const events = require("sdk/event/core");

var PerformanceEntriesActor = exports.PerformanceEntriesActor = ActorClass({

  typeName: "performanceEntries",

  listenerAdded: false,

  events: {
    "entry" : {
      type: "entry",
      detail: Arg(0, "json") // object containing performance entry name, type,
                             // origin, and epoch.
    }
  },

  initialize: function(conn, tabActor) {
    Actor.prototype.initialize.call(this, conn);
    this.window = tabActor.window;
  },

  /**
   * Start tracking the user timings.
   */
  start: method(function() {
    if (!this.listenerAdded) {
      this.onPerformanceEntry = this.onPerformanceEntry.bind(this);
      this.window.addEventListener("performanceentry", this.onPerformanceEntry, true);
      this.listenerAdded = true;
    }
  }),

  /**
   * Stop tracking the user timings.
   */
  stop: method(function() {
    if (this.listenerAdded) {
      this.window.removeEventListener("performanceentry", this.onPerformanceEntry, true);
      this.listenerAdded = false;
    }
  }),

  disconnect: function() {
    this.destroy();
  },

  destroy: function() {
    this.stop();
    Actor.prototype.destroy.call(this);
  },

  onPerformanceEntry: function (e) {
    let emitDetail = {
      type: e.entryType,
      name: e.name,
      origin: e.origin,
      epoch: e.epoch
    };
    events.emit(this, 'entry', emitDetail);
  }
});

exports.PerformanceEntriesFront = FrontClass(PerformanceEntriesActor, {
  initialize: function(client, form) {
    Front.prototype.initialize.call(this, client);
    this.actorID = form.performanceEntriesActor;
    this.manage(this);
  },
});
