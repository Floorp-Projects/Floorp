/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "experimental"
};

const { Cu } = require("chrome");
const { Class } = require("../sdk/core/heritage");
const { MessagePort, MessageChannel } = require("../sdk/messaging");
const { DevToolsShim } = Cu.import("chrome://devtools-shim/content/DevToolsShim.jsm", {});

const outputs = new WeakMap();
const inputs = new WeakMap();
const targets = new WeakMap();
const transports = new WeakMap();

const inputFor = port => inputs.get(port);
const outputFor = port => outputs.get(port);
const transportFor = port => transports.get(port);

const fromTarget = target => {
  const debuggee = new Debuggee();
  const { port1, port2 } = new MessageChannel();
  inputs.set(debuggee, port1);
  outputs.set(debuggee, port2);
  targets.set(debuggee, target);

  return debuggee;
};
exports.fromTarget = fromTarget;

const Debuggee = Class({
  extends: MessagePort.prototype,
  close: function() {
    const server = transportFor(this);
    if (server) {
      transports.delete(this);
      server.close();
    }
    outputFor(this).close();
  },
  start: function() {
    const target = targets.get(this);
    if (target.isLocalTab) {
      // Since a remote protocol connection will be made, let's start the
      // DebuggerServer here, once and for all tools.
      let transport = DevToolsShim.connectDebuggerServer();
      transports.set(this, transport);
    }
    // TODO: Implement support for remote connections (See Bug 980421)
    else {
      throw Error("Remote targets are not yet supported");
    }

    // pipe messages send to the debuggee to an actual
    // server via remote debugging protocol transport.
    inputFor(this).addEventListener("message", ({data}) =>
      transportFor(this).send(data));

    // pipe messages received from the remote debugging
    // server transport onto the this debuggee.
    transportFor(this).hooks = {
      onPacket: packet => inputFor(this).postMessage(packet),
      onClosed: () => inputFor(this).close()
    };

    inputFor(this).start();
    outputFor(this).start();
  },
  postMessage: function(data) {
    return outputFor(this).postMessage(data);
  },
  get onmessage() {
    return outputFor(this).onmessage;
  },
  set onmessage(onmessage) {
    outputFor(this).onmessage = onmessage;
  },
  addEventListener: function(...args) {
    return outputFor(this).addEventListener(...args);
  },
  removeEventListener: function(...args) {
    return outputFor(this).removeEventListener(...args);
  }
});
exports.Debuggee = Debuggee;
