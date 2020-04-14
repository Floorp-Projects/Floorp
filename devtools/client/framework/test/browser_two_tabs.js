/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check regression when opening two tabs
 */

var { DevToolsServer } = require("devtools/server/devtools-server");
var { DevToolsClient } = require("devtools/client/devtools-client");

const TAB_URL_1 = "data:text/html;charset=utf-8,foo";
const TAB_URL_2 = "data:text/html;charset=utf-8,bar";

add_task(async () => {
  DevToolsServer.init();
  DevToolsServer.registerAllActors();

  const tab1 = await addTab(TAB_URL_1);
  const tab2 = await addTab(TAB_URL_2);

  // Connect to devtools server to fetch the two target actors for each tab
  const client = new DevToolsClient(DevToolsServer.connectPipe());
  await client.connect();

  const tabDescriptors = await client.mainRoot.listTabs();
  const tabs = await Promise.all(tabDescriptors.map(d => d.getTarget()));
  const targetFront1 = tabs.find(a => a.url === TAB_URL_1);
  const targetFront2 = tabs.find(a => a.url === TAB_URL_2);

  await checkGetTab(client, tab1, tab2, targetFront1, targetFront2);
  await checkGetTabFailures(client);
  await checkSelectedTargetActor(targetFront2);

  await removeTab(tab2);
  await checkFirstTargetActor(targetFront1);

  await removeTab(tab1);
  await client.close();
});

async function checkGetTab(client, tab1, tab2, targetFront1, targetFront2) {
  let front = await client.mainRoot.getTab({ tab: tab1 });
  is(targetFront1, front, "getTab returns the same target form for first tab");
  const filter = {};
  // Filter either by tabId or outerWindowID,
  // if we are running tests OOP or not.
  if (tab1.linkedBrowser.frameLoader.remoteTab) {
    filter.tabId = tab1.linkedBrowser.frameLoader.remoteTab.tabId;
  } else {
    const windowUtils = tab1.linkedBrowser.contentWindow.windowUtils;
    filter.outerWindowID = windowUtils.outerWindowID;
  }
  front = await client.mainRoot.getTab(filter);
  is(
    targetFront1,
    front,
    "getTab returns the same target form when filtering by tabId/outerWindowID"
  );
  front = await client.mainRoot.getTab({ tab: tab2 });
  is(targetFront2, front, "getTab returns the same target form for second tab");
}

async function checkGetTabFailures(client) {
  try {
    await client.mainRoot.getTab({ tabId: -999 });
    ok(false, "getTab unexpectedly succeed with a wrong tabId");
  } catch (error) {
    is(
      error.message,
      "Protocol error (noTab): Unable to find tab with tabId '-999' from: " +
        client.mainRoot.actorID
    );
  }

  try {
    await client.mainRoot.getTab({ outerWindowID: -999 });
    ok(false, "getTab unexpectedly succeed with a wrong outerWindowID");
  } catch (error) {
    is(
      error.message,
      "Protocol error (noTab): Unable to find tab with outerWindowID '-999' from: " +
        client.mainRoot.actorID
    );
  }
}

async function checkSelectedTargetActor(targetFront2) {
  // Send a naive request to the second target actor to check if it works
  await targetFront2.attach();
  const consoleFront = await targetFront2.getFront("console");
  const response = await consoleFront.startListeners([]);
  ok(
    "startedListeners" in response,
    "Actor from the selected tab should respond to the request."
  );
}

async function checkFirstTargetActor(targetFront1) {
  // then send a request to the first target actor to check if it still works
  await targetFront1.attach();
  const consoleFront = await targetFront1.getFront("console");
  const response = await consoleFront.startListeners([]);
  ok(
    "startedListeners" in response,
    "Actor from the first tab should still respond."
  );
}
