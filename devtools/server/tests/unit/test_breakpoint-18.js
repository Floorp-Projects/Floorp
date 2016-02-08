/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that we only break on offsets that are entry points for the line we are
 * breaking on. Bug 907278.
 */

var gDebuggee;
var gClient;
var gThreadClient;
var gCallback;

function run_test()
{
  run_test_with_server(DebuggerServer, function () {
    run_test_with_server(WorkerDebuggerServer, do_test_finished);
  });
  do_test_pending();
};

function run_test_with_server(aServer, aCallback)
{
  gCallback = aCallback;
  initTestDebuggerServer(aServer);
  gDebuggee = addTestGlobal("test-breakpoints", aServer);
  gDebuggee.console = { log: x => void x };
  gClient = new DebuggerClient(aServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient,
                           "test-breakpoints",
                           function (aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
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

function setBreakpoint(aEvent, aPacket) {
  let source = gThreadClient.source(aPacket.frame.where.source);
  gClient.addOneTimeListener("resumed", runCode);

  source.setBreakpoint({ line: 2 }, ({ error }) => {
    do_check_true(!error);
    gThreadClient.resume();
  });
}

function runCode() {
  gClient.addOneTimeListener("paused", testBPHit);
  gDebuggee.test();
}

function testBPHit(event, { why }) {
  do_check_eq(why.type, "breakpoint");
  gClient.addOneTimeListener("paused", testDbgStatement);
  gThreadClient.resume();
}

function testDbgStatement(event, { why }) {
  // Should continue to the debugger statement.
  do_check_eq(why.type, "debuggerStatement");
  // Not break on another offset from the same line (that isn't an entry point
  // to the line)
  do_check_neq(why.type, "breakpoint");
  gClient.close(gCallback);
}
