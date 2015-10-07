var Cu = Components.utils;
var Cc = Components.classes;
var Ci = Components.interfaces;

var { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

// Always log packets when running tests.
Services.prefs.setBoolPref("devtools.debugger.log", true);
SimpleTest.registerCleanupFunction(function() {
  Services.prefs.clearUserPref("devtools.debugger.log");
});

var { Task } = Cu.import("resource://gre/modules/Task.jsm", {});
var { require } =
  Cu.import("resource://gre/modules/devtools/shared/Loader.jsm", {});

var { DebuggerClient } = require("devtools/shared/client/main");
var { DebuggerServer } = require("devtools/server/main");
var { MemprofFront } = require("devtools/server/actors/memprof");

function startServerAndGetSelectedTabMemprof() {
  DebuggerServer.init();
  DebuggerServer.addBrowserActors();
  var client = new DebuggerClient(DebuggerServer.connectPipe());

  return new Promise((resolve, reject) => {
    client.connect(response => {
      if (response.error) {
        reject(new Error(response.error + ": " + response.message));
        return;
      }

      client.listTabs(response => {
        if (response.error) {
          reject(new Error(response.error + ": " + response.message));
          return;
        }

        var form = response.tabs[response.selected];
        var memprof = MemprofFront(client, form);

        resolve({ memprof, client });
      });
    });
  });
}

function destroyServerAndFinish(client) {
  client.close(() => {
    DebuggerServer.destroy();
    SimpleTest.finish();
  });
}
