/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

const { require } = ChromeUtils.import(
  "resource://devtools/shared/loader/Loader.jsm"
);
const EventEmitter = require("devtools/shared/event-emitter");
const discovery = require("devtools/shared/discovery/discovery");
const { setTimeout, clearTimeout } = ChromeUtils.import(
  "resource://gre/modules/Timer.jsm"
);

Services.prefs.setBoolPref("devtools.discovery.log", true);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.discovery.log");
});

function log(msg) {
  info("DISCOVERY: " + msg);
}

// Global map of actively listening ports to TestTransport instances
var gTestTransports = {};

/**
 * Implements the same API as Transport in discovery.js.  Here, no UDP sockets
 * are used.  Instead, messages are delivered immediately.
 */
function TestTransport(port) {
  EventEmitter.decorate(this);
  this.port = port;
  gTestTransports[this.port] = this;
}

TestTransport.prototype = {
  send(object, port) {
    log("Send to " + port + ":\n" + JSON.stringify(object, null, 2));
    if (!gTestTransports[port]) {
      log("No listener on port " + port);
      return;
    }
    const message = JSON.stringify(object);
    gTestTransports[port].onPacketReceived(null, message);
  },

  destroy() {
    delete gTestTransports[this.port];
  },

  // nsIUDPSocketListener

  onPacketReceived(socket, message) {
    const object = JSON.parse(message);
    object.from = "localhost";
    log("Recv on " + this.port + ":\n" + JSON.stringify(object, null, 2));
    this.emit("message", object);
  },

  onStopListening(socket, status) {},
};

// Use TestTransport instead of the usual Transport
discovery._factories.Transport = TestTransport;

// Ignore name generation on b2g and force a fixed value
Object.defineProperty(discovery.device, "name", {
  get() {
    return "test-device";
  },
});

add_task(async function() {
  // At startup, no remote devices are known
  deepEqual(discovery.getRemoteDevicesWithService("devtools"), []);
  deepEqual(discovery.getRemoteDevicesWithService("penguins"), []);

  discovery.scan();

  // No services added yet, still empty
  deepEqual(discovery.getRemoteDevicesWithService("devtools"), []);
  deepEqual(discovery.getRemoteDevicesWithService("penguins"), []);

  discovery.addService("devtools", { port: 1234 });

  // Changes not visible until next scan
  deepEqual(discovery.getRemoteDevicesWithService("devtools"), []);
  deepEqual(discovery.getRemoteDevicesWithService("penguins"), []);

  await scanForChange("devtools", "added");

  // Now we see the new service
  deepEqual(discovery.getRemoteDevicesWithService("devtools"), ["test-device"]);
  deepEqual(discovery.getRemoteDevicesWithService("penguins"), []);

  discovery.addService("penguins", { tux: true });
  await scanForChange("penguins", "added");

  deepEqual(discovery.getRemoteDevicesWithService("devtools"), ["test-device"]);
  deepEqual(discovery.getRemoteDevicesWithService("penguins"), ["test-device"]);
  deepEqual(discovery.getRemoteDevices(), ["test-device"]);

  deepEqual(discovery.getRemoteService("devtools", "test-device"), {
    port: 1234,
    host: "localhost",
  });
  deepEqual(discovery.getRemoteService("penguins", "test-device"), {
    tux: true,
    host: "localhost",
  });

  discovery.removeService("devtools");
  await scanForChange("devtools", "removed");

  discovery.addService("penguins", { tux: false });
  await scanForChange("penguins", "updated");

  // Scan again, but nothing should be removed
  await scanForNoChange("penguins", "removed");

  // Split the scanning side from the service side to simulate the machine with
  // the service becoming unreachable
  gTestTransports = {};

  discovery.removeService("penguins");
  await scanForChange("penguins", "removed");
});

function scanForChange(service, changeType) {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      reject(new Error("Reply never arrived"));
    }, discovery.replyTimeout + 500);
    discovery.on(service + "-device-" + changeType, function onChange() {
      discovery.off(service + "-device-" + changeType, onChange);
      clearTimeout(timer);
      resolve();
    });
    discovery.scan();
  });
}

function scanForNoChange(service, changeType) {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      resolve();
    }, discovery.replyTimeout + 500);
    discovery.on(service + "-device-" + changeType, function onChange() {
      discovery.off(service + "-device-" + changeType, onChange);
      clearTimeout(timer);
      reject(new Error("Unexpected change occurred"));
    });
    discovery.scan();
  });
}
