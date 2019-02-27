/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that conditions are respected when specified in a logpoint.
 */

const { getLastThreadActor } = require("xpcshell-test/testactors");

var gDebuggee;
var gClient;
var gThreadClient;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-logpoint");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-logpoint",
                           function(response, targetFront, threadClient) {
                             gThreadClient = threadClient;
                             test_simple_breakpoint();
                           });
  });
  do_test_pending();
}

function test_simple_breakpoint() {
  let lastMessage;
  getLastThreadActor()._parent._consoleActor = {
    onConsoleAPICall(message) {
      lastMessage = message;
    },
  };

  gThreadClient.addOneTimeListener("paused", async function(event, packet) {
    const source = await getSourceById(
      gThreadClient,
      packet.frame.where.actor
    );

    // Set a logpoint which should invoke console.log.
    gThreadClient.setBreakpoint({
      sourceUrl: source.url,
      line: 4,
    }, { logValue: "a", condition: "a === 5" });

    // Execute the rest of the code.
    gThreadClient.resume();
  });

  /* eslint-disable */
  Cu.evalInSandbox("debugger;\n" + // 1
                   "var a = 1;\n" +  // 2
                   "while (a < 10) {\n" + // 3
                   "  a++;\n" + // 4
                   "}",
                   gDebuggee,
                   "1.8",
                   "test.js",
                   1);
  /* eslint-enable */

  Assert.equal(lastMessage.arguments[0], 5);
  finishClient(gClient);
}
