/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test out of scope objects with synchronous functions.
 */

var gDebuggee;
var gThreadFront;

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    gThreadFront = threadFront;
    gDebuggee = debuggee;
    await testObjectGroup();
  })
);

function evalCode() {
  evalCallback(gDebuggee, function runTest() {
    const ugh = [];
    let i = 0;

    (function () {
      (function () {
        ugh.push(i++);
        debugger;
      })();
    })();

    debugger;
  });
}

const testObjectGroup = async function () {
  let packet = await executeOnNextTickAndWaitForPause(evalCode, gThreadFront);

  const environment = await packet.frame.getEnvironment();
  const ugh = environment.parent.parent.bindings.variables.ugh;
  const ughClient = await gThreadFront.pauseGrip(ugh.value);

  packet = await getPrototypeAndProperties(ughClient);
  packet = await resumeAndWaitForPause(gThreadFront);

  const environment2 = await packet.frame.getEnvironment();
  const ugh2 = environment2.bindings.variables.ugh;
  const ugh2Client = gThreadFront.pauseGrip(ugh2.value);

  packet = await getPrototypeAndProperties(ugh2Client);
  Assert.equal(packet.ownProperties.length.value, 1);

  await resume(gThreadFront);
};
