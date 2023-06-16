/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 * Check that you can step from one script or event to another
 */

add_task(
  threadFrontTest(async ({ commands, threadFront, debuggee }) => {
    Cu.evalInSandbox(
      `function blackboxed(callback) { return () => callback(); }`,
      debuggee,
      "1.8",
      "http://example.com/blackboxed.js",
      1
    );

    const { sources } = await getSources(threadFront);
    const blackboxedSourceFront = threadFront.source(
      sources.find(source => source.url == "http://example.com/blackboxed.js")
    );
    blackBox(blackboxedSourceFront);

    const testStepping = async function (wrapperName, stepHandler, message) {
      commands.scriptCommand.execute(`(function () {
          const p = Promise.resolve();
          p.then(${wrapperName}(() => { debugger; }))
          .then(${wrapperName}(() => { }));
        })();`);

      await waitForEvent(threadFront, "paused");
      const step = await stepHandler(threadFront);
      Assert.equal(step.frame.where.line, 4, message);
      await resume(threadFront);
    };

    const stepTwice = async function () {
      await stepOver(threadFront);
      return stepOver(threadFront);
    };

    await testStepping("", stepTwice, "Step over on the outermost frame");
    await testStepping("blackboxed", stepTwice, "Step over with blackboxing");
    await testStepping("", stepOut, "Step out on the outermost frame");
    await testStepping("blackboxed", stepOut, "Step out with blackboxing");

    commands.scriptCommand.execute(`(async function () {
        const p = Promise.resolve();
        const p2 = p.then(() => {
          debugger;
          return "async stepping!";
        });
        debugger;
        await p;
        const result = await p2;
        return result;
      })();
      `);

    await waitForEvent(threadFront, "paused");
    await stepOver(threadFront);
    await stepOver(threadFront);
    const step = await stepOut(threadFront);
    await resume(threadFront);
    Assert.equal(step.frame.where.line, 9, "Step out of promise into async fn");
  })
);
