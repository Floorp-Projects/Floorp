/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the behavior of the debugger statement.
 */

// Import helpers for the workers
/* import-globals-from helper_workers.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/helper_workers.js",
  this
);

const TAB_URL = TEST_URI_ROOT + "doc_inline-debugger-statement.html";

add_task(async () => {
  const tab = await addTab(TAB_URL);
  const target = await TargetFactory.forTab(tab);
  await target.attach();
  const { client } = target;

  const threadClient = await testEarlyDebuggerStatement(client, tab, target);
  await testDebuggerStatement(client, tab, threadClient);

  await target.destroy();
});

async function testEarlyDebuggerStatement(client, tab, targetFront) {
  const onPaused = function(packet) {
    ok(false, "Pause shouldn't be called before we've attached!");
  };

  // using the DebuggerClient to listen to the pause packet, as the
  // threadClient is not yet attached.
  client.on("paused", onPaused);

  // This should continue without nesting an event loop and calling
  // the onPaused hook, because we haven't attached yet.
  callInTab(tab, "runDebuggerStatement");

  client.off("paused", onPaused);

  // Now attach and resume...
  const [, threadClient] = await targetFront.attachThread();
  await threadClient.resume();
  ok(true, "Pause wasn't called before we've attached.");

  return threadClient;
}

async function testDebuggerStatement(client, tab, threadClient) {
  const onPaused = new Promise(resolve => {
    threadClient.on("paused", async packet => {
      await threadClient.resume();
      ok(true, "The pause handler was triggered on a debugger statement.");
      resolve();
    });
  });

  // Reach around the debugging protocol and execute the debugger statement.
  callInTab(tab, "runDebuggerStatement");

  return onPaused;
}
