/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var {DebuggerServer} = require("devtools/server/main");

var promise = require("promise");

var protocol = require("devtools/shared/protocol");
const {longStringSpec} = require("devtools/shared/specs/string");

exports.LongStringActor = protocol.ActorClassWithSpec(longStringSpec, {
  initialize: function (conn, str) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.str = str;
    this.short = (this.str.length < DebuggerServer.LONG_STRING_LENGTH);
  },

  destroy: function () {
    this.str = null;
    protocol.Actor.prototype.destroy.call(this);
  },

  form: function () {
    if (this.short) {
      return this.str;
    }
    return {
      type: "longString",
      actor: this.actorID,
      length: this.str.length,
      initial: this.str.substring(0, DebuggerServer.LONG_STRING_INITIAL_LENGTH)
    };
  },

  substring: function (start, end) {
    return promise.resolve(this.str.substring(start, end));
  },

  release: function () { }
});
