/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that sourceURL has the correct effect when using gThreadClient.eval.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-stack");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-stack",
                           function (response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             test_simple_new_source();
                           });
  });
  do_test_pending();
}

function test_simple_new_source() {
  gThreadClient.addOneTimeListener("paused", function () {
    gThreadClient.addOneTimeListener("newSource", function (event, packet) {
      Assert.equal(event, "newSource");
      Assert.equal(packet.type, "newSource");
      Assert.ok(!!packet.source);
      Assert.ok(!!packet.source.url.match(/example\.com/));

      finishClient(gClient);
    });
    gThreadClient.eval(null, "function f() { }\n//# sourceURL=http://example.com/code.js");
  });

  /* eslint-disable */
  gDebuggee.eval("(" + function () {
    function stopMe(arg1) { debugger; }
    stopMe({obj: true});
  } + ")()");
  /* eslint-enable */
}
