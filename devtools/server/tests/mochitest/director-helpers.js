var Cu = Components.utils;
const {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
const {DebuggerClient} = require("devtools/shared/client/main");
const {DebuggerServer} = require("devtools/server/main");
const Services = require("Services");

// Always log packets when running tests.
Services.prefs.setBoolPref("devtools.debugger.log", true);
Services.prefs.setBoolPref("dom.mozBrowserFramesEnabled", true);

SimpleTest.registerCleanupFunction(function () {
  Services.prefs.clearUserPref("devtools.debugger.log");
  Services.prefs.clearUserPref("dom.mozBrowserFramesEnabled");
});

const { DirectorRegistry } = require("devtools/server/actors/director-registry");
const { DirectorRegistryFront } = require("devtools/shared/fronts/director-registry");

const { DirectorManagerFront } = require("devtools/shared/fronts/director-manager");

const { Task } = require("devtools/shared/task");

/** *********************************
 *  director helpers functions
 **********************************/

function* newConnectedDebuggerClient(opts) {
  var transport = DebuggerServer.connectPipe();
  var client = new DebuggerClient(transport);

  yield client.connect();

  var root = yield client.listTabs();

  return {
    client: client,
    root: root,
    transport: transport
  };
}

function purgeInstalledDirectorScripts() {
  DirectorRegistry.clear();
}
