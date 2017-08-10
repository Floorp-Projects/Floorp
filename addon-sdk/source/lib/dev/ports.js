/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "experimental"
};

// This module provides `marshal` and `demarshal` functions
// that can be used to send  MessagePort's over `nsIFrameMessageManager`
// until Bug 914974 is fixed.

const { add, iterator } = require("../sdk/lang/weak-set");
const { curry } = require("../sdk/lang/functional");

var id = 0;
const ports = new WeakMap();

// Takes `nsIFrameMessageManager` and `MessagePort` instances
// and returns a handle representing given `port`. Messages
// received on given `port` will be forwarded to a message
// manager under `sdk/port/message` and messages like:
// { port: { type: "MessagePort", id: 2}, data: data }
// Where id is an identifier associated with a given `port`
// and `data` is an `event.data` received on port.
const marshal = curry((manager, port) => {
  if (!ports.has(port)) {
    id = id + 1;
    const handle = {type: "MessagePort", id: id};
    // Bind id to the given port
    ports.set(port, handle);

    // Obtain a weak reference to a port.
    add(exports, port);

    port.onmessage = event => {
      manager.sendAsyncMessage("sdk/port/message", {
        port: handle,
        message: event.data
      });
    };

    return handle;
  }
  return ports.get(port);
});
exports.marshal = marshal;

// Takes `nsIFrameMessageManager` instance and a handle returned
// `marshal(manager, port)` returning a `port` that was passed
// to it. Note that `port` may be GC-ed in which case returned
// value will be `null`.
const demarshal = curry((manager, {type, id}) => {
  if (type === "MessagePort") {
    for (let port of iterator(exports)) {
      if (id === ports.get(port).id)
        return port;
    }
  }
  return null;
});
exports.demarshal = demarshal;
