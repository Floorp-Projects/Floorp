/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cu } = require("chrome");
let protocol = require("devtools/server/protocol");
let { method, RetVal, Arg, types } = protocol;
const { reportException } = require("devtools/shared/DevToolsUtils");
loader.lazyRequireGetter(this, "events", "sdk/event/core");

let MemprofActor = protocol.ActorClass({
  typeName: "memprof",

  initialize: function(conn) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this._profiler = Cc["@mozilla.org/tools/memory-profiler;1"]
      .getService(Ci.nsIMemoryProfiler);
  },

  destroy: function() {
    this._profiler = null;
    protocol.Actor.prototype.destroy.call(this);
  },

  startProfiler: method(function() {
    this._profiler.startProfiler();
  }, {
    request: {},
    response: {}
  }),

  stopProfiler: method(function() {
    this._profiler.stopProfiler();
  }, {
    request: {},
    response: {}
  }),

  resetProfiler: method(function() {
    this._profiler.resetProfiler();
  }, {
    request: {},
    response: {}
  }),

  getResults: method(function() {
    return this._profiler.getResults();
  }, {
    request: {},
    response: {
      ret: RetVal("json")
    }
  })
});

exports.MemprofActor = MemprofActor;

exports.MemprofFront = protocol.FrontClass(MemprofActor, {
  initialize: function(client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);
    this.actorID = form.memprofActor;
    this.manage(this);
  }
});
