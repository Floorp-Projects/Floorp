/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can get a stack to a promise's allocation point.
 */

"use strict";

const TAB_URL = URL_ROOT + "doc_promise-get-allocation-stack.html";

const ObjectClient = require("devtools/shared/client/object-client");

add_task(async function test() {
  const browser = await addTab(TAB_URL);
  const tab = gBrowser.getTabForBrowser(browser);
  const target = await TargetFactory.forTab(tab);
  await target.attach();

  await testGetAllocationStack(tab, target);

  await target.destroy();
});

async function testGetAllocationStack(tab, target) {
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

  await ContentTask.spawn(tab.linkedBrowser, {}, () => {
    content.wrappedJSObject.makePromises();
  });

  const form = await onNewPromise;
  ok(form, "Found our promise p");

  const objectClient = new ObjectClient(target.client, form);
  ok(objectClient, "Got Object Client");

  const response = await objectClient.getPromiseAllocationStack();
  ok(response.allocationStack.length, "Got promise allocation stack.");

  for (const stack of response.allocationStack) {
    is(stack.source.url, TAB_URL, "Got correct source URL.");
    is(
      stack.functionDisplayName,
      "makePromises",
      "Got correct function display name."
    );
    is(typeof stack.line, "number", "Expect stack line to be a number.");
    is(typeof stack.column, "number", "Expect stack column to be a number.");
  }

  await front.detach();
}
