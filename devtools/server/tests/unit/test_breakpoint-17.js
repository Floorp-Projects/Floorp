/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Test that when we add 2 breakpoints to the same line at different columns and
 * then remove one of them, we don't remove them both.
 */

const code =
"(" + function(global) {
  global.foo = function() {
    Math.abs(-1); Math.log(0.5);
    debugger;
  };
  debugger;
} + "(this))";

const firstLocation = {
  line: 3,
  column: 4,
};

const secondLocation = {
  line: 3,
  column: 18,
};

add_task(threadClientTest(({ threadClient, debuggee, client }) => {
  return new Promise(resolve => {
    client.addOneTimeListener("paused", async (event, packet) => {
      const [ first, second ] = await set_breakpoints(packet, threadClient);
      test_different_actors(first, second);
      await test_remove_one(first, second, threadClient, debuggee, client);
      resolve();
    });

    Cu.evalInSandbox(code, debuggee, "1.8", "http://example.com/", 1);
  });
}));

function set_breakpoints(packet, threadClient) {
  return new Promise(async resolve => {
    let first, second;
    const source = await getSourceById(
      threadClient,
      packet.frame.where.actor
    );

    source.setBreakpoint(firstLocation).then(function([{ actualLocation },
                                                       breakpointClient]) {
      Assert.ok(!actualLocation, "Should not get an actualLocation");
      first = breakpointClient;

      source.setBreakpoint(secondLocation).then(function([{ actualLocation },
                                                          breakpointClient]) {
        Assert.ok(!actualLocation, "Should not get an actualLocation");
        second = breakpointClient;

        resolve([first, second]);
      });
    });
  });
}

function test_different_actors(first, second) {
  Assert.notEqual(first.actor, second.actor,
                  "Each breakpoint should have a different actor");
}

function test_remove_one(first, second, threadClient, debuggee, client) {
  return new Promise(resolve => {
    first.remove(function({error}) {
      Assert.ok(!error, "Should not get an error removing a breakpoint");

      let hitSecond;
      client.addListener("paused", function _onPaused(event, {why, frame}) {
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
          threadClient.resume();
          return;
        }

        if (why.type == "debuggerStatement") {
          client.removeListener("paused", _onPaused);
          Assert.ok(hitSecond,
                    "We should still hit `second`, but not `first`.");

          resolve();
          return;
        }

        Assert.ok(false, "Should never get here");
      });

      threadClient.resume().then(() => debuggee.foo());
    });
  });
}
