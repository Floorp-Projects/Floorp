/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {DebuggerServer} = require("devtools/server/main");
const promise = require("promise");
const {longStringSpec, SimpleStringFront} = require("devtools/shared/specs/string");
const protocol = require("devtools/shared/protocol");

const LongStringFront = protocol.FrontClassWithSpec(longStringSpec, {
  initialize: function(client) {
    protocol.Front.prototype.initialize.call(this, client);
  },

  destroy: function() {
    this.initial = null;
    this.length = null;
    this.strPromise = null;
    protocol.Front.prototype.destroy.call(this);
  },

  form: function(form) {
    this.actorID = form.actor;
    this.initial = form.initial;
    this.length = form.length;
  },

  string: function() {
    if (!this.strPromise) {
      const promiseRest = (thusFar) => {
        if (thusFar.length === this.length) {
          return promise.resolve(thusFar);
        }
        return this.substring(thusFar.length,
                              thusFar.length + DebuggerServer.LONG_STRING_READ_LENGTH)
          .then((next) => promiseRest(thusFar + next));
      };

      this.strPromise = promiseRest(this.initial);
    }
    return this.strPromise;
  }
});

exports.LongStringFront = LongStringFront;
exports.SimpleStringFront = SimpleStringFront;
