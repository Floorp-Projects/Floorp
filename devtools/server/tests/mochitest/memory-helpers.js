/* exported Task, startServerAndGetSelectedTabMemory, destroyServerAndFinish,
   waitForTime, waitUntil */
"use strict";

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
const Services = require("Services");
const { DebuggerClient } = require("devtools/shared/client/debugger-client");
const { DebuggerServer } = require("devtools/server/main");

const { MemoryFront } = require("devtools/shared/fronts/memory");

// Always log packets when running tests.
Services.prefs.setBoolPref("devtools.debugger.log", true);
var gReduceTimePrecision = Services.prefs.getBoolPref("privacy.reduceTimerPrecision");
Services.prefs.setBoolPref("privacy.reduceTimerPrecision", false);
SimpleTest.registerCleanupFunction(function() {
  Services.prefs.clearUserPref("devtools.debugger.log");
  Services.prefs.setBoolPref("privacy.reduceTimerPrecision", gReduceTimePrecision);
});

function startServerAndGetSelectedTabMemory() {
  DebuggerServer.init();
  DebuggerServer.registerAllActors();
  const client = new DebuggerClient(DebuggerServer.connectPipe());

  return client.connect()
    .then(() => client.listTabs())
    .then(response => {
      const form = response.tabs[response.selected];
      const memory = MemoryFront(client, form, response);

      return { memory, client };
    });
}

function destroyServerAndFinish(client) {
  client.close().then(() => {
    DebuggerServer.destroy();
    SimpleTest.finish();
  });
}

function waitForTime(ms) {
  return new Promise((resolve, reject) => {
    setTimeout(resolve, ms);
  });
}

function waitUntil(predicate) {
  if (predicate()) {
    return Promise.resolve(true);
  }
  return new Promise(resolve => setTimeout(() => waitUntil(predicate)
         .then(() => resolve(true)), 10));
}
