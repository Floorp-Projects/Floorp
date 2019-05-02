/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check getSources functionality when there are lots of sources.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test() {
  Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
  });
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-sources");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-sources",
                           function(response, targetFront, threadClient) {
                             gThreadClient = threadClient;
                             test_simple_listsources();
                           });
  });
  do_test_pending();
}

function test_simple_listsources() {
  gThreadClient.addOneTimeListener("paused", function(event, packet) {
    gThreadClient.getSources().then(function(response) {
      Assert.ok(
        !response.error,
        "There shouldn't be an error fetching large amounts of sources.");

      Assert.ok(response.sources.some(function(s) {
        return s.url.match(/foo-999.js$/);
      }));

      gThreadClient.resume().then(function() {
        finishClient(gClient);
      });
    });
  });

  for (let i = 0; i < 1000; i++) {
    Cu.evalInSandbox("function foo###() {return ###;}".replace(/###/g, i),
                     gDebuggee,
                     "1.8",
                     "http://example.com/foo-" + i + ".js",
                     1);
  }
  gDebuggee.eval("debugger;");
}
