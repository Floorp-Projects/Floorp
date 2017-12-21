/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that stepping out of a function returns the right return value.
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
    attachTestTabAndResume(
      gClient, "test-stack",
      function (response, tabClient, threadClient) {
        gThreadClient = threadClient;
        // XXX: We have to do an executeSoon so that the error isn't caught and
        // reported by DebuggerClient.requester (because we are using the local
        // transport and share a stack) which causes the test to fail.
        Services.tm.dispatchToMainThread({
          run: test_simple_stepping
        });
      });
  });
}

function test_simple_stepping() {
  gThreadClient.addOneTimeListener("paused", function (event, packet) {
    gThreadClient.addOneTimeListener("paused", function (event, packet) {
      // Check that the return value is 10.
      Assert.equal(packet.type, "paused");
      Assert.equal(packet.frame.where.line, gDebuggee.line0 + 5);
      Assert.equal(packet.why.type, "resumeLimit");
      Assert.equal(packet.why.frameFinished.return, 10);

      gThreadClient.addOneTimeListener("paused", function (event, packet) {
        gThreadClient.addOneTimeListener("paused", function (event, packet) {
          // Check that the return value is undefined.
          Assert.equal(packet.type, "paused");
          Assert.equal(packet.frame.where.line, gDebuggee.line0 + 8);
          Assert.equal(packet.why.type, "resumeLimit");
          Assert.equal(packet.why.frameFinished.return.type, "undefined");

          gThreadClient.addOneTimeListener("paused", function (event, packet) {
            gThreadClient.addOneTimeListener("paused", function (event, packet) {
              // Check that the exception was thrown.
              Assert.equal(packet.type, "paused");
              Assert.equal(packet.frame.where.line, gDebuggee.line0 + 11);
              Assert.equal(packet.why.type, "resumeLimit");
              Assert.equal(packet.why.frameFinished.throw, "ah");

              gThreadClient.resume(function () {
                gClient.close().then(gCallback);
              });
            });
            gThreadClient.stepOut();
          });
          gThreadClient.resume();
        });
        gThreadClient.stepOut();
      });
      gThreadClient.resume();
    });
    gThreadClient.stepOut();
  });

  /* eslint-disable */
  gDebuggee.eval("var line0 = Error().lineNumber;\n" +
                 "function f() {\n" +                   // line0 + 1
                 "  debugger;\n" +                      // line0 + 2
                 "  var a = 10;\n" +                    // line0 + 3
                 "  return a;\n" +                      // line0 + 4
                 "}\n" +                                // line0 + 5
                 "function g() {\n" +                   // line0 + 6
                 "  debugger;\n" +                      // line0 + 7
                 "}\n" +                                // line0 + 8
                 "function h() {\n" +                   // line0 + 9
                 "  debugger;\n" +                      // line0 + 10
                 "  throw 'ah';\n" +                    // line0 + 11
                 "  return 2;\n" +                      // line0 + 12
                 "}\n" +                                // line0 + 13
                 "f();\n" +                             // line0 + 14
                 "g();\n" +                             // line0 + 15
                 "try { h() } catch (ex) { };\n");      // line0 + 16
    /* eslint-enable */
}
