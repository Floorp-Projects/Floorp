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

  let tabs = await client.mainRoot.listTabs();
  is(tabs.length, 2, "Should be two tabs");
  const tabFront = tabs.filter(a => a.url == TAB1_URL).pop();
  ok(tabFront, "Should have an actor for the tab");

  await tabFront.attach();

  const previousActorID = tabFront.actorID;
  await tabFront.detach();

  tabs = await client.mainRoot.listTabs();
  const newFront = tabs.find(a => a.url == TAB1_URL);
  is(
    newFront.actorID,
    previousActorID,
    "Should have the same actor for the same tab"
  );
  isnot(newFront, tabFront, "But the front should be a new one");

  await newFront.attach();
  await newFront.detach();

  await removeTab(tab);
  await client.close();
});
