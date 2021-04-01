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

  // To run last as it will close the client and remove the tab
  await assertGetTab(client, client.mainRoot, tab);
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
    !tabDescriptor.shouldCloseClient,
    "Tab descriptors from listTabs shouldn't auto-close their client"
  );
  ok(isTargetAttached(tabTarget), "The tab target should be attached");

  info("Detach the tab target");
  const onTargetDestroyed = tabTarget.once("target-destroyed");
  const onDescriptorDestroyed = tabDescriptor.once("descriptor-destroyed");
  await tabTarget.detach();

  info("Wait for target destruction");
  await onTargetDestroyed;

  // listTabs returns non-local tabs, which we suppose are remote debugging.
  // And because of that, the TabDescriptor self-destroy itself on target being destroyed.
  // That's because we don't yet support top level target switching in case
  // of remote debugging. So we prefer destroy the descriptor as well as the toolbox.
  // Bug 1698891 should revisit that.
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
    tabDescriptor.shouldCloseClient,
    "Tab descriptor from getTab should close their client"
  );
  ok(isTargetAttached(tabTarget), "The tab target should be attached");

  info("Detach the tab target");
  const onTargetDestroyed = tabTarget.once("target-destroyed");
  await tabTarget.detach();

  info("Wait for target destruction");
  await onTargetDestroyed;

  // When the target is detached and destroyed,
  // the descriptor stays alive, because the tab is still opened.
  // And compared to listTabs, getTab's descriptor are considered local tabs.
  // And as we support top level target switching, the descriptor can be kept alive
  // when the target is destroyed.
  ok(
    !tabDescriptor.isDestroyed(),
    "The tab descriptor isn't destroyed on target detach"
  );

  info("Close the descriptor's tab");
  const onDescriptorDestroyed = tabDescriptor.once("descriptor-destroyed");
  const onClientClosed = client.once("closed");
  await removeTab(tab);

  info("Wait for descriptor destruction");
  await onDescriptorDestroyed;

  ok(
    tabTarget.isDestroyed(),
    "The tab target should be destroyed after closing the tab"
  );
  ok(
    tabDescriptor.isDestroyed(),
    "The tab descriptor is also always destroyed after tab closing"
  );

  // Tab Descriptors returned by getTab({ tab }) are considered as local tabs
  // and auto-close their client.
  info("Wait for client being auto-closed by the descriptor");
  await onClientClosed;
}

function isTargetAttached(targetFront) {
  return !!targetFront?.targetForm?.threadActor;
}
