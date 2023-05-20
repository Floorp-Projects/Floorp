/* exported addTabAndCreateCommands */
"use strict";

const { require } = ChromeUtils.importESModule(
  "resource://devtools/shared/loader/Loader.sys.mjs"
);
const {
  DevToolsServer,
} = require("resource://devtools/server/devtools-server.js");
const {
  CommandsFactory,
} = require("resource://devtools/shared/commands/commands-factory.js");

// Always log packets when running tests.
Services.prefs.setBoolPref("devtools.debugger.log", true);
SimpleTest.registerCleanupFunction(function () {
  Services.prefs.clearUserPref("devtools.debugger.log");
});

if (!DevToolsServer.initialized) {
  DevToolsServer.init();
  DevToolsServer.registerAllActors();
  SimpleTest.registerCleanupFunction(function () {
    DevToolsServer.destroy();
  });
}

/**
 * Open a tab, load the url, find the tab with the devtools server,
 * and attach the console to it.
 *
 * @param {string} url : url to navigate to
 * @return {Promise} Promise resolving when commands are initialized
 *         The Promise resolves with the commands.
 */
async function addTabAndCreateCommands(url) {
  const tab = await addTab(url);
  const commands = await CommandsFactory.forTab(tab);
  await commands.targetCommand.startListening();
  return commands;
}

/**
 * Naive implementaion of addTab working from a mochitest-chrome test.
 */
async function addTab(url) {
  const { gBrowser } = Services.wm.getMostRecentWindow("navigator:browser");
  const { BrowserTestUtils } = ChromeUtils.importESModule(
    "resource://testing-common/BrowserTestUtils.sys.mjs"
  );
  const tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, url));
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  return tab;
}
