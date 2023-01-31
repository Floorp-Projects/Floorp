/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol.js");
const {
  longStringSpec,
} = require("resource://devtools/shared/specs/string.js");

const {
  DevToolsServer,
} = require("resource://devtools/server/devtools-server.js");

exports.LongStringActor = class LongStringActor extends Actor {
  constructor(conn, str) {
    super(conn, longStringSpec);
    this.str = str;
    this.short = this.str.length < DevToolsServer.LONG_STRING_LENGTH;
  }

  destroy() {
    this.str = null;
    super.destroy();
  }

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
  }

  substring(start, end) {
    return this.str.substring(start, end);
  }

  release() {}
};
