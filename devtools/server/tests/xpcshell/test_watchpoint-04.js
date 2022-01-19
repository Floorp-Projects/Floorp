/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that watchpoints ignore blackboxed sources
 */

const BLACK_BOXED_URL = "http://example.com/blackboxme.js";
const SOURCE_URL = "http://example.com/source.js";

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    info(`blackbox the source`);
    const { error, sources } = await threadFront.getSources();
    Assert.ok(!error, "Should not get an error: " + error);
    const sourceFront = threadFront.source(
      sources.filter(s => s.url == BLACK_BOXED_URL)[0]
    );

    await blackBox(sourceFront);

    await threadFront.resume();
    const packet = await executeOnNextTickAndWaitForPause(
      debuggee.runTest,
      threadFront
    );

    Assert.equal(
      packet.frame.where.line,
      3,
      "Paused at first debugger statement"
    );

    await addWatchpoint(threadFront, packet.frame, "obj", "a", "set");

    info(`Resume and skip the watchpoint`);
    const pausePacket = await resumeAndWaitForPause(threadFront);

    Assert.equal(
      pausePacket.frame.where.line,
      5,
      "Paused at second debugger statement"
    );

    await threadFront.resume();
  })
);

function evalCode(debuggee) {
  Cu.evalInSandbox(
    `function doStuff(obj) {
      obj.a = 2;
    }`,
    debuggee,
    "1.8",
    BLACK_BOXED_URL,
    1
  );
  Cu.evalInSandbox(
    `function runTest() {
      const obj = {a: 1}
      debugger
      doStuff(obj);
      debugger
    }; debugger;`,
    debuggee,
    "1.8",
    SOURCE_URL,
    1
  );
}
