/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Assert the lifecycle of tab descriptors between listTabs and getTab
 */

var {
  DevToolsServer,
} = require("resource://devtools/server/devtools-server.js");
var {
  DevToolsClient,
} = require("resource://devtools/client/devtools-client.js");

const TAB1_URL = EXAMPLE_URL + "doc_empty-tab-01.html";

add_task(async function test() {
  // This test assert RDP APIs which were only meaningful when doing client side targets.
  // Now that targets are all created by the Watcher, it is no longer meaningful to cover this.
  // We should probably remove this test in bug 1721852.
  await pushPref("devtools.target-switching.server.enabled", false);

  DevToolsServer.init();
  DevToolsServer.registerAllActors();

  const transport = DevToolsServer.connectPipe();
  const client = new DevToolsClient(transport);
  const [type] = await client.connect();
  is(type, "browser", "Root actor should identify itself as a browser.");
  const tab1 = await addTab(TAB1_URL);

  await assertListTabs(tab1, client.mainRoot);

  const tab2 = await addTab(TAB1_URL);
  // To run last as it will close the client
  await assertGetTab(client, client.mainRoot, tab2);
});

async function assertListTabs(tab, rootFront) {
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
  await tabTarget.detach();

  info("Wait for target destruction");
  await onTargetDestroyed;

  // When the target is detached and destroyed,
  // the descriptor stays alive, because the tab is still opened.
  // As we support top level target switching, the descriptor can be kept alive
  // when the target is destroyed.
  ok(
    !tabDescriptor.isDestroyed(),
    "The tab descriptor isn't destroyed on target detach"
  );

  info("Close the descriptor's tab");
  const onDescriptorDestroyed = tabDescriptor.once("descriptor-destroyed");
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
    browserId: tab2.linkedBrowser.browserId,
  });
  ok(
    !tabDescriptor2.isLocalTab,
    "getTab's tab descriptor aren't considered as local tabs when we pass an browserId"
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
  // As we support top level target switching, the descriptor can be kept alive
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
