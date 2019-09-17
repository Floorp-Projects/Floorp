/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Test that when we add 2 breakpoints to the same line at different columns and
 * then remove one of them, we don't remove them both.
 */

const code =
  "(" +
  function(global) {
    global.foo = function() {
      Math.abs(-1);
      Math.log(0.5);
      debugger;
    };
    debugger;
  } +
  "(this))";

const firstLocation = {
  line: 3,
  column: 4,
};

const secondLocation = {
  line: 3,
  column: 18,
};

add_task(
  threadFrontTest(({ threadFront, debuggee }) => {
    return new Promise(resolve => {
      threadFront.on("paused", async packet => {
        const [first, second] = await set_breakpoints(packet, threadFront);
        test_different_actors(first, second);
        await test_remove_one(first, second, threadFront, debuggee);
        resolve();
      });

      Cu.evalInSandbox(code, debuggee, "1.8", "http://example.com/", 1);
    });
  })
);

async function set_breakpoints(packet, threadFront) {
  const source = await getSourceById(threadFront, packet.frame.where.actor);
  return new Promise(resolve => {
    let first, second;

    source
      .setBreakpoint(firstLocation)
      .then(function([{ actualLocation }, breakpointClient]) {
        Assert.ok(!actualLocation, "Should not get an actualLocation");
        first = breakpointClient;

        source
          .setBreakpoint(secondLocation)
          .then(function([{ actualLocation }, breakpointClient]) {
            Assert.ok(!actualLocation, "Should not get an actualLocation");
            second = breakpointClient;

            resolve([first, second]);
          });
      });
  });
}

function test_different_actors(first, second) {
  Assert.notEqual(
    first.actor,
    second.actor,
    "Each breakpoint should have a different actor"
  );
}

function test_remove_one(first, second, threadFront, debuggee) {
  return new Promise(resolve => {
    first.remove(function({ error }) {
      Assert.ok(!error, "Should not get an error removing a breakpoint");

      let hitSecond;
      threadFront.on("paused", function _onPaused({ why, frame }) {
        if (why.type == "breakpoint") {
          hitSecond = true;
          Assert.equal(
            why.actors.length,
            1,
            "Should only be paused because of one breakpoint actor"
          );
          Assert.equal(
            why.actors[0],
            second.actor,
            "Should be paused because of the correct breakpoint actor"
          );
          Assert.equal(
            frame.where.line,
            secondLocation.line,
            "Should be at the right line"
          );
          Assert.equal(
            frame.where.column,
            secondLocation.column,
            "Should be at the right column"
          );
          threadFront.resume();
          return;
        }

        if (why.type == "debuggerStatement") {
          threadFront.off("paused", _onPaused);
          Assert.ok(
            hitSecond,
            "We should still hit `second`, but not `first`."
          );

          resolve();
          return;
        }

        Assert.ok(false, "Should never get here");
      });

      threadFront.resume().then(() => debuggee.foo());
    });
  });
}
