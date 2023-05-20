/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that we can detect nested event loops in tabs with the same URL.

add_task(async function () {
  const GLOBAL_NAME = "test-nesting1";

  initTestDevToolsServer();
  addTestGlobal(GLOBAL_NAME);
  addTestGlobal(GLOBAL_NAME);

  // Connect two thread actors, debugging the same debuggee, and both being paused.
  const firstClient = new DevToolsClient(DevToolsServer.connectPipe());
  await firstClient.connect();
  const { threadFront: firstThreadFront } = await attachTestThread(
    firstClient,
    GLOBAL_NAME
  );
  await firstThreadFront.interrupt();

  const secondClient = new DevToolsClient(DevToolsServer.connectPipe());
  await secondClient.connect();
  const { threadFront: secondThreadFront } = await attachTestThread(
    secondClient,
    GLOBAL_NAME
  );
  await secondThreadFront.interrupt();

  // Then check how concurrent resume work
  let result;
  try {
    result = await firstThreadFront.resume();
  } catch (e) {
    Assert.ok(e.includes("wrongOrder"), "rejects with the wrong order");
  }
  Assert.ok(!result, "no response");

  result = await secondThreadFront.resume();
  Assert.ok(true, "resumed as expected");

  await firstThreadFront.resume();

  Assert.ok(true, "resumed as expected");
  await firstClient.close();

  await finishClient(secondClient);
});
