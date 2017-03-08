/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 1333219 - make that setBreakpoint fails when script is not found
 * at the specified line.
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
}

function run_test_with_server(aServer, aCallback)
{
  gCallback = aCallback;
  initTestDebuggerServer(aServer);
  gDebuggee = addTestGlobal("test-breakpoints", aServer);
  gClient = new DebuggerClient(aServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient,
                           "test-breakpoints",
                           function (aResponse, aTabClient, aThreadClient) {
                             gThreadClient = aThreadClient;
                             test();
                           });
  });
}

const test = Task.async(function* () {
  // Populate the `ScriptStore` so that we only test that the script
  // is added through `onNewScript`
  yield getSources(gThreadClient);

  let packet = yield executeOnNextTickAndWaitForPause(evalCode, gClient);
  let source = gThreadClient.source(packet.frame.where.source);
  let location = {
    line: gDebuggee.line0 + 2
  };

  let [res, bpClient] = yield setBreakpoint(source, location);
  ok(!res.error);

  let location2 = {
    line: gDebuggee.line0 + 5
  };

  yield source.setBreakpoint(location2).then(_ => {
    do_throw("no code shall not be found the specified line or below it");
  }, reason => {
    do_check_eq(reason.error, "noCodeAtLineColumn");
    ok(reason.message);
  });

  yield resume(gThreadClient);
  finishClient(gClient);
});

function evalCode() {
  // Start a new script
  Components.utils.evalInSandbox(`
var line0 = Error().lineNumber;
function some_function() {
  // breakpoint is valid here -- it slides one line below (line0 + 2)
}
debugger;
// no breakpoint is allowed here (line0 + 5)
`, gDebuggee);
}
