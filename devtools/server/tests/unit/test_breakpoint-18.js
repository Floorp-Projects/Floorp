/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that we only break on offsets that are entry points for the line we are
 * breaking on. Bug 907278.
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
  gDebuggee = addTestGlobal("test-breakpoints", server);
  gDebuggee.console = { log: x => void x };
  gClient = new DebuggerClient(server.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient,
                           "test-breakpoints",
                           function (response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             setUpCode();
                           });
  });
}

function setUpCode() {
  gClient.addOneTimeListener("paused", setBreakpoint);
  Cu.evalInSandbox(
    "debugger;\n" +
    function test() {
      console.log("foo bar");
      debugger;
    },
    gDebuggee,
    "1.8",
    "http://example.com/",
    1
  );
}

function setBreakpoint(event, packet) {
  let source = gThreadClient.source(packet.frame.where.source);
  gClient.addOneTimeListener("resumed", runCode);

  source.setBreakpoint({ line: 2 }, ({ error }) => {
    Assert.ok(!error);
    gThreadClient.resume();
  });
}

function runCode() {
  gClient.addOneTimeListener("paused", testBPHit);
  gDebuggee.test();
}

function testBPHit(event, { why }) {
  Assert.equal(why.type, "breakpoint");
  gClient.addOneTimeListener("paused", testDbgStatement);
  gThreadClient.resume();
}

function testDbgStatement(event, { why }) {
  // Should continue to the debugger statement.
  Assert.equal(why.type, "debuggerStatement");
  // Not break on another offset from the same line (that isn't an entry point
  // to the line)
  Assert.notEqual(why.type, "breakpoint");
  gClient.close().then(gCallback);
}
