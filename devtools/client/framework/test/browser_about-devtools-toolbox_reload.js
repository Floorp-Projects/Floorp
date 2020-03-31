/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that about:devtools-toolbox is reloaded correctly when reusing the same debugger
 * client instance.
 */
add_task(async function() {
  const devToolsClient = await createLocalClient();

  info(
    "Preload a local DevToolsClient as this-firefox in the remoteClientManager"
  );
  const {
    remoteClientManager,
  } = require("devtools/client/shared/remote-debugging/remote-client-manager");
  remoteClientManager.setClient(
    "this-firefox",
    "this-firefox",
    devToolsClient,
    {}
  );
  registerCleanupFunction(() => {
    remoteClientManager.removeAllClients();
  });

  info("Create a dummy target tab");
  const targetTab = await addTab("data:text/html,somehtml");

  const { tab } = await openAboutToolbox({
    id: targetTab.linkedBrowser.outerWindowID,
    remoteId: "this-firefox-this-firefox",
    type: "tab",
  });

  info("Reload about:devtools-toolbox page");
  const onToolboxReady = gDevTools.once("toolbox-ready");
  tab.linkedBrowser.reload();
  await onToolboxReady;

  info("Check if about:devtools-toolbox was reloaded correctly");
  const refreshedDoc = tab.linkedBrowser.contentDocument;
  ok(
    refreshedDoc.querySelector(".debug-target-info"),
    "about:devtools-toolbox header is correctly displayed"
  );

  const onToolboxDestroy = gDevTools.once("toolbox-destroyed");
  await removeTab(tab);
  await onToolboxDestroy;
  await devToolsClient.close();
  await removeTab(targetTab);
});

async function createLocalClient() {
  const { DevToolsClient } = require("devtools/client/devtools-client");
  const { DevToolsServer } = require("devtools/server/devtools-server");
  DevToolsServer.init();
  DevToolsServer.registerAllActors();
  DevToolsServer.allowChromeProcess = true;

  const devToolsClient = new DevToolsClient(DevToolsServer.connectPipe());
  await devToolsClient.connect();
  return devToolsClient;
}
