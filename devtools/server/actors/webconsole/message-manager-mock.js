/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Implements a fake MessageManager class that allows to use the message
 * manager API within the same process. This implementation will forward
 * messages within the same process.
 *
 * It helps having the same codepath for actors being evaluated in the same
 * process *and* in a remote one.
 */
function MessageManagerMock() {
  this._listeners = new Map();
}
MessageManagerMock.prototype = {
  addMessageListener(name, listener) {
    let listeners = this._listeners.get(name);
    if (!listeners) {
      listeners = [];
      this._listeners.set(name, listeners);
    }
    if (!listeners.includes(listener)) {
      listeners.push(listener);
    }
  },
  removeMessageListener(name, listener) {
    const listeners = this._listeners.get(name);
    const idx = listeners.indexOf(listener);
    listeners.splice(idx, 1);
  },
  sendAsyncMessage(name, data) {
    this.other.internalSendAsyncMessage(name, data);
  },
  internalSendAsyncMessage(name, data) {
    const listeners = this._listeners.get(name);
    if (!listeners) {
      return;
    }
    const message = {
      target: this,
      data,
    };
    for (const listener of listeners) {
      if (
        typeof listener === "object" &&
        typeof listener.receiveMessage === "function"
      ) {
        listener.receiveMessage(message);
      } else if (typeof listener === "function") {
        listener(message);
      }
    }
  },
};

/**
 * Create two MessageManager mocks, connected to each others.
 * Calling sendAsyncMessage on the first will dispatch messages on the second one,
 * and the other way around
 */
exports.createMessageManagerMocks = function() {
  const a = new MessageManagerMock();
  const b = new MessageManagerMock();
  a.other = b;
  b.other = a;
  return [a, b];
};
