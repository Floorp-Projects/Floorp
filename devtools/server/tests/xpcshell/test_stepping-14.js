/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 * Check that is possible to step into both the inner and outer function
 * calls.
 */

add_task(
  threadFrontTest(async ({ commands, threadFront }) => {
    dumpn("Evaluating test code and waiting for first debugger statement");

    commands.scriptCommand.execute(`(function () {
        async function f() {
          const p = Promise.resolve(43);
          await p;
          return p;
        }

        function call_f() {
          Promise.resolve(42).then(forty_two => {
            return forty_two;
          });

          f().then(v => {
            return v;
          });
        }
        debugger;
        call_f();
      })()`);

    const packet = await waitForEvent(threadFront, "paused");
    const location = {
      sourceId: packet.frame.where.actor,
      line: 4,
      column: 10,
    };

    await threadFront.setBreakpoint(location, {});

    const packet2 = await resumeAndWaitForPause(threadFront);
    Assert.equal(packet2.frame.where.line, 4, "landed at await");

    const packet3 = await stepIn(threadFront);
    Assert.equal(packet3.frame.where.line, 5, "step to the next line");

    await threadFront.resume();
  })
);
