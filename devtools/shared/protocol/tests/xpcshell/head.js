/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
const Services = require("Services");
const { DevToolsServer } = require("devtools/server/devtools-server");
const { DevToolsClient } = require("devtools/client/devtools-client");

function dumpn(msg) {
  dump("DBG-TEST: " + msg + "\n");
}

function connectPipeTracing() {
  return new TracingTransport(DevToolsServer.connectPipe());
}

/**
 * Mock the `Transport` class in order to intercept all the packet
 * getting in and out and then being able to assert them and dump them.
 */
function TracingTransport(childTransport) {
  this.hooks = null;
  this.child = childTransport;
  this.child.hooks = this;

  this.expectations = [];
  this.packets = [];
  this.checkIndex = 0;
}

TracingTransport.prototype = {
  // Remove actor names
  normalize: function(packet) {
    return JSON.parse(
      JSON.stringify(packet, (key, value) => {
        if (key === "to" || key === "from" || key === "actor") {
          return "<actorid>";
        }
        return value;
      })
    );
  },
  send: function(packet) {
    this.packets.push({
      type: "sent",
      packet: this.normalize(packet),
    });
    return this.child.send(packet);
  },
  close: function() {
    return this.child.close();
  },
  ready: function() {
    return this.child.ready();
  },
  onPacket: function(packet) {
    this.packets.push({
      type: "received",
      packet: this.normalize(packet),
    });
    this.hooks.onPacket(packet);
  },
  onTransportClosed: function() {
    if (this.hooks.onTransportClosed) {
      this.hooks.onTransportClosed();
    }
  },

  expectSend: function(expected) {
    const packet = this.packets[this.checkIndex++];
    Assert.equal(packet.type, "sent");
    deepEqual(packet.packet, this.normalize(expected));
  },

  expectReceive: function(expected) {
    const packet = this.packets[this.checkIndex++];
    Assert.equal(packet.type, "received");
    deepEqual(packet.packet, this.normalize(expected));
  },

  // Write your tests, call dumpLog at the end, inspect the output,
  // then sprinkle the calls through the right places in your test.
  dumpLog: function() {
    for (const entry of this.packets) {
      if (entry.type === "sent") {
        dumpn("trace.expectSend(" + entry.packet + ");");
      } else {
        dumpn("trace.expectReceive(" + entry.packet + ");");
      }
    }
  },
};
