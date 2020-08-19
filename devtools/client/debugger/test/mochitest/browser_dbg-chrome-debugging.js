/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Tests that chrome debugging works.
 */

add_task(async function() {
  const client = initDevToolsClient();

  const [type] = await client.connect();
  is(type, "browser", "Root actor should identify itself as a browser.");

  const descriptorFront = await client.mainRoot.getMainProcess();
  const front = await descriptorFront.getTarget();
  await front.attach();
  const threadFront = await front.attachThread();

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:mozilla");

  // listen for a new source and global
  const onFooDotComNewSource = new Promise(resolve => {
    function onNewSource(packet) {
      if (packet.source.url == "http://foo.com/") {
        threadFront.off("newSource", onNewSource);
        resolve(packet);
      }
    }
    threadFront.on("newSource", onNewSource);
  });

  // Force the creation of a new privileged source
  const systemPrincipal = Cc["@mozilla.org/systemprincipal;1"].createInstance(
    Ci.nsIPrincipal
  );
  const sandbox = Cu.Sandbox(systemPrincipal);
  Cu.evalInSandbox("function foo() {}", sandbox, null, "http://foo.com");

  const packet = await onFooDotComNewSource;
  ok(true, "Received the custom script source: " + packet.source.url);

  await threadFront.resume();
  await client.close();
});

function initDevToolsClient() {
  const { DevToolsLoader } = ChromeUtils.import(
    "resource://devtools/shared/Loader.jsm"
  );
  const customLoader = new DevToolsLoader({
    invisibleToDebugger: true,
  });
  const { DevToolsServer } = customLoader.require(
    "devtools/server/devtools-server"
  );
  const { DevToolsClient } = require("devtools/client/devtools-client");

  DevToolsServer.init();
  DevToolsServer.registerAllActors();
  DevToolsServer.allowChromeProcess = true;

  const transport = DevToolsServer.connectPipe();
  return new DevToolsClient(transport);
}
