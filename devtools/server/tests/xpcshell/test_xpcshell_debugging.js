/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the xpcshell-test debug support.  Ideally we should have this test
// next to the xpcshell support code, but that's tricky...

// HACK: ServiceWorkerManager requires the "profile-change-teardown" to cleanly
// shutdown, and setting _profileInitialized to `true` will trigger those
// notifications (see /testing/xpcshell/head.js).
// eslint-disable-next-line no-undef
_profileInitialized = true;

add_task(async function () {
  const testFile = do_get_file("xpcshell_debugging_script.js");

  // _setupDevToolsServer is from xpcshell-test's head.js
  /* global _setupDevToolsServer */
  let testInitialized = false;
  const { DevToolsServer } = _setupDevToolsServer([testFile.path], () => {
    testInitialized = true;
  });
  const transport = DevToolsServer.connectPipe();
  const client = new DevToolsClient(transport);
  await client.connect();

  // Ensure that global actors are available. Just test the device actor.
  const deviceFront = await client.mainRoot.getFront("device");
  const desc = await deviceFront.getDescription();
  equal(
    desc.geckobuildid,
    Services.appinfo.platformBuildID,
    "device actor works"
  );

  // Even though we have no tabs, getMainProcess gives us the chrome debugger.
  const targetDescriptor = await client.mainRoot.getMainProcess();
  const front = await targetDescriptor.getTarget();
  const watcher = await targetDescriptor.getWatcher();

  const threadFront = await front.attachThread();

  // Checks that the thread actor initializes immediately and that _setupDevToolsServer
  // callback gets called.
  ok(testInitialized);

  const onPause = waitForPause(threadFront);

  // Now load our test script,
  // in another event loop so that the test can keep running!
  Services.tm.dispatchToMainThread(() => {
    load(testFile.path);
  });

  // and our "paused" listener should get hit.
  info("Wait for first paused event");
  const packet1 = await onPause;
  equal(
    packet1.why.type,
    "breakpoint",
    "yay - hit the breakpoint at the first line in our script"
  );

  // Resume again - next stop should be our "debugger" statement.
  info("Wait for second pause event");
  const packet2 = await resumeAndWaitForPause(threadFront);
  equal(
    packet2.why.type,
    "debuggerStatement",
    "yay - hit the 'debugger' statement in our script"
  );

  info("Dynamically add a breakpoint after the debugger statement");
  const breakpointsFront = await watcher.getBreakpointListActor();
  await breakpointsFront.setBreakpoint(
    { sourceUrl: testFile.path, line: 11, column: 0 },
    {}
  );

  // Resume again - next stop should be the new breakpoint.
  info("Wait for third pause event");
  const packet3 = await resumeAndWaitForPause(threadFront);
  equal(
    packet3.why.type,
    "breakpoint",
    "yay - hit the breakpoint added after starting the test"
  );
  finishClient(client);
});
