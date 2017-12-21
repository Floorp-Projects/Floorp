/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Bug 1122064 - make sure that scripts introduced via onNewScripts
 * properly populate the `ScriptStore` with all there nested child
 * scripts, so you can set breakpoints on deeply nested scripts
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test() {
  run_test_with_server(DebuggerServer, function () {
    run_test_with_server(WorkerDebuggerServer, do_test_finished);
  });
  do_test_pending();
}

function run_test_with_server(server, callback) {
  initTestDebuggerServer(server);
  gDebuggee = addTestGlobal("test-breakpoints", server);
  gClient = new DebuggerClient(server.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient,
                           "test-breakpoints",
                           function (response, tabClient, threadClient) {
                             gThreadClient = threadClient;
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
    line: gDebuggee.line0 + 8
  };

  let [res, bpClient] = yield setBreakpoint(source, location);
  ok(!res.error);

  yield resume(gThreadClient);
  packet = yield waitForPause(gClient);
  Assert.equal(packet.type, "paused");
  Assert.equal(packet.why.type, "breakpoint");
  Assert.equal(packet.why.actors[0], bpClient.actor);
  Assert.equal(packet.frame.where.source.actor, source.actor);
  Assert.equal(packet.frame.where.line, location.line);

  yield resume(gThreadClient);
  finishClient(gClient);
});

/* eslint-disable */
function evalCode() {
  // Start a new script
  Components.utils.evalInSandbox(
    "var line0 = Error().lineNumber;\n(" + function () {
      debugger;
      var a = (function () {
        return (function () {
          return (function () {
            return (function () {
              return (function () {
                var x = 10; // This line gets a breakpoint
                return 1;
              })();
            })();
          })();
        })();
      })();
    } + ")()",
    gDebuggee
  );
}
