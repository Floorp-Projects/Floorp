/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Test that we don't stop at debugger statements inside black boxed sources.
 */

var gDebuggee;
var gClient;
var gThreadClient;
var gBpClient;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-black-box");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-black-box",
                           function (response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             test_black_box();
                           });
  });
  do_test_pending();
}

const BLACK_BOXED_URL = "http://example.com/blackboxme.js";
const SOURCE_URL = "http://example.com/source.js";

function test_black_box() {
  gClient.addOneTimeListener("paused", function (event, packet) {
    let source = gThreadClient.source(packet.frame.where.source);
    source.setBreakpoint({
      line: 4
    }, function ({error}, bpClient) {
      gBpClient = bpClient;
      Assert.ok(!error, "Should not get an error: " + error);
      gThreadClient.resume(test_black_box_dbg_statement);
    });
  });

  /* eslint-disable no-multi-spaces */
  Components.utils.evalInSandbox(
    "" + function doStuff(k) { // line 1
      debugger;                // line 2 - Break here
      k(100);                  // line 3
    },                         // line 4
    gDebuggee,
    "1.8",
    BLACK_BOXED_URL,
    1
  );

  Components.utils.evalInSandbox(
    "" + function runTest() { // line 1
      doStuff(                // line 2
        function (n) {        // line 3
          Math.abs(n);        // line 4 - Break here
        }                     // line 5
      );                      // line 6
    }                         // line 7
    + "\n debugger;",         // line 8
    gDebuggee,
    "1.8",
    SOURCE_URL,
    1
  );
  /* eslint-enable no-multi-spaces */
}

function test_black_box_dbg_statement() {
  gThreadClient.getSources(function ({error, sources}) {
    Assert.ok(!error, "Should not get an error: " + error);
    let sourceClient = gThreadClient.source(
      sources.filter(s => s.url == BLACK_BOXED_URL)[0]
    );

    sourceClient.blackBox(function ({error}) {
      Assert.ok(!error, "Should not get an error: " + error);

      gClient.addOneTimeListener("paused", function (event, packet) {
        Assert.equal(packet.why.type, "breakpoint",
                     "We should pass over the debugger statement.");
        gBpClient.remove(function ({error}) {
          Assert.ok(!error, "Should not get an error: " + error);
          gThreadClient.resume(test_unblack_box_dbg_statement.bind(null, sourceClient));
        });
      });
      gDebuggee.runTest();
    });
  });
}

function test_unblack_box_dbg_statement(sourceClient) {
  sourceClient.unblackBox(function ({error}) {
    Assert.ok(!error, "Should not get an error: " + error);

    gClient.addOneTimeListener("paused", function (event, packet) {
      Assert.equal(packet.why.type, "debuggerStatement",
                   "We should stop at the debugger statement again");
      finishClient(gClient);
    });
    gDebuggee.runTest();
  });
}
