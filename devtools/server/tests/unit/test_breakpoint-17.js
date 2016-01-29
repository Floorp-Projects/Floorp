/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that when we add 2 breakpoints to the same line at different columns and
 * then remove one of them, we don't remove them both.
 */

var gDebuggee;
var gClient;
var gThreadClient;
var gCallback;

function run_test()
{
  run_test_with_server(DebuggerServer, do_test_finished);
  do_test_pending();
};

function run_test_with_server(aServer, aCallback)
{
  gCallback = aCallback;
  initTestDebuggerServer(aServer);
  gDebuggee = addTestGlobal("test-breakpoints", aServer);
  gClient = new DebuggerClient(aServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-breakpoints", function(aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      test_breakpoints_columns();
    });
  });
}

const code =
"(" + function (global) {
  global.foo = function () {
    Math.abs(-1); Math.log(0.5);
    debugger;
  };
  debugger;
} + "(this))";

const firstLocation = {
  line: 3,
  column: 4
};

const secondLocation = {
  line: 3,
  column: 18
};

function test_breakpoints_columns() {
  gClient.addOneTimeListener("paused", set_breakpoints);

  Components.utils.evalInSandbox(code, gDebuggee, "1.8", "http://example.com/", 1);
}

function set_breakpoints(aEvent, aPacket) {
  let first, second;
  let source = gThreadClient.source(aPacket.frame.where.source);

  source.setBreakpoint(firstLocation, function ({ error, actualLocation },
                                        aBreakpointClient) {
    do_check_true(!error, "Should not get an error setting the breakpoint");
    do_check_true(!actualLocation, "Should not get an actualLocation");
    first = aBreakpointClient;

    source.setBreakpoint(secondLocation, function ({ error, actualLocation },
                                                          aBreakpointClient) {
      do_check_true(!error, "Should not get an error setting the breakpoint");
      do_check_true(!actualLocation, "Should not get an actualLocation");
      second = aBreakpointClient;

      test_different_actors(first, second);
    });
  });
}

function test_different_actors(aFirst, aSecond) {
  do_check_neq(aFirst.actor, aSecond.actor,
               "Each breakpoint should have a different actor");
  test_remove_one(aFirst, aSecond);
}

function test_remove_one(aFirst, aSecond) {
  aFirst.remove(function ({error}) {
    do_check_true(!error, "Should not get an error removing a breakpoint");

    let hitSecond;
    gClient.addListener("paused", function _onPaused(aEvent, {why, frame}) {
      if (why.type == "breakpoint") {
        hitSecond = true;
        do_check_eq(why.actors.length, 1,
                    "Should only be paused because of one breakpoint actor");
        do_check_eq(why.actors[0], aSecond.actor,
                    "Should be paused because of the correct breakpoint actor");
        do_check_eq(frame.where.line, secondLocation.line,
                    "Should be at the right line");
        do_check_eq(frame.where.column, secondLocation.column,
                    "Should be at the right column");
        gThreadClient.resume();
        return;
      }

      if (why.type == "debuggerStatement") {
        gClient.removeListener("paused", _onPaused);
        do_check_true(hitSecond,
                      "We should still hit `second`, but not `first`.");

        gClient.close(gCallback);
        return;
      }

      do_check_true(false, "Should never get here");
    });

    gThreadClient.resume(() => gDebuggee.foo());
  });
}
