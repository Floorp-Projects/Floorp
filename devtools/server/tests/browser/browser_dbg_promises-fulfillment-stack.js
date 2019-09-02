/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can get a stack to a promise's fulfillment point.
 */

"use strict";

const TAB_URL = URL_ROOT + "doc_promise-get-fulfillment-stack.html";

const ObjectClient = require("devtools/shared/client/object-client");

const TEST_DATA = [
  {
    functionDisplayName: "returnPromise/<",
    line: 21,
    column: 44,
  },
  {
    functionDisplayName: "returnPromise",
    line: 21,
    column: 14,
  },
  {
    functionDisplayName: "makePromise",
    line: 16,
    column: 17,
  },
];

add_task(async () => {
  const browser = await addTab(TAB_URL);
  const tab = gBrowser.getTabForBrowser(browser);
  const target = await TargetFactory.forTab(tab);
  await target.attach();

  await testGetFulfillmentStack(tab, target);
  await target.destroy();
});

async function testGetFulfillmentStack(tab, target) {
  const front = await target.getFront("promises");

  await front.attach();
  await front.listPromises();

  // Get the grip for promise p
  const onNewPromise = new Promise(resolve => {
    front.on("new-promises", promises => {
      for (const p of promises) {
        if (
          p.preview.ownProperties.name &&
          p.preview.ownProperties.name.value === "p"
        ) {
          resolve(p);
        }
      }
    });
  });

  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    content.wrappedJSObject.makePromise();
  });

  const form = await onNewPromise;
  ok(form, "Found our promise p");

  const objectClient = new ObjectClient(target.client, form);
  ok(objectClient, "Got Object Client");

  const response = await objectClient.getPromiseFulfillmentStack();
  ok(response.fulfillmentStack.length, "Got promise allocation stack.");

  for (let i = 0; i < TEST_DATA.length; i++) {
    const stack = response.fulfillmentStack[i];
    const data = TEST_DATA[i];
    is(stack.source.url, TAB_URL, "Got correct source URL.");
    is(
      stack.functionDisplayName,
      data.functionDisplayName,
      "Got correct function display name."
    );
    is(stack.line, data.line, "Got correct stack line number.");
    is(stack.column, data.column, "Got correct stack column number.");
  }

  await front.detach();
}
