/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

this.EXPORTED_SYMBOLS = [
  "EventEmitter"
];

// Simple event emitter abstraction for storage objects to use.
function EventEmitter() {
  this._events = new Map();
}

EventEmitter.prototype = {
  on(event, listener) {
    if (this._events.has(event)) {
      this._events.get(event).add(listener);
    } else {
      this._events.set(event, new Set([listener]));
    }
  },
  off(event, listener) {
    if (!this._events.has(event)) {
      return;
    }
    this._events.get(event).delete(listener);
  },
  emit(event, ...args) {
    if (!this._events.has(event)) {
      return;
    }
    for (let listener of this._events.get(event).values()) {
      try {
        listener.apply(this, args);
      } catch (e) {
        Cu.reportError(e);
      }
    }
  },
};

