/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Assert the lifecycle of tab descriptors between listTabs and getTab
 */

var { DevToolsServer } = require("devtools/server/devtools-server");
var { DevToolsClient } = require("devtools/client/devtools-client");

const TAB1_URL = EXAMPLE_URL + "doc_empty-tab-01.html";

add_task(async function test() {
  DevToolsServer.init();
  DevToolsServer.registerAllActors();

  const transport = DevToolsServer.connectPipe();
  const client = new DevToolsClient(transport);
  const [type] = await client.connect();
  is(type, "browser", "Root actor should identify itself as a browser.");
  const tab = await addTab(TAB1_URL);

  await assertListTabs(client.mainRoot);

  // To run last as it will close the client
  await assertGetTab(client, client.mainRoot, tab);

  await removeTab(tab);
});

async function assertListTabs(rootFront) {
  const tabDescriptors = await rootFront.listTabs();
  is(tabDescriptors.length, 2, "Should be two tabs");

  const tabDescriptor = tabDescriptors.find(d => d.url == TAB1_URL);
  ok(tabDescriptor, "Should have a descriptor actor for the tab");
  ok(
    !tabDescriptor.isLocalTab,
    "listTabs's tab descriptor aren't considered as local tabs"
  );

  const tabTarget = await tabDescriptor.getTarget();
  ok(
    !tabTarget.shouldCloseClient,
    "Tab targets from listTabs shouldn't auto-close their client"
  );
  ok(isTargetAttached(tabTarget), "The tab target should be attached");

  info("Detach the tab target");
  const onTargetDestroyed = tabTarget.once("target-destroyed");
  const onDescriptorDestroyed = tabDescriptor.once("descriptor-destroyed");
  await tabTarget.detach();

  info("Wait for target destruction");
  await onTargetDestroyed;

  info("Wait for descriptor destruction");
  await onDescriptorDestroyed;

  // Tab Descriptors returned by listTabs are not considered as local tabs
  // and follow the lifecycle of their target. So both target and descriptor are destroyed.
  ok(
    tabTarget.isDestroyed(),
    "The tab target should be destroyed after detach"
  );
  ok(
    tabDescriptor.isDestroyed(),
    "The tab descriptor should be destroyed after detach"
  );

  // Compared to getTab, the client should be kept alive.
  // Do another request to assert that.
  await rootFront.listTabs();
}

async function assertGetTab(client, rootFront, tab) {
  const tabDescriptor = await rootFront.getTab({ tab });
  ok(tabDescriptor, "Should have a descriptor actor for the tab");
  ok(
    tabDescriptor.isLocalTab,
    "getTab's tab descriptor are considered as local tabs, but only when a tab argument is given"
  );

  // Also assert that getTab only return local tabs when passing the "tab" argument.
  // Other arguments will return a non-local tab, having same behavior as listTabs's descriptors
  // Test on another tab, otherwise we would return the same descriptor as the one retrieved
  // from assertGetTab.
  const tab2 = await addTab("data:text/html,second tab");
  const tabDescriptor2 = await rootFront.getTab({
    outerWindowID: tab2.linkedBrowser.outerWindowID,
  });
  ok(
    !tabDescriptor2.isLocalTab,
    "getTab's tab descriptor aren't considered as local tabs when we pass an outerWindowID"
  );
  await removeTab(tab2);

  const tabTarget = await tabDescriptor.getTarget();
  ok(
    tabTarget.shouldCloseClient,
    "Tab targets from getTab shoul close their client"
  );
  ok(isTargetAttached(tabTarget), "The tab target should be attached");

  info("Detach the tab target");
  const onTargetDestroyed = tabTarget.once("target-destroyed");
  const onDescriptorDestroyed = tabDescriptor.once("descriptor-destroyed");
  const onClientClosed = client.once("closed");
  await tabTarget.detach();

  info("Wait for target destruction");
  await onTargetDestroyed;

  info("Wait for descriptor destruction");
  await onDescriptorDestroyed;

  // Tab Descriptors returned by getTab are considered as local tabs.
  // So that when the target is destroyed, the descriptor stays alive.
  // But as their target have shouldCloseClient set to true... everything ends up being destroyed anyway.
  ok(
    tabTarget.isDestroyed(),
    "The tab target should be destroyed after detach"
  );
  ok(
    tabDescriptor.isDestroyed(),
    "The tab descriptor kept running after detach"
  );

  info("Wait for client being auto-closed by the target");
  await onClientClosed;
}

function isTargetAttached(targetFront) {
  return !!targetFront?.targetForm?.threadActor;
}
