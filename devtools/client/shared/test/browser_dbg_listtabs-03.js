/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Make sure the listTabs request works as specified.
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

  const tabDescriptors = await client.mainRoot.listTabs();
  is(tabDescriptors.length, 2, "Should be two tabs");
  const tabDescriptor = tabDescriptors.find(d => d.url == TAB1_URL);
  ok(tabDescriptor, "Should have a descriptor actor for the tab");

  let tabTarget = await tabDescriptor.getTarget();
  ok(isTargetAttached(tabTarget), "The tab target should be attached");
  const targetActorId = tabTarget.actorID;

  info("Detach the tab target");
  await tabTarget.detach();

  info("Wait until the tab target is destroyed");
  await waitUntil(() => !tabTarget.actorID);

  info("Call getTarget() again on the tabDescriptor");
  tabTarget = await tabDescriptor.getTarget();
  ok(targetActorId !== tabTarget.actorID, "We should get a new target actor");
  ok(isTargetAttached(tabTarget), "The new tab target should also be attached");

  await removeTab(tab);
  await client.close();
});

function isTargetAttached(targetFront) {
  return !!targetFront?.targetForm?.threadActor;
}
