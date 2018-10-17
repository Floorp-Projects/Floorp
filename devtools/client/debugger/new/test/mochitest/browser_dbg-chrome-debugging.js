/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that chrome debugging works.
 */

var gClient, gThreadClient;
var gNewChromeSource = promise.defer();

var { DevToolsLoader } = ChromeUtils.import(
  "resource://devtools/shared/Loader.jsm",
  {}
);
var customLoader = new DevToolsLoader();
customLoader.invisibleToDebugger = true;
var { DebuggerServer } = customLoader.require("devtools/server/main");
var { DebuggerClient } = require("devtools/shared/client/debugger-client");

function initDebuggerClient() {
  DebuggerServer.init();
  DebuggerServer.registerAllActors();
  DebuggerServer.allowChromeProcess = true;

  let transport = DebuggerServer.connectPipe();
  return new DebuggerClient(transport);
}

async function attachThread(client, actor) {
  let [response, targetFront] = await client.attachTarget(actor);
  let [response2, threadClient] = await targetFront.attachThread(null);
  return threadClient;
}

function onNewSource(event, packet) {
  if (packet.source.url.startsWith("chrome:")) {
    ok(true, "Received a new chrome source: " + packet.source.url);
    gThreadClient.removeListener("newSource", onNewSource);
    gNewChromeSource.resolve();
  }
}

function resumeAndCloseConnection() {
  return new Promise(resolve => {
    gThreadClient.resume(() => resolve(gClient.close()));
  });
}

registerCleanupFunction(function() {
  gClient = null;
  gThreadClient = null;
  gNewChromeSource = null;

  customLoader = null;
  DebuggerServer = null;
});

add_task(async function() {
  gClient = initDebuggerClient();

  const [type] = await gClient.connect();
  is(type, "browser", "Root actor should identify itself as a browser.");

  const response = await gClient.mainRoot.getProcess(0);
  let actor = response.form.actor;
  gThreadClient = await attachThread(gClient, actor);
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:mozilla");

  // listen for a new source and global
  gThreadClient.addListener("newSource", onNewSource);
  await gNewChromeSource.promise;

  await resumeAndCloseConnection();
});
