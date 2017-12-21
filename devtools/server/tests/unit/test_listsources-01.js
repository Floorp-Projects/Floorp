/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check basic getSources functionality.
 */

var gDebuggee;
var gClient;
var gThreadClient;

var gNumTimesSourcesSent = 0;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-stack");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.request = (function (origRequest) {
    return function (request, onResponse) {
      if (request.type === "sources") {
        ++gNumTimesSourcesSent;
      }
      return origRequest.call(this, request, onResponse);
    };
  }(gClient.request));
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-stack",
                           function (response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             test_simple_listsources();
                           });
  });
  do_test_pending();
}

function test_simple_listsources() {
  gThreadClient.addOneTimeListener("paused", function (event, packet) {
    gThreadClient.getSources(function (response) {
      Assert.ok(response.sources.some(function (s) {
        return s.url && s.url.match(/test_listsources-01.js/);
      }));

      Assert.ok(gNumTimesSourcesSent <= 1,
                "Should only send one sources request at most, even though we"
                + " might have had to send one to determine feature support.");

      gThreadClient.resume(function () {
        finishClient(gClient);
      });
    });
  });

  /* eslint-disable */
  Components.utils.evalInSandbox("var line0 = Error().lineNumber;\n" +
                                "debugger;\n" +   // line0 + 1
                                "var a = 1;\n" +  // line0 + 2
                                "var b = 2;\n",   // line0 + 3
                                gDebuggee);
  /* eslint-enable */
}
