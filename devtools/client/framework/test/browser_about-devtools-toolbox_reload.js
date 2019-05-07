/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that about:devtools-toolbox is reloaded correctly when reusing the same debugger
 * client instance.
 */
add_task(async function() {
  const debuggerClient = await createLocalClient();

  info("Preload a local DebuggerClient as this-firefox in the remoteClientManager");
  const { remoteClientManager } =
    require("devtools/client/shared/remote-debugging/remote-client-manager");
  remoteClientManager.setClient("this-firefox", "this-firefox", debuggerClient, {});
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
  ok(refreshedDoc.querySelector(".debug-target-info"),
     "about:devtools-toolbox header is correctly displayed");

  await removeTab(tab);
  await removeTab(targetTab);
});

async function createLocalClient() {
  const { DebuggerClient } = require("devtools/shared/client/debugger-client");
  const { DebuggerServer } = require("devtools/server/main");
  DebuggerServer.init();
  DebuggerServer.registerAllActors();
  DebuggerServer.allowChromeProcess = true;

  const debuggerClient = new DebuggerClient(DebuggerServer.connectPipe());
  await debuggerClient.connect();
  return debuggerClient;
}
