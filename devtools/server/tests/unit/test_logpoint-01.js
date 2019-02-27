/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that logpoints generate console messages.
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
      line: 3,
    }, { logValue: "a" });

    // Execute the rest of the code.
    gThreadClient.resume();
  });

  /* eslint-disable */
  Cu.evalInSandbox("debugger;\n" + // 1
                   "var a = 'three';\n" +  // 2
                   "var b = 2;\n", // 3
                   gDebuggee,
                   "1.8",
                   "test.js",
                   1);
  /* eslint-enable */

  Assert.equal(lastMessage.arguments[0], "three");
  finishClient(gClient);
}
