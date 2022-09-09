/* exported addTabAndCreateCommands */
"use strict";

const { require } = ChromeUtils.import(
  "resource://devtools/shared/loader/Loader.jsm"
);
const { DevToolsServer } = require("devtools/server/devtools-server");
const {
  CommandsFactory,
} = require("devtools/shared/commands/commands-factory");

// Always log packets when running tests.
Services.prefs.setBoolPref("devtools.debugger.log", true);
SimpleTest.registerCleanupFunction(function() {
  Services.prefs.clearUserPref("devtools.debugger.log");
});

if (!DevToolsServer.initialized) {
  DevToolsServer.init();
  DevToolsServer.registerAllActors();
  SimpleTest.registerCleanupFunction(function() {
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
  const {
    BrowserTestUtils,
  } = require("resource://testing-common/BrowserTestUtils.jsm");
  const tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, url));
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  return tab;
}
