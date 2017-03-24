/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that setting a breakpoint on a line without code will skip
 * forward when we know the script isn't GCed (the debugger is connected,
 * so it's kept alive).
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
    attachTestTabAndResume(gClient,
                           "test-stack",
                           function (response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             test_skip_breakpoint();
                           });
  });
}

var test_no_skip_breakpoint = Task.async(function* (source, location) {
  let [response, bpClient] = yield source.setBreakpoint(
    Object.assign({}, location, { noSliding: true })
  );

  do_check_true(!response.actualLocation);
  do_check_eq(bpClient.location.line, gDebuggee.line0 + 3);
  yield bpClient.remove();
});

var test_skip_breakpoint = function () {
  gThreadClient.addOneTimeListener("paused", Task.async(function* (event, packet) {
    let location = { line: gDebuggee.line0 + 3 };
    let source = gThreadClient.source(packet.frame.where.source);

    // First, make sure that we can disable sliding with the
    // `noSliding` option.
    yield test_no_skip_breakpoint(source, location);

    // Now make sure that the breakpoint properly slides forward one line.
    const [response, bpClient] = yield source.setBreakpoint(location);
    do_check_true(!!response.actualLocation);
    do_check_eq(response.actualLocation.source.actor, source.actor);
    do_check_eq(response.actualLocation.line, location.line + 1);

    gThreadClient.addOneTimeListener("paused", function (event, packet) {
      // Check the return value.
      do_check_eq(packet.type, "paused");
      do_check_eq(packet.frame.where.source.actor, source.actor);
      do_check_eq(packet.frame.where.line, location.line + 1);
      do_check_eq(packet.why.type, "breakpoint");
      do_check_eq(packet.why.actors[0], bpClient.actor);
      // Check that the breakpoint worked.
      do_check_eq(gDebuggee.a, 1);
      do_check_eq(gDebuggee.b, undefined);

      // Remove the breakpoint.
      bpClient.remove(function (response) {
        gThreadClient.resume(function () {
          gClient.close().then(gCallback);
        });
      });
    });

    gThreadClient.resume();
  }));

  // Use `evalInSandbox` to make the debugger treat it as normal
  // globally-scoped code, where breakpoint sliding rules apply.
  /* eslint-disable */
  Cu.evalInSandbox(
    "var line0 = Error().lineNumber;\n" +
    "debugger;\n" +      // line0 + 1
    "var a = 1;\n" +     // line0 + 2
    "// A comment.\n" +  // line0 + 3
    "var b = 2;",        // line0 + 4
    gDebuggee
  );
  /* eslint-enable */
};
