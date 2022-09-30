/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  DevToolsServer,
} = require("resource://devtools/server/devtools-server.js");
const {
  longStringSpec,
  SimpleStringFront,
} = require("resource://devtools/shared/specs/string.js");
const {
  FrontClassWithSpec,
  registerFront,
} = require("resource://devtools/shared/protocol.js");

class LongStringFront extends FrontClassWithSpec(longStringSpec) {
  destroy() {
    this.initial = null;
    this.length = null;
    this.strPromise = null;
    super.destroy();
  }

  form(data) {
    this.actorID = data.actor;
    this.initial = data.initial;
    this.length = data.length;

    this._grip = data;
  }

  // We expose the grip so consumers (e.g. ObjectInspector) that handle webconsole
  // evaluations (which can return primitive, object fronts or longString front),
  // can directly call this without further check.
  getGrip() {
    return this._grip;
  }

  string() {
    if (!this.strPromise) {
      const promiseRest = thusFar => {
        if (thusFar.length === this.length) {
          return Promise.resolve(thusFar);
        }
        return this.substring(
          thusFar.length,
          thusFar.length + DevToolsServer.LONG_STRING_READ_LENGTH
        ).then(next => promiseRest(thusFar + next));
      };

      this.strPromise = promiseRest(this.initial);
    }
    return this.strPromise;
  }
}

exports.LongStringFront = LongStringFront;
registerFront(LongStringFront);
exports.SimpleStringFront = SimpleStringFront;
registerFront(SimpleStringFront);
