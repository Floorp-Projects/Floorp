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
  run_test_with_server(DebuggerServer, function() {
    run_test_with_server(WorkerDebuggerServer, do_test_finished);
  });
  do_test_pending();
}

function run_test_with_server(server, callback) {
  gCallback = callback;
  initTestDebuggerServer(server);
  gDebuggee = addTestGlobal("test-stack", server);
  gClient = new DebuggerClient(server.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient,
                           "test-stack",
                           function(response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             test_skip_breakpoint();
                           });
  });
}

var test_no_skip_breakpoint = async function(source, location) {
  const [response, bpClient] = await source.setBreakpoint(
    Object.assign({}, location, { noSliding: true })
  );

  Assert.ok(!response.actualLocation);
  Assert.equal(bpClient.location.line, gDebuggee.line0 + 3);
  await bpClient.remove();
};

var test_skip_breakpoint = function() {
  gThreadClient.addOneTimeListener("paused", async function(event, packet) {
    const location = { line: gDebuggee.line0 + 3 };
    const source = gThreadClient.source(packet.frame.where.source);

    // First, make sure that we can disable sliding with the
    // `noSliding` option.
    await test_no_skip_breakpoint(source, location);

    // Now make sure that the breakpoint properly slides forward one line.
    const [response, bpClient] = await source.setBreakpoint(location);
    Assert.ok(!!response.actualLocation);
    Assert.equal(response.actualLocation.source.actor, source.actor);
    Assert.equal(response.actualLocation.line, location.line + 1);

    gThreadClient.addOneTimeListener("paused", function(event, packet) {
      // Check the return value.
      Assert.equal(packet.type, "paused");
      Assert.equal(packet.frame.where.source.actor, source.actor);
      Assert.equal(packet.frame.where.line, location.line + 1);
      Assert.equal(packet.why.type, "breakpoint");
      Assert.equal(packet.why.actors[0], bpClient.actor);
      // Check that the breakpoint worked.
      Assert.equal(gDebuggee.a, 1);
      Assert.equal(gDebuggee.b, undefined);

      // Remove the breakpoint.
      bpClient.remove(function(response) {
        gThreadClient.resume(function() {
          gClient.close().then(gCallback);
        });
      });
    });

    gThreadClient.resume();
  });

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
