/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

// Test the xpcshell-test debug support.  Ideally we should have this test
// next to the xpcshell support code, but that's tricky...

function run_test() {
  const testFile = do_get_file("xpcshell_debugging_script.js");

  // _setupDebuggerServer is from xpcshell-test's head.js
  /* global _setupDebuggerServer */
  let testResumed = false;
  const DebuggerServer = _setupDebuggerServer([testFile.path], () => {
    testResumed = true;
  });
  const transport = DebuggerServer.connectPipe();
  const client = new DebuggerClient(transport);
  client.connect().then(() => {
    // Even though we have no tabs, listTabs gives us the chromeDebugger.
    client.getProcess().then(response => {
      const actor = response.form.actor;
      client.attachTab(actor).then(([response, tabClient]) => {
        tabClient.attachThread(null).then(([response, threadClient]) => {
          threadClient.addOneTimeListener("paused", (event, packet) => {
            equal(packet.why.type, "breakpoint",
                "yay - hit the breakpoint at the first line in our script");
            // Resume again - next stop should be our "debugger" statement.
            threadClient.addOneTimeListener("paused", (event, packet) => {
              equal(packet.why.type, "debuggerStatement",
                    "yay - hit the 'debugger' statement in our script");
              threadClient.resume(() => {
                finishClient(client);
              });
            });
            threadClient.resume();
          });
          // tell the thread to do the initial resume.  This would cause the
          // xpcshell test harness to resume and load the file under test.
          threadClient.resume(response => {
            // should have been told to resume the test itself.
            ok(testResumed);
            // Now load our test script.
            load(testFile.path);
            // and our "paused" listener above should get hit.
          });
        });
      });
    });
  });
  do_test_pending();
}
