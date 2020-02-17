/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that chrome debugging works.
 */

var gClient, gThreadFront;
var gNewChromeSource = promise.defer();

var { DevToolsLoader } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
var customLoader = new DevToolsLoader({
  invisibleToDebugger: true,
});
var { DevToolsServer } = customLoader.require("devtools/server/devtools-server");
var { DebuggerClient } = require("devtools/shared/client/debugger-client");

function initDebuggerClient() {
  DevToolsServer.init();
  DevToolsServer.registerAllActors();
  DevToolsServer.allowChromeProcess = true;

  let transport = DevToolsServer.connectPipe();
  return new DebuggerClient(transport);
}

function onNewSource(packet) {
  if (packet.source.url.startsWith("chrome:")) {
    ok(true, "Received a new chrome source: " + packet.source.url);
    gThreadFront.off("newSource", onNewSource);
    gNewChromeSource.resolve();
  }
}

async function resumeAndCloseConnection() {
  await gThreadFront.resume();
  return gClient.close();
}

registerCleanupFunction(function() {
  gClient = null;
  gThreadFront = null;
  gNewChromeSource = null;

  customLoader = null;
  DevToolsServer = null;
});

add_task(async function() {
  gClient = initDebuggerClient();

  const [type] = await gClient.connect();
  is(type, "browser", "Root actor should identify itself as a browser.");

  const front = await gClient.mainRoot.getMainProcess();
  await front.attach();
  const [, threadFront] = await front.attachThread();
  gThreadFront = threadFront;
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:mozilla");

  // listen for a new source and global
  gThreadFront.on("newSource", onNewSource);
  await gNewChromeSource.promise;

  await resumeAndCloseConnection();
});
