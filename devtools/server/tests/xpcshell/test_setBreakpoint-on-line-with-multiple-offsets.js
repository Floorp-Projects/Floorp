"use strict";

const SOURCE_URL = getFileUrl("setBreakpoint-on-line-with-multiple-offsets.js");

add_task(
  threadFrontTest(
    async ({ threadFront, debuggee }) => {
      const promise = waitForNewSource(threadFront, SOURCE_URL);
      loadSubScript(SOURCE_URL, debuggee);
      const { source } = await promise;
      const sourceFront = threadFront.source(source);

      const location = { sourceUrl: sourceFront.url, line: 4 };
      setBreakpoint(threadFront, location);

      let packet = await executeOnNextTickAndWaitForPause(function () {
        Cu.evalInSandbox("f()", debuggee);
      }, threadFront);
      let why = packet.why;
      let environment = await packet.frame.getEnvironment();
      Assert.equal(why.type, "breakpoint");
      Assert.equal(why.actors.length, 1);
      let frame = packet.frame;
      let where = frame.where;
      Assert.equal(where.actor, source.actor);
      Assert.equal(where.line, location.line);
      let variables = environment.bindings.variables;
      Assert.equal(variables.i.value.type, "undefined");

      const location2 = { sourceUrl: sourceFront.url, line: 7 };
      setBreakpoint(threadFront, location2);

      packet = await executeOnNextTickAndWaitForPause(
        () => resume(threadFront),
        threadFront
      );
      environment = await packet.frame.getEnvironment();
      why = packet.why;
      Assert.equal(why.type, "breakpoint");
      Assert.equal(why.actors.length, 1);
      frame = packet.frame;
      where = frame.where;
      Assert.equal(where.actor, source.actor);
      Assert.equal(where.line, location2.line);
      variables = environment.bindings.variables;
      Assert.equal(variables.i.value, 1);

      await resume(threadFront);
    },
    { doNotRunWorker: true }
  )
);
