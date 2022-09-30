/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check regression when opening two tabs
 */

var {
  DevToolsServer,
} = require("resource://devtools/server/devtools-server.js");
var {
  DevToolsClient,
} = require("resource://devtools/client/devtools-client.js");
const {
  createCommandsDictionary,
} = require("resource://devtools/shared/commands/index.js");

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
  await Promise.all(
    tabDescriptors.map(async descriptor => {
      const commands = await createCommandsDictionary(descriptor);
      // Descriptor's getTarget will only work if the TargetCommand watches for the first top target
      await commands.targetCommand.startListening();
    })
  );
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
  let front = await getTabTarget(client, { tab: tab1 });
  is(targetFront1, front, "getTab returns the same target form for first tab");
  front = await getTabTarget(client, {
    browserId: tab1.linkedBrowser.browserId,
  });
  is(
    targetFront1,
    front,
    "getTab returns the same target form when filtering by browserId"
  );
  front = await getTabTarget(client, { tab: tab2 });
  is(targetFront2, front, "getTab returns the same target form for second tab");
}

async function checkGetTabFailures(client) {
  try {
    await getTabTarget(client, { browserId: -999 });
    ok(false, "getTab unexpectedly succeed with a wrong browserId");
  } catch (error) {
    is(
      error.message,
      "Protocol error (noTab): Unable to find tab with browserId '-999' (no browsing-context) from: " +
        client.mainRoot.actorID
    );
  }
}

async function checkSelectedTargetActor(targetFront2) {
  // Send a naive request to the second target actor to check if it works
  const consoleFront = await targetFront2.getFront("console");
  const response = await consoleFront.startListeners([]);
  ok(
    "startedListeners" in response,
    "Actor from the selected tab should respond to the request."
  );
}

async function checkFirstTargetActor(targetFront1) {
  // then send a request to the first target actor to check if it still works
  const consoleFront = await targetFront1.getFront("console");
  const response = await consoleFront.startListeners([]);
  ok(
    "startedListeners" in response,
    "Actor from the first tab should still respond."
  );
}

async function getTabTarget(client, filter) {
  const descriptor = await client.mainRoot.getTab(filter);
  // By default, descriptor returned by getTab will close the client
  // when the tab is closed. Disable this default behavior for this test.
  // Bug 1698890: The test should probably stop assuming this.
  descriptor.shouldCloseClient = false;
  return descriptor.getTarget();
}
