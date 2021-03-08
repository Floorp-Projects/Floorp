/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DevToolsClient } = require("devtools/client/devtools-client");
const { DevToolsServer } = require("devtools/server/devtools-server");

add_task(async () => {
  // Disable the preloaded process as it gets created lazily and may interfere
  // with process count assertions
  await pushPref("dom.ipc.processPrelaunch.enabled", false);
  // This preference helps destroying the content process when we close the tab
  await pushPref("dom.ipc.keepProcessesAlive.web", 1);

  // Init debugger server.
  DevToolsServer.init();
  DevToolsServer.registerAllActors();

  // Allow pulling descriptor for processes
  DevToolsServer.allowChromeProcess = true;

  // Create and connect a debugger client.
  const transport = DevToolsServer.connectPipe();
  const client = new DevToolsClient(transport);
  await client.connect();

  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "data:text/html,foo",
    forceNewProcess: true,
  });

  const { osPid } = tab.linkedBrowser.browsingContext.currentWindowGlobal;

  // Get the fresh process descriptor
  const descriptor = await client.mainRoot.getProcess(osPid);
  ok(descriptor, "Got the process descriptor");
  is(descriptor.id, osPid, "descriptor id is the PID");
  is(descriptor.isParent, false, "isParent is false for content processes");

  // Force getting the target, otherwise we don't connect to the process
  // via the connector and don't know when the process is destroyed.
  await descriptor.getTarget();

  const onDestroyed = descriptor.once("descriptor-destroyed");
  BrowserTestUtils.removeTab(tab);
  info("Wait for descriptor destruction");
  await onDestroyed;

  await client.close();
});
