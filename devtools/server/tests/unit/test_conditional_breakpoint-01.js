/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check conditional breakpoint when condition evaluates to true.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-conditional-breakpoint");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-conditional-breakpoint",
                           function (response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             test_simple_breakpoint();
                           });
  });
  do_test_pending();
}

function test_simple_breakpoint() {
  gThreadClient.addOneTimeListener("paused", function (event, packet) {
    let source = gThreadClient.source(packet.frame.where.source);
    source.setBreakpoint({
      line: 3,
      condition: "a === 1"
    }, function (response, bpClient) {
      gThreadClient.addOneTimeListener("paused", function (event, packet) {
        // Check the return value.
        Assert.equal(packet.why.type, "breakpoint");
        Assert.equal(packet.frame.where.line, 3);

        // Remove the breakpoint.
        bpClient.remove(function (response) {
          gThreadClient.resume(function () {
            finishClient(gClient);
          });
        });
      });
      // Continue until the breakpoint is hit.
      gThreadClient.resume();
    });
  });

  /* eslint-disable */
  Components.utils.evalInSandbox("debugger;\n" +   // 1
                                 "var a = 1;\n" +  // 2
                                 "var b = 2;\n",  // 3
                                 gDebuggee,
                                 "1.8",
                                 "test.js",
                                 1);
  /* eslint-enable */
}
