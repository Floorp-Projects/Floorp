/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Test that when we add 2 breakpoints to the same line at different columns and
 * then remove one of them, we don't remove them both.
 */

var gDebuggee;
var gClient;
var gThreadClient;
var gCallback;

function run_test() {
  run_test_with_server(DebuggerServer, do_test_finished);
  do_test_pending();
}

function run_test_with_server(server, callback) {
  gCallback = callback;
  initTestDebuggerServer(server);
  gDebuggee = addTestGlobal("test-breakpoints", server);
  gClient = new DebuggerClient(server.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-breakpoints",
                           function (response, tabClient, threadClient) {
                             gThreadClient = threadClient;
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

function set_breakpoints(event, packet) {
  let first, second;
  let source = gThreadClient.source(packet.frame.where.source);

  source.setBreakpoint(firstLocation, function ({ error, actualLocation },
                                        breakpointClient) {
    Assert.ok(!error, "Should not get an error setting the breakpoint");
    Assert.ok(!actualLocation, "Should not get an actualLocation");
    first = breakpointClient;

    source.setBreakpoint(secondLocation, function ({ error, actualLocation },
                                                          breakpointClient) {
      Assert.ok(!error, "Should not get an error setting the breakpoint");
      Assert.ok(!actualLocation, "Should not get an actualLocation");
      second = breakpointClient;

      test_different_actors(first, second);
    });
  });
}

function test_different_actors(first, second) {
  Assert.notEqual(first.actor, second.actor,
                  "Each breakpoint should have a different actor");
  test_remove_one(first, second);
}

function test_remove_one(first, second) {
  first.remove(function ({error}) {
    Assert.ok(!error, "Should not get an error removing a breakpoint");

    let hitSecond;
    gClient.addListener("paused", function _onPaused(event, {why, frame}) {
      if (why.type == "breakpoint") {
        hitSecond = true;
        Assert.equal(why.actors.length, 1,
                     "Should only be paused because of one breakpoint actor");
        Assert.equal(why.actors[0], second.actor,
                     "Should be paused because of the correct breakpoint actor");
        Assert.equal(frame.where.line, secondLocation.line,
                     "Should be at the right line");
        Assert.equal(frame.where.column, secondLocation.column,
                     "Should be at the right column");
        gThreadClient.resume();
        return;
      }

      if (why.type == "debuggerStatement") {
        gClient.removeListener("paused", _onPaused);
        Assert.ok(hitSecond,
                  "We should still hit `second`, but not `first`.");

        gClient.close().then(gCallback);
        return;
      }

      Assert.ok(false, "Should never get here");
    });

    gThreadClient.resume(() => gDebuggee.foo());
  });
}
