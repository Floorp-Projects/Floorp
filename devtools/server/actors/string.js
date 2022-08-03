/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { DevToolsServer } = require("devtools/server/devtools-server");

var protocol = require("devtools/shared/protocol");
const { longStringSpec } = require("devtools/shared/specs/string");

exports.LongStringActor = protocol.ActorClassWithSpec(longStringSpec, {
  initialize(conn, str) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.str = str;
    this.short = this.str.length < DevToolsServer.LONG_STRING_LENGTH;
  },

  destroy() {
    this.str = null;
    protocol.Actor.prototype.destroy.call(this);
  },

  form() {
    if (this.short) {
      return this.str;
    }
    return {
      type: "longString",
      actor: this.actorID,
      length: this.str.length,
      initial: this.str.substring(0, DevToolsServer.LONG_STRING_INITIAL_LENGTH),
    };
  },

  substring(start, end) {
    return this.str.substring(start, end);
  },

  release() {},
});
