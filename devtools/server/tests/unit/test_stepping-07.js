/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that stepping over an implicit return makes sense. Bug 1155966.
 */

var gDebuggee;
var gClient;
var gCallback;

function run_test() {
  do_test_pending();
  run_test_with_server(DebuggerServer, function () {
    run_test_with_server(WorkerDebuggerServer, do_test_finished);
  });
}

function run_test_with_server(aServer, aCallback) {
  gCallback = aCallback;
  initTestDebuggerServer(aServer);
  gDebuggee = addTestGlobal("test-stepping", aServer);
  gClient = new DebuggerClient(aServer.connectPipe());
  gClient.connect(testSteppingAndReturns);
}

const testSteppingAndReturns = Task.async(function* () {
  const [attachResponse, tabClient, threadClient] = yield attachTestTabAndResume(gClient, "test-stepping");
  ok(!attachResponse.error, "Should not get an error attaching");

  dumpn("Evaluating test code and waiting for first debugger statement");
  const dbgStmt1 = yield executeOnNextTickAndWaitForPause(evaluateTestCode, gClient);
  equal(dbgStmt1.frame.where.line, 3,
        "Should be at debugger statement on line 3");

  dumpn("Testing stepping with implicit return");
  const step1 = yield stepOver(gClient, threadClient);
  equal(step1.frame.where.line, 4, "Should step to line 4");
  const step2 = yield stepOver(gClient, threadClient);
  equal(step2.frame.where.line, 7,
        "Should step to line 7, the implicit return at the last line of the function");
  // This assertion doesn't pass yet. You would need to do *another*
  // step at the end of this function to get the frameFinished.
  // See bug 923975.
  //
  // ok(step2.why.frameFinished, "This should be the implicit function return");

  dumpn("Continuing and waiting for second debugger statement");
  const dbgStmt2 = yield resumeAndWaitForPause(gClient, threadClient);
  equal(dbgStmt2.frame.where.line, 12,
        "Should be at debugger statement on line 3");

  dumpn("Testing stepping with explicit return");
  const step3 = yield stepOver(gClient, threadClient);
  equal(step3.frame.where.line, 13, "Should step to line 13");
  const step4 = yield stepOver(gClient, threadClient);
  equal(step4.frame.where.line, 15, "Should step out of the function from line 15");
  // This step is a bit funny, see bug 1013219 for details.
  const step5 = yield stepOver(gClient, threadClient);
  equal(step5.frame.where.line, 15, "Should step out of the function from line 15");
  ok(step5.why.frameFinished, "This should be the explicit function return");

  finishClient(gClient, gCallback);
});

function evaluateTestCode() {
  Cu.evalInSandbox(
    `                                   //  1
    function implicitReturn() {         //  2
      debugger;                         //  3
      if (this.someUndefinedProperty) { //  4
        yikes();                        //  5
      }                                 //  6
    }                                   //  7
                                        //  8
    var yes = true;                     //  9
    function explicitReturn() {         // 10
      if (yes) {                        // 11
        debugger;                       // 12
        return 1;                       // 13
      }                                 // 14
    }                                   // 15
                                        // 16
    implicitReturn();                   // 17
    explicitReturn();                   // 18
    `,                                  // 19
    gDebuggee,
    "1.8",
    "test_stepping-07-test-code.js",
    1
  );
}
