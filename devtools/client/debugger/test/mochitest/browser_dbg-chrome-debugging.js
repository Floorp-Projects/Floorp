/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Tests that chrome debugging works.
 */

let gClient, gThreadFront;
let gNewChromeSource = promise.defer();

let { DevToolsLoader } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
let customLoader = new DevToolsLoader({
  invisibleToDebugger: true,
});
let { DevToolsServer } = customLoader.require("devtools/server/devtools-server");
let { DevToolsClient } = require("devtools/client/devtools-client");

add_task(async function() {
  gClient = initDevToolsClient();

  const [type] = await gClient.connect();
  is(type, "browser", "Root actor should identify itself as a browser.");

  const descriptorFront = await gClient.mainRoot.getMainProcess();
  const front = await descriptorFront.getTarget();
  await front.attach();
  const threadFront = await front.attachThread();
  gThreadFront = threadFront;
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:mozilla");

  // listen for a new source and global
  gThreadFront.on("newSource", onNewSource);

  // Force the creation of a new privileged source
  const systemPrincipal = Cc["@mozilla.org/systemprincipal;1"].createInstance(
      Ci.nsIPrincipal
    );
  const sandbox = Cu.Sandbox(systemPrincipal);
  Cu.evalInSandbox("function foo() {}", sandbox, null, "http://foo.com");

  await gNewChromeSource.promise;

  await resumeAndCloseConnection();
});

function initDevToolsClient() {
  DevToolsServer.init();
  DevToolsServer.registerAllActors();
  DevToolsServer.allowChromeProcess = true;

  let transport = DevToolsServer.connectPipe();
  return new DevToolsClient(transport);
}

function onNewSource(packet) {
  if (packet.source.url == "http://foo.com/") {
    ok(true, "Received the custom script source: " + packet.source.url);
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
