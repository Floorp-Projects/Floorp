/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that setting a breakpoint in a line without code in a deeply-nested
 * child script will skip forward.
 */

var gDebuggee;
var gClient;
var gThreadClient;
var gCallback;

function run_test() {
  run_test_with_server(DebuggerServer, function () {
    run_test_with_server(WorkerDebuggerServer, do_test_finished);
  });
  do_test_pending();
}

function run_test_with_server(server, callback) {
  gCallback = callback;
  initTestDebuggerServer(server);
  gDebuggee = addTestGlobal("test-stack", server);
  gClient = new DebuggerClient(server.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-stack",
                           function (response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             test_nested_breakpoint();
                           });
  });
}

function test_nested_breakpoint() {
  gThreadClient.addOneTimeListener("paused", function (event, packet) {
    let source = gThreadClient.source(packet.frame.where.source);
    let location = { line: gDebuggee.line0 + 5 };

    source.setBreakpoint(location, function (response, bpClient) {
      // Check that the breakpoint has properly skipped forward one line.
      Assert.equal(response.actualLocation.source.actor, source.actor);
      Assert.equal(response.actualLocation.line, location.line + 1);

      gThreadClient.addOneTimeListener("paused", function (event, packet) {
        // Check the return value.
        Assert.equal(packet.type, "paused");
        Assert.equal(packet.frame.where.source.actor, source.actor);
        Assert.equal(packet.frame.where.line, location.line + 1);
        Assert.equal(packet.why.type, "breakpoint");
        Assert.equal(packet.why.actors[0], bpClient.actor);
        // Check that the breakpoint worked.
        Assert.equal(gDebuggee.a, 1);
        Assert.equal(gDebuggee.b, undefined);

        // Remove the breakpoint.
        bpClient.remove(function (response) {
          gThreadClient.resume(function () {
            gClient.close().then(gCallback);
          });
        });
      });

      // Continue until the breakpoint is hit.
      gThreadClient.resume();
    });
  });

  /* eslint-disable */
  Cu.evalInSandbox(
    "var line0 = Error().lineNumber;\n" +
    "function foo() {\n" +     // line0 + 1
    "  function bar() {\n" +   // line0 + 2
    "    function baz() {\n" + // line0 + 3
    "      this.a = 1;\n" +    // line0 + 4
    "      // A comment.\n" +  // line0 + 5
    "      this.b = 2;\n" +    // line0 + 6
    "    }\n" +                // line0 + 7
    "    baz();\n" +           // line0 + 8
    "  }\n" +                  // line0 + 9
    "  bar();\n" +             // line0 + 10
    "}\n" +                    // line0 + 11
    "debugger;\n" +            // line0 + 12
    "foo();\n",               // line0 + 13
    gDebuggee
  );
  /* eslint-enable */
}
