/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Check basic step-out functionality.
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
                             test_simple_stepping();
                           });
  });
}

function test_simple_stepping() {
  gThreadClient.addOneTimeListener("paused", function (event, packet) {
    gThreadClient.addOneTimeListener("paused", function (event, packet) {
      // Check the return value.
      Assert.equal(packet.type, "paused");
      Assert.equal(packet.frame.where.line, gDebuggee.line0 + 5);
      Assert.equal(packet.why.type, "resumeLimit");
      // Check that stepping worked.
      Assert.equal(gDebuggee.a, 1);
      Assert.equal(gDebuggee.b, 2);

      gThreadClient.resume(function () {
        gClient.close().then(gCallback);
      });
    });
    gThreadClient.stepOut();
  });

  /* eslint-disable */
  gDebuggee.eval("var line0 = Error().lineNumber;\n" +
                 "function f() {\n" + // line0 + 1
                 "  debugger;\n" +    // line0 + 2
                 "  this.a = 1;\n" +  // line0 + 3
                 "  this.b = 2;\n" +  // line0 + 4
                 "}\n" +              // line0 + 5
                 "f();\n");           // line0 + 6
  /* eslint-enable */
}
