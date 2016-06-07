/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {DebuggerServer} = require("devtools/server/main");
const promise = require("promise");
const {Class} = require("sdk/core/heritage");
const {longStringSpec} = require("devtools/shared/specs/string");
const protocol = require("devtools/shared/protocol");

const LongStringFront = protocol.FrontClassWithSpec(longStringSpec, {
  initialize: function (client) {
    protocol.Front.prototype.initialize.call(this, client);
  },

  destroy: function () {
    this.initial = null;
    this.length = null;
    this.strPromise = null;
    protocol.Front.prototype.destroy.call(this);
  },

  form: function (form) {
    this.actorID = form.actor;
    this.initial = form.initial;
    this.length = form.length;
  },

  string: function () {
    if (!this.strPromise) {
      let promiseRest = (thusFar) => {
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

/**
 * When a caller is expecting a LongString actor but the string is already available on
 * client, the SimpleStringFront can be used as it shares the same API as a
 * LongStringFront but will not make unnecessary trips to the server.
 */
exports.SimpleStringFront = Class({
  initialize: function (str) {
    this.str = str;
  },

  get length() {
    return this.str.length;
  },

  get initial() {
    return this.str;
  },

  string: function () {
    return promise.resolve(this.str);
  },

  substring: function (start, end) {
    return promise.resolve(this.str.substring(start, end));
  },

  release: function () {
    this.str = null;
    return promise.resolve(undefined);
  }
});
