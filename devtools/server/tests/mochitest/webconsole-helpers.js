/* exported attachURL, evaluateJS */
"use strict";

var Cu = Components.utils;

const {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
const {DebuggerClient} = require("devtools/shared/client/debugger-client");
const {DebuggerServer} = require("devtools/server/main");

const Services = require("Services");

// Always log packets when running tests.
Services.prefs.setBoolPref("devtools.debugger.log", true);
SimpleTest.registerCleanupFunction(function () {
  Services.prefs.clearUserPref("devtools.debugger.log");
});

if (!DebuggerServer.initialized) {
  DebuggerServer.init();
  DebuggerServer.addBrowserActors();
  SimpleTest.registerCleanupFunction(function () {
    DebuggerServer.destroy();
  });
}

/**
 * Open a tab, load the url, find the tab with the debugger server,
 * and attach the console to it.
 *
 * @param {string} url : url to navigate to
 * @return {Promise} Promise resolving when the console is attached.
 *         The Promise resolves with an object containing :
 *           - tab: the attached tab
 *           - tabClient: the tab client
 *           - consoleClient: the console client
 *           - cleanup: a generator function which can be called to close
 *             the opened tab and disconnect its debugger client.
 */
async function attachURL(url) {
  let win = window.open(url, "_blank");
  let client = null;

  let cleanup = function* () {
    if (client) {
      yield client.close();
      client = null;
    }
    if (win) {
      win.close();
      win = null;
    }
  };
  SimpleTest.registerCleanupFunction(cleanup);

  client = new DebuggerClient(DebuggerServer.connectPipe());
  await client.connect();
  let {tabs} = await client.listTabs();
  let attachedTab = tabs.find(tab => tab.url === url);

  if (!attachedTab) {
    throw new Error(`Could not find a tab matching URL ${url}`);
  }

  const [, tabClient] = await client.attachTab(attachedTab.actor);
  const [, consoleClient] = await client.attachConsole(attachedTab.consoleActor, []);

  return {
    tab: attachedTab,
    tabClient,
    consoleClient,
    cleanup,
  };
}
