var Cu = Components.utils;
var Cc = Components.classes;
var Ci = Components.interfaces;

var { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
const { Task } = require("devtools/shared/task");
var Services = require("Services");
var { DebuggerClient } = require("devtools/shared/client/main");
var { DebuggerServer } = require("devtools/server/main");

var { MemoryFront } = require("devtools/shared/fronts/memory");

// Always log packets when running tests.
Services.prefs.setBoolPref("devtools.debugger.log", true);
SimpleTest.registerCleanupFunction(function () {
  Services.prefs.clearUserPref("devtools.debugger.log");
});

function startServerAndGetSelectedTabMemory() {
  DebuggerServer.init();
  DebuggerServer.addBrowserActors();
  var client = new DebuggerClient(DebuggerServer.connectPipe());

  return client.connect()
    .then(() => client.listTabs())
    .then(response => {
      var form = response.tabs[response.selected];
      var memory = MemoryFront(client, form, response);

      return { memory, client };
    });
}

function destroyServerAndFinish(client) {
  client.close(() => {
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
  return new Promise(resolve => setTimeout(() => waitUntil(predicate).then(() => resolve(true)), 10));
}
