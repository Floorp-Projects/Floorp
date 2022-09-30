/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Make sure the listTabs request works as specified.
 */

var {
  DevToolsServer,
} = require("resource://devtools/server/devtools-server.js");
var {
  DevToolsClient,
} = require("resource://devtools/client/devtools-client.js");

const TAB1_URL = EXAMPLE_URL + "doc_empty-tab-01.html";
const TAB2_URL = EXAMPLE_URL + "doc_empty-tab-02.html";

add_task(async function test() {
  DevToolsServer.init();
  DevToolsServer.registerAllActors();

  const transport = DevToolsServer.connectPipe();
  const client = new DevToolsClient(transport);
  const [aType] = await client.connect();
  is(aType, "browser", "Root actor should identify itself as a browser.");

  const firstTab = await testFirstTab(client);
  const secondTab = await testSecondTab(client, firstTab.front);
  await testRemoveTab(client, firstTab.tab);
  await testAttachRemovedTab(secondTab.tab, secondTab.front);
  await client.close();
});

async function testFirstTab(client) {
  const tab = await addTab(TAB1_URL);

  const front = await getDescriptorActorForUrl(client, TAB1_URL);
  ok(front, "Should find a target actor for the first tab.");
  return { tab, front };
}

async function testSecondTab(client, firstTabFront) {
  const tab = await addTab(TAB2_URL);

  const firstFront = await getDescriptorActorForUrl(client, TAB1_URL);
  const secondFront = await getDescriptorActorForUrl(client, TAB2_URL);
  is(firstFront, firstTabFront, "First tab's actor shouldn't have changed.");
  ok(secondFront, "Should find a target actor for the second tab.");
  return { tab, front: secondFront };
}

async function testRemoveTab(client, firstTab) {
  await removeTab(firstTab);
  const front = await getDescriptorActorForUrl(client, TAB1_URL);
  ok(!front, "Shouldn't find a target actor for the first tab anymore.");
}

async function testAttachRemovedTab(secondTab, secondTabFront) {
  await removeTab(secondTab);

  const { actorID } = secondTabFront;
  try {
    await secondTabFront.getFavicon({});
    ok(
      false,
      "any request made to the descriptor for a closed tab should have failed"
    );
  } catch (error) {
    ok(
      error.message.includes(
        `Connection closed, pending request to ${actorID}, type getFavicon failed`
      ),
      "Actor is gone since the tab was removed."
    );
  }
}

async function getDescriptorActorForUrl(client, url) {
  const tabDescriptors = await client.mainRoot.listTabs();
  const tabDescriptor = tabDescriptors.find(front => front.url == url);
  return tabDescriptor;
}
