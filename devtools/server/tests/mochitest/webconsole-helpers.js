/* exported attachURL, evaluateJS */
"use strict";

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
const { DebuggerServer } = require("devtools/server/main");
const { TargetFactory } = require("devtools/client/framework/target");

const Services = require("Services");

// Always log packets when running tests.
Services.prefs.setBoolPref("devtools.debugger.log", true);
SimpleTest.registerCleanupFunction(function() {
  Services.prefs.clearUserPref("devtools.debugger.log");
});

if (!DebuggerServer.initialized) {
  DebuggerServer.init();
  DebuggerServer.registerAllActors();
  SimpleTest.registerCleanupFunction(function() {
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
 *           - targetFront: the target front
 *           - consoleClient: the console client
 *           - cleanup: a generator function which can be called to close
 *             the opened tab and disconnect its debugger client.
 */
async function attachURL(url) {
  const tab = await addTab(url);
  const target = await TargetFactory.forTab(tab);
  await target.attach();
  return {
    consoleClient: target.activeConsole,
  };
}

/**
 * Naive implementaion of addTab working from a mochitest-chrome test.
 */
async function addTab(url) {
  const { gBrowser } = Services.wm.getMostRecentWindow("navigator:browser");
  const {
    BrowserTestUtils,
  } = require("resource://testing-common/BrowserTestUtils.jsm");
  const tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, url));
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  return tab;
}
